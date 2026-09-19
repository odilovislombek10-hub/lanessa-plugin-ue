#include "LanessaInteriorTourWidget.h"
#include "LanessaCustomShapes.h"
#include "LanessaLineIcon.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "HAL/PlatformFileManager.h"
#include "Fonts/CompositeFont.h"
#include "Brushes/SlateImageBrush.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

// Same palette/font-loading approach as LanessaV2Widget.h's V2 namespace (kept as a private
// duplicate rather than a shared header - each is `static`, so no ODR conflict across the two
// translation units, and it avoids risking a cross-file refactor of the already-shipping v2 HUD).
namespace InteriorV2
{
	static const FLinearColor Paper(1.f, 1.f, 1.f, 1.f);
	static const FLinearColor OliveGlow(0.788f, 0.788f, 0.604f, 1.f);   // #c9c99a
	static const FLinearColor TextDim(1.f, 1.f, 1.f, 0.56f);
	static const FLinearColor Transparent(0.f, 0.f, 0.f, 0.f);
	static const FLinearColor IconInk(0.039f, 0.039f, 0.031f, 1.f);     // #0a0a08 (used on olive fills)
	static const FLinearColor PanelSolid(0.039f, 0.039f, 0.035f, 1.f);  // #0a0a09
	static const FLinearColor PanelPill(0.f, 0.f, 0.f, 0.55f);          // room bar's own translucent pill
	static const FLinearColor CircleDark(0.078f, 0.078f, 0.063f, 0.92f); // #141410-ish, top-right dark circles

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

// A tiny SLeafWidget that draws one FLanessaIconPrim set as filled lines/circles - same tessellation
// approach as the icons already in BuildUtility()/BuildRail(), just factored out here as its own
// reusable leaf so this file doesn't need to duplicate a whole custom-vert painter per icon use.
class SLanessaInteriorIcon : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SLanessaInteriorIcon)
		: _Color(FLinearColor::White)
		, _StrokeWidth(1.6f)
		{}
		SLATE_ARGUMENT(TArray<FLanessaIconPrim>, Prims)
		SLATE_ATTRIBUTE(FLinearColor, Color)
		SLATE_ARGUMENT(float, StrokeWidth)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Prims = InArgs._Prims;
		Color = InArgs._Color;
		StrokeWidth = InArgs._StrokeWidth;
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		const FVector2D Size = AllottedGeometry.GetLocalSize();
		const float Scale = FMath::Min(Size.X, Size.Y) / 24.f;
		const FLinearColor Col = Color.Get();
		for (const FLanessaIconPrim& Prim : Prims)
		{
			if (Prim.Type == FLanessaIconPrim::EType::Circle)
			{
				TArray<FVector2D> Pts;
				const int32 Segs = 24;
				for (int32 i = 0; i <= Segs; i++)
				{
					const float A = (float)i / Segs * 2.f * PI;
					Pts.Add((Prim.Center + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Prim.Radius) * Scale);
				}
				FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Col, true, StrokeWidth);
			}
			else
			{
				TArray<FVector2D> Pts;
				for (const FVector2D& P : Prim.Points) { Pts.Add(P * Scale); }
				FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Col, true, StrokeWidth);
			}
		}
		return LayerId + 1;
	}

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(24.f, 24.f); }

private:
	TArray<FLanessaIconPrim> Prims;
	TAttribute<FLinearColor> Color;
	float StrokeWidth = 1.6f;
};

// Simple map-pin: a ring (the head) + a downward-pointing triangle (the tip) - not traced from any
// source SVG (the reference screenshots don't expose their markup), built plain in the same
// primitive language as LanessaIcons:: so it sits visually consistent with the rest of the HUD.
static TArray<FLanessaIconPrim> MakeMapPinIcon()
{
	TArray<FLanessaIconPrim> R;
	R.Add(FLanessaIconPrim::MakeCircle({ 12, 9 }, 5.f));
	R.Add(FLanessaIconPrim::MakeLine({ {8,12}, {12,20}, {16,12}, {8,12} }));
	return R;
}
// Four corner brackets - the common "expand/fullscreen" glyph, built from straight lines only so it
// needs no arc-tessellation.
static TArray<FLanessaIconPrim> MakeExpandIcon()
{
	return {
		FLanessaIconPrim::MakeLine({ {2,8}, {2,2}, {8,2} }),
		FLanessaIconPrim::MakeLine({ {16,2}, {22,2}, {22,8} }),
		FLanessaIconPrim::MakeLine({ {22,16}, {22,22}, {16,22} }),
		FLanessaIconPrim::MakeLine({ {8,22}, {2,22}, {2,16} }),
	};
}
// Plain X - the exit button's close glyph.
static TArray<FLanessaIconPrim> MakeCloseIcon()
{
	return {
		FLanessaIconPrim::MakeLine({ {5,5}, {19,19} }),
		FLanessaIconPrim::MakeLine({ {19,5}, {5,19} }),
	};
}

void ULanessaInteriorTourWidget::SetRooms(const TArray<FString>& InRoomIds, const TArray<FString>& InRoomLabels)
{
	if (InRoomIds.Num() != InRoomLabels.Num())
	{
		// Both arrays are indexed together below and by BuildRoomBar - a shorter label array would
		// read past its end. They come from one loop over the same room set, so a length mismatch
		// means that loop is broken; rejecting says so instead of crashing or silently truncating.
		UE_LOG(LogTemp, Warning, TEXT("[LanessaInterior] SetRooms ignored: %d ids vs %d labels"),
			InRoomIds.Num(), InRoomLabels.Num());
		return;
	}

	// A manual SetRooms supplies BP's own ids, which have no relation to the actors a previous
	// PopulateFromPlayerStarts cached - the map is rebuilt below from whatever this list resolves to.
	RoomActors.Reset();
	RoomIds.Reset();
	RoomLabels.Reset();

	// Resolve each incoming entry against the level's PlayerStarts so the tag test can run and the
	// teleport gets a real actor. BP identifies rooms by their Player Start Tag, so that is the
	// primary key; the actor name is accepted too, for callers that pass GetDisplayName().
	UWorld* World = GetWorld();
	TMap<FString, APlayerStart*> ByKey;
	if (World)
	{
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			APlayerStart* Start = *It;
			if (!IsValid(Start)) { continue; }
			const FName StartTag = Start->PlayerStartTag;
			if (!StartTag.IsNone()) { ByKey.Add(StartTag.ToString().ToLower(), Start); }
			ByKey.Add(Start->GetName().ToLower(), Start);
		}
	}

	int32 Dropped = 0;
	for (int32 i = 0; i < InRoomIds.Num(); i++)
	{
		APlayerStart** Found = ByKey.Find(InRoomIds[i].ToLower());
		if (!Found) { Found = ByKey.Find(InRoomLabels[i].ToLower()); }
		APlayerStart* Start = Found ? *Found : nullptr;

		// Unresolvable entries are kept - this list may come from room markers or a data table, and
		// dropping those would break a caller that never used PlayerStarts in the first place.
		if (Start && !AutoFilterTag.IsNone() && !Start->ActorHasTag(AutoFilterTag))
		{
			Dropped++;
			continue;
		}

		RoomIds.Add(InRoomIds[i]);
		RoomLabels.Add(InRoomLabels[i]);
		if (Start) { RoomActors.Add(InRoomIds[i], Start); }
	}

	if (Dropped > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[LanessaInterior] SetRooms: kept %d, dropped %d without tag '%s'"),
			RoomIds.Num(), Dropped, *AutoFilterTag.ToString());
	}

	if (!RoomIds.Contains(CurrentRoomId))
	{
		CurrentRoomId = RoomIds.Num() > 0 ? RoomIds[0] : FString();
	}
	RefreshRoomRow();
	Invalidate(EInvalidateWidgetReason::Layout);
}

void ULanessaInteriorTourWidget::SetCurrentRoom(const FString& RoomId)
{
	CurrentRoomId = RoomId;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void ULanessaInteriorTourWidget::SetPlanImage(UTexture2D* Texture)
{
	PlanTexture = Texture;
	Invalidate(EInvalidateWidgetReason::Layout);
}

int32 ULanessaInteriorTourWidget::PopulateFromPlayerStarts(UObject* WorldContextObject, FName RequiredTag)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World) { World = GetWorld(); }
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LanessaInterior] PopulateFromPlayerStarts: no world"));
		return 0;
	}

	struct FEntry { FString Label; FString Id; TWeakObjectPtr<AActor> Actor; };
	TArray<FEntry> Entries;

	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		APlayerStart* Start = *It;
		if (!IsValid(Start)) { continue; }
		// None means "no filter" - an explicit opt-out for a streamed interior whose level contains
		// nothing but room starts, rather than forcing a tag nobody needs.
		if (!RequiredTag.IsNone() && !Start->ActorHasTag(RequiredTag)) { continue; }

		const FName StartTag = Start->PlayerStartTag;
		FString Label = StartTag.IsNone() ? FString() : StartTag.ToString();
		if (Label.IsEmpty() || Label == TEXT("None")) { Label = Start->GetName(); }

		Entries.Add({ Label, Start->GetName(), Start });
	}

	// Deterministic order: TActorIterator follows registration order, which shifts between loads.
	// Sorting on the authored label also gives the user order control without another field.
	Entries.Sort([](const FEntry& A, const FEntry& B) { return A.Label < B.Label; });

	RoomIds.Reset();
	RoomLabels.Reset();
	RoomActors.Reset();
	for (const FEntry& E : Entries)
	{
		RoomIds.Add(E.Id);
		RoomLabels.Add(E.Label);
		RoomActors.Add(E.Id, E.Actor);
	}

	if (!RoomIds.Contains(CurrentRoomId))
	{
		CurrentRoomId = RoomIds.Num() > 0 ? RoomIds[0] : FString();
	}

	UE_LOG(LogTemp, Log, TEXT("[LanessaInterior] PopulateFromPlayerStarts(tag='%s') -> %d room(s)"),
		*RequiredTag.ToString(), RoomIds.Num());

	RefreshRoomRow();
	Invalidate(EInvalidateWidgetReason::Layout);
	return RoomIds.Num();
}

bool ULanessaInteriorTourWidget::TeleportToRoom(UObject* WorldContextObject, const FString& RoomId)
{
	const TWeakObjectPtr<AActor>* Found = RoomActors.Find(RoomId);
	AActor* Target = Found ? Found->Get() : nullptr;
	if (!Target)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LanessaInterior] TeleportToRoom: unknown or unloaded room '%s'"), *RoomId);
		return false;
	}

	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World) { World = GetWorld(); }
	APlayerController* PC = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LanessaInterior] TeleportToRoom: no possessed pawn"));
		return false;
	}

	// TeleportPhysics so the walkthrough pawn's movement component does not sweep it through the
	// walls between the old room and the new one.
	const bool bMoved = Pawn->SetActorLocationAndRotation(
		Target->GetActorLocation(), Target->GetActorRotation(), /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
	if (bMoved) { PC->SetControlRotation(Target->GetActorRotation()); }
	return bMoved;
}

void ULanessaInteriorTourWidget::TogglePlan()
{
	bPlanOpen = !bPlanOpen;
	Invalidate(EInvalidateWidgetReason::Paint);
}

FText ULanessaInteriorTourWidget::GetCurrentRoomLabel() const
{
	const int32 Idx = RoomIds.IndexOfByKey(CurrentRoomId);
	return RoomLabels.IsValidIndex(Idx) ? FText::FromString(RoomLabels[Idx]) : FText::GetEmpty();
}

// .room-bar { position:absolute; bottom:32; left:50%; translateX(-50%); border-radius:999px } - the
// bottom pill bar from the reference screenshot's "1. Holl  2. Yotoqxona  3. Sanuzel ..." row.
// Clicking a pill broadcasts OnRoomSelected - BP_Explorer_PC is expected to teleport the walkthrough
// pawn to that room's own spawn point (mirrors the qirqim floor-rail's click-behaves-like-the-3D-icon
// pattern: this bar and the plan popup's room markers are meant to do the exact same thing).
TSharedRef<SWidget> ULanessaInteriorTourWidget::BuildRoomBar()
{
	using namespace InteriorV2;

	RoomRow = SNew(SHorizontalBox);
	RefreshRoomRow();

	return SNew(SBox).HeightOverride(38.f)
	[
		SNew(SLanessaCutBorder).CutSize(19.f).FillColor(PanelPill)
		.Content()
		[
			SNew(SBox).Padding(4.f, 2.f)[RoomRow.ToSharedRef()]
		]
	];
}

void ULanessaInteriorTourWidget::RefreshRoomRow()
{
	using namespace InteriorV2;

	if (!RoomRow.IsValid()) { return; }   // bar not built yet - BuildRoomBar will call us itself
	RoomRow->ClearChildren();

	TSharedRef<SHorizontalBox> Row = RoomRow.ToSharedRef();
	for (int32 i = 0; i < RoomIds.Num(); i++)
	{
		const FString RoomId = RoomIds[i];
		const FString Label = FString::Printf(TEXT("%d. %s"), i + 1, *RoomLabels[i]);
		TAttribute<FLinearColor> BgColor = TAttribute<FLinearColor>::Create([this, RoomId]() { return CurrentRoomId == RoomId ? OliveGlow : Transparent; });
		TAttribute<FSlateColor> TxtColor = TAttribute<FSlateColor>::Create([this, RoomId]() { return FSlateColor(CurrentRoomId == RoomId ? IconInk : Paper); });
		Row->AddSlot().AutoWidth().Padding(2.f, 0.f)
		[
			SNew(SBox).HeightOverride(34.f)
			[
				SNew(SLanessaCutBorder).CutSize(17.f).FillColor(BgColor).HoverColor(FLinearColor(1.f, 1.f, 1.f, 0.10f)).bAnimateHover(true)
				.OnClicked(FSimpleDelegate::CreateLambda([this, RoomId]()
				{
					SetCurrentRoom(RoomId);
					// Move before broadcasting, so a BP handler that fades or checks state sees the
					// pawn already at the destination - same order a BP-driven jump would produce.
					if (bTeleportOnClick) { TeleportToRoom(this, RoomId); }
					OnRoomSelected.Broadcast(RoomId);
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

// Top-right icon cluster (map / expand / close), matching the reference screenshot's corner HUD.
// The map icon opens BuildPlanPopup(); expand and close are visual-only placeholders for now (no
// fullscreen mode or exit wiring exists yet beyond OnExitClicked on the popup's own X). No
// always-visible mini thumbnail here - the click-to-open popup is the only plan surface, per user
// feedback that a second, permanently-shown empty "PLAN" panel was redundant.
TSharedRef<SWidget> ULanessaInteriorTourWidget::BuildTopRightCluster()
{
	using namespace InteriorV2;

	auto CircleBtn = [this](const TArray<FLanessaIconPrim>& Prims, FLinearColor Fill, FLinearColor IconColor, FSimpleDelegate OnClicked)
	{
		return SNew(SBox).WidthOverride(42.f).HeightOverride(42.f)
		[
			SNew(SLanessaCutBorder).CutSize(0.f).FillColor(Fill).HoverColor(FLinearColor(1.f, 1.f, 1.f, 0.12f)).bAnimateHover(true)
			.OnClicked(OnClicked)
			.Content()
			[
				SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(11.f)
				[SNew(SLanessaInteriorIcon).Prims(Prims).Color(IconColor).StrokeWidth(1.7f)]
			]
		];
	};

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 6.f, 0.f)
		[CircleBtn(MakeMapPinIcon(), OliveGlow, IconInk, FSimpleDelegate::CreateLambda([this]() { TogglePlan(); }))]
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 6.f, 0.f)
		[CircleBtn(MakeExpandIcon(), CircleDark, Paper, FSimpleDelegate())]
		+ SHorizontalBox::Slot().AutoWidth()
		[CircleBtn(MakeCloseIcon(), CircleDark, Paper, FSimpleDelegate::CreateLambda([this]() { OnExitClicked.Broadcast(); }))];
}

// .plan-popup { position:absolute; top:64; right:16; width:340 } - the larger floor-plan card the
// mini thumbnail/map icon opens. Room markers here broadcast the exact same OnRoomSelected as the
// bottom bar (see BuildRoomBar's comment - both entry points must behave identically).
TSharedRef<SWidget> ULanessaInteriorTourWidget::BuildPlanPopup()
{
	using namespace InteriorV2;

	TSharedRef<SVerticalBox> RoomList = SNew(SVerticalBox);
	for (int32 i = 0; i < RoomIds.Num(); i++)
	{
		const FString RoomId = RoomIds[i];
		const FString Label = FString::Printf(TEXT("%d. %s"), i + 1, *RoomLabels[i]);
		TAttribute<FLinearColor> BgColor = TAttribute<FLinearColor>::Create([this, RoomId]() { return CurrentRoomId == RoomId ? OliveGlow : FLinearColor(1.f, 1.f, 1.f, 0.04f); });
		TAttribute<FSlateColor> TxtColor = TAttribute<FSlateColor>::Create([this, RoomId]() { return FSlateColor(CurrentRoomId == RoomId ? IconInk : Paper); });
		RoomList->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
		[
			SNew(SBox).HeightOverride(30.f)
			[
				SNew(SLanessaCutBorder).CutSize(0.f).FillColor(BgColor).HoverColor(FLinearColor(1.f, 1.f, 1.f, 0.10f)).bAnimateHover(true)
				.OnClicked(FSimpleDelegate::CreateLambda([this, RoomId]()
				{
					SetCurrentRoom(RoomId);
					if (bTeleportOnClick) { TeleportToRoom(this, RoomId); }
					OnRoomSelected.Broadcast(RoomId);
					bPlanOpen = false;
					Invalidate(EInvalidateWidgetReason::Paint);
				}))
				.Content()
				[
					SNew(SBox).VAlign(VAlign_Center).Padding(10.f, 0.f)
					[SNew(STextBlock).Text(FText::FromString(Label)).Font(F(10)).ColorAndOpacity(TxtColor)]
				]
			]
		];
	}

	return SNew(SBox).WidthOverride(300.f)
	[
		SNew(SLanessaCutBorder).CutSize(0.f).FillColor(PanelSolid)
		.Content()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(16.f, 14.f, 16.f, 8.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[SNew(STextBlock).Text(FText::FromString(TEXT("PLAN"))).Font(F(9)).ColorAndOpacity(FSlateColor(TextDim))]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(22.f).HeightOverride(22.f)
					[
						SNew(SLanessaCutBorder).CutSize(0.f).FillColor(Transparent).HoverColor(FLinearColor(1.f, 1.f, 1.f, 0.08f)).bAnimateHover(true)
						.OnClicked(FSimpleDelegate::CreateLambda([this]() { TogglePlan(); }))
						.Content()
						[SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(STextBlock).Text(FText::FromString(TEXT("\u2715"))).Font(F(11)).ColorAndOpacity(FSlateColor(TextDim))]]
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(16.f, 0.f, 16.f, 10.f)
			[
				SNew(SBox).HeightOverride(180.f)
				[
					PlanTexture
						? StaticCastSharedRef<SWidget>(SNew(SImage).Image(new FSlateImageBrush(PlanTexture, FVector2D(1.f, 1.f))))
						: StaticCastSharedRef<SWidget>(
							SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(1.f, 1.f, 1.f, 0.05f))
							[
								SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
								[SNew(STextBlock).Text(FText::FromString(TEXT("Reja tasviri hali yuklanmagan"))).Font(F(9)).ColorAndOpacity(FSlateColor(TextDim))]
							])
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(16.f, 0.f, 16.f, 16.f)
			[RoomList]
		]
	];
}

// .room-label { position:absolute; top:24; left:24 } - current-room readout, top-left.
TSharedRef<SWidget> ULanessaInteriorTourWidget::BuildRoomLabel()
{
	using namespace InteriorV2;

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[SNew(STextBlock).Text(TAttribute<FText>::Create([this]() { return GetCurrentRoomLabel(); })).Font(F(16)).ColorAndOpacity(FSlateColor(Paper))]
		+ SVerticalBox::Slot().AutoHeight()
		[SNew(STextBlock).Text(FText::FromString(TEXT("INTERYER"))).Font(F(8)).ColorAndOpacity(FSlateColor(TextDim))];
}

TSharedRef<SWidget> ULanessaInteriorTourWidget::RebuildWidget()
{
	using namespace InteriorV2;

	// Mock room list for this design-only pass, matching the reference screenshot exactly - real
	// content (actual per-unit room ids/labels) comes in later via SetRooms() once the interior
	// level's own room markers exist to source it from.
	if (RoomIds.Num() == 0)
	{
		SetRooms(
			{ TEXT("holl"), TEXT("yotoqxona"), TEXT("sanuzel"), TEXT("oshxona"), TEXT("mehmonxona") },
			{ TEXT("Holl"), TEXT("Yotoqxona"), TEXT("Sanuzel"), TEXT("Oshxona"), TEXT("Mehmonxona") });
	}

	return SNew(SOverlay)
		+ SOverlay::Slot().Padding(24.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)[BuildRoomLabel()]
		]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(24.f)
		[BuildTopRightCluster()]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(24.f, 76.f, 24.f, 0.f)
		[
			SNew(SBox).Visibility(TAttribute<EVisibility>::Create([this]() { return bPlanOpen ? EVisibility::Visible : EVisibility::Collapsed; }))
			[BuildPlanPopup()]
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, 32.f)
		[BuildRoomBar()];
}
