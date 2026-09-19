#include "LanessaWalkPointsWidget.h"
#include "LanessaCustomShapes.h"
#include "LanessaLineIcon.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/CoreStyle.h"
#include "HAL/PlatformFileManager.h"
#include "Fonts/CompositeFont.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

// Private palette/font duplicate, same arrangement as InteriorV2 in LanessaInteriorTourWidget.cpp:
// each is `static`, so there is no ODR conflict across translation units, and keeping it local
// avoids a cross-file refactor of the already-shipping v2 HUD just to share eight constants.
namespace WalkV2
{
	static const FLinearColor Paper(1.f, 1.f, 1.f, 1.f);
	static const FLinearColor OliveGlow(0.788f, 0.788f, 0.604f, 1.f);   // #c9c99a - selected pill fill
	static const FLinearColor TextDim(1.f, 1.f, 1.f, 0.56f);
	static const FLinearColor Transparent(0.f, 0.f, 0.f, 0.f);
	static const FLinearColor IconInk(0.039f, 0.039f, 0.031f, 1.f);     // #0a0a08 - text on olive
	static const FLinearColor PanelPill(0.f, 0.f, 0.f, 0.55f);          // the bar's own translucent pill
	static const FLinearColor CircleDark(0.078f, 0.078f, 0.063f, 0.92f); // #141410-ish, exit circle

	static FSlateFontInfo LoadSystemFont(const FString& FilePath, int32 Size, bool bBold)
	{
		static TMap<FString, TSharedPtr<FStandaloneCompositeFont>> Cache;
		if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*FilePath))
		{
			return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
		}
		TSharedPtr<FStandaloneCompositeFont>& Entry = Cache.FindOrAdd(FilePath);
		if (!Entry.IsValid())
		{
			Entry = MakeShared<FStandaloneCompositeFont>(FName("Face"), FilePath, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
		}
		return FSlateFontInfo(Entry, (float)Size, FName("Face"));
	}
	static FSlateFontInfo F(int32 Size, bool bBold = false)
	{
		return LoadSystemFont(bBold ? TEXT("C:/Windows/Fonts/georgiab.ttf") : TEXT("C:/Windows/Fonts/georgia.ttf"), Size, bBold);
	}
}

void ULanessaWalkPointsWidget::SetPoints(const TArray<FString>& InPointIds, const TArray<FString>& InPointLabels)
{
	if (InPointIds.Num() != InPointLabels.Num())
	{
		// Half-applying would paint out of bounds; the caller's arrays came from one loop over the
		// same PlayerStart set, so a length mismatch means that loop is broken, not that we should cope.
		UE_LOG(LogTemp, Warning, TEXT("[LanessaWalk] SetPoints ignored: %d ids vs %d labels"),
			InPointIds.Num(), InPointLabels.Num());
		return;
	}

	PointIds = InPointIds;
	PointLabels = InPointLabels;
	// See ULanessaInteriorTourWidget::SetRooms - BP's own ids have no relation to the actors a
	// previous PopulateFromPlayerStarts cached, so the map is dropped rather than risking a
	// teleport to a stale actor on a name collision.
	PointActors.Reset();

	// Keep the highlight only if that id survived the refresh - otherwise start on the first point so
	// the bar is never drawn with nothing lit while the pawn is demonstrably standing somewhere.
	if (!PointIds.Contains(CurrentPointId))
	{
		CurrentPointId = PointIds.Num() > 0 ? PointIds[0] : FString();
	}

	RefreshPointRow();
	Invalidate(EInvalidateWidgetReason::Layout);
}

void ULanessaWalkPointsWidget::SetCurrentPoint(const FString& PointId)
{
	CurrentPointId = PointId;
	Invalidate(EInvalidateWidgetReason::Paint);
}

int32 ULanessaWalkPointsWidget::PopulateFromPlayerStarts(UObject* WorldContextObject, FName RequiredTag)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		World = GetWorld();   // called with a null context from a widget graph is common enough to cover
	}
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LanessaWalk] PopulateFromPlayerStarts: no world"));
		return 0;
	}

	// Collected as (label, id, actor) so the bar can be ordered by what the user actually authored.
	struct FEntry { FString Label; FString Id; TWeakObjectPtr<AActor> Actor; };
	TArray<FEntry> Entries;

	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		APlayerStart* Start = *It;
		if (!IsValid(Start)) { continue; }

		// RequiredTag == None means "take every PlayerStart" - an explicit opt-out for a level that
		// has only walk starts, rather than making callers invent a tag they do not need.
		if (!RequiredTag.IsNone() && !Start->ActorHasTag(RequiredTag)) { continue; }

		const FName StartTag = Start->PlayerStartTag;
		FString Label = StartTag.IsNone() ? FString() : StartTag.ToString();
		if (Label.IsEmpty() || Label == TEXT("None"))
		{
			Label = Start->GetName();
		}

		Entries.Add({ Label, Start->GetName(), Start });
	}

	// TActorIterator order is spawn/registration order, which is not stable across loads - sorting by
	// the authored label makes the bar deterministic, and gives the user a way to control the order
	// (prefix the Player Start Tag) without needing another field.
	Entries.Sort([](const FEntry& A, const FEntry& B) { return A.Label < B.Label; });

	PointIds.Reset();
	PointLabels.Reset();
	PointActors.Reset();
	for (const FEntry& E : Entries)
	{
		PointIds.Add(E.Id);
		PointLabels.Add(E.Label);
		PointActors.Add(E.Id, E.Actor);
	}

	if (!PointIds.Contains(CurrentPointId))
	{
		CurrentPointId = PointIds.Num() > 0 ? PointIds[0] : FString();
	}

	UE_LOG(LogTemp, Log, TEXT("[LanessaWalk] PopulateFromPlayerStarts(tag='%s') -> %d point(s)"),
		*RequiredTag.ToString(), PointIds.Num());

	RefreshPointRow();
	Invalidate(EInvalidateWidgetReason::Layout);
	return PointIds.Num();
}

bool ULanessaWalkPointsWidget::TeleportToPoint(UObject* WorldContextObject, const FString& PointId)
{
	const TWeakObjectPtr<AActor>* Found = PointActors.Find(PointId);
	AActor* Target = Found ? Found->Get() : nullptr;
	if (!Target)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LanessaWalk] TeleportToPoint: unknown or unloaded point '%s'"), *PointId);
		return false;
	}

	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World) { World = GetWorld(); }
	APlayerController* PC = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LanessaWalk] TeleportToPoint: no possessed pawn"));
		return false;
	}

	// TeleportPhysics, not a plain SetActorLocation: the walk pawn has a movement component, and a
	// non-teleport move would sweep it through whatever is between here and the target room.
	const bool bMoved = Pawn->SetActorLocationAndRotation(
		Target->GetActorLocation(), Target->GetActorRotation(), /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);

	// The controller keeps its own rotation for the camera; without this the view stays pointing
	// wherever it was and the new spawn point's framing is lost.
	if (bMoved) { PC->SetControlRotation(Target->GetActorRotation()); }

	return bMoved;
}

FText ULanessaWalkPointsWidget::GetCurrentPointLabel() const
{
	const int32 Idx = PointIds.IndexOfByKey(CurrentPointId);
	return PointLabels.IsValidIndex(Idx) ? FText::FromString(PointLabels[Idx]) : FText::GetEmpty();
}

// The bottom pill row. Clicking a pill highlights it locally and broadcasts OnPointSelected -
// BP_Explorer_PC is expected to teleport the walk pawn to that PlayerStart, mirroring exactly what
// the interior bar does for rooms. The widget deliberately moves its own highlight first rather than
// waiting for BP to call SetCurrentPoint back: the click should feel instant even if the teleport
// takes a frame, and BP is still free to correct it if the move is refused.
TSharedRef<SWidget> ULanessaWalkPointsWidget::BuildPointBar()
{
	using namespace WalkV2;

	PointRow = SNew(SHorizontalBox);
	RefreshPointRow();

	return SNew(SBox).HeightOverride(38.f)
	[
		SNew(SLanessaCutBorder).CutSize(19.f).FillColor(PanelPill)
		.Content()
		[
			SNew(SBox).Padding(4.f, 2.f)[PointRow.ToSharedRef()]
		]
	];
}

void ULanessaWalkPointsWidget::RefreshPointRow()
{
	using namespace WalkV2;

	if (!PointRow.IsValid()) { return; }   // bar not built yet - BuildPointBar will call us itself
	PointRow->ClearChildren();

	TSharedRef<SHorizontalBox> Row = PointRow.ToSharedRef();
	for (int32 i = 0; i < PointIds.Num(); i++)
	{
		const FString PointId = PointIds[i];
		const FString Label = FString::Printf(TEXT("%d. %s"), i + 1, *PointLabels[i]);

		TAttribute<FLinearColor> BgColor = TAttribute<FLinearColor>::Create(
			[this, PointId]() { return CurrentPointId == PointId ? OliveGlow : Transparent; });
		TAttribute<FSlateColor> TxtColor = TAttribute<FSlateColor>::Create(
			[this, PointId]() { return FSlateColor(CurrentPointId == PointId ? IconInk : Paper); });

		Row->AddSlot().AutoWidth().Padding(2.f, 0.f)
		[
			SNew(SBox).HeightOverride(34.f)
			[
				SNew(SLanessaCutBorder).CutSize(17.f).FillColor(BgColor)
				.HoverColor(FLinearColor(1.f, 1.f, 1.f, 0.10f)).bAnimateHover(true)
				.OnClicked(FSimpleDelegate::CreateLambda([this, PointId]()
				{
					SetCurrentPoint(PointId);
					// Move first, then tell BP: a handler that opens a fade or checks state should see
					// the pawn already at the destination, the same order a BP-driven jump would give.
					if (bTeleportOnClick) { TeleportToPoint(this, PointId); }
					OnPointSelected.Broadcast(PointId);
				}))
				.Content()
				[
					SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(18.f, 0.f)
					[SNew(STextBlock).Text(FText::FromString(Label)).Font(F(11)).ColorAndOpacity(TxtColor)]
				]
			]
		];
	}
}

// Top-left corner label: the mode name over the current point's name, matching the interior tour's
// own corner label so the two modes read as the same product.
TSharedRef<SWidget> ULanessaWalkPointsWidget::BuildCornerLabel()
{
	using namespace WalkV2;

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock)
			.Text(TAttribute<FText>::Create([this]() { return GetCurrentPointLabel(); }))
			.Font(F(20, true)).ColorAndOpacity(FSlateColor(Paper))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
		[
			SNew(STextBlock).Text(FText::FromString(TEXT("SAYR"))).Font(F(8)).ColorAndOpacity(FSlateColor(TextDim))
		];
}

// Top-right exit button: the same 42x42 dark circle with a stroked "X" that the interior tour's
// corner cluster ends with, so leaving either mode looks and sits identically. Only the close button
// is here - the interior's map and expand siblings have no meaning for free-walk.
//
// The X is built from two crossing lines rather than reusing LanessaIcons::Exit(), which is the
// door/arrow "log out" glyph used by the nav rail - visually a different gesture from closing an
// overlay, and the interior already established the X for this exact spot.
TSharedRef<SWidget> ULanessaWalkPointsWidget::BuildExitButton()
{
	using namespace WalkV2;

	TArray<FLanessaIconPrim> CloseIcon;
	CloseIcon.Add(FLanessaIconPrim::MakeLine({ {5, 5}, {19, 19} }));
	CloseIcon.Add(FLanessaIconPrim::MakeLine({ {19, 5}, {5, 19} }));

	return SNew(SBox).WidthOverride(42.f).HeightOverride(42.f)
	[
		SNew(SLanessaCutBorder).CutSize(0.f).FillColor(CircleDark)
		.HoverColor(FLinearColor(1.f, 1.f, 1.f, 0.12f)).bAnimateHover(true)
		.OnClicked(FSimpleDelegate::CreateLambda([this]() { OnExitClicked.Broadcast(); }))
		.Content()
		[
			SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(11.f)
			[SNew(SLanessaLineIcon).Primitives(CloseIcon).StrokeColor(Paper).StrokeWidth(1.7f)]
		]
	];
}

TSharedRef<SWidget> ULanessaWalkPointsWidget::RebuildWidget()
{
	// No mock list here, unlike the interior tour's design-only pass: this bar is driven from real
	// PlayerStarts from the start, and showing invented point names would make an empty/misconfigured
	// tag set look like it is working.
	return SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(24.f)
		[
			SNew(SBox)
			.Visibility(TAttribute<EVisibility>::Create(
				[this]() { return PointIds.Num() > 0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed; }))
			[BuildCornerLabel()]
		]
		// Deliberately NOT gated on PointIds.Num(): if the level's walk PlayerStarts are untagged or
		// missing, the bar collapses - hiding the exit too would strand the user in walk mode with no
		// way back, since entering it hides the v2 nav rail.
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(24.f)
		[BuildExitButton()]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, 32.f)
		[
			SNew(SBox)
			.Visibility(TAttribute<EVisibility>::Create(
				[this]() { return PointIds.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed; }))
			[BuildPointBar()]
		];
}
