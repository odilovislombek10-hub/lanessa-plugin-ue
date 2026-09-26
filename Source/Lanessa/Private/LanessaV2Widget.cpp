#include "LanessaV2Widget.h"
#include "LanessaDayNight.h"
#include "LanessaDayNightSettings.h"
#include "LanessaWalkPointsWidget.h"
#include "LanessaLevelStreaming.h"
#include "LanessaConstruction.h"
#include "LanessaRemoteSettings.h"

// Sozlama sahifasiga kirish. Ta'rifi fayl oxiridagi Remote blokida - u yerda
// LanessaGameWorld va boshqa yordamchilar bilan yonma-yon tursin. Bu yerda
// oldindan e'lon qilinadi, chunki RebuildWidget undan ancha oldin turadi.
static const class ULanessaRemoteSettings& LanessaCfg();
#include "LanessaLineIcon.h"
#include "LanessaCustomShapes.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Styling/SlateBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/Texture2D.h"
#include "Fonts/SlateFontInfo.h"
#include "Fonts/CompositeFont.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/MovementComponent.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "StructUtils/UserDefinedStruct.h"
#include "Engine/DataTable.h"
#include "Materials/MaterialInstanceDynamic.h"
#if WITH_EDITOR
#include "Kismet2/StructureEditorUtils.h"
#endif

ULanessaV2Widget* ULanessaV2Widget::CreateForTest(UObject* WorldContextObject)
{
	UWorld* World = WorldContextObject && GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	return World ? CreateWidget<ULanessaV2Widget>(World, ULanessaV2Widget::StaticClass()) : nullptr;
}

bool ULanessaV2Widget::AddIntFieldToStruct(UUserDefinedStruct* Struct, const FString& FieldName)
{
#if WITH_EDITOR
	if (!Struct) { return false; }
	FEdGraphPinType PinType;
	PinType.PinCategory = TEXT("int");
	if (!FStructureEditorUtils::AddVariable(Struct, PinType)) { return false; }

	// FStructVariableDescription's full definition is private to UnrealEd's own .cpp, so GetVarDesc()'s
	// array elements aren't accessible here - find the new property directly instead (AddVariable appends
	// it as the struct's last field) and resolve its Guid via GetGuidForProperty.
	FProperty* LastProp = nullptr;
	for (TFieldIterator<FProperty> It(Struct); It; ++It) { LastProp = *It; }
	if (!LastProp) { return false; }
	const FGuid NewGuid = FStructureEditorUtils::GetGuidForProperty(LastProp);
	const bool bRenamed = FStructureEditorUtils::RenameVariable(Struct, NewGuid, FieldName);
	// AddVariable/RenameVariable don't by themselves notify dependent systems (e.g. the Blueprint editor's
	// node-spawner/action database) that the struct changed - without this, freshly-created "Break<Struct>"
	// nodes keep showing the OLD field list even after a full Editor restart. OnStructureChanged is what
	// actually broadcasts the change.
	FStructureEditorUtils::OnStructureChanged(Struct);
	return bRenamed;
#else
	return false;
#endif
}

void ULanessaV2Widget::RefreshStructActions(UUserDefinedStruct* Struct)
{
#if WITH_EDITOR
	if (Struct) { FStructureEditorUtils::OnStructureChanged(Struct); }
#endif
}

bool ULanessaV2Widget::SetDataTableRowStructAndClear(UDataTable* Table, UScriptStruct* NewRowStruct)
{
#if WITH_EDITOR
	if (!Table || !NewRowStruct) { return false; }
	Table->EmptyTable();
	Table->RowStruct = NewRowStruct;
	Table->Modify();
	return true;
#else
	return false;
#endif
}

TArray<FString> ULanessaV2Widget::GetStructFieldNames(UUserDefinedStruct* Struct)
{
	TArray<FString> Names;
	if (!Struct) { return Names; }
	for (TFieldIterator<FProperty> It(Struct); It; ++It)
	{
		Names.Add(It->GetName());
	}
	return Names;
}

// Test-only console command: "Lanessa.ShowV2" spawns and shows the widget, standing in for the
// missing Python/MCP bridge to Unreal in this session. The actual creation is deferred by a short
// timer rather than run inline - when this command comes from "-ExecCmds" on process launch, the
// game viewport/local player is not fully initialized yet and AddToViewport() would crash.
static FAutoConsoleCommandWithWorld GLanessaShowV2Cmd(
	TEXT("Lanessa.ShowV2"),
	TEXT("Spawns ULanessaV2Widget and adds it to the viewport (test-only)."),
	FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* World)
	{
		if (!World) { return; }
		TWeakObjectPtr<UWorld> WeakWorld(World);
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateStatic([](TWeakObjectPtr<UWorld> InWorld)
		{
			if (UWorld* W = InWorld.Get())
			{
				if (ULanessaV2Widget* Widget = ULanessaV2Widget::CreateForTest(W))
				{
					Widget->AddToViewport();
					// -game/PIE default input mode is Game Only (mouse locked/hidden for camera look) -
					// the mockup's buttons need real clicks, so switch to UI input with a visible cursor.
					if (APlayerController* PC = W->GetFirstPlayerController())
					{
						PC->SetShowMouseCursor(true);
						FInputModeGameAndUI InputMode;
						InputMode.SetWidgetToFocus(Widget->TakeWidget());
						InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
						PC->SetInputMode(InputMode);
					}
				}
			}
		}, WeakWorld), 1.5f, false);
	})
);

// :root custom properties, ported verbatim from the v2 <style> block.
namespace V2
{
	static const FLinearColor Ink(0.f, 0.f, 0.f, 1.f);
	static const FLinearColor Paper(1.f, 1.f, 1.f, 1.f);
	static const FLinearColor Olive(0.6f, 0.6f, 0.4f, 1.f);                 // #999966
	static const FLinearColor OliveDeep(0.435f, 0.435f, 0.278f, 1.f);       // #6f6f47
	static const FLinearColor OliveGlow(0.788f, 0.788f, 0.604f, 1.f);       // #c9c99a
	static const FLinearColor Line(1.f, 1.f, 1.f, 0.16f);
	static const FLinearColor LineSoft(1.f, 1.f, 1.f, 0.08f);
	static const FLinearColor Panel(0.024f, 0.024f, 0.02f, 0.72f);
	static const FLinearColor PanelSolid(0.039f, 0.039f, 0.035f, 1.f);      // #0a0a09
	static const FLinearColor TextDim(1.f, 1.f, 1.f, 0.56f);
	static const FLinearColor Transparent(0.f, 0.f, 0.f, 0.f);
	static const FLinearColor IconInk(0.039f, 0.039f, 0.031f, 1.f);         // #0a0a08 (used on olive fills)
	static const FLinearColor StageBase(0.047f, 0.043f, 0.031f, 1.f);       // approximates .stage gradient (flat, no radial-gradient brush in Slate)

	// v2's CSS: --sans: -apple-system,"Segoe UI",... ; --display: Didot,"Bodoni MT",Georgia,...
	// Neither Didot nor Bodoni MT ship on Windows, so a real browser on this OS actually renders
	// --display as Georgia and --sans as Segoe UI - loaded here directly from the OS font files
	// (Slate has no "system font by name" lookup) so the C++ port matches what v2 truly renders,
	// not Unreal's built-in Roboto.
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

	// Originally var(--sans)/Segoe UI, matching v2's own CSS split between body (--sans) and
	// display (--display) fonts. The user previewed both variants directly in the v2 HTML mockup
	// (a plain --sans:Segoe-UI version vs. all-text-in-Georgia) and explicitly preferred the
	// all-Georgia look, so body/nav/button text now loads Georgia too, same as D() below.
	static FSlateFontInfo F(int32 Size, bool bBold = false)
	{
		return LoadSystemFont(bBold ? TEXT("C:/Windows/Fonts/georgiab.ttf") : TEXT("C:/Windows/Fonts/georgia.ttf"), Size, bBold);
	}

	// var(--display) - Georgia (brand wordmark, .side-title, .chip-card .time, .poi h3, .plan-head h3)
	static FSlateFontInfo D(int32 Size)
	{
		return LoadSystemFont(TEXT("C:/Windows/Fonts/georgia.ttf"), Size, false);
	}

	static TSharedRef<SWidget> Fill(const FLinearColor& Color)
	{
		return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Color).Padding(0.f)[SNew(SSpacer)];
	}

	static TSharedRef<SWidget> Diamond(float Size, const FLinearColor& Color)
	{
		return SNew(SBox).WidthOverride(Size).HeightOverride(Size)
			.RenderTransform(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(45.f))))
			.RenderTransformPivot(FVector2D(0.5f, 0.5f))
			[
				Fill(Color)
			];
	}
}

void ULanessaV2Widget::SetActiveView(const FString& ViewId)
{
	ActiveView = ViewId;

	// Sahifaning sub levellari (Project Settings -> Plugins -> Lanessa Level Streaming).
	// Broadcast dan OLDIN: BP_Explorer_PC nav hodisasini eshitganda levellar allaqachon
	// ochiq bo'lsin - masalan progulka PlayerStart lari quyida shu levellardan olinadi.
	if (UWorld* World = GetWorld())
	{
		if (ULanessaLevelStreamingSubsystem* Streaming = World->GetSubsystem<ULanessaLevelStreamingSubsystem>())
		{
			Streaming->ApplyPageId(ViewId);
		}
		// Qurilish animatsiyasi istalgan sahifa bosilganda to'xtaydi va bino to'liq holatga
		// qaytadi - yarim qirqilgan fasad keyingi sahifada qolib ketmasin.
		if (ULanessaConstructionSubsystem* Build = World->GetSubsystem<ULanessaConstructionSubsystem>())
		{
			Build->StopConstruction();
			Build->MoveCameraForPageId(ViewId);   // faqat qurilish sahifalari, qolganini BP qiladi
		}
	}
	ActiveBuildStage.Reset();

	// "sayr" (Progulka) owns the screen the way the interior walkthrough does, so it gets the same
	// kind of chrome: a spawn-point bar at the bottom and an exit button top-right. Created here, on
	// first entry, because nothing in Blueprint creates it - pressing Progulka showed no bar at all.
	if (ViewId == TEXT("sayr"))
	{
		if (!WalkWidget)
		{
			// GetOwningPlayer() is null when this HUD was created without an owning controller (a
			// plain CreateWidget(World, ...) in BP does that) - CreateWidget with a null owner
			// returns null, so fall back to player 0 rather than silently producing no bar.
			APlayerController* Owner = GetOwningPlayer();
			if (!Owner && GetWorld()) { Owner = UGameplayStatics::GetPlayerController(GetWorld(), 0); }
			if (Owner)
			{
				WalkWidget = CreateWidget<ULanessaWalkPointsWidget>(Owner, ULanessaWalkPointsWidget::StaticClass());
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[LanessaV2] sayr: no player controller, walk bar not created"));
			}
			if (WalkWidget)
			{
				// Leaving via the X must put the rail back, which is exactly what entering "none"
				// does - routing through SetActiveView keeps one exit path instead of two.
				WalkWidget->OnExitClicked.AddDynamic(this, &ULanessaV2Widget::HandleWalkExit);
			}
		}
		if (WalkWidget)
		{
			// Populate BEFORE the first AddToViewport: that is when the widget's Slate tree is
			// built, and a bar built while the list was still empty would come up with no pills.
			// Re-scanned on every entry rather than once, because the walk starts can live in a
			// streamed level and a list captured on first entry goes stale when it unloads.
			WalkWidget->PopulateFromPlayerStarts(this, TEXT("Walk"));
			if (!WalkWidget->IsInViewport())
			{
				// Above the 3D scene but below the v2 HUD's own layers, same as any mode chrome.
				WalkWidget->AddToViewport(10);
			}
		}
	}
	else if (WalkWidget && WalkWidget->IsInViewport())
	{
		WalkWidget->RemoveFromParent();
	}

	if (ViewId == TEXT("qirqim"))
	{
		// mirrors the mockup's `if (view === 'qirqim') selectFloor(9)` default - this was previously
		// just a comment with no actual assignment, so QIRQIM always opened on whatever floor was
		// last selected instead of resetting to 9 like v2 does.
		SelectedFloor = 9;
	}
	Invalidate(EInvalidateWidgetReason::Paint);
	OnNavClicked.Broadcast(ViewId);
	if (ViewId == TEXT("qidiruv"))
	{
		// Opening Qidiruv left the unit markers invisible until the user touched a control:
		// BP_Explorer_PC answers the nav broadcast above by running Hide_POI over every unit
		// (Opacity_Total -> 0 on each marker's dynamic material), and the only thing that ever
		// shows them again is OnSearchApplied, which nothing broadcast on entry - ApplySearch was
		// reachable solely from a chip toggle, a range drag and the QO'LLASH button. Measured, not
		// assumed: the marker's component visibility and colour are identical in both states, the
		// single difference is Opacity_Total 0 vs 1.
		// Must run AFTER the broadcast, so it shows the matching units back over the hide the
		// handler just performed rather than being undone by it. Default handles match every unit,
		// which mirrors the real product opening its unit search with everything already filtered
		// in (see ApplySearch's own comment).
		ApplySearch();
	}
}

void ULanessaV2Widget::SelectBuildStage(const FString& StageId)
{
	ActiveBuildStage = StageId;

	UWorld* World = GetWorld();
	ULanessaLevelStreamingSubsystem* Streaming = World ? World->GetSubsystem<ULanessaLevelStreamingSubsystem>() : nullptr;
	ULanessaConstructionSubsystem* Build = World ? World->GetSubsystem<ULanessaConstructionSubsystem>() : nullptr;

	// Oldingi qirqim (QIRQIM sahifasidan yoki yer osti qavatidan) bosqichni ko'rsatishga
	// xalaqit bermasin - BP_Explorer_PC buni Reset_SectionView ga javoban bekor qiladi.
	Reset_SectionView.Broadcast();

	if (Build) { Build->StopConstruction(); }
	if (StageId == TEXT("qurilishetapi"))
	{
		if (Build) { Build->PlayConstruction(); }
	}
	else if (Streaming)
	{
		Streaming->ApplyPageId(StageId);
	}
	if (Build) { Build->MoveCameraForPageId(StageId); }

	Invalidate(EInvalidateWidgetReason::Paint);
	// Bosh sahifa / atrof / qulayliklar bilan bir xil yo'l: BP_Explorer_PC.OnNavClicked_Event
	// ViewId bo'yicha kamerani (SetLocation_New ... Focus) o'zi qo'yadi. Bosqich id lari:
	// "kotlovan" "yerosti" "karkas" "devor" "fasad" "qurilishetapi" - BP da shu nomlar bilan
	// tarmoq qo'shilsa, kamera o'sha yerda sozlanadi.
	OnNavClicked.Broadcast(StageId);
}

void ULanessaV2Widget::HandleWalkExit()
{
	// "none" is the rail's own idle view - it re-shows the nav and drops the walk chrome through
	// SetActiveView's else-branch above, so the exit needs no separate teardown of its own.
	SetActiveView(TEXT("none"));
}

void ULanessaV2Widget::ShowTourToast(const FString& Label)
{
	ToastLabel = Label;
	ToastTimeLeft = 1.8f;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void ULanessaV2Widget::SelectPoi(const FString& PoiId)
{
	SelectedPoiId = PoiId;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void ULanessaV2Widget::SetPoiData(const FString& Name, const FString& Information, const FString& Footer, UTexture2D* Image,
	bool bHasLevel, const FString& LevelButtonText, bool bHasLevel2, const FString& Level2ButtonText,
	bool bHas360, bool bHasMedia, const FString& MediaButtonText)
{
	PoiName = Name;
	PoiInformation = Information;
	PoiFooter = Footer;
	PoiImageTexture = Image;
	if (Image)
	{
		PoiImageBrush.SetResourceObject(Image);
		PoiImageBrush.ImageSize = FVector2D(270.f, 150.f);
		PoiImageBrush.DrawAs = ESlateBrushDrawType::Image;
	}
	bPoiHasLevel = bHasLevel;
	PoiLevelButtonText = LevelButtonText;
	bPoiHasLevel2 = bHasLevel2;
	PoiLevel2ButtonText = Level2ButtonText;
	bPoiHas360 = bHas360;
	bPoiHasMedia = bHasMedia;
	PoiMediaButtonText = MediaButtonText;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void ULanessaV2Widget::SelectFloor(int32 Floor)
{
	SelectedFloor = Floor;
	Invalidate(EInvalidateWidgetReason::Paint);
	OnFloorSelected.Broadcast(Floor);
}

void ULanessaV2Widget::SetAvailableFloors(const TArray<int32>& Floors)
{
	AvailableFloors = Floors;
	AvailableFloors.Sort();
	PopulateFloorRow(FloorRailRow, false);
	PopulateFloorRow(UndergroundRailRow, true);
}

void ULanessaV2Widget::PopulateFloorRow(const TSharedPtr<SHorizontalBox>& FloorRow, bool bUnderground)
{
	if (!FloorRow.IsValid()) { return; }

	FloorRow->ClearChildren();
	using namespace V2;
	for (int32 Floor : AvailableFloors)
	{
		// Yer osti paneli faqat Floor < 1 (0, -1, -2 ...) qavatlarni ko'rsatadi. QIRQIM
		// paneli avvalgidek hamma qavatni.
		if (bUnderground && Floor >= 1) { continue; }

		TAttribute<FLinearColor> BgColor = TAttribute<FLinearColor>::Create([this, Floor]() { return SelectedFloor == Floor ? V2::Olive : V2::Transparent; });
		TAttribute<FSlateColor> TxtColor = TAttribute<FSlateColor>::Create([this, Floor]() { return FSlateColor(SelectedFloor == Floor ? V2::IconInk : V2::TextDim); });
		FloorRow->AddSlot().AutoWidth()
		[
			SNew(SBox).WidthOverride(30.f).HeightOverride(30.f)
			[
				SNew(SLanessaCutBorder).CutSize(0.f).FillColor(BgColor).HoverColor(OliveGlow).bAnimateHover(true)
				.OnClicked(FSimpleDelegate::CreateLambda([this, Floor]() { SelectFloor(Floor); }))
				.Content()
				[
					SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
					[SNew(STextBlock).Text(FText::AsNumber(Floor)).Font(F(11)).ColorAndOpacity(TxtColor)]
				]
			]
		];
	}
}

void ULanessaV2Widget::SetPlanOpen(bool bOpen)
{
	bPlanOpen = bOpen;
	Invalidate(EInvalidateWidgetReason::Paint);
}


void ULanessaV2Widget::ToggleChip(const FString& Key)
{
	bool Current = IsToggled(Key, false);
	ToggleState.Add(Key, !Current);
	Invalidate(EInvalidateWidgetReason::Paint);
	// Real BP_UnitSearch_Widget calls Custom|Filter (Show_POI/Hide_POI) directly from every single toggle
	// button's OnClicked - there is no separate "apply" step (see lanessa-real-product-behavior memory).
	// Only actually re-filter for qidiruv:* chip keys - other ToggleChip callers (category panel toggles
	// etc.) reuse this same function for unrelated UI state that ComputeMatchingPoiIds doesn't read.
	if (Key.StartsWith(TEXT("qidiruv:"))) { ApplySearch(); }
}

bool ULanessaV2Widget::IsToggled(const FString& Key, bool bDefault) const
{
	if (const bool* V = ToggleState.Find(Key)) { return *V; }
	return bDefault;
}

void ULanessaV2Widget::SetWeather(const FString& WeatherId)
{
	SelectedWeather = WeatherId;
	Invalidate(EInvalidateWidgetReason::Paint);
	OnWeatherChanged.Broadcast(WeatherId);
}

void ULanessaV2Widget::SetSeason(const FString& SeasonId)
{
	SelectedSeason = SeasonId;
	Invalidate(EInvalidateWidgetReason::Paint);
	OnSeasonChanged.Broadcast(SeasonId);
}

void ULanessaV2Widget::SetTimePct(float Pct)
{
	TimePct = FMath::Clamp(Pct, 0.f, 1.f);
	Invalidate(EInvalidateWidgetReason::Paint);
	OnTimePctChanged.Broadcast(TimePct);
	// Day/night lighting follows the slider. Deliberately AFTER the broadcast and deliberately
	// reading Time of Day back off the UDS actor rather than converting TimePct here: BP_Explorer_PC's
	// OnTimePctChanged_Handler is what writes "Time of Day" (ToD = 600 + Pct*1600), so by the time the
	// broadcast returns, UDS already holds the authoritative value. Re-deriving it here would duplicate
	// that mapping in a second place and let the two drift apart.
	// ApplyDayNight is edge-triggered internally, so calling it on every drag frame costs one float
	// compare until the 07:00/19:00 boundary is actually crossed.
	ULanessaDayNight::ApplyDayNightFromUDS(this, /*bForce=*/false);
}

FVector2D ULanessaV2Widget::GetRangeHandle(const FString& FieldId, float DefaultA, float DefaultB) const
{
	if (const FVector2D* V = RangeHandles.Find(FieldId)) { return *V; }
	return FVector2D(DefaultA, DefaultB);
}

void ULanessaV2Widget::SetRangeHandlePct(const FString& FieldId, float Pct)
{
	Pct = FMath::Clamp(Pct, 0.f, 1.f);
	// Must match ComputeMatchingPoiIds()'s and BuildSidePanelQidiruvContent()'s defaults (full range,
	// 0..1). Using anything else made the first drag silently jump the *other* handle inward, filtering
	// out units the user never excluded.
	FVector2D Handle = GetRangeHandle(FieldId, 0.f, 1.f);
	// drag whichever handle (low/high) is nearer the pointer
	if (FMath::Abs(Pct - Handle.X) <= FMath::Abs(Pct - Handle.Y))
	{
		Handle.X = FMath::Min(Pct, Handle.Y);
	}
	else
	{
		Handle.Y = FMath::Max(Pct, Handle.X);
	}
	RangeHandles.Add(FieldId, Handle);
	Invalidate(EInvalidateWidgetReason::Paint);
	// Real BP_UnitSearch_Widget's Update_Slider_Budget/Update_Slider_Surface both call Custom|Filter
	// directly on every slider drag - live re-filtering, same reasoning as ToggleChip above.
	ApplySearch();
}

void ULanessaV2Widget::SetSearchUnits(const TArray<FString>& PoiIds, const TArray<int32>& Surfaces, const TArray<int32>& Bedrooms,
	const TArray<int32>& Bathrooms, const TArray<FString>& Availabilities, const TArray<int32>& Prices)
{
	const int32 Count = PoiIds.Num();
	if (Surfaces.Num() != Count || Bedrooms.Num() != Count || Bathrooms.Num() != Count || Availabilities.Num() != Count || Prices.Num() != Count)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Lanessa] SetSearchUnits: array length mismatch (Ids=%d Surfaces=%d Bedrooms=%d Bathrooms=%d Availabilities=%d Prices=%d), ignoring"),
			Count, Surfaces.Num(), Bedrooms.Num(), Bathrooms.Num(), Availabilities.Num(), Prices.Num());
		return;
	}
	SearchUnits.Reset(Count);
	for (int32 i = 0; i < Count; ++i)
	{
		FLanessaV2SearchUnit Unit;
		Unit.PoiId = PoiIds[i];
		Unit.Surface = Surfaces[i];
		Unit.Bedrooms = Bedrooms[i];
		Unit.Bathrooms = Bathrooms[i];
		Unit.Availability = Availabilities[i];
		Unit.Price = Prices[i];
		SearchUnits.Add(MoveTemp(Unit));
	}
	Invalidate(EInvalidateWidgetReason::Paint);
}

// Reads POIActor's own live Filter.Availability via reflection - same technique as RefreshPOIFilterColor
// (BP_POI's own POI_Type/Availability getter nodes are permanently bound to the wrong same-name-collision
// enum asset, so this must go through raw FProperty iteration instead of Blueprint getter nodes). Returns
// empty string if POIActor is null or the field can't be found, so the caller can fall back to Table's value.
static FString FindLiveAvailability(AActor* POIActor)
{
	if (!POIActor) { return FString(); }
	const FStructProperty* FilterProp = nullptr;
	for (TFieldIterator<FProperty> It(POIActor->GetClass()); It; ++It)
	{
		if (It->GetName().StartsWith(TEXT("Filter"), ESearchCase::IgnoreCase))
		{
			FilterProp = CastField<FStructProperty>(*It);
			if (FilterProp) { break; }
		}
	}
	if (!FilterProp) { return FString(); }
	const void* FilterPtr = FilterProp->ContainerPtrToValuePtr<void>(POIActor);
	for (TFieldIterator<FProperty> It(FilterProp->Struct); It; ++It)
	{
		if (!It->GetName().StartsWith(TEXT("availability"), ESearchCase::IgnoreCase)) { continue; }
		if (const FByteProperty* ByteProp = CastField<FByteProperty>(*It))
		{
			if (ByteProp->Enum)
			{
				const int64 Value = ByteProp->GetPropertyValue_InContainer(FilterPtr);
				return ByteProp->Enum->GetDisplayNameTextByValue(Value).ToString();
			}
		}
		if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(*It))
		{
			const FNumericProperty* Underlying = EnumProp->GetUnderlyingProperty();
			const int64 Value = Underlying->GetSignedIntPropertyValue(EnumProp->ContainerPtrToValuePtr<void>(FilterPtr));
			if (UEnum* Enum = EnumProp->GetEnum()) { return Enum->GetDisplayNameTextByValue(Value).ToString(); }
		}
	}
	return FString();
}

// Same reflection walk as FindLiveAvailability, for the struct's int fields. Used for the price, which
// the seller edits in the Google Sheet at runtime - reading it off the DataTable would make the search
// slider filter on a stale price while the POI card shows the new one.
static int32 FindLiveInt(AActor* POIActor, const TCHAR* FieldPrefix)
{
	if (!POIActor) { return 0; }
	const FStructProperty* FilterProp = nullptr;
	for (TFieldIterator<FProperty> It(POIActor->GetClass()); It; ++It)
	{
		if (It->GetName().StartsWith(TEXT("Filter"), ESearchCase::IgnoreCase))
		{
			FilterProp = CastField<FStructProperty>(*It);
			if (FilterProp) { break; }
		}
	}
	if (!FilterProp) { return 0; }
	const void* FilterPtr = FilterProp->ContainerPtrToValuePtr<void>(POIActor);
	for (TFieldIterator<FIntProperty> It(FilterProp->Struct); It; ++It)
	{
		if (It->GetName().StartsWith(FieldPrefix, ESearchCase::IgnoreCase))
		{
			return It->GetPropertyValue_InContainer(FilterPtr);
		}
	}
	return 0;
}

void ULanessaV2Widget::PopulateSearchUnitsFromTable(UDataTable* Table, const TArray<FString>& PoiIds, const TArray<FName>& RowNames, const TArray<AActor*>& POIActors)
{
	SearchUnits.Reset();
	if (!Table || PoiIds.Num() != RowNames.Num()) { return; }

	const UScriptStruct* Struct = Table->GetRowStruct();
	if (!Struct) { return; }

	auto FindIntField = [Struct](uint8* RowPtr, const TCHAR* Prefix) -> int32
	{
		for (TFieldIterator<FIntProperty> It(Struct); It; ++It)
		{
			if (It->GetName().StartsWith(Prefix, ESearchCase::IgnoreCase)) { return It->GetPropertyValue_InContainer(RowPtr); }
		}
		return 0;
	};
	auto FindEnumDisplayName = [Struct](uint8* RowPtr, const TCHAR* Prefix) -> FString
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			if (!It->GetName().StartsWith(Prefix, ESearchCase::IgnoreCase)) { continue; }
			if (const FByteProperty* ByteProp = CastField<FByteProperty>(*It))
			{
				if (ByteProp->Enum)
				{
					const int64 Value = ByteProp->GetPropertyValue_InContainer(RowPtr);
					return ByteProp->Enum->GetDisplayNameTextByValue(Value).ToString();
				}
			}
			if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(*It))
			{
				const FNumericProperty* Underlying = EnumProp->GetUnderlyingProperty();
				const int64 Value = Underlying->GetSignedIntPropertyValue(EnumProp->ContainerPtrToValuePtr<void>(RowPtr));
				if (UEnum* Enum = EnumProp->GetEnum()) { return Enum->GetDisplayNameTextByValue(Value).ToString(); }
			}
		}
		return FString();
	};

	SearchUnits.Reserve(PoiIds.Num());
	for (int32 i = 0; i < PoiIds.Num(); ++i)
	{
		uint8* RowPtr = Table->FindRowUnchecked(RowNames[i]);
		if (!RowPtr) { continue; }

		FLanessaV2SearchUnit Unit;
		Unit.PoiId = PoiIds[i];
		Unit.Surface = FindIntField(RowPtr, TEXT("surface"));
		Unit.Bedrooms = FindIntField(RowPtr, TEXT("bedroomscount"));
		Unit.Bathrooms = FindIntField(RowPtr, TEXT("bathroomscount"));
		const FString LiveAvailability = POIActors.IsValidIndex(i) ? FindLiveAvailability(POIActors[i]) : FString();
		Unit.Availability = !LiveAvailability.IsEmpty() ? LiveAvailability : FindEnumDisplayName(RowPtr, TEXT("availability"));
		// 0 means "the CSV never reached this actor" (no internet and no backup, or a POI with no row) -
		// fall back to the DataTable's starting price rather than filtering everything out at 0.
		const int32 LivePrice = POIActors.IsValidIndex(i) ? FindLiveInt(POIActors[i], TEXT("narx")) : 0;
		Unit.Price = LivePrice > 0 ? LivePrice : FindIntField(RowPtr, TEXT("narx"));
		SearchUnits.Add(MoveTemp(Unit));
	}
	Invalidate(EInvalidateWidgetReason::Paint);
	UE_LOG(LogTemp, Warning, TEXT("[LanessaDebug] PopulateSearchUnitsFromTable: Table=%s PoiIds.Num=%d -> SearchUnits.Num=%d"),
		Table ? *Table->GetName() : TEXT("NULL"), PoiIds.Num(), SearchUnits.Num());
	if (SearchUnits.Num() > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LanessaDebug]   sample unit: PoiId=%s Surface=%d Bedrooms=%d Bathrooms=%d Availability=%s Price=%d"),
			*SearchUnits[0].PoiId, SearchUnits[0].Surface, SearchUnits[0].Bedrooms, SearchUnits[0].Bathrooms, *SearchUnits[0].Availability, SearchUnits[0].Price);
	}
}

TArray<FString> ULanessaV2Widget::DebugListActorProperties(AActor* POIActor)
{
	TArray<FString> Result;
	if (!POIActor) { return Result; }
	for (TFieldIterator<FProperty> It(POIActor->GetClass()); It; ++It)
	{
		Result.Add(FString::Printf(TEXT("%s (%s)"), *It->GetName(), *It->GetClass()->GetName()));
	}
	return Result;
}

int32 ULanessaV2Widget::GetPOITypeAsInt(AActor* POIActor)
{
	if (!POIActor) { return -1; }
	for (TFieldIterator<FProperty> It(POIActor->GetClass()); It; ++It)
	{
		if (!It->GetName().StartsWith(TEXT("POI_Type"), ESearchCase::IgnoreCase)) { continue; }
		if (const FByteProperty* ByteProp = CastField<FByteProperty>(*It))
		{
			return (int32)ByteProp->GetPropertyValue_InContainer(POIActor);
		}
		if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(*It))
		{
			const FNumericProperty* Underlying = EnumProp->GetUnderlyingProperty();
			return (int32)Underlying->GetSignedIntPropertyValue(EnumProp->ContainerPtrToValuePtr<void>(POIActor));
		}
	}
	return -1;
}

void ULanessaV2Widget::RefreshPOIFilterColor(AActor* POIActor)
{
	if (!POIActor) { return; }
	UClass* Class = POIActor->GetClass();

	auto FindByteValue = [](const void* Container, UStruct* Struct, const TCHAR* Prefix) -> int32
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			if (!It->GetName().StartsWith(Prefix, ESearchCase::IgnoreCase)) { continue; }
			if (const FByteProperty* ByteProp = CastField<FByteProperty>(*It))
			{
				return (int32)ByteProp->GetPropertyValue_InContainer(Container);
			}
			if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(*It))
			{
				const FNumericProperty* Underlying = EnumProp->GetUnderlyingProperty();
				return (int32)Underlying->GetSignedIntPropertyValue(EnumProp->ContainerPtrToValuePtr<void>(Container));
			}
		}
		return -1;
	};

	const int32 PoiType = FindByteValue(POIActor, Class, TEXT("POI_Type"));
	if (PoiType != 2) { return; } // only POI_FILTER (real-estate unit) markers get availability-based color

	const FStructProperty* FilterProp = nullptr;
	for (TFieldIterator<FProperty> It(Class); It; ++It)
	{
		if (It->GetName().StartsWith(TEXT("Filter"), ESearchCase::IgnoreCase))
		{
			FilterProp = CastField<FStructProperty>(*It);
			if (FilterProp) { break; }
		}
	}
	if (!FilterProp) { return; }
	const void* FilterPtr = FilterProp->ContainerPtrToValuePtr<void>(POIActor);
	const int32 Availability = FindByteValue(FilterPtr, FilterProp->Struct, TEXT("availability"));
	if (Availability < 0) { return; }

	UObject* HoloDynMatObj = nullptr;
	for (TFieldIterator<FProperty> It(Class); It; ++It)
	{
		if (It->GetName().StartsWith(TEXT("Holo_DynMat"), ESearchCase::IgnoreCase))
		{
			if (const FObjectProperty* ObjProp = CastField<FObjectProperty>(*It))
			{
				HoloDynMatObj = ObjProp->GetPropertyValue_InContainer(POIActor);
			}
			break;
		}
	}
	UMaterialInstanceDynamic* Holo = Cast<UMaterialInstanceDynamic>(HoloDynMatObj);
	if (!Holo) { return; }

	FLinearColor Color;
	switch (Availability)
	{
	case 0: Color = FLinearColor(0.f, 0.436081f, 1.f, 1.f); break;  // Sotilmagan - blue
	case 1: Color = FLinearColor(1.f, 0.543095f, 0.f, 1.f); break;  // Band Qilingan - orange
	case 2: Color = FLinearColor(1.f, 0.f, 0.f, 1.f); break;        // Sotilgan - red
	default: return;
	}
	Holo->SetVectorParameterValue(TEXT("Color"), Color);
}

FString ULanessaV2Widget::BuildPoiInfoText(AActor* POIActor, const FString& FallbackInformation)
{
	if (!POIActor) { return FallbackInformation; }
	UClass* Class = POIActor->GetClass();

	auto FindByteValue = [](const void* Container, UStruct* Struct, const TCHAR* Prefix) -> int32
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			if (!It->GetName().StartsWith(Prefix, ESearchCase::IgnoreCase)) { continue; }
			if (const FByteProperty* ByteProp = CastField<FByteProperty>(*It))
			{
				return (int32)ByteProp->GetPropertyValue_InContainer(Container);
			}
			if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(*It))
			{
				const FNumericProperty* Underlying = EnumProp->GetUnderlyingProperty();
				return (int32)Underlying->GetSignedIntPropertyValue(EnumProp->ContainerPtrToValuePtr<void>(Container));
			}
		}
		return -1;
	};

	const int32 PoiType = FindByteValue(POIActor, Class, TEXT("POI_Type"));
	if (PoiType != 2) { return FallbackInformation; } // only POI_FILTER (real-estate unit) markers get the spec sheet

	const FStructProperty* FilterProp = nullptr;
	for (TFieldIterator<FProperty> It(Class); It; ++It)
	{
		if (It->GetName().StartsWith(TEXT("Filter"), ESearchCase::IgnoreCase))
		{
			FilterProp = CastField<FStructProperty>(*It);
			if (FilterProp) { break; }
		}
	}
	if (!FilterProp) { return FallbackInformation; }
	const void* FilterPtr = FilterProp->ContainerPtrToValuePtr<void>(POIActor);
	UStruct* FilterStruct = FilterProp->Struct;

	auto FindIntValue = [FilterStruct](const void* Container, const TCHAR* Prefix) -> int32
	{
		for (TFieldIterator<FIntProperty> It(FilterStruct); It; ++It)
		{
			if (It->GetName().StartsWith(Prefix, ESearchCase::IgnoreCase))
			{
				return It->GetPropertyValue_InContainer(Container);
			}
		}
		return 0;
	};
	auto FindEnumDisplayName = [FilterStruct](const void* Container, const TCHAR* Prefix) -> FString
	{
		for (TFieldIterator<FProperty> It(FilterStruct); It; ++It)
		{
			if (!It->GetName().StartsWith(Prefix, ESearchCase::IgnoreCase)) { continue; }
			if (const FByteProperty* ByteProp = CastField<FByteProperty>(*It))
			{
				if (ByteProp->Enum)
				{
					return ByteProp->Enum->GetDisplayNameTextByValue(ByteProp->GetPropertyValue_InContainer(Container)).ToString();
				}
			}
			if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(*It))
			{
				const FNumericProperty* Underlying = EnumProp->GetUnderlyingProperty();
				const int64 Value = Underlying->GetSignedIntPropertyValue(EnumProp->ContainerPtrToValuePtr<void>(Container));
				if (UEnum* Enum = EnumProp->GetEnum()) { return Enum->GetDisplayNameTextByValue(Value).ToString(); }
			}
		}
		return FString();
	};

	const int32 Surface = FindIntValue(FilterPtr, TEXT("Surface"));
	const int32 Bedrooms = FindIntValue(FilterPtr, TEXT("Bedroomscount"));
	const int32 Bathrooms = FindIntValue(FilterPtr, TEXT("Bathroomscount"));
	const int32 Narx = FindIntValue(FilterPtr, TEXT("Narx"));
	const FString Availability = FindEnumDisplayName(FilterPtr, TEXT("Availability"));

	// Orientation is a nested DirectionStruct{N,E,S,W} sub-struct of Filter, not a flat field -
	// join whichever compass bools are set into a display string (e.g. "Shimoliy, Sharqiy").
	FString OrientationStr;
	for (TFieldIterator<FProperty> It(FilterStruct); It; ++It)
	{
		if (!It->GetName().StartsWith(TEXT("Orientation"), ESearchCase::IgnoreCase)) { continue; }
		const FStructProperty* OrientProp = CastField<FStructProperty>(*It);
		if (!OrientProp) { continue; }
		const void* OrientPtr = OrientProp->ContainerPtrToValuePtr<void>(FilterPtr);
		UStruct* OrientStruct = OrientProp->Struct;
		static const TCHAR* Letters[] = { TEXT("N"), TEXT("E"), TEXT("S"), TEXT("W") };
		static const TCHAR* Labels[] = { TEXT("Shimoliy"), TEXT("Sharqiy"), TEXT("Janubiy"), TEXT("G'arbiy") };
		TArray<FString> Parts;
		for (int32 i = 0; i < 4; ++i)
		{
			for (TFieldIterator<FBoolProperty> BIt(OrientStruct); BIt; ++BIt)
			{
				if (BIt->GetName().Equals(Letters[i], ESearchCase::IgnoreCase))
				{
					if (BIt->GetPropertyValue_InContainer(OrientPtr)) { Parts.Add(Labels[i]); }
					break;
				}
			}
		}
		OrientationStr = FString::Join(Parts, TEXT(", "));
		break;
	}

	// mirrors real BP_Info_Widget's Update_Text FormatText template for POI_FILTER (case NewEnumerator2),
	// extended with Narx/Orientation since this project's own DT_Demonstration_01_POI_Filter DataTable
	// (unlike the real ArchVizExplorer product's schema) carries both fields - see user's screenshot of
	// the DataTable row asking for exactly these fields to show on the info card.
	return FString::Printf(TEXT("Maydon: %d m²\nXonalar soni: %d\nSanuzellar soni: %d\nYo'nalish: %s\nNarx: %d\nHolati: %s"),
		Surface, Bedrooms, Bathrooms, *OrientationStr, Narx, *Availability);
}

void ULanessaV2Widget::PopulatePoiFilterFields(AActor* POIActor, UDataTable* Table)
{
	bPoiIsFilterCard = false;
	if (!POIActor) { return; }
	UClass* Class = POIActor->GetClass();

	auto FindByteValue = [](const void* Container, UStruct* Struct, const TCHAR* Prefix) -> int32
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			if (!It->GetName().StartsWith(Prefix, ESearchCase::IgnoreCase)) { continue; }
			if (const FByteProperty* ByteProp = CastField<FByteProperty>(*It))
			{
				return (int32)ByteProp->GetPropertyValue_InContainer(Container);
			}
			if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(*It))
			{
				const FNumericProperty* Underlying = EnumProp->GetUnderlyingProperty();
				return (int32)Underlying->GetSignedIntPropertyValue(EnumProp->ContainerPtrToValuePtr<void>(Container));
			}
		}
		return -1;
	};

	const int32 PoiType = FindByteValue(POIActor, Class, TEXT("POI_Type"));
	if (PoiType != 2) { return; } // only POI_FILTER (real-estate unit) markers get the stat-grid card

	const FStructProperty* FilterProp = nullptr;
	for (TFieldIterator<FProperty> It(Class); It; ++It)
	{
		if (It->GetName().StartsWith(TEXT("Filter"), ESearchCase::IgnoreCase))
		{
			FilterProp = CastField<FStructProperty>(*It);
			if (FilterProp) { break; }
		}
	}
	if (!FilterProp) { return; }
	const void* FilterPtr = FilterProp->ContainerPtrToValuePtr<void>(POIActor);
	UStruct* FilterStruct = FilterProp->Struct;

	auto FindIntValue = [FilterStruct](const void* Container, const TCHAR* Prefix) -> int32
	{
		for (TFieldIterator<FIntProperty> It(FilterStruct); It; ++It)
		{
			if (It->GetName().StartsWith(Prefix, ESearchCase::IgnoreCase))
			{
				return It->GetPropertyValue_InContainer(Container);
			}
		}
		return 0;
	};
	auto FindEnumDisplayName = [FilterStruct](const void* Container, const TCHAR* Prefix) -> FString
	{
		for (TFieldIterator<FProperty> It(FilterStruct); It; ++It)
		{
			if (!It->GetName().StartsWith(Prefix, ESearchCase::IgnoreCase)) { continue; }
			if (const FByteProperty* ByteProp = CastField<FByteProperty>(*It))
			{
				if (ByteProp->Enum)
				{
					return ByteProp->Enum->GetDisplayNameTextByValue(ByteProp->GetPropertyValue_InContainer(Container)).ToString();
				}
			}
			if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(*It))
			{
				const FNumericProperty* Underlying = EnumProp->GetUnderlyingProperty();
				const int64 Value = Underlying->GetSignedIntPropertyValue(EnumProp->ContainerPtrToValuePtr<void>(Container));
				if (UEnum* Enum = EnumProp->GetEnum()) { return Enum->GetDisplayNameTextByValue(Value).ToString(); }
			}
		}
		return FString();
	};

	bPoiIsFilterCard = true;
	PoiSurfaceVal = FindIntValue(FilterPtr, TEXT("Surface"));
	PoiRoomsCount = FindIntValue(FilterPtr, TEXT("Bedroomscount"));
	PoiBathroomsVal = FindIntValue(FilterPtr, TEXT("Bathroomscount"));

	// Floor comes from Table (see header comment) rather than FilterPtr like the fields above.
	PoiFloor = 0;
	if (Table)
	{
		FString RowName;
		for (TFieldIterator<FProperty> It(Class); It; ++It)
		{
			if (!It->GetName().StartsWith(TEXT("rowName"), ESearchCase::IgnoreCase)) { continue; }
			if (const FStrProperty* StrProp = CastField<FStrProperty>(*It))
			{
				RowName = StrProp->GetPropertyValue_InContainer(POIActor);
				break;
			}
			if (const FNameProperty* NameProp = CastField<FNameProperty>(*It))
			{
				RowName = NameProp->GetPropertyValue_InContainer(POIActor).ToString();
				break;
			}
		}
		if (const UScriptStruct* RowStruct = Table->GetRowStruct())
		{
			if (uint8* RowPtr = !RowName.IsEmpty() ? Table->FindRowUnchecked(FName(*RowName)) : nullptr)
			{
				for (TFieldIterator<FIntProperty> It(RowStruct); It; ++It)
				{
					if (It->GetName().StartsWith(TEXT("Floor"), ESearchCase::IgnoreCase))
					{
						PoiFloor = It->GetPropertyValue_InContainer(RowPtr);
						break;
					}
				}
			}
		}
	}

	const FString Availability = FindEnumDisplayName(FilterPtr, TEXT("Availability"));
	// screenshot's card shows the short form ("Band") rather than the full enum display name
	// ("Band Qilingan") used elsewhere (search chips, BuildPoiInfoText) - map explicitly rather than
	// truncating the string, since "Sotilmagan" (not-sold/vacant) reads better as "Bo'sh" here.
	if (Availability == TEXT("Sotilmagan")) { PoiAvailabilityShort = TEXT("Bo'sh"); }
	else if (Availability == TEXT("Band Qilingan")) { PoiAvailabilityShort = TEXT("Band"); }
	else if (Availability == TEXT("Sotilgan")) { PoiAvailabilityShort = TEXT("Sotilgan"); }
	else { PoiAvailabilityShort = Availability; }

	// REJA button visibility: scan POIActor's own POI_Info_Struct.Media array for an entry titled
	// "FLOOR PLAN" (case-insensitive, per user's screenshot of the Details panel showing exactly that
	// MediaTitle/PreviewImage pairing). REJA's click itself now opens the real BP_Gallery_Widget with
	// the POI's full Media array (OnPoiCardAction "media", handled in BP_Explorer_PC) rather than
	// showing just this one image, but a unit only gets the REJA button at all once it has a plan.
	bPoiHasPlan = false;
	for (TFieldIterator<FProperty> It(Class); It; ++It)
	{
		if (!It->GetName().StartsWith(TEXT("POI_Info_Struct"), ESearchCase::IgnoreCase)) { continue; }
		const FStructProperty* InfoProp = CastField<FStructProperty>(*It);
		if (!InfoProp) { continue; }
		const void* InfoPtr = InfoProp->ContainerPtrToValuePtr<void>(POIActor);
		for (TFieldIterator<FProperty> MIt(InfoProp->Struct); MIt; ++MIt)
		{
			if (!MIt->GetName().StartsWith(TEXT("Media"), ESearchCase::IgnoreCase)) { continue; }
			const FArrayProperty* MediaArrayProp = CastField<FArrayProperty>(*MIt);
			if (!MediaArrayProp) { continue; }
			const FStructProperty* ElemStructProp = CastField<FStructProperty>(MediaArrayProp->Inner);
			if (!ElemStructProp) { break; }
			FScriptArrayHelper Helper(MediaArrayProp, MediaArrayProp->ContainerPtrToValuePtr<void>(InfoPtr));
			for (int32 i = 0; i < Helper.Num(); ++i)
			{
				const void* ElemPtr = Helper.GetRawPtr(i);
				FString Title;
				for (TFieldIterator<FProperty> EIt(ElemStructProp->Struct); EIt; ++EIt)
				{
					if (EIt->GetName().StartsWith(TEXT("MediaTitle"), ESearchCase::IgnoreCase))
					{
						if (const FStrProperty* StrProp = CastField<FStrProperty>(*EIt)) { Title = StrProp->GetPropertyValue_InContainer(ElemPtr); }
						break;
					}
				}
				if (Title.TrimStartAndEnd().Equals(TEXT("FLOOR PLAN"), ESearchCase::IgnoreCase))
				{
					bPoiHasPlan = true;
					break;
				}
			}
			break;
		}
		break;
	}
}

void ULanessaV2Widget::GetPOIFloorAndBuilding(AActor* POIActor, UDataTable* Table, int32& OutFloor, FString& OutBuilding)
{
	OutFloor = 0;
	OutBuilding.Empty();
	if (!POIActor || !Table) { return; }

	FString RowName;
	for (TFieldIterator<FProperty> It(POIActor->GetClass()); It; ++It)
	{
		if (!It->GetName().StartsWith(TEXT("rowName"), ESearchCase::IgnoreCase)) { continue; }
		if (const FStrProperty* StrProp = CastField<FStrProperty>(*It))
		{
			RowName = StrProp->GetPropertyValue_InContainer(POIActor);
			break;
		}
		if (const FNameProperty* NameProp = CastField<FNameProperty>(*It))
		{
			RowName = NameProp->GetPropertyValue_InContainer(POIActor).ToString();
			break;
		}
	}
	if (RowName.IsEmpty()) { return; }

	const UScriptStruct* RowStruct = Table->GetRowStruct();
	if (!RowStruct) { return; }
	uint8* RowPtr = Table->FindRowUnchecked(FName(*RowName));
	if (!RowPtr) { return; }

	for (TFieldIterator<FIntProperty> It(RowStruct); It; ++It)
	{
		if (It->GetName().StartsWith(TEXT("Floor"), ESearchCase::IgnoreCase))
		{
			OutFloor = It->GetPropertyValue_InContainer(RowPtr);
			break;
		}
	}
	for (TFieldIterator<FStrProperty> It(RowStruct); It; ++It)
	{
		if (It->GetName().StartsWith(TEXT("Building"), ESearchCase::IgnoreCase))
		{
			OutBuilding = It->GetPropertyValue_InContainer(RowPtr);
			break;
		}
	}
}

bool ULanessaV2Widget::DoesPOIMatchFloorBuilding(AActor* POIActor, UDataTable* Table, int32 Floor, const FString& Building)
{
	int32 ActorFloor = 0;
	FString ActorBuilding;
	GetPOIFloorAndBuilding(POIActor, Table, ActorFloor, ActorBuilding);
	return ActorFloor == Floor && ActorBuilding.Equals(Building, ESearchCase::IgnoreCase);
}

void ULanessaV2Widget::CallPOIShowHide(AActor* POIActor, bool bShow)
{
	if (!POIActor) { return; }
	const FName FuncName = bShow ? TEXT("Show_POI") : TEXT("Hide_POI");
	if (UFunction* Func = POIActor->FindFunction(FuncName))
	{
		POIActor->ProcessEvent(Func, nullptr);
	}
}

void ULanessaV2Widget::CallPOISelectPOI(AActor* POIActor)
{
	if (!POIActor) { return; }
	if (UFunction* Func = POIActor->FindFunction(TEXT("Select_POI")))
	{
		POIActor->ProcessEvent(Func, nullptr);
	}
}

// Writes BP_Explorer_Pawn's own Location_New/Pitch_New/Yaw_New/TargetArmLength_New by exact name via
// reflection (see SetExplorerPawnCameraTarget's header comment for why reflection and not a setter
// node). Split out of SetExplorerPawnCameraTarget so the live-drag path in RemoteCameraSet can keep
// those four in step without also triggering the pawn's Focus animation.
static void LanessaWritePawnCameraVars(AActor* PawnActor, FVector Location, double Pitch, double Yaw, double TargetArmLength)
{
	if (!PawnActor) { return; }
	for (TFieldIterator<FProperty> It(PawnActor->GetClass()); It; ++It)
	{
		const FString Name = It->GetName();
		if (Name.Equals(TEXT("Location_New"), ESearchCase::IgnoreCase))
		{
			if (FStructProperty* StructProp = CastField<FStructProperty>(*It))
			{
				void* ValuePtr = StructProp->ContainerPtrToValuePtr<void>(PawnActor);
				StructProp->Struct->CopyScriptStruct(ValuePtr, &Location);
			}
		}
		else if (Name.Equals(TEXT("Pitch_New"), ESearchCase::IgnoreCase))
		{
			if (FDoubleProperty* DP = CastField<FDoubleProperty>(*It)) { DP->SetPropertyValue_InContainer(PawnActor, Pitch); }
			else if (FFloatProperty* FP = CastField<FFloatProperty>(*It)) { FP->SetPropertyValue_InContainer(PawnActor, (float)Pitch); }
		}
		else if (Name.Equals(TEXT("Yaw_New"), ESearchCase::IgnoreCase))
		{
			if (FDoubleProperty* DP = CastField<FDoubleProperty>(*It)) { DP->SetPropertyValue_InContainer(PawnActor, Yaw); }
			else if (FFloatProperty* FP = CastField<FFloatProperty>(*It)) { FP->SetPropertyValue_InContainer(PawnActor, (float)Yaw); }
		}
		else if (Name.Equals(TEXT("TargetArmLength_New"), ESearchCase::IgnoreCase))
		{
			if (FDoubleProperty* DP = CastField<FDoubleProperty>(*It)) { DP->SetPropertyValue_InContainer(PawnActor, TargetArmLength); }
			else if (FFloatProperty* FP = CastField<FFloatProperty>(*It)) { FP->SetPropertyValue_InContainer(PawnActor, (float)TargetArmLength); }
		}
	}
}

void ULanessaV2Widget::SetExplorerPawnCameraTarget(AActor* PawnActor, FVector Location, double Pitch, double Yaw, double TargetArmLength)
{
	if (!PawnActor) { return; }
	LanessaWritePawnCameraVars(PawnActor, Location, Pitch, Yaw, TargetArmLength);
	if (UFunction* FocusFunc = PawnActor->FindFunction(TEXT("Focus")))
	{
		PawnActor->ProcessEvent(FocusFunc, nullptr);
	}
}

void ULanessaV2Widget::SetActorStringVariable(AActor* Target, const FString& VariableName, const FString& Value)
{
	if (!Target) { return; }
	for (TFieldIterator<FProperty> It(Target->GetClass()); It; ++It)
	{
		if (It->GetName().Equals(VariableName, ESearchCase::IgnoreCase))
		{
			if (FStrProperty* StrProp = CastField<FStrProperty>(*It))
			{
				StrProp->SetPropertyValue_InContainer(Target, Value);
			}
			return;
		}
	}
}

void ULanessaV2Widget::CallActorFunction(AActor* Target, const FString& FunctionName)
{
	if (!Target) { return; }
	if (UFunction* Func = Target->FindFunction(FName(*FunctionName)))
	{
		Target->ProcessEvent(Func, nullptr);
	}
}

void ULanessaV2Widget::CallParentFunction(AActor* Target, const FString& FunctionName)
{
	if (!Target) { return; }
	UClass* SuperClass = Target->GetClass()->GetSuperClass();
	if (!SuperClass) { return; }
	if (UFunction* Func = SuperClass->FindFunctionByName(FName(*FunctionName)))
	{
		Target->ProcessEvent(Func, nullptr);
	}
}

void ULanessaV2Widget::GetOwnFloorAndBuilding(AActor* POIActor, int32& OutFloor, FString& OutBuilding)
{
	OutFloor = 0;
	OutBuilding.Empty();
	if (!POIActor) { return; }

	const FStructProperty* FilterProp = nullptr;
	for (TFieldIterator<FProperty> It(POIActor->GetClass()); It; ++It)
	{
		if (It->GetName().StartsWith(TEXT("Filter"), ESearchCase::IgnoreCase))
		{
			FilterProp = CastField<FStructProperty>(*It);
			if (FilterProp) { break; }
		}
	}
	if (!FilterProp) { return; }
	const void* FilterPtr = FilterProp->ContainerPtrToValuePtr<void>(POIActor);
	const UStruct* FilterStruct = FilterProp->Struct;

	for (TFieldIterator<FIntProperty> It(FilterStruct); It; ++It)
	{
		if (It->GetName().StartsWith(TEXT("Floor"), ESearchCase::IgnoreCase))
		{
			OutFloor = It->GetPropertyValue_InContainer(FilterPtr);
			break;
		}
	}
	for (TFieldIterator<FStrProperty> It(FilterStruct); It; ++It)
	{
		if (It->GetName().StartsWith(TEXT("Building"), ESearchCase::IgnoreCase))
		{
			OutBuilding = It->GetPropertyValue_InContainer(FilterPtr);
			break;
		}
	}
}

void ULanessaV2Widget::ApplySearch()
{
	TArray<FString> Matching = ComputeMatchingPoiIds();
	int32 PMin, PMax, SMin, SMax;
	ComputeFieldRange(TEXT("narx"), PMin, PMax);
	ComputeFieldRange(TEXT("kvadratura"), SMin, SMax);
	const FVector2D PH = GetRangeHandle(TEXT("narx"), 0.f, 1.f);
	const FVector2D SH = GetRangeHandle(TEXT("kvadratura"), 0.f, 1.f);
	UE_LOG(LogTemp, Warning, TEXT("[LanessaDebug] ApplySearch: SearchUnits.Num=%d -> Matching.Num=%d | narx %d-%d | kvadratura %d-%d"),
		SearchUnits.Num(), Matching.Num(),
		PMin + FMath::RoundToInt(PH.X * (PMax - PMin)), PMin + FMath::RoundToInt(PH.Y * (PMax - PMin)),
		SMin + FMath::RoundToInt(SH.X * (SMax - SMin)), SMin + FMath::RoundToInt(SH.Y * (SMax - SMin)));
	if (Matching.Num() > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LanessaDebug]   sample match PoiId=%s"), *Matching[0]);
	}
	OnSearchApplied.Broadcast(Matching);
}

void ULanessaV2Widget::ComputeFieldRange(const FString& FieldId, int32& OutMin, int32& OutMax) const
{
	const bool bPrice = (FieldId == TEXT("narx"));
	if (SearchUnits.Num() == 0)
	{
		OutMin = bPrice ? 420 : 52;
		OutMax = bPrice ? 980 : 118;
		return;
	}
	OutMin = TNumericLimits<int32>::Max();
	OutMax = TNumericLimits<int32>::Min();
	for (const FLanessaV2SearchUnit& Unit : SearchUnits)
	{
		const int32 V = bPrice ? Unit.Price : Unit.Surface;
		OutMin = FMath::Min(OutMin, V);
		OutMax = FMath::Max(OutMax, V);
	}
}

// Chip-group definitions (label + default "on" state) shared verbatim between BuildSidePanelQidiruvContent
// (renders them) and ComputeMatchingPoiIds (evaluates them) so the two can never drift apart.
namespace LanessaSearchChips
{
	// Real BP_UnitSearch_Widget's BedroomCount/BathroomCount/Availability Sets all start FULL (every
	// value included, see lanessa-real-product-behavior memory) - i.e. no filtering until the user
	// actively excludes something. All chips default "off" here on purpose: ComputeMatchingPoiIds
	// treats zero-selected-in-a-group as "don't restrict on this criterion", reproducing that same
	// show-everything-by-default behavior instead of the v2 mockup's arbitrary example on/off states.
	static const TArray<TPair<FString, bool>> Bedrooms = { {TEXT("1"), false}, {TEXT("2"), false}, {TEXT("3"), false}, {TEXT("4+"), false} };
	static const TArray<TPair<FString, bool>> Bathrooms = { {TEXT("1"), false}, {TEXT("2"), false}, {TEXT("3+"), false} };
	static const TArray<TPair<FString, bool>> Availability = { {TEXT("Sotilmagan"), false}, {TEXT("Band qilingan"), false}, {TEXT("Sotilgan"), false} };

	static bool BedroomMatches(const FString& Label, int32 InBedrooms) { return Label == TEXT("4+") ? InBedrooms >= 4 : FCString::Atoi(*Label) == InBedrooms; }
	static bool BathroomMatches(const FString& Label, int32 InBathrooms) { return Label == TEXT("3+") ? InBathrooms >= 3 : FCString::Atoi(*Label) == InBathrooms; }
	static bool AvailabilityMatches(const FString& Label, const FString& InAvailability)
	{
		const FString Real = (Label == TEXT("Band qilingan")) ? FString(TEXT("Band Qilingan")) : Label;
		return Real == InAvailability;
	}
}

TArray<FString> ULanessaV2Widget::ComputeMatchingPoiIds() const
{
	TArray<FString> Result;
	if (SearchUnits.Num() == 0) { return Result; }

	int32 SurfaceMin, SurfaceMax, PriceMin, PriceMax;
	ComputeFieldRange(TEXT("kvadratura"), SurfaceMin, SurfaceMax);
	ComputeFieldRange(TEXT("narx"), PriceMin, PriceMax);

	const FVector2D SurfaceHandle = GetRangeHandle(TEXT("kvadratura"), 0.f, 1.f);
	const FVector2D PriceHandle = GetRangeHandle(TEXT("narx"), 0.f, 1.f);
	const int32 SurfaceLo = SurfaceMin + FMath::RoundToInt(SurfaceHandle.X * (SurfaceMax - SurfaceMin));
	const int32 SurfaceHi = SurfaceMin + FMath::RoundToInt(SurfaceHandle.Y * (SurfaceMax - SurfaceMin));
	const int32 PriceLo = PriceMin + FMath::RoundToInt(PriceHandle.X * (PriceMax - PriceMin));
	const int32 PriceHi = PriceMin + FMath::RoundToInt(PriceHandle.Y * (PriceMax - PriceMin));

	auto ActiveLabels = [this](const FString& Group, const TArray<TPair<FString, bool>>& Chips)
	{
		TArray<FString> On;
		for (const auto& C : Chips)
		{
			if (IsToggled(FString::Printf(TEXT("qidiruv:%s:%s"), *Group, *C.Key), C.Value)) { On.Add(C.Key); }
		}
		return On;
	};
	const TArray<FString> OnBedrooms = ActiveLabels(TEXT("yotoqxona"), LanessaSearchChips::Bedrooms);
	const TArray<FString> OnBathrooms = ActiveLabels(TEXT("sanuzel"), LanessaSearchChips::Bathrooms);
	const TArray<FString> OnAvailability = ActiveLabels(TEXT("holati"), LanessaSearchChips::Availability);

	for (const FLanessaV2SearchUnit& Unit : SearchUnits)
	{
		if (Unit.Surface < SurfaceLo || Unit.Surface > SurfaceHi) { continue; }
		if (Unit.Price < PriceLo || Unit.Price > PriceHi) { continue; }
		if (OnBedrooms.Num() > 0 && !OnBedrooms.ContainsByPredicate([&Unit](const FString& L) { return LanessaSearchChips::BedroomMatches(L, Unit.Bedrooms); })) { continue; }
		if (OnBathrooms.Num() > 0 && !OnBathrooms.ContainsByPredicate([&Unit](const FString& L) { return LanessaSearchChips::BathroomMatches(L, Unit.Bathrooms); })) { continue; }
		if (OnAvailability.Num() > 0 && !OnAvailability.ContainsByPredicate([&Unit](const FString& L) { return LanessaSearchChips::AvailabilityMatches(L, Unit.Availability); })) { continue; }
		Result.Add(Unit.PoiId);
	}
	return Result;
}

static bool LanessaReadDouble(AActor* Actor, FName Prefix, double& Out);

void ULanessaV2Widget::RefreshEnvironmentCaption()
{
	UWorld* World = GetWorld();
	AActor* Sky = EnvironmentSky.Get();
	if (!Sky || Sky->GetWorld() != World)
	{
		EnvironmentSky.Reset();
		Sky = nullptr;
		const FString& Prefix = ULanessaDayNightSettings::Get().UdsActorClassPrefix;
		if (World && !Prefix.IsEmpty())
		{
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (IsValid(*It) && It->GetClass()->GetName().StartsWith(Prefix))
				{
					Sky = *It;
					EnvironmentSky = Sky;
					break;
				}
			}
		}
	}
	// Actor hali yuklanmagan bo'lsa, qayta qidirish har kadrda bajarilmaydi.
	EnvironmentCaptionRefreshLeft = Sky ? 0.25f : 2.f;
	FText NewCaption = FText::FromString(TEXT("—"));
	const FIntProperty* MonthProperty = Sky ? FindFProperty<FIntProperty>(Sky->GetClass(), TEXT("Month")) : nullptr;
	if (MonthProperty)
	{
		const int32 Month = MonthProperty->GetPropertyValue_InContainer(Sky);
		static const TCHAR* Months[] = { TEXT("YANVAR"), TEXT("FEVRAL"), TEXT("MART"), TEXT("APREL"),
			TEXT("MAY"), TEXT("IYUN"), TEXT("IYUL"), TEXT("AVGUST"), TEXT("SENTYABR"),
			TEXT("OKTYABR"), TEXT("NOYABR"), TEXT("DEKABR") };
		if (Month >= 1 && Month <= 12)
		{
			FString Caption = Months[Month - 1];
			double TimeOfDay = 0., Sunrise = 0., Sunset = 0.;
			const FBoolProperty* Simulation = FindFProperty<FBoolProperty>(Sky->GetClass(), TEXT("Simulate Real Sun"));
			if (Simulation)
			{
				const bool bSimulated = Simulation->GetPropertyValue_InContainer(Sky);
				// Yorliq UDSning amaldagi jadvalini ko'rsatadi; lampalarning 17:00 chegarasi ishlatilmaydi.
				if (LanessaReadDouble(Sky, TEXT("Time of Day"), TimeOfDay)
					&& LanessaReadDouble(Sky, bSimulated ? TEXT("Simulated Sunrise Time") : TEXT("Dawn Time"), Sunrise)
					&& LanessaReadDouble(Sky, bSimulated ? TEXT("Simulated Sunset Time") : TEXT("Dusk Time"), Sunset)
					&& FMath::IsFinite(TimeOfDay) && FMath::IsFinite(Sunrise) && FMath::IsFinite(Sunset)
					&& Sunrise >= 0. && Sunset <= 2400. && Sunrise < Sunset)
				{
					TimeOfDay = FMath::Fmod(FMath::Fmod(TimeOfDay, 2400.) + 2400., 2400.);
					Caption += TimeOfDay >= Sunrise && TimeOfDay < Sunset ? TEXT(" · KUNDUZI") : TEXT(" · TUN");
				}
			}
			NewCaption = FText::FromString(Caption);
		}
	}
	if (!EnvironmentCaption.EqualTo(NewCaption))
	{
		EnvironmentCaption = NewCaption;
		Invalidate(EInvalidateWidgetReason::Paint);
	}
}

void ULanessaV2Widget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	EnvironmentCaptionRefreshLeft -= InDeltaTime;
	if (EnvironmentCaptionRefreshLeft <= 0.f)
	{
		RefreshEnvironmentCaption();
	}
	// mirrors launchTourToast()'s setTimeout(..., 1800) auto-hide
	if (ToastTimeLeft > 0.f)
	{
		ToastTimeLeft = FMath::Max(0.f, ToastTimeLeft - InDeltaTime);
	}
	// One-shot initial day/night sync - see bDayNightSynced. Retries every tick only until it
	// succeeds, so a UDS actor that streams in late is still picked up; once it has run, this is a
	// single bool test per frame.
	if (!bDayNightSynced && ULanessaDayNight::ApplyDayNightFromUDS(this, /*bForce=*/true))
	{
		bDayNightSynced = true;
	}
}

bool ULanessaV2Widget::IsClusterDisplayMode()
{
	// nDisplay ning o'z modulini so'ramaymiz - bu plaginni nDisplay ga bog'lab
	// qo'yardi. Bayroqning o'zi yetarli va aynan shu bayroqni dvijok ham o'qiydi
	// (UDisplayClusterGameEngine::DetectOperationMode), ya'ni ikkalasi bir manbadan.
	static const bool bCluster = FParse::Param(FCommandLine::Get(), TEXT("dc_cluster"));
	return bCluster;
}

TSharedRef<SWidget> ULanessaV2Widget::RebuildWidget()
{
	using namespace V2;

	// 180/360 yoki ko'p ekranli rejimda EKRANDA FAQAT SAHNA qolishi kerak.
	// Boshqaruv operator panelida, ya'ni menyu, kartalar va soat ekranga chizilmaydi.
	// Aks holda ular ekranlar orasiga bo'linib ketadi: o'lchangan - logotip va
	// qavat tugmalari birinchi ekranda, soat va POI kartasi ikkinchisida qolgan.
	//
	// Vidjetning O'ZI yashamoqda davom etadi, faqat hech narsa chizmaydi. Bu muhim:
	// Remote* funksiyalari uning uzatmalari (OnNavClicked, Reset_SectionView va h.k.)
	// orqali ishlaydi, ularni BP_Explorer_PC eshitadi - ya'ni panel bu rejimda ham
	// sahnani to'liq boshqaradi.
	if (IsClusterDisplayMode() && LanessaCfg().bHideUiInClusterMode)
	{
		UE_LOG(LogTemp, Log,
			TEXT("[Lanessa] nDisplay rejimi: ekran interfeysi chizilmaydi, boshqaruv operator panelida"));
		return SNullWidget::NullWidget;
	}

	return SNew(SOverlay)

		// NOTE: v2 mockup's .stage background (linear-gradient(180deg,#1c1b16,#0c0c0a,#000), drawn via
		// SLanessaStageGradient) is intentionally NOT drawn here anymore. In the static HTML mockup that
		// opaque gradient stood in for "the 3D scene" since there was no real 3D renderer behind it. In
		// the real game this widget is overlaid on top of the actual rendered 3D world (via AddToViewport),
		// so a full-screen opaque layer here completely blocks the real scene from being seen at all -
		// confirmed by the user in a live Play-in-Editor test (2026-07-18): with the gradient in place,
		// only UI chrome was visible and the world behind it was solid black. Matches the same
		// "don't bake in something that belongs to the real 3D layer" lesson as the marker/tour-start
		// removal above - this master menu is UI chrome only, the world underneath must show through.

		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(48.f, 36.f, 0.f, 0.f)[BuildBrand()]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(65.f, 100.f, 0.f, 40.f)[BuildSpine()]

		// NOTE: v2's mockup diamond markers are NOT drawn here. Investigation of the real
		// ArchVizExplorer product (2026-07-18) confirmed the master menu widget's own WidgetTree
		// draws no markers at all - the center area is left transparent, and what looks like markers
		// are separate 3D world-space UWidgetComponents (class BP_3D_Widget) attached to each BP_POI
		// actor, projected to screen by the renderer wherever that actor sits in the level. Baking
		// fake 2D markers into this widget was an architecture mistake from porting the mockup
		// visually without first checking the real product's structure. SelectPoi() stays available
		// for a real BP_POI's 3D click to call into (opening BuildPoiCard below), matching how
		// BP_Explorer_PC.MasterMenu.BP_Info_Menu_01.Show_InfoMenu() is really invoked.

		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(0.f, 176.f, 0.f, 0.f)[BuildRail()]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, 44.f)[BuildRailFoot()]

		// .side-panel: closeAllPanels() / navitem click shows the matching one - driven live by ActiveView
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(224.f, 176.f, 0.f, 0.f)
		[
			BuildSidePanel(TEXT("qidiruv"), BuildSidePanelQidiruvContent())
		]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(224.f, 176.f, 0.f, 0.f)
		[
			// Category rows are NOT hardcoded here - starts empty, BP_Explorer_PC calls SetPanelCategories()
			// (typically right after CreateWidget) with the real category tag/label list, so renaming a
			// category is a Blueprint-side data change, not a C++ recompile.
			BuildSidePanel(TEXT("atrofi"), BuildSidePanelCatListContent(TEXT("Atrofi"), TEXT("XARITADA KO'RSATISH"), TEXT("Surroundings")))
		]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(224.f, 176.f, 0.f, 0.f)
		[
			BuildSidePanel(TEXT("qulay"), BuildSidePanelCatListContent(TEXT("Qulayliklar"), TEXT("HUDUD ICHIDA"), TEXT("Amenities")))
		]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(224.f, 176.f, 0.f, 0.f)
		[
			BuildSidePanel(TEXT("qurilish"), BuildSidePanelQurilishContent())
		]

		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(0.f, 36.f, 48.f, 0.f)[BuildChipCard()]

		// NOTE: v2 mockup's .tour-start (play button + pulsing ring) is not drawn here - confirmed
		// with the user (2026-07-18) it has no equivalent in the real ArchVizExplorer product and was
		// only ever a decorative mockup element, not real functionality worth porting.

		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, 40.f)
		[
			SNew(SBox).Visibility(TAttribute<EVisibility>::Create([this]() { return ActiveView == TEXT("qirqim") ? EVisibility::Visible : EVisibility::Collapsed; }))
			[BuildFloorRail()]
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, 40.f)
		[
			SNew(SBox).Visibility(TAttribute<EVisibility>::Create([this]()
			{
				return (ActiveView == TEXT("qurilish") && ActiveBuildStage == TEXT("yerosti")) ? EVisibility::Visible : EVisibility::Collapsed;
			}))
			[BuildUndergroundRail()]
		]

		// .poi.open - visible once a marker is clicked (SelectedPoiId non-empty)
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(0.f, 0.f, 48.f, 0.f)
		[
			SNew(SBox).Visibility(TAttribute<EVisibility>::Create([this]() { return SelectedPoiId.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; }))
			[BuildPoiCard()]
		]

		+ SOverlay::Slot()
		[
			SNew(SBox).Visibility(TAttribute<EVisibility>::Create([this]() { return bPlanOpen ? EVisibility::Visible : EVisibility::Collapsed; }))[BuildPlanModal()]
		]

		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 48.f, 40.f)[BuildUtility()]

		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, 112.f)[BuildTourToast()];
}

// .brand { top:36px; left:48px; gap:14px; } img{height:46px}
// NOTE: real v2 HTML has NO separate "LANESSA" text element - the wordmark is baked into the logo image itself.
TSharedRef<SWidget> ULanessaV2Widget::BuildBrand()
{
	using namespace V2;

	if (!BrandLogoTexture)
	{
		BrandLogoTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/ArchVizExplorer/Blueprints/New_widgets/Textures/T_Lanessa_Logo_Horizontal.T_Lanessa_Logo_Horizontal"));
	}
	FSlateBrush* LogoBrush = new FSlateBrush();
	if (BrandLogoTexture) { LogoBrush->SetResourceObject(BrandLogoTexture); }
	LogoBrush->ImageSize = FVector2D(182.f, 46.f);

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 14.f, 0.f)
		[
			SNew(SBox).HeightOverride(46.f)[SNew(SImage).Image(LogoBrush)]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			// .brand-word .tag { font-size:9.5px; letter-spacing:.11em; text-transform:uppercase; color:var(--text-dim); }
			SNew(STextBlock).Text(FText::FromString(TEXT("HAR BIR CHIZIQDA SAN'AT"))).Font(F(9)).ColorAndOpacity(FSlateColor(TextDim))
		];
}

// .spine { width:1px; background: linear-gradient(line -> transparent); } - flat line-soft used, Slate has no gradient brush here.
TSharedRef<SWidget> ULanessaV2Widget::BuildSpine()
{
	return SNew(SBox).WidthOverride(1.f)[V2::Fill(V2::LineSoft)];
}

// .navitem: click sets ActiveView (see .navitem click handler in v2's <script>), which drives the matching
// .side-panel's visibility live. Hover uses the same rgba(olive,.10) tint as .active per the CSS rule
// `.navitem:hover, .navitem.active { background: rgba(153,153,102,.10); }`.
TSharedRef<SWidget> ULanessaV2Widget::BuildRail()
{
	using namespace V2;
	using namespace LanessaIcons;

	struct FNav { FString Id; FString Label; TArray<FLanessaIconPrim> Icon; };
	TArray<FNav> Items = {
		{TEXT("home"), Label_Home, Home()},
		{TEXT("atrofi"), Label_Atrofi, Atrofi()},
		{TEXT("qulay"), Label_Qulay, Qulay()},
		{TEXT("qidiruv"), Label_Qidiruv, Qidiruv()},
		{TEXT("qirqim"), Label_Qirqim, Qirqim()},
		{TEXT("galereya"), Label_Galereya, Galereya()},
		// no v2 counterpart - added on the user's separate, explicit request to extend the rail
		// (same 3 items/labels/icons as ULanessaMasterMenuWidget's BuildNavRail). No side panel
		// exists for these, so clicking just sets ActiveView (highlights + closes other panels).
		{TEXT("vr"), Label_VRProgulka, VRProgulka()},
		{TEXT("sayr"), Label_Progulka, Progulka()},
		{TEXT("interyer"), Label_Interyer, Interyer()},
		{TEXT("qurilish"), Label_Qurilish, Qurilish()},
	};

	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	for (const FNav& It : Items)
	{
		FString Id = It.Id;
		TAttribute<bool> bActive = TAttribute<bool>::Create([this, Id]() { return ActiveView == Id; });
		TAttribute<FLinearColor> TextColor = TAttribute<FLinearColor>::Create([this, Id]() { return ActiveView == Id ? V2::Paper : V2::TextDim; });
		TAttribute<FLinearColor> StemColor = TAttribute<FLinearColor>::Create([this, Id]() { return ActiveView == Id ? V2::OliveGlow : V2::LineSoft; });
		TAttribute<FLinearColor> IconColor = TAttribute<FLinearColor>::Create([this, Id]() { return ActiveView == Id ? V2::OliveGlow : FLinearColor(V2::Paper.R, V2::Paper.G, V2::Paper.B, 0.8f); });
		// Solid-ish panel fill (not v2's plain transparent/olive-tint-only background) so the label and
		// icon stay legible no matter what's behind the rail in the 3D scene (bright sky, light building
		// facades, etc. - a hairline border alone still let busy scene content wash out the text). Built
		// from the brand's own logo color (Olive/OliveDeep - see Lanessa brandbook: olive/khaki monogram),
		// not the neutral near-black Panel used elsewhere, so the rail itself reads as on-brand. Resting
		// state uses the darker OliveDeep; active brightens to full Olive so the click state stays distinct.
		// Resting state stays the neutral dark Panel fill (legible over any scene content behind the
		// rail, approved as-is); only the clicked/active item switches to the brand logo's own Olive
		// color, so the active state reads as distinct without recoloring every item at rest.
		TAttribute<FLinearColor> BgColor = TAttribute<FLinearColor>::Create([this, Id]() {
			return ActiveView == Id
				? FLinearColor(V2::Olive.R, V2::Olive.G, V2::Olive.B, 0.92f)
				: V2::Panel;
		});

		// Only "home" is always shown; every other item collapses unless the dropdown (toggled by
		// clicking "home") is open. Keeps AutoHeight() slot behavior (Collapsed removes the row
		// entirely, not just hides it) so the rail shrinks back down when closed.
		const bool bIsHomeItem = (Id == TEXT("home"));
		TAttribute<EVisibility> ItemVisibility = TAttribute<EVisibility>::Create([this, bIsHomeItem]() {
			return (bIsHomeItem || bNavExpanded) ? EVisibility::Visible : EVisibility::Collapsed;
		});

		Box->AddSlot().AutoHeight()
		[
			SNew(SBox).WidthOverride(208.f).HeightOverride(54.f).Visibility(ItemVisibility)
			[
				// .navitem { clip-path: polygon(0 0, 100% 0, 100% 100%, 18px 100%, 0 calc(100% - 18px)); }
				SNew(SLanessaCutBorder)
				.FillColor(BgColor)
				.CutSize(18.f)
				.HoverColor(FLinearColor(Olive.R, Olive.G, Olive.B, 0.18f))
				.bAnimateHover(true)
				.OnClicked(FSimpleDelegate::CreateLambda([this, Id, bIsHomeItem]() {
					if (bIsHomeItem)
					{
						if (!bNavExpanded)
						{
							// 1st click (closed): just open the dropdown. Whatever view is currently
							// active (including "sayr"/Progulka) keeps running untouched.
							bNavExpanded = true;
							bHomeActivatedThisSession = false;
							return;
						}
						if (!bHomeActivatedThisSession)
						{
							// 2nd click (open, not yet used this session): actually navigate home, but
							// leave the dropdown open so the rest of the rail stays reachable.
							bHomeActivatedThisSession = true;
							SetActiveView(Id);
							return;
						}
						// 3rd click (open, already navigated home this session): just close the
						// dropdown - no further camera/nav action.
						bNavExpanded = false;
						bHomeActivatedThisSession = false;
						return;
					}
					// Every other item leaves the dropdown open so the rest of the rail stays
					// reachable, EXCEPT "sayr" (Progulka), which fills the screen with the 3D
					// walkthrough and should visually clear the rail like entering any other level.
					if (Id == TEXT("sayr")) { bNavExpanded = false; }
					// Navigating to any real destination invalidates "home already activated this
					// session" - clicking home again afterwards should re-run its nav, not just close.
					bHomeActivatedThisSession = false;
					SetActiveView(Id);
				}))
				.Content()
				[
				SNew(SOverlay)
				+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Fill).Padding(0.f, 8.f, 0.f, 8.f)
				[
					SNew(SBox).WidthOverride(2.f)
					[
						SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(TAttribute<FSlateColor>::Create([StemColor]() { return FSlateColor(StemColor.Get()); })).Padding(0.f)[SNew(SSpacer)]
					]
				]
				+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(48.f, 0.f, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 14.f, 0.f)
					[
						SNew(SLanessaLineIcon).Primitives(It.Icon).IconSize(18.f).StrokeWidth(1.3f).StrokeColor(IconColor)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(FText::FromString(It.Label)).Font(F(12)).ColorAndOpacity(TAttribute<FSlateColor>::Create([TextColor]() { return FSlateColor(TextColor.Get()); }))
					]
				]
				// .care: 5x5 circle, olive-glow, visible only when active
				+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(0.f, 0.f, 18.f, 0.f)
				[
					SNew(SBox).WidthOverride(5.f).HeightOverride(5.f)
					.Visibility(TAttribute<EVisibility>::Create([bActive]() { return bActive.Get() ? EVisibility::Visible : EVisibility::Collapsed; }))
					[
						SNew(SBorder).BorderImage(new FSlateRoundedBoxBrush(OliveGlow, 2.5f, FVector2D(5.f,5.f))).Padding(0.f)[SNew(SSpacer)]
					]
				]
				]
			]
		];
	}
	return Box;
}

// .rail-foot { bottom:44px; left:0; } single navitem, class="navitem" data-view="none" in the real
// markup - it participates in the SAME click handler as the main rail (clicking it just closes
// every side panel, since "none" matches no panel id), which was missing here entirely before.
TSharedRef<SWidget> ULanessaV2Widget::BuildRailFoot()
{
	using namespace V2;
	using namespace LanessaIcons;

	const FString Id = TEXT("none");
	TAttribute<bool> bActive = TAttribute<bool>::Create([this]() { return ActiveView == TEXT("none"); });
	TAttribute<FLinearColor> TextColor = TAttribute<FLinearColor>::Create([this]() { return ActiveView == TEXT("none") ? V2::Paper : V2::TextDim; });
	TAttribute<FLinearColor> StemColor = TAttribute<FLinearColor>::Create([this]() { return ActiveView == TEXT("none") ? V2::OliveGlow : V2::LineSoft; });
	TAttribute<FLinearColor> IconColor = TAttribute<FLinearColor>::Create([this]() { return ActiveView == TEXT("none") ? V2::OliveGlow : FLinearColor(V2::Paper.R, V2::Paper.G, V2::Paper.B, 0.8f); });
	TAttribute<FLinearColor> BgColor = TAttribute<FLinearColor>::Create([this]() {
		return ActiveView == TEXT("none") ? FLinearColor(V2::Olive.R, V2::Olive.G, V2::Olive.B, 0.10f) : V2::Transparent;
	});

	return SNew(SBox).WidthOverride(208.f).HeightOverride(54.f)
	[
		SNew(SLanessaCutBorder)
		.FillColor(BgColor)
		.CutSize(18.f)
		.HoverColor(FLinearColor(Olive.R, Olive.G, Olive.B, 0.10f))
		.bAnimateHover(true)
		.OnClicked(FSimpleDelegate::CreateLambda([this]() { SetActiveView(TEXT("none")); }))
		.Content()
		[
		SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Fill).Padding(0.f, 8.f, 0.f, 8.f)
		[
			SNew(SBox).WidthOverride(2.f)
			[
				SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(TAttribute<FSlateColor>::Create([StemColor]() { return FSlateColor(StemColor.Get()); })).Padding(0.f)[SNew(SSpacer)]
			]
		]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(48.f, 0.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 14.f, 0.f)
			[
				SNew(SLanessaLineIcon).Primitives(LanessaIcons::Settings()).IconSize(18.f).StrokeWidth(1.3f).StrokeColor(IconColor)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("SOZLAMALAR"))).Font(F(12)).ColorAndOpacity(TAttribute<FSlateColor>::Create([TextColor]() { return FSlateColor(TextColor.Get()); }))
			]
		]
		]
	];
}

// .side-panel { backdrop-filter: blur(16px); border-right: 1px solid var(--line-soft); } - real blur via SBackgroundBlur;
// visibility driven by ActiveView so nav clicks genuinely open/close the matching panel.
TSharedRef<SWidget> ULanessaV2Widget::BuildSidePanel(const FString& ViewId, TSharedRef<SWidget> Content)
{
	using namespace V2;
	return SNew(SBox)
		.Visibility(TAttribute<EVisibility>::Create([this, ViewId]() { return ActiveView == ViewId ? EVisibility::Visible : EVisibility::Collapsed; }))
		[
			SNew(SBackgroundBlur).BlurStrength(8.f).bApplyAlphaToBlur(true)
			[
				SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Panel).Padding(FMargin(26.f,26.f,26.f,22.f))
				[
					Content
				]
			]
		];
}

// .side-panel (open) width:300 .side-inner padding:26,26,22
TSharedRef<SWidget> ULanessaV2Widget::BuildSidePanelQidiruvContent()
{
	using namespace V2;

	// .range-track: draggable two-handle range slider (kvadratura/narx) - real mouse-drag via SLanessaDragTrack,
	// handle positions read live from RangeHandles through GetRangeHandle/SetRangeHandlePct.
	const float RangeTrackWidth = 248.f; // .side-panel width:300 - .side-inner padding 26+26
	// Value label is live: reads real Surface/Price min-max off SearchUnits (pushed in via SetSearchUnits)
	// through the same ComputeFieldRange used for actual filtering, so what's displayed always matches
	// what "QO'LLASH" will apply. Falls back to the v2 mockup's static 52-118/420-980 until data arrives.
	auto MakeField = [this, RangeTrackWidth](const FString& FieldId, const FString& Label, float DefaultAPct, float DefaultBPct)
	{
		const float DefaultA = DefaultAPct / 100.f;
		const float DefaultB = DefaultBPct / 100.f;
		TAttribute<FVector2D> Handle = TAttribute<FVector2D>::Create([this, FieldId, DefaultA, DefaultB]() { return GetRangeHandle(FieldId, DefaultA, DefaultB); });
		TAttribute<FText> ValueText = TAttribute<FText>::Create([this, FieldId, DefaultA, DefaultB]()
		{
			int32 Lo, Hi;
			ComputeFieldRange(FieldId, Lo, Hi);
			const FVector2D H = GetRangeHandle(FieldId, DefaultA, DefaultB);
			const int32 ValLo = Lo + FMath::RoundToInt(H.X * (Hi - Lo));
			const int32 ValHi = Lo + FMath::RoundToInt(H.Y * (Hi - Lo));
			return FText::FromString(FieldId == TEXT("narx")
				? FString::Printf(TEXT("%d – %d mln"), ValLo, ValHi)
				: FString::Printf(TEXT("%d – %d m²"), ValLo, ValHi));
		});
		const float W = RangeTrackWidth;

		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)[SNew(STextBlock).Text(FText::FromString(Label)).Font(F(9)).ColorAndOpacity(FSlateColor(OliveGlow))]
				+ SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(ValueText).Font(F(11)).ColorAndOpacity(FSlateColor(Paper))]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4.f, 18.f, 4.f, 0.f)
			[
				SNew(SBox).WidthOverride(W).HeightOverride(14.f)
				[
					SNew(SLanessaDragTrack)
					.OnPctChanged(SLanessaDragTrack::FOnPctChanged::CreateLambda([this, FieldId](float Pct) { SetRangeHandlePct(FieldId, Pct); }))
					[
						SNew(SOverlay)
						+ SOverlay::Slot().VAlign(VAlign_Center)[SNew(SBox).HeightOverride(2.f)[Fill(Line)]]
						+ SOverlay::Slot().VAlign(VAlign_Center).HAlign(HAlign_Left)
						.Padding(TAttribute<FMargin>::Create([Handle, W]() { return FMargin(Handle.Get().X * W, 0.f, 0.f, 0.f); }))
						[
							SNew(SBox).WidthOverride(TAttribute<FOptionalSize>::Create([Handle, W]() { return FOptionalSize((Handle.Get().Y - Handle.Get().X) * W); })).HeightOverride(2.f)[Fill(Olive)]
						]
						+ SOverlay::Slot().VAlign(VAlign_Center).HAlign(HAlign_Left)
						.Padding(TAttribute<FMargin>::Create([Handle, W]() { return FMargin(Handle.Get().X * W - 5.f, 0.f, 0.f, 0.f); }))
						[
							Diamond(10.f, OliveGlow)
						]
						+ SOverlay::Slot().VAlign(VAlign_Center).HAlign(HAlign_Left)
						.Padding(TAttribute<FMargin>::Create([Handle, W]() { return FMargin(Handle.Get().Y * W - 5.f, 0.f, 0.f, 0.f); }))
						[
							Diamond(10.f, OliveGlow)
						]
					]
				]
			];
	};

	// .chip { clip-path: polygon(...) } .chip:hover{border-color:olive-glow} .chip.on{background:olive} -
	// v2's JS: `c.addEventListener('click', ()=> c.classList.toggle('on'))` - chips had NO click handling
	// at all before (bOn was a hardcoded bool baked into the array, never changed). Now real, toggled
	// through the same ToggleState map as the category-list checkmarks, keyed "qidiruv:<group>:<label>".
	auto MakeChip = [this](const FString& Group, const FString& Text, bool bDefaultOn)
	{
		FString Key = FString::Printf(TEXT("qidiruv:%s:%s"), *Group, *Text);
		TAttribute<FLinearColor> BgColor = TAttribute<FLinearColor>::Create([this, Key, bDefaultOn]() { return IsToggled(Key, bDefaultOn) ? V2::Olive : V2::Transparent; });
		TAttribute<FLinearColor> TxtColor = TAttribute<FLinearColor>::Create([this, Key, bDefaultOn]() { return IsToggled(Key, bDefaultOn) ? V2::IconInk : V2::TextDim; });
		return SNew(SLanessaCutBorder).FillColor(BgColor).CutSize(8.f)
			.HoverColor(FLinearColor(OliveGlow.R, OliveGlow.G, OliveGlow.B, 0.18f)).bAnimateHover(true)
			.OnClicked(FSimpleDelegate::CreateLambda([this, Key]() { ToggleChip(Key); }))
			.Content()
			[
				SNew(SBox).Padding(FMargin(14.f, 8.f))
				[
					SNew(STextBlock).Text(FText::FromString(Text)).Font(F(11)).ColorAndOpacity(TAttribute<FSlateColor>::Create([TxtColor]() { return FSlateColor(TxtColor.Get()); }))
				]
			];
	};

	// .field { margin-bottom:22px } - one labeled chip-row group (Yotoqxona/Sanuzel/Holati), all three
	// present in v2's real markup; only "Yotoqxona" had been ported before, Sanuzel and Holati were
	// missing from the C++ port entirely.
	// .chip-row { display:flex; flex-wrap:wrap; gap:8px; } - SWrapBox mirrors the CSS wrap behaviour
	// (needed for "Holati", whose labels are long enough to overflow a single fixed-width row).
	auto MakeChipGroup = [this, MakeChip](const FString& Group, const FString& Label, const TArray<TPair<FString,bool>>& Items)
	{
		TSharedRef<SWrapBox> Row = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8.f, 8.f));
		for (const auto& It : Items)
		{
			Row->AddSlot()[MakeChip(Group, It.Key, It.Value)];
		}
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f,0.f,0.f,10.f)[SNew(STextBlock).Text(FText::FromString(Label)).Font(F(9)).ColorAndOpacity(FSlateColor(OliveGlow))]
			+ SVerticalBox::Slot().AutoHeight()[Row];
	};

	// "N TA NATIJA" is the dataset size (SearchUnits.Num(), pushed in via SetSearchUnits - 0 until
	// BP_Explorer_PC calls it), "Topildi: N" is the LIVE filtered count - both re-evaluate every paint
	// so dragging a slider or toggling a chip updates the numbers immediately, before QO'LLASH is clicked.
	TAttribute<FText> FiltrlarText = TAttribute<FText>::Create([this]()
	{
		return FText::FromString(FString::Printf(TEXT("FILTRLAR · %d TA NATIJA"), SearchUnits.Num()));
	});
	TAttribute<FText> TopildiText = TAttribute<FText>::Create([this]()
	{
		return FText::FromString(FString::Printf(TEXT("Topildi: %d"), ComputeMatchingPoiIds().Num()));
	});

	TSharedRef<SVerticalBox> Root = SNew(SVerticalBox);
	Root->AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(TEXT("Qidiruv"))).Font(D(18)).ColorAndOpacity(FSlateColor(Paper))];
	Root->AddSlot().AutoHeight().Padding(0.f,4.f,0.f,22.f)[SNew(STextBlock).Text(FiltrlarText).Font(F(10)).ColorAndOpacity(FSlateColor(TextDim))];
	// 0-100 default (full range, nothing excluded) - matches the real Surface/Budget sliders' own
	// default position (see lanessa-real-product-behavior memory: real Filter.Surface/Budget start
	// from GetSliderValue_Current at PreConstruct, i.e. the slider's own default handle position,
	// which passes every unit through until the user actively drags it).
	Root->AddSlot().AutoHeight().Padding(0.f,0.f,0.f,22.f)[MakeField(TEXT("kvadratura"), TEXT("KVADRATURA"), 0.f, 100.f)];
	Root->AddSlot().AutoHeight().Padding(0.f,0.f,0.f,22.f)[MakeField(TEXT("narx"), TEXT("NARX"), 0.f, 100.f)];

	Root->AddSlot().AutoHeight().Padding(0.f,0.f,0.f,22.f)
		[MakeChipGroup(TEXT("yotoqxona"), TEXT("YOTOQXONA"), LanessaSearchChips::Bedrooms)];
	Root->AddSlot().AutoHeight().Padding(0.f,0.f,0.f,22.f)
		[MakeChipGroup(TEXT("sanuzel"), TEXT("SANUZEL"), LanessaSearchChips::Bathrooms)];
	Root->AddSlot().AutoHeight().Padding(0.f,0.f,0.f,22.f)
		[MakeChipGroup(TEXT("holati"), TEXT("HOLATI"), LanessaSearchChips::Availability)];

	Root->AddSlot().AutoHeight().Padding(0.f,18.f,0.f,0.f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[SNew(STextBlock).Text(TopildiText).Font(F(11)).ColorAndOpacity(FSlateColor(TextDim))]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			// .side-footer button { clip-path: polygon(0 0, 100% 0, 100% 100%, 10px 100%, 0 calc(100% - 10px)); }
			// OnClicked broadcasts OnSearchApplied with the currently-matching PoiIds - BP_Explorer_PC
			// binds this to actually Show_POI/Hide_POI the level's actors (see the delegate's own comment).
			SNew(SLanessaCutBorder).FillColor(Olive).CutSize(10.f)
			.OnClicked(FSimpleDelegate::CreateLambda([this]() { ApplySearch(); }))
			.Content()
			[
				SNew(SBox).Padding(FMargin(18.f,10.f))
				[SNew(STextBlock).Text(FText::FromString(TEXT("QO'LLASH"))).Font(F(10)).ColorAndOpacity(FSlateColor(IconInk))]
			]
		]
	];

	return SNew(SBox).WidthOverride(300.f)[Root];
}

// One category row (header + its nested, initially-empty POI list). Shared by SetPanelCategories
// (fresh build) - factored out so the row-building logic exists in exactly one place.
TSharedRef<SWidget> ULanessaV2Widget::BuildCategoryRow(const FString& PanelTag, const FString& Title, const FLanessaV2CategoryDef& Cat)
{
	using namespace V2;
	FString Key = Title + TEXT(":") + Cat.Label;
	bool bDefault = Cat.bDefaultOn;
	FString CategoryTag = Cat.Tag;
	TAttribute<FSlateColor> TxtColor = TAttribute<FSlateColor>::Create([this, Key, bDefault]() { return FSlateColor(IsToggled(Key, bDefault) ? V2::Paper : V2::TextDim); });
	TAttribute<FLinearColor> DotColor = TAttribute<FLinearColor>::Create([this, Key, bDefault]() { return IsToggled(Key, bDefault) ? V2::OliveGlow : V2::Transparent; });

	TSharedRef<SVerticalBox> Row = SNew(SVerticalBox);
	Row->AddSlot().AutoHeight()
	[
		SNew(SLanessaCutBorder).CutSize(0.f).FillColor(Transparent).HoverColor(FLinearColor(1,1,1,0.05f)).bAnimateHover(true)
		.OnClicked(FSimpleDelegate::CreateLambda([this, Key, PanelTag, CategoryTag, bDefault]()
		{
			ToggleChip(Key);
			OnCategoryToggled.Broadcast(PanelTag, CategoryTag, IsToggled(Key, bDefault));
		}))
		.Content()
		[
			SNew(SBox).Padding(FMargin(4.f,13.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)[SNew(STextBlock).Text(FText::FromString(Cat.Label)).Font(F(12)).ColorAndOpacity(TxtColor)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(6.f).HeightOverride(6.f)
					.RenderTransform(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(45.f))))
					.RenderTransformPivot(FVector2D(0.5f,0.5f))
					[
						SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(TAttribute<FSlateColor>::Create([DotColor]() { return FSlateColor(DotColor.Get()); })).Padding(0.f)[SNew(SSpacer)]
					]
				]
			]
		]
	];

	// Nested, dynamically-populated list of real POI entries under this category - starts empty and
	// stays empty until BP_Explorer_PC calls SetCategoryPois(PanelTag, Cat.Tag, ...) with actors
	// actually tagged with both PanelTag and Cat.Tag. Mirrors the real BP_EntryList_Widget ->
	// BP_Entry_Widget row-per-POI pattern exactly (see lanessa-real-product-behavior memory): no
	// tagged POI = no row shown, not a placeholder/dummy entry.
	TSharedRef<SVerticalBox> PoiList = SNew(SVerticalBox);
	FString MapKey = PanelTag + TEXT(":") + CategoryTag;
	CategoryPoiListBoxes.Add(MapKey, PoiList);
	Row->AddSlot().AutoHeight().Padding(14.f, 0.f, 0.f, 4.f)[PoiList];
	return Row;
}

// Category rows are NOT baked in here - the panel starts with an empty category list, and
// SetPanelCategories() (called from BP_Explorer_PC, typically right after CreateWidget) fills it in.
// This makes the category set itself data-driven: renaming/adding/removing a category is a Blueprint-
// side change, no C++ recompile needed.
TSharedRef<SWidget> ULanessaV2Widget::BuildSidePanelCatListContent(const FString& Title, const FString& Sub, const FString& PanelTag)
{
	using namespace V2;
	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	PanelCategoryListBoxes.Add(PanelTag, TPair<TSharedPtr<SVerticalBox>, FString>(List, Title));
	TSharedRef<SVerticalBox> Root = SNew(SVerticalBox);
	Root->AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Title)).Font(D(18)).ColorAndOpacity(FSlateColor(Paper))];
	Root->AddSlot().AutoHeight().Padding(0.f,4.f,0.f,22.f)[SNew(STextBlock).Text(FText::FromString(Sub)).Font(F(10)).ColorAndOpacity(FSlateColor(TextDim))];

	// The category list scrolls once it outgrows the screen. Without this the panel simply kept
	// growing downwards and everything past the bottom edge was unreachable - reported with many
	// POIs tagged into Atrofi/Qulayliklar, where the lower categories could not be opened at all.
	//
	// MaxDesiredHeight rather than a fixed HeightOverride: a short list must still hug its content,
	// so the panel does not render as a tall half-empty box on levels with only a few categories.
	// The cap is computed from the live viewport instead of being hardcoded because this panel is
	// anchored to the top of the screen, so how much room it has depends entirely on the window.
	TAttribute<FOptionalSize> MaxListHeight = TAttribute<FOptionalSize>::Create([this]()
	{
		// Slate units, not pixels - divide out DPI scale or the cap is wrong on scaled displays.
		const float Scale = UWidgetLayoutLibrary::GetViewportScale(this);
		const FVector2D ViewportPx = UWidgetLayoutLibrary::GetViewportSize(this);
		const float ViewportSlate = (Scale > KINDA_SMALL_NUMBER) ? (float)ViewportPx.Y / Scale : (float)ViewportPx.Y;

		// What sits above/below the list and must be left alone: the panel's own top offset in
		// BuildMain (176), the side-panel border padding (26 top + 22 bottom), the title+sub block
		// (~52 incl. its 22 bottom padding), and a little breathing room off the screen edge (40).
		const float Reserved = 176.f + 26.f + 22.f + 52.f + 40.f;
		return FOptionalSize(FMath::Max(160.f, ViewportSlate - Reserved));
	});

	Root->AddSlot().AutoHeight()
	[
		SNew(SBox).MaxDesiredHeight(MaxListHeight)
		[
			SNew(SScrollBox)
			.ScrollBarAlwaysVisible(false)   // no permanent gutter stealing width from a short list
			+ SScrollBox::Slot()[List]
		]
	];
	return SNew(SBox).WidthOverride(300.f)[Root];
}

void ULanessaV2Widget::SetPanelCategories(const FString& PanelTag, const TArray<FString>& CategoryTags, const TArray<FString>& CategoryLabels, const TArray<bool>& CategoryDefaultsOn)
{
	TPair<TSharedPtr<SVerticalBox>, FString>* Found = PanelCategoryListBoxes.Find(PanelTag);
	if (!Found || !Found->Key.IsValid()) { return; }
	TSharedPtr<SVerticalBox> Box = Found->Key;
	const FString& Title = Found->Value;

	// Drop this panel's previously-registered POI list boxes - they belonged to category rows that are
	// about to be destroyed; stale entries here would otherwise leak and could be mistaken for live ones.
	for (auto It = CategoryPoiListBoxes.CreateIterator(); It; ++It)
	{
		if (It.Key().StartsWith(PanelTag + TEXT(":"))) { It.RemoveCurrent(); }
	}

	Box->ClearChildren();
	const int32 Num = FMath::Min(CategoryTags.Num(), CategoryLabels.Num());
	for (int32 i = 0; i < Num; ++i)
	{
		FLanessaV2CategoryDef Cat;
		Cat.Tag = CategoryTags[i];
		Cat.Label = CategoryLabels[i];
		Cat.bDefaultOn = CategoryDefaultsOn.IsValidIndex(i) ? CategoryDefaultsOn[i] : true;
		Box->AddSlot().AutoHeight()[BuildCategoryRow(PanelTag, Title, Cat)];
	}
}

void ULanessaV2Widget::SetCategoryPois(const FString& PanelTag, const FString& CategoryTag, const TArray<FString>& PoiIds, const TArray<FString>& PoiLabels)
{
	using namespace V2;
	FString MapKey = PanelTag + TEXT(":") + CategoryTag;
	TSharedPtr<SVerticalBox>* Found = CategoryPoiListBoxes.Find(MapKey);
	if (!Found || !Found->IsValid()) { return; }
	TSharedPtr<SVerticalBox> Box = *Found;
	Box->ClearChildren();
	const int32 Num = FMath::Min(PoiIds.Num(), PoiLabels.Num());
	for (int32 i = 0; i < Num; ++i)
	{
		FString PoiId = PoiIds[i];
		Box->AddSlot().AutoHeight().Padding(0.f,5.f,0.f,5.f)
		[
			SNew(SLanessaCutBorder).CutSize(0.f).FillColor(Transparent).HoverColor(FLinearColor(1,1,1,0.06f)).bAnimateHover(true)
			.OnClicked(FSimpleDelegate::CreateLambda([this, PoiId]() { OnPoiEntryClicked.Broadcast(PoiId); }))
			.Content()
			[
				SNew(SBox).Padding(FMargin(6.f,4.f))
				[
					SNew(STextBlock).Text(FText::FromString(PoiLabels[i])).Font(F(11)).ColorAndOpacity(FSlateColor(TextDim))
				]
			]
		];
	}
}

// .chip-card { top:36 right:48 width:268 padding:18,20,20 }
TSharedRef<SWidget> ULanessaV2Widget::BuildChipCard()
{
	using namespace V2;
	using namespace LanessaIcons;

	// .weather-row button { cursor:pointer } .weather-row button.on { background:olive } - these were
	// completely static before (no OnClicked at all); now wired to SelectedWeather like every other
	// clickable element in this file. "qor" (snow) has no v2 counterpart - added on the user's
	// separate, explicit request to extend the row with a winter option.
	TSharedRef<SHorizontalBox> WeatherRow = SNew(SHorizontalBox);
	struct FW { FString Id; TArray<FLanessaIconPrim> Icon; };
	TArray<FW> Ws = {
		{TEXT("quyosh"), WeatherSun()},
		{TEXT("bulut"), WeatherCloud()},
		{TEXT("aralash"), WeatherPartly()},
		{TEXT("yomgir"), WeatherRain()},
		{TEXT("qor"), WeatherSnow()},
	};
	for (int32 i = 0; i < Ws.Num(); i++)
	{
		FString WId = Ws[i].Id;
		TAttribute<FLinearColor> BgColor = TAttribute<FLinearColor>::Create([this, WId]() { return SelectedWeather == WId ? V2::Olive : V2::Transparent; });
		TAttribute<FLinearColor> IconColor = TAttribute<FLinearColor>::Create([this, WId]() { return SelectedWeather == WId ? V2::IconInk : V2::TextDim; });
		WeatherRow->AddSlot().FillWidth(1.f).Padding(i==0?0.f:6.f, 0.f, 0.f, 0.f)
		[
			SNew(SBox).HeightOverride(38.f)
			[
				SNew(SLanessaCutBorder).CutSize(0.f).FillColor(BgColor)
				.HoverColor(FLinearColor(OliveGlow.R, OliveGlow.G, OliveGlow.B, 0.20f)).bAnimateHover(true)
				.OnClicked(FSimpleDelegate::CreateLambda([this, WId]() { SetWeather(WId); }))
				.Content()
				[
					SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
					[
						SNew(SLanessaLineIcon).Primitives(Ws[i].Icon).IconSize(16.f).StrokeWidth(1.3f).StrokeColor(IconColor)
					]
				]
			]
		];
	}

	// .season-row - built exactly like the weather row above, sitting directly under it. Four chips,
	// and unlike the weather row these change only the season: the two rows are independent axes, so
	// picking "yomgir" must not drag the scene into autumn behind the user's back.
	TSharedRef<SHorizontalBox> SeasonRow = SNew(SHorizontalBox);
	struct FS { FString Id; TArray<FLanessaIconPrim> Icon; };
	TArray<FS> Ss = {
		{TEXT("bahor"), SeasonSpring()},
		{TEXT("yoz"), SeasonSummer()},
		{TEXT("kuz"), SeasonAutumn()},
		{TEXT("qish"), SeasonWinter()},
	};
	for (int32 i = 0; i < Ss.Num(); i++)
	{
		FString SId = Ss[i].Id;
		TAttribute<FLinearColor> SBg = TAttribute<FLinearColor>::Create([this, SId]() { return SelectedSeason == SId ? V2::Olive : V2::Transparent; });
		TAttribute<FLinearColor> SInk = TAttribute<FLinearColor>::Create([this, SId]() { return SelectedSeason == SId ? V2::IconInk : V2::TextDim; });
		SeasonRow->AddSlot().FillWidth(1.f).Padding(i==0?0.f:6.f, 0.f, 0.f, 0.f)
		[
			SNew(SBox).HeightOverride(38.f)
			[
				SNew(SLanessaCutBorder).CutSize(0.f).FillColor(SBg)
				.HoverColor(FLinearColor(OliveGlow.R, OliveGlow.G, OliveGlow.B, 0.20f)).bAnimateHover(true)
				.OnClicked(FSimpleDelegate::CreateLambda([this, SId]() { SetSeason(SId); }))
				.Content()
				[
					SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
					[
						SNew(SLanessaLineIcon).Primitives(Ss[i].Icon).IconSize(16.f).StrokeWidth(1.3f).StrokeColor(SInk)
					]
				]
			]
		];
	}

	// .chip-card { backdrop-filter: blur(14px); }
	return SNew(SBox).WidthOverride(268.f)
	[
		SNew(SBackgroundBlur).BlurStrength(7.f).bApplyAlphaToBlur(true)
		[
		SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Panel).Padding(FMargin(20.f,18.f,20.f,20.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					// TimePct 0-1 maps across the track's 06:00-22:00 display range (see the "06:00"/
					// "14:00"/"22:00" labels below) - was a hardcoded "16:24" string, now driven by the
					// same TimePct the drag track and Ultra Dynamic Sky sync (OnTimePctChanged) use.
					SNew(STextBlock)
					.Text(TAttribute<FText>::Create([this]() {
						const int32 TotalMinutes = FMath::RoundToInt((6.f + TimePct * 16.f) * 60.f);
						const int32 H = TotalMinutes / 60;
						const int32 M = TotalMinutes % 60;
						return FText::FromString(FString::Printf(TEXT("%02d:%02d"), H, M));
					}))
					.Font(D(28)).ColorAndOpacity(FSlateColor(Paper))
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0.f,0.f,0.f,4.f)
				// Ishonchli harorat readbacki ulanmaguncha uydirma son ko'rsatilmaydi.
				[SNew(STextBlock).Text(FText::FromString(TEXT("—°C"))).Font(F(12)).ColorAndOpacity(FSlateColor(OliveGlow))]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f,3.f,0.f,0.f)
			[SNew(STextBlock).Text(TAttribute<FText>::Create([this]() { return EnvironmentCaption; })).Font(F(9)).ColorAndOpacity(FSlateColor(TextDim))]

			+ SVerticalBox::Slot().AutoHeight().Padding(3.f,20.f,3.f,10.f)
			[
				// .time-track: draggable position slider, default 63% - real mouse-drag via SLanessaDragTrack.
				SNew(SBox).HeightOverride(14.f)
				[
					SNew(SLanessaDragTrack)
					.OnPctChanged(SLanessaDragTrack::FOnPctChanged::CreateLambda([this](float Pct) { SetTimePct(Pct); }))
					[
						SNew(SOverlay)
						+ SOverlay::Slot().VAlign(VAlign_Center)[SNew(SBox).HeightOverride(2.f)[Fill(Line)]]
						+ SOverlay::Slot().VAlign(VAlign_Center).HAlign(HAlign_Left)
						[
							SNew(SBox).WidthOverride(TAttribute<FOptionalSize>::Create([this]() { return FOptionalSize(TimePct * 228.f); })).HeightOverride(2.f)[Fill(Olive)]
						]
						+ SOverlay::Slot().VAlign(VAlign_Center).HAlign(HAlign_Left)
						.Padding(TAttribute<FMargin>::Create([this]() { return FMargin(TimePct * 228.f - 5.5f, 0.f, 0.f, 0.f); }))
						[
							Diamond(11.f, OliveGlow)
						]
					]
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0.f,8.f,0.f,0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)[SNew(STextBlock).Text(FText::FromString(TEXT("06:00"))).Font(F(8)).ColorAndOpacity(FSlateColor(FLinearColor(1,1,1,0.3f)))]
				+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Center)[SNew(STextBlock).Text(FText::FromString(TEXT("14:00"))).Font(F(8)).ColorAndOpacity(FSlateColor(FLinearColor(1,1,1,0.3f)))]
				+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Right)[SNew(STextBlock).Text(FText::FromString(TEXT("22:00"))).Font(F(8)).ColorAndOpacity(FSlateColor(FLinearColor(1,1,1,0.3f)))]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0.f,18.f,0.f,0.f)[WeatherRow]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f,6.f,0.f,0.f)[SeasonRow]
		]
		]
	];
}

// .tour-toast { bottom:112px center } - "3D tur <b>{label}</b> boshlanmoqda..." shown for 1.8s after
// tour-start / plan-room clicks, ported from the mockup's launchTourToast(); this element (and the
// interactivity that drives it) did not exist in the C++ port at all before.
TSharedRef<SWidget> ULanessaV2Widget::BuildTourToast()
{
	using namespace V2;
	return SNew(SBox)
		.Visibility(TAttribute<EVisibility>::Create([this]() { return ToastTimeLeft > 0.f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; }))
		[
			SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(PanelSolid).Padding(FMargin(18.f,12.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(FText::FromString(TEXT("3D tur "))).Font(F(12)).ColorAndOpacity(FSlateColor(Paper))]
				+ SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(TAttribute<FText>::Create([this]() { return FText::FromString(ToastLabel); })).Font(F(12,true)).ColorAndOpacity(FSlateColor(OliveGlow))]
				+ SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(FText::FromString(TEXT(" boshlanmoqda…"))).Font(F(12)).ColorAndOpacity(FSlateColor(Paper))]
			]
		];
}

// Foydalanuvchi qirqayotgan binoning qavat ikonkalarini qaytadan ko'rsatadi -
// nega qirqimni bekor qilishning o'zi yetarli emasligi ta'rifi yonida yozilgan.
static void LanessaRestoreQirqimBuilding();

// .floor-rail { bottom:40 center; padding:10,12 } - 12 numbered buttons + cut-label
TSharedRef<SWidget> ULanessaV2Widget::BuildFloorRail()
{
	using namespace V2;
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
	Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.f,0.f,12.f,0.f)
		[SNew(STextBlock).Text(FText::FromString(TEXT("QIRQIM · QAVAT"))).Font(F(9)).ColorAndOpacity(FSlateColor(OliveGlow))];
	// Numbers live in their OWN nested row, separate from the "QIRQIM · QAVAT" label above - so
	// SetAvailableFloors()'s ClearChildren()+repopulate only ever touches the numbers, never wipes the
	// label out too (which is what happened when FloorRailRow pointed at the label's own parent row).
	TSharedRef<SHorizontalBox> NumbersRow = SNew(SHorizontalBox);
	Row->AddSlot().AutoWidth()[NumbersRow];
	FloorRailRow = NumbersRow;
	// Numbers themselves are populated by SetAvailableFloors() (called from BP_Explorer_PC once the
	// level's real placed markers are known) rather than a hardcoded range here - the row starts empty.
	if (AvailableFloors.Num() > 0)
	{
		// Re-entrant call (e.g. RebuildWidget() firing again) with floors already pushed in - repopulate
		// immediately instead of waiting for another SetAvailableFloors() call that may never come.
		TArray<int32> Floors = AvailableFloors;
		SetAvailableFloors(Floors);
	}
	// "CHIQISH" - resets the section-view cutaway back to fully intact (broadcasts the Reset_SectionView
	// delegate; BP_Explorer_PC binds it to the same Reset_SectionView() it already runs for home/atrofi/
	// qulay/qidiruv). Separate from picking a nav item - this only undoes the cut, it doesn't navigate
	// anywhere or touch which side panel is open.
	Row->AddSlot().AutoWidth().Padding(12.f,0.f,0.f,0.f)
	[
		SNew(SBox).HeightOverride(30.f)
		[
			SNew(SLanessaCutBorder).CutSize(6.f).FillColor(V2::Transparent).HoverColor(V2::OliveGlow).bAnimateHover(true)
			.OnClicked(FSimpleDelegate::CreateLambda([this]() {
				Reset_SectionView.Broadcast();
				// Qirqimni bekor qilish qavat ikonkalarini yashirin qoldiradi - ularni ham tiklaymiz.
				LanessaRestoreQirqimBuilding();
			}))
			.Content()
			[
				SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(10.f,0.f)
				[SNew(STextBlock).Text(FText::FromString(TEXT("CHIQISH"))).Font(F(9)).ColorAndOpacity(FSlateColor(V2::TextDim))]
			]
		]
	];
	return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Panel).Padding(FMargin(12.f,10.f))[Row];
}

// QURILISH -> Yer osti qavati. Qirqimning qavat paneli bilan bir xil ko'rinish va bir xil
// yo'l: raqam bosilsa SelectFloor -> OnFloorSelected, BP_Explorer_PC o'sha qavatning
// BP_FloorSectionMarker ini topib qirqadi - qirqimdagi kabi.
TSharedRef<SWidget> ULanessaV2Widget::BuildUndergroundRail()
{
	using namespace V2;
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
	Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.f,0.f,12.f,0.f)
		[SNew(STextBlock).Text(FText::FromString(TEXT("YER OSTI · QAVAT"))).Font(F(9)).ColorAndOpacity(FSlateColor(OliveGlow))];
	TSharedRef<SHorizontalBox> NumbersRow = SNew(SHorizontalBox);
	Row->AddSlot().AutoWidth()[NumbersRow];
	UndergroundRailRow = NumbersRow;
	PopulateFloorRow(UndergroundRailRow, true);
	Row->AddSlot().AutoWidth().Padding(12.f,0.f,0.f,0.f)
	[
		SNew(SBox).HeightOverride(30.f)
		[
			SNew(SLanessaCutBorder).CutSize(6.f).FillColor(V2::Transparent).HoverColor(V2::OliveGlow).bAnimateHover(true)
			.OnClicked(FSimpleDelegate::CreateLambda([this]() {
				Reset_SectionView.Broadcast();
				LanessaRestoreQirqimBuilding();
			}))
			.Content()
			[
				SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(10.f,0.f)
				[SNew(STextBlock).Text(FText::FromString(TEXT("CHIQISH"))).Font(F(9)).ColorAndOpacity(FSlateColor(V2::TextDim))]
			]
		]
	];
	return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Panel).Padding(FMargin(12.f,10.f))[Row];
}

// QURILISH yon paneli - Atrofi/Qulayliklar bilan bir xil ramka (BuildSidePanel), ichida
// bosqich tugmalari. Tanlangan bosqich olive rangda yonadi.
TSharedRef<SWidget> ULanessaV2Widget::BuildSidePanelQurilishContent()
{
	using namespace V2;
	using namespace LanessaIcons;

	struct FStage { FString Id; FString Label; };
	const TArray<FStage> Stages = {
		{TEXT("kotlovan"),      TEXT("Kotlovan")},
		{TEXT("yerosti"),       TEXT("Yer osti qavati")},
		{TEXT("karkas"),        TEXT("Karkas")},
		{TEXT("devor"),         TEXT("Devor")},
		{TEXT("fasad"),         TEXT("Fasad")},
		{TEXT("qurilishetapi"), TEXT("Qurilish etapi")},
	};

	TSharedRef<SVerticalBox> Root = SNew(SVerticalBox);
	Root->AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(TEXT("Qurilish"))).Font(D(18)).ColorAndOpacity(FSlateColor(Paper))];
	Root->AddSlot().AutoHeight().Padding(0.f,4.f,0.f,22.f)[SNew(STextBlock).Text(FText::FromString(TEXT("BOSQICHLAR"))).Font(F(10)).ColorAndOpacity(FSlateColor(TextDim))];

	for (const FStage& Stage : Stages)
	{
		const FString Id = Stage.Id;
		const bool bAnimation = (Id == TEXT("qurilishetapi"));
		TAttribute<FSlateColor> TxtColor = TAttribute<FSlateColor>::Create([this, Id]() { return FSlateColor(ActiveBuildStage == Id ? V2::Paper : V2::TextDim); });
		TAttribute<FLinearColor> BgColor = TAttribute<FLinearColor>::Create([this, Id]() { return ActiveBuildStage == Id ? FLinearColor(V2::Olive.R, V2::Olive.G, V2::Olive.B, 0.18f) : V2::Transparent; });
		TAttribute<FLinearColor> MarkColor = TAttribute<FLinearColor>::Create([this, Id]() { return ActiveBuildStage == Id ? V2::OliveGlow : FLinearColor(V2::Paper.R, V2::Paper.G, V2::Paper.B, 0.35f); });

		// Oddiy bosqichlarda o'ng tomonda romb, animatsiyada "play" uchburchagi - bu tugma
		// boshqalardan farqli ravishda jarayonni o'ynatishini ko'rsatadi.
		TSharedRef<SWidget> Mark = bAnimation
			? StaticCastSharedRef<SWidget>(SNew(SLanessaLineIcon).Primitives(PlayTriangle()).IconSize(12.f).StrokeWidth(1.3f).StrokeColor(MarkColor))
			: StaticCastSharedRef<SWidget>(SNew(SBox).WidthOverride(6.f).HeightOverride(6.f)
				.RenderTransform(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(45.f))))
				.RenderTransformPivot(FVector2D(0.5f,0.5f))
				[
					SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(TAttribute<FSlateColor>::Create([MarkColor]() { return FSlateColor(MarkColor.Get()); })).Padding(0.f)[SNew(SSpacer)]
				]);

		Root->AddSlot().AutoHeight().Padding(0.f, bAnimation ? 10.f : 0.f, 0.f, 0.f)
		[
			SNew(SLanessaCutBorder).CutSize(bAnimation ? 10.f : 0.f).FillColor(BgColor).HoverColor(FLinearColor(1,1,1,0.05f)).bAnimateHover(true)
			.OnClicked(FSimpleDelegate::CreateLambda([this, Id]() { SelectBuildStage(Id); }))
			.Content()
			[
				SNew(SBox).Padding(FMargin(8.f,13.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f)[SNew(STextBlock).Text(FText::FromString(Stage.Label)).Font(F(12)).ColorAndOpacity(TxtColor)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Mark]
				]
			]
		];
	}
	return SNew(SBox).WidthOverride(300.f)[Root];
}

// .poi { width:270; right:48; vertical-center } .ph height:150 .poi-body padding:18,20,20
// Real POI_Info_Struct-derived data, pushed in via SetPoiData() (called from BP_Explorer_PC/BP_POI
// after a Break POI_Info_Struct). Action buttons mirror BP_Info_Widget's real Border_Level/Button_FR/
// Border_360/Border_MediaGallery - each shown only if THAT POI's own data has it set (Level/Level2
// non-empty, Texture_360 valid, Media non-empty), matching the real product exactly (see
// lanessa-real-product-behavior memory: this is per-POI-data-driven, not per-entry-point). POI_FILTER
// (apartment) markers instead get the stat-grid layout below, per user's approved "Xonadon N214"
// mockup - its Maydon/Yotoqxona/Sanuzel/Holati come from the real Filter DataTable struct
// (PopulatePoiFilterFields), and REJA shows the real POI_Info_Struct.Media "FLOOR PLAN" entry's image
// (also PopulatePoiFilterFields) - not invented/mockup-only data.
TSharedRef<SWidget> ULanessaV2Widget::BuildPoiCard()
{
	using namespace V2;

	// .poi .btn { clip-path: polygon(0 0, 100% 0, 100% 100%, 12px 100%, 0 calc(100% - 12px)); }
	auto Btn = [](TAttribute<FText> Text, bool bSolid, FSimpleDelegate OnClick = FSimpleDelegate())
	{
		return SNew(SBox).HeightOverride(40.f)
		[
			SNew(SLanessaCutBorder).FillColor(bSolid ? Olive : Transparent).CutSize(12.f)
			.HoverColor(Olive).bAnimateHover(true)
			.OnClicked(OnClick)
			.Content()
			[
				SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
				[SNew(STextBlock).Text(Text).Font(F(10)).ColorAndOpacity(FSlateColor(bSolid ? IconInk : Paper))]
			]
		];
	};

	// Visibility-gated action button - wraps Btn so a POI missing that field collapses it entirely
	// (matches Update_LevelButton/etc.'s SetVisibility(Collapsed) behavior exactly).
	auto ActionBtn = [this, &Btn](TAttribute<FText> Text, bool bSolid, TAttribute<bool> bVisible, const FString& ActionId)
	{
		return SNew(SBox)
			.Visibility(TAttribute<EVisibility>::Create([bVisible]() { return bVisible.Get() ? EVisibility::Visible : EVisibility::Collapsed; }))
			[Btn(Text, bSolid, FSimpleDelegate::CreateLambda([this, ActionId]() { OnPoiCardAction.Broadcast(ActionId); }))];
	};

	// Plain borderless clickable text - "VR TUR" and "REJA" in the apartment card mockup have no
	// button-box around them, unlike the olive "3D TUR" (which reuses Btn/ActionBtn above). Reuses
	// SLanessaCutBorder (CutSize 0, transparent fill) like every other clickable element in this file,
	// rather than SButton, so no new Slate include is needed.
	auto LinkBtn = [this](TAttribute<FText> Text, TAttribute<bool> bVisible, FSimpleDelegate OnClick)
	{
		return SNew(SBox)
			.Visibility(TAttribute<EVisibility>::Create([bVisible]() { return bVisible.Get() ? EVisibility::Visible : EVisibility::Collapsed; }))
			.HAlign(HAlign_Center)
			[
				SNew(SLanessaCutBorder).CutSize(0.f).FillColor(Transparent).OnClicked(OnClick)
				.Content()
				[SNew(STextBlock).Text(Text).Font(F(11)).ColorAndOpacity(FSlateColor(Paper))]
			];
	};

	// "Xonadon N214" mockup: title is the unit number - pull the trailing digit run off PoiName
	// (e.g. "UNIT 214" -> "214") rather than a separate field, since PoiName already carries it.
	auto ExtractUnitNumber = [](const FString& Name) -> FString
	{
		int32 Start = INDEX_NONE;
		for (int32 i = Name.Len() - 1; i >= 0; --i)
		{
			if (FChar::IsDigit(Name[i])) { Start = i; } else if (Start != INDEX_NONE) { break; }
		}
		return Start != INDEX_NONE ? Name.RightChop(Start) : Name;
	};

	auto StatCell = [](const FString& Label, TAttribute<FText> Value)
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f,0.f,0.f,3.f)
			[SNew(STextBlock).Text(FText::FromString(Label)).Font(F(10)).ColorAndOpacity(FSlateColor(TextDim))]
			+ SVerticalBox::Slot().AutoHeight()
			[SNew(STextBlock).Text(Value).Font(F(14)).ColorAndOpacity(FSlateColor(Paper))];
	};

	return SNew(SBox).WidthOverride(TAttribute<FOptionalSize>::Create([this]() { return bPoiIsFilterCard ? FOptionalSize(300.f) : FOptionalSize(270.f); }))
	[
		SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(PanelSolid).Padding(0.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SBox).HeightOverride(150.f)[Fill(FLinearColor(0.09f,0.09f,0.07f,1.f))]]
				+ SOverlay::Slot()
				[
					SNew(SBox).HeightOverride(150.f)
					[
						SNew(SImage).Image(&PoiImageBrush)
						.Visibility(TAttribute<EVisibility>::Create([this]() { return PoiImageTexture ? EVisibility::Visible : EVisibility::Collapsed; }))
					]
				]
				// .ph .x - close button, click clears SelectedPoiId
				+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(10.f)
				[
					SNew(SBox).WidthOverride(26.f).HeightOverride(26.f)
					[
						SNew(SLanessaCutBorder).CutSize(0.f).FillColor(FLinearColor(0,0,0,0.4f))
						.OnClicked(FSimpleDelegate::CreateLambda([this]() { SelectPoi(FString()); }))
						.Content()
						[
							SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
							[SNew(STextBlock).Text(FText::FromString(TEXT("✕"))).Font(F(13)).ColorAndOpacity(FSlateColor(TextDim))]
						]
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(20.f,18.f,20.f,20.f))
			[
				SNew(SOverlay)
				// Generic card (every non-POI_FILTER type: amenities, surroundings, POI_CENTER, ...)
				+ SOverlay::Slot()
				[
					SNew(SVerticalBox)
					.Visibility(TAttribute<EVisibility>::Create([this]() { return bPoiIsFilterCard ? EVisibility::Collapsed : EVisibility::Visible; }))
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f,0.f,0.f,6.f)
					[
						SNew(STextBlock).Text(TAttribute<FText>::Create([this]() { return FText::FromString(PoiName); }))
						.Font(D(19)).ColorAndOpacity(FSlateColor(Paper))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f,0.f,0.f,14.f)
					[
						SNew(STextBlock).Text(TAttribute<FText>::Create([this]() { return FText::FromString(PoiInformation); }))
						.Font(F(12)).ColorAndOpacity(FSlateColor(TextDim)).AutoWrapText(true)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f,0.f,0.f,14.f)
					[
						SNew(STextBlock).Text(TAttribute<FText>::Create([this]() { return FText::FromString(PoiFooter); }))
						.Font(F(10)).ColorAndOpacity(FSlateColor(OliveGlow))
						.Visibility(TAttribute<EVisibility>::Create([this]() { return PoiFooter.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; }))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f,0.f,0.f,8.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f,0.f,8.f,0.f)
						[ActionBtn(TAttribute<FText>::Create([this](){ return FText::FromString(PoiLevelButtonText); }), true,
							TAttribute<bool>::Create([this](){ return bPoiHasLevel; }), TEXT("level"))]
						+ SHorizontalBox::Slot().FillWidth(1.f)
						[ActionBtn(TAttribute<FText>::Create([this](){ return FText::FromString(PoiLevel2ButtonText); }), false,
							TAttribute<bool>::Create([this](){ return bPoiHasLevel2; }), TEXT("level2"))]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f,0.f,0.f,8.f)
					[ActionBtn(FText::FromString(TEXT("360° KO'RISH")), false, TAttribute<bool>::Create([this](){ return bPoiHas360; }), TEXT("360"))]
					+ SVerticalBox::Slot().AutoHeight()
					[ActionBtn(TAttribute<FText>::Create([this](){ return FText::FromString(PoiMediaButtonText); }), false,
						TAttribute<bool>::Create([this](){ return bPoiHasMedia; }), TEXT("media"))]
				]
				// Apartment (POI_FILTER) stat-grid card, per user's approved "Xonadon N214" mockup -
				// eyebrow (floor/rooms) + big unit number + 2x2 Maydon/Yotoqxona/Sanuzel/Holati grid +
				// 3D TUR (=level)/VR TUR (=level2) + REJA (opens the real "FLOOR PLAN" Media image).
				+ SOverlay::Slot()
				[
					SNew(SVerticalBox)
					.Visibility(TAttribute<EVisibility>::Create([this]() { return bPoiIsFilterCard ? EVisibility::Visible : EVisibility::Collapsed; }))
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f,0.f,0.f,6.f)
					[
						SNew(STextBlock)
						.Text(TAttribute<FText>::Create([this]() {
							return PoiFloor > 0
								? FText::FromString(FString::Printf(TEXT("%d-QAVAT · %d XONALI"), PoiFloor, PoiRoomsCount))
								: FText::FromString(FString::Printf(TEXT("%d XONALI"), PoiRoomsCount));
						}))
						.Font(F(10)).ColorAndOpacity(FSlateColor(TextDim))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f,0.f,0.f,16.f)
					[
						SNew(STextBlock)
						.Text(TAttribute<FText>::Create([this, ExtractUnitNumber]() { return FText::FromString(TEXT("Xonadon №") + ExtractUnitNumber(PoiName)); }))
						.Font(D(22)).ColorAndOpacity(FSlateColor(Paper))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f,0.f,0.f,14.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f,0.f,0.f,12.f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.f)
							[StatCell(TEXT("Maydon"), TAttribute<FText>::Create([this](){ return FText::FromString(FString::Printf(TEXT("%d m²"), PoiSurfaceVal)); }))]
							+ SHorizontalBox::Slot().FillWidth(1.f)
							[StatCell(TEXT("Yotoqxona"), TAttribute<FText>::Create([this](){ return FText::AsNumber(PoiRoomsCount); }))]
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.f)
							[StatCell(TEXT("Sanuzel"), TAttribute<FText>::Create([this](){ return FText::AsNumber(PoiBathroomsVal); }))]
							+ SHorizontalBox::Slot().FillWidth(1.f)
							[StatCell(TEXT("Holati"), TAttribute<FText>::Create([this](){ return FText::FromString(PoiAvailabilityShort); }))]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f,0.f,0.f,10.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f,0.f,8.f,0.f)
						[ActionBtn(FText::FromString(TEXT("3D TUR")), true,
							TAttribute<bool>::Create([this](){ return bPoiHasLevel; }), TEXT("level"))]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[LinkBtn(FText::FromString(TEXT("VR TUR")),
							TAttribute<bool>::Create([this](){ return bPoiHasLevel2; }), FSimpleDelegate::CreateLambda([this]() { OnPoiCardAction.Broadcast(TEXT("level2")); }))]
					]
					+ SVerticalBox::Slot().AutoHeight()
					// Real product: REJA/Media galereya opens the real BP_Gallery_Widget seeded with this
					// POI's full Media array (Border_MediaGallery -> SetMedia(POI) -> Open_Gallery), not a
					// single floor-plan image - see BP_Explorer_PC's OnPoiCardAction_Event "media" branch.
					[LinkBtn(FText::FromString(TEXT("REJA")), TAttribute<bool>::Create([this](){ return bPoiHasPlan; }),
						FSimpleDelegate::CreateLambda([this]() { OnPoiCardAction.Broadcast(TEXT("media")); }))]
				]
			]
		]
	];
}

// .plan-modal { width:640 } .plan-grid rooms
TSharedRef<SWidget> ULanessaV2Widget::BuildPlanModal()
{
	using namespace V2;

	// #planGrid addEventListener('click', ...) -> closes the modal and calls launchTourToast(room+' xonasidan') -
	// rooms had no click handling at all before (purely decorative boxes).
	auto Room = [this](const FString& Label, const FString& Area, bool bTagFilled, float FillProportion)
	{
		return SNew(SBox).HeightOverride(150.f)
		[
			SNew(SLanessaCutBorder).CutSize(0.f).FillColor(FLinearColor(0.063f,0.059f,0.047f,1.f))
			.HoverColor(FLinearColor(1.f,1.f,1.f,0.06f)).bAnimateHover(true)
			.OnClicked(FSimpleDelegate::CreateLambda([this, Label]() { SetPlanOpen(false); ShowTourToast(Label + TEXT(" xonasidan")); }))
			.Content()
			[
			SNew(SBox).HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(FMargin(10.f))
			[
				SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(bTagFilled ? Olive : FLinearColor(0.078f,0.078f,0.063f,0.9f))
				.Padding(FMargin(9.f,5.f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Label)).Font(F(10)).ColorAndOpacity(FSlateColor(bTagFilled ? IconInk : Paper))]
					+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Area)).Font(F(11)).ColorAndOpacity(FSlateColor(bTagFilled ? IconInk : Paper))]
				]
			]
			]
		];
	};

	// .plan-backdrop { background:rgba(0,0,0,.72); display:flex; align-items:center; justify-content:center; }
	// clicking the backdrop itself (not the modal) closes it, matching `if(e.target===planBackdrop) close()`.
	return SNew(SLanessaCutBorder).CutSize(0.f).FillColor(FLinearColor(0.f,0.f,0.f,0.72f))
	.OnClicked(FSimpleDelegate::CreateLambda([this]() { SetPlanOpen(false); }))
	.Content()
	[
	SNew(SOverlay)
	+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
	[
	SNew(SBox).WidthOverride(640.f)
	[
		SNew(SLanessaCutBorder).CutSize(0.f).FillColor(FLinearColor(0,0,0,0)) // inert layer to stop clicks reaching the backdrop behind the modal
		.Content()
		[
		SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(PanelSolid).Padding(0.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(24.f,20.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)[SNew(STextBlock).Text(FText::FromString(TEXT("Xonadon №14 · Rejasi"))).Font(D(18)).ColorAndOpacity(FSlateColor(Paper))]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(28.f).HeightOverride(28.f)
					[
						SNew(SLanessaCutBorder).CutSize(0.f).FillColor(Transparent).HoverColor(FLinearColor(1,1,1,0.08f)).bAnimateHover(true)
						.OnClicked(FSimpleDelegate::CreateLambda([this]() { SetPlanOpen(false); }))
						.Content()
						[
							SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
							[SNew(STextBlock).Text(FText::FromString(TEXT("✕"))).Font(F(13)).ColorAndOpacity(FSlateColor(TextDim))]
						]
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(24.f,0.f,24.f,0.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.3f).Padding(0.f,0.f,1.f,0.f)[Room(TEXT("Yashash xonasi"), TEXT("23.7 m²"), true, 1.3f)]
					+ SHorizontalBox::Slot().FillWidth(0.9f)[Room(TEXT("Yotoqxona"), TEXT("14.2 m²"), true, 0.9f)]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f,1.f,0.f,0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f,0.f,1.f,0.f)[Room(TEXT("Oshxona"), TEXT("11.3 m²"), false, 1.f)]
					+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f,0.f,1.f,0.f)[Room(TEXT("Yo'lak"), TEXT("5.6 m²"), false, 1.f)]
					+ SHorizontalBox::Slot().FillWidth(1.f)[Room(TEXT("Sanuzel"), TEXT("3.0 m²"), false, 1.f)]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(24.f,12.f,24.f,0.f)
			[SNew(STextBlock).Text(FText::FromString(TEXT("XONANI BOSING — 3D TUR O'SHA XONADAN BOSHLANADI"))).Font(F(9)).ColorAndOpacity(FSlateColor(TextDim))]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(24.f,18.f,24.f,24.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[SNew(STextBlock).Text(FText::FromString(TEXT("Umumiy maydon 78 m²"))).Font(F(11)).ColorAndOpacity(FSlateColor(TextDim))]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					// .plan-foot button { clip-path: polygon(0 0, 100% 0, 100% 100%, 10px 100%, 0 calc(100% - 10px)); }
					SNew(SLanessaCutBorder).FillColor(Olive).CutSize(10.f)
					.Content()
					[
						SNew(SBox).Padding(FMargin(22.f,12.f))
						[SNew(STextBlock).Text(FText::FromString(TEXT("XONADONNI TANLASH"))).Font(F(10)).ColorAndOpacity(FSlateColor(IconInk))]
					]
				]
			]
		]
		]
		]
		]
	];
}

// .utility { bottom:40 right:48; button 50x50 }
// .utility button { cursor:pointer } .utility button:hover { background:#141410 } - had zero hover
// feedback before (plain static SBorder); v2's own JS has no click action for these two buttons
// either (Sozlamalar/Chiqish are hover-only in the mockup), so only the hover state is added here.
// O'ngdagi "Chiqish" tugmasi dasturning o'zidan (exe) chiqadi - progulkadagi X o'rniga
// yagona chiqish yo'li shu.
TSharedRef<SWidget> ULanessaV2Widget::BuildUtility()
{
	using namespace V2;
	using namespace LanessaIcons;
	auto Btn = [](const TArray<FLanessaIconPrim>& Icon, FSimpleDelegate OnClicked = FSimpleDelegate())
	{
		return SNew(SBox).WidthOverride(50.f).HeightOverride(50.f)
		[
			SNew(SLanessaCutBorder).CutSize(0.f).FillColor(PanelSolid)
			.HoverColor(FLinearColor(0.078f, 0.078f, 0.063f, 1.f)) // #141410
			.bAnimateHover(true)
			.OnClicked(OnClicked)
			.Content()
			[
				SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
				[SNew(SLanessaLineIcon).Primitives(Icon).IconSize(16.f).StrokeWidth(1.3f).StrokeColor(FLinearColor(Paper.R,Paper.G,Paper.B,0.8f))]
			]
		];
	};
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.f,0.f,1.f,0.f)[Btn(UtilitySettings())]
		+ SHorizontalBox::Slot().AutoWidth()[Btn(Exit(), FSimpleDelegate::CreateLambda([this]()
		{
			// QuitGame, not FPlatformMisc::RequestExit: it goes through the normal shutdown path, and
			// in PIE it only ends the play session instead of closing the editor.
			APlayerController* PC = GetOwningPlayer();
			if (!PC && GetWorld()) { PC = UGameplayStatics::GetPlayerController(GetWorld(), 0); }
			UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false);
		}))];
}

// Plaginning o'z sozlama sahifasi (Project Settings -> Plugins -> Lanessa Remote).
// Quyidagi hamma nom shundan o'qiladi, kodda qattiq yozilmagan: boshqa loyihaga
// ko'chirganda C++ ga tegmasdan, sozlamadan moslash mumkin.
static const ULanessaRemoteSettings& LanessaCfg()
{
	return ULanessaRemoteSettings::Get();
}

// ==== Operator remote control ===================================================================
// See the "Operator remote control" block in LanessaV2Widget.h for why every entry point here is a
// static resolved against the class default object rather than a method on the live widget.

// The world the operator is actually looking at. An editor session can hold several worlds at once
// (the editor world, a PIE world, preview worlds for widget/material thumbnails); only a game world
// is the one whose widget and pawn the panel should drive.
static UWorld* LanessaGameWorld()
{
	if (!GEngine) { return nullptr; }
	for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
	{
		if ((Ctx.WorldType == EWorldType::PIE || Ctx.WorldType == EWorldType::Game) && Ctx.World())
		{
			return Ctx.World();
		}
	}
	return nullptr;
}

// Writes a double/float property by EXACT name. BP_Explorer_Pawn's live camera state lives in
// Pitch_Current/Yaw_Current/Location_Current, which Blueprint marks non-editable - Python cannot
// touch them ("cannot be edited on instances") but reflection from C++ can, the same way
// LanessaWritePawnCameraVars already writes the _New targets.
static bool LanessaWriteDouble(AActor* Actor, FName Name, double Value)
{
	if (!Actor || Name.IsNone()) { return false; }
	const FString Want = Name.ToString();
	for (TFieldIterator<FProperty> It(Actor->GetClass()); It; ++It)
	{
		if (!It->GetName().Equals(Want, ESearchCase::IgnoreCase)) { continue; }
		if (FDoubleProperty* DP = CastField<FDoubleProperty>(*It)) { DP->SetPropertyValue_InContainer(Actor, Value); return true; }
		if (FFloatProperty* FP = CastField<FFloatProperty>(*It)) { FP->SetPropertyValue_InContainer(Actor, (float)Value); return true; }
	}
	return false;
}

// Same for a bool - used for "Allow_Idle?", the pawn's attract-mode auto-rotation switch (its name
// really does end in a question mark).
static bool LanessaWriteBool(AActor* Actor, FName Name, bool Value)
{
	if (!Actor || Name.IsNone()) { return false; }
	const FString Want = Name.ToString();
	for (TFieldIterator<FProperty> It(Actor->GetClass()); It; ++It)
	{
		if (!It->GetName().Equals(Want, ESearchCase::IgnoreCase)) { continue; }
		if (FBoolProperty* BP = CastField<FBoolProperty>(*It)) { BP->SetPropertyValue_InContainer(Actor, Value); return true; }
	}
	return false;
}

// Reads a double/float property by name prefix - BP-generated variables can carry a GUID suffix, so
// prefix matching is the same approach BuildPoiInfoText and GetPOITypeAsInt already use here.
static bool LanessaReadDouble(AActor* Actor, FName Prefix, double& Out)
{
	if (!Actor || Prefix.IsNone()) { return false; }
	const FString Want = Prefix.ToString();
	for (TFieldIterator<FProperty> It(Actor->GetClass()); It; ++It)
	{
		if (!It->GetName().StartsWith(Want, ESearchCase::IgnoreCase)) { continue; }
		if (const FDoubleProperty* DP = CastField<FDoubleProperty>(*It)) { Out = DP->GetPropertyValue_InContainer(Actor); return true; }
		if (const FFloatProperty* FP = CastField<FFloatProperty>(*It)) { Out = (double)FP->GetPropertyValue_InContainer(Actor); return true; }
	}
	return false;
}

// The POI's display name out of its POI_Info_Struct (field "Name", GUID-suffixed in the generated
// struct). Falls back to the actor's object name so a list row is never blank.
static FString LanessaPoiDisplayName(AActor* POIActor)
{
	if (!POIActor) { return FString(); }
	for (TFieldIterator<FProperty> It(POIActor->GetClass()); It; ++It)
	{
		if (!It->GetName().StartsWith(LanessaCfg().PoiInfoStructPrefix, ESearchCase::IgnoreCase)) { continue; }
		const FStructProperty* InfoProp = CastField<FStructProperty>(*It);
		if (!InfoProp) { continue; }
		const void* InfoPtr = InfoProp->ContainerPtrToValuePtr<void>(POIActor);
		for (TFieldIterator<FProperty> F(InfoProp->Struct); F; ++F)
		{
			if (!F->GetName().StartsWith(LanessaCfg().PoiNameField.ToString(), ESearchCase::IgnoreCase)) { continue; }
			// POI_Info_Struct is a Blueprint UserDefinedStruct, so whoever authored it chose
			// String, Text or Name for this field - assuming FStrProperty silently produced an
			// empty value and every list row fell back to the actor's object name.
			FString Value;
			if (const FStrProperty* SP = CastField<FStrProperty>(*F))
			{
				Value = SP->GetPropertyValue_InContainer(InfoPtr);
			}
			else if (const FTextProperty* TP = CastField<FTextProperty>(*F))
			{
				Value = TP->GetPropertyValue_InContainer(InfoPtr).ToString();
			}
			else if (const FNameProperty* NP = CastField<FNameProperty>(*F))
			{
				Value = NP->GetPropertyValue_InContainer(InfoPtr).ToString();
			}
			if (!Value.IsEmpty()) { return Value; }
		}
	}
	return POIActor->GetName();
}

static APawn* LanessaExplorerPawn()
{
	UWorld* World = LanessaGameWorld();
	return World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
}

/**
 * CHIQISH ikkita narsani bekor qilishi kerak, lekin ulardan faqat bittasi kimningdir
 * vazifasi. Qirqimning o'zini BP_Explorer_PC Reset_SectionView ga javob berib bekor
 * qiladi. Qavat ikonkalari esa yo'q: ular BP_FloorSectionMarker aktyorlari va ularni
 * ko'rsatish/yashirish butunlay BP_Explorer_PC ning navigatsiya ishlovchilarida.
 * Ya'ni 22 qavatli minorani 9-qavatdan qirqsangiz, 9 dan yuqoridagi hamma ikonka
 * yashirinadi va hech narsa ularni qaytarmaydi - CHIQISH ataylab navigatsiya
 * qilmaydi, shuning uchun OnNavClicked umuman ishlamaydi.
 *
 * C++ qila oladigan narsa - ularni tiklaydigan yagona amalni takrorlash: bino
 * markerida Select_POI, xuddi o'sha binoning 3D ikonkasini bosgandek. Qaysi bino
 * ekani BP_Explorer_PC ning CurrentQirqimBuilding o'zgaruvchisidan olinadi.
 * Aynan o'sha bino bo'lishi shart: levelda bir nechta minora bo'ladi va hammasini
 * tiklash foydalanuvchi ochmagan binolarning ikonkalarini ham yoqib yuborardi.
 */
static void LanessaRestoreQirqimBuilding()
{
	UWorld* World = LanessaGameWorld();
	if (!World) { return; }

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (!PC) { return; }

	const FStrProperty* Prop = FindFProperty<FStrProperty>(PC->GetClass(), TEXT("CurrentQirqimBuilding"));
	if (!Prop)
	{
		// Nom bo'yicha qidiriladi, shuning uchun qayta nomlansa xato shu yerda ko'rinadi.
		UE_LOG(LogTemp, Warning, TEXT("[LanessaQirqim] %s da CurrentQirqimBuilding yo'q - qavat ikonkalari tiklanmadi"),
			*PC->GetClass()->GetName());
		return;
	}

	const FString TargetBuilding = Prop->GetPropertyValue_InContainer(PC);
	if (TargetBuilding.IsEmpty())
	{
		// Yo hech qanday bino ochilmagan - u holda ikonka ham yashirilmagan - yoki
		// BP_Explorer_PC bino ikonkasi bosilganda o'zgaruvchini yozmagan. Log ikkisini ajratadi.
		UE_LOG(LogTemp, Warning, TEXT("[LanessaQirqim] CurrentQirqimBuilding bo'sh - hech narsa tiklanmadi"));
		return;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		// Sinf nomi sozlamadan olinadi (Project Settings -> Plugins -> Lanessa Remote),
		// qolgan hamma joydagidek - boshqa loyihada marker boshqacha atalishi mumkin.
		if (!IsValid(Actor) ||
		    !Actor->GetClass()->GetName().StartsWith(LanessaCfg().BuildingMarkerClassPrefix, ESearchCase::IgnoreCase))
		{
			continue;
		}

		int32 Floor = 0;
		FString Building;
		ULanessaV2Widget::GetOwnFloorAndBuilding(Actor, Floor, Building);
		if (!Building.Equals(TargetBuilding, ESearchCase::IgnoreCase)) { continue; }

		ULanessaV2Widget::CallPOISelectPOI(Actor);
		UE_LOG(LogTemp, Log, TEXT("[LanessaQirqim] '%s' binosi %s orqali tiklandi"), *TargetBuilding, *Actor->GetName());
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[LanessaQirqim] '%s' ga mos BuildingSectionMarker topilmadi"), *TargetBuilding);
}

ULanessaV2Widget* ULanessaV2Widget::GetLiveWidget()
{
	const UWorld* GameWorld = LanessaGameWorld();
	ULanessaV2Widget* Fallback = nullptr;
	for (TObjectIterator<ULanessaV2Widget> It; It; ++It)
	{
		ULanessaV2Widget* W = *It;
		if (!IsValid(W) || W->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)) { continue; }
		if (GameWorld && W->GetWorld() == GameWorld) { return W; }
		Fallback = W;
	}
	return Fallback;
}

bool ULanessaV2Widget::RemoteSetSeason(const FString& SeasonId)
{
	ULanessaV2Widget* W = GetLiveWidget();
	if (!W) { return false; }
	W->SetSeason(SeasonId);   // updates the chip AND broadcasts OnSeasonChanged -> BP_Explorer_PC
	return true;
}

bool ULanessaV2Widget::RemoteSetWeather(const FString& WeatherId)
{
	ULanessaV2Widget* W = GetLiveWidget();
	if (!W) { return false; }
	W->SetWeather(WeatherId);
	return true;
}

bool ULanessaV2Widget::RemoteSetTimePct(float Pct)
{
	ULanessaV2Widget* W = GetLiveWidget();
	if (!W) { return false; }
	W->SetTimePct(Pct);
	return true;
}

bool ULanessaV2Widget::RemoteSelectFloor(int32 Floor)
{
	ULanessaV2Widget* W = GetLiveWidget();
	if (!W) { return false; }
	W->SelectFloor(Floor);
	return true;
}

bool ULanessaV2Widget::RemoteSetActiveView(const FString& ViewId)
{
	ULanessaV2Widget* W = GetLiveWidget();
	if (!W) { return false; }
	W->SetActiveView(ViewId);
	return true;
}

bool ULanessaV2Widget::RemoteSelectPoi(const FString& PoiId)
{
	ULanessaV2Widget* W = GetLiveWidget();
	if (!W) { return false; }

	// Broadcast first so BP_Explorer_PC still does its full job (POI card data, filter state) for
	// every id it does know about.
	W->OnPoiEntryClicked.Broadcast(PoiId);

	// Then move the camera directly rather than trusting that broadcast to do it. The handler
	// resolves the id against the game instance's own BP_POIs list, which is only populated for
	// POIs belonging to a category panel, so broadcasting a plain viewpoint id reaches nothing and
	// the camera never moves - measured, not assumed. Select_POI is the same call that chain ends
	// in, and running it twice for an id the handler did resolve is harmless: it re-targets the
	// same focus animation at the same place.
	AActor* Found = nullptr;
	if (UWorld* World = LanessaGameWorld())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (IsValid(*It) && It->GetName() == PoiId) { Found = *It; break; }
		}
	}
	if (!Found) { return false; }
	CallPOISelectPOI(Found);
	return true;
}

bool ULanessaV2Widget::RemoteResetSectionView()
{
	ULanessaV2Widget* W = GetLiveWidget();
	if (!W) { return false; }
	W->Reset_SectionView.Broadcast();
	// Ekrandagi CHIQISH tugmasi bilan AYNAN bir xil ish qilishi shart. Omborda bu
	// chaqiruv yo'q edi: u yerda tuzatish faqat ekrandagi tugmaga qo'shilgan, natijada
	// telefondan qirqimni qaytarganda qavat ikonkalari yashirin qolib ketardi.
	LanessaRestoreQirqimBuilding();
	return true;
}

void ULanessaV2Widget::RemoteListPois(bool bFilterUnits, TArray<FString>& OutIds, TArray<FString>& OutNames)
{
	OutIds.Reset();
	OutNames.Reset();
	UWorld* World = LanessaGameWorld();
	if (!World) { return; }
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (!IsValid(A) || !A->GetClass()->GetName().StartsWith(LanessaCfg().PoiClassPrefix, ESearchCase::IgnoreCase)) { continue; }
		// 2 = POI_FILTER (an apartment marker), anything else is a plain named viewpoint.
		const bool bIsUnit = (GetPOITypeAsInt(A) == 2);
		if (bIsUnit != bFilterUnits) { continue; }
		OutIds.Add(A->GetName());
		OutNames.Add(LanessaPoiDisplayName(A));
	}
}

bool ULanessaV2Widget::RemoteCameraSet(FVector Pivot, double Pitch, double Yaw, double ArmLength, bool bAnimate)
{
	APawn* Pawn = LanessaExplorerPawn();
	if (!Pawn) { return false; }

	// Clamp to the pawn's own limits so the operator cannot reach anywhere the mouse could not.
	// Min/Max are read rather than assumed, and re-ordered defensively in case a level ever has them
	// authored the wrong way round.
	double PitchMin = -89.0, PitchMax = 89.0, ArmMin = 0.0, ArmMax = 1000000.0;
	LanessaReadDouble(Pawn, LanessaCfg().PitchLimitMin, PitchMin);
	LanessaReadDouble(Pawn, LanessaCfg().PitchLimitMax, PitchMax);
	LanessaReadDouble(Pawn, LanessaCfg().ArmLengthMin, ArmMin);
	LanessaReadDouble(Pawn, LanessaCfg().ArmLengthMax, ArmMax);
	Pitch     = FMath::Clamp(Pitch, FMath::Min(PitchMin, PitchMax), FMath::Max(PitchMin, PitchMax));
	ArmLength = FMath::Clamp(ArmLength, FMath::Min(ArmMin, ArmMax), FMath::Max(ArmMin, ArmMax));

	if (bAnimate)
	{
		SetExplorerPawnCameraTarget(Pawn, Pivot, Pitch, Yaw, ArmLength);
		return true;
	}

	// Keep the pawn's own target variables in step even on the direct path, otherwise the next Focus()
	// (any nav button or in-game POI click) would animate back to wherever the last animated move left
	// them rather than continuing from where the operator dragged to.
	LanessaWritePawnCameraVars(Pawn, Pivot, Pitch, Yaw, ArmLength);

	// The live state is Pitch_Current/Yaw_Current/Location_Current - the pawn rebuilds its transform
	// from those every tick. Setting the controller's control rotation instead does nothing: it is
	// overwritten on the very next frame, which is why a dragged camera snapped straight back to
	// wherever it already was (measured: set Yaw=65, read Yaw=-115 with zero delay).
	// Pawn ning o'z hisobini ham yangilab qo'yamiz, aks holda keyingi sichqoncha
	// harakati eski qiymatdan davom etadi.
	LanessaWriteDouble(Pawn, LanessaCfg().PitchCurrent, Pitch);
	LanessaWriteDouble(Pawn, LanessaCfg().YawCurrent, Yaw);

	// Attract-mode auto-rotation (Idle_Rotate_Yaw 45/s, Idle_Rotate_Pitch 8/s) keeps turning the
	// camera on its own, so a manually placed shot drifts away within seconds. Manual control and
	// idle spin are mutually exclusive; RemoteSetIdleRotation(true) puts it back for attract mode.
	LanessaWriteBool(Pawn, LanessaCfg().IdleFlag, false);

	Pawn->SetActorLocation(Pivot);

	// Ekranga chiqadigan burilish - BOSHQARUVCHINIKI. O'lchangan: kamera menejeri
	// yaw=-99.71 va boshqaruvchi yaw=-99.07 mos keladi, spring arm esa o'sha paytda
	// 30.53 da turadi. Ya'ni arm ning dunyo burilishi bu yerda kadrni ifodalamaydi.
	// Avval shu yerda Arm->SetWorldRotation yozilardi: u arm ni kadrdan uzib qo'yar,
	// natijada RemoteCameraGet 130 gradusgacha xato qiymat qaytarar edi.
	if (AController* C = Pawn->GetController())
	{
		C->SetControlRotation(FRotator(Pitch, Yaw, 0.0));
	}
	if (USpringArmComponent* Arm = Pawn->FindComponentByClass<USpringArmComponent>())
	{
		Arm->TargetArmLength = (float)ArmLength;
	}
	return true;
}

bool ULanessaV2Widget::RemoteSetIdleRotation(bool bEnabled)
{
	APawn* Pawn = LanessaExplorerPawn();
	if (!Pawn) { return false; }
	return LanessaWriteBool(Pawn, LanessaCfg().IdleFlag, bEnabled);
}

bool ULanessaV2Widget::RemoteCameraGet(FVector& Pivot, double& Pitch, double& Yaw, double& ArmLength)
{
	Pivot = FVector::ZeroVector;
	Pitch = 0.0;
	Yaw = 0.0;
	ArmLength = 0.0;

	APawn* Pawn = LanessaExplorerPawn();
	if (!Pawn) { return false; }

	Pivot = Pawn->GetActorLocation();
	// RemoteCameraSet bilan BIR XIL manbadan o'qishi shart, aks holda panel Get qilib
	// olgan burchakka nisbatan Set yuborganda doimiy og'ish paydo bo'ladi. Ekrandagi
	// kadr boshqaruvchining burilishi bilan bir xil (o'lchangan: kamera menejeri
	// -99.71 / boshqaruvchi -99.07), spring arm esa o'sha paytda 30.53 da edi.
	// Normallashtiriladi, chunki panel bu sonlar ustida arifmetika qiladi va
	// 0..360 dan -180..180 ga o'tishda surish o'rtasida to'liq aylanib ketardi.
	const FRotator R = (Pawn->GetController() ? Pawn->GetController()->GetControlRotation()
	                                          : Pawn->GetActorRotation()).GetNormalized();
	Pitch = R.Pitch;
	Yaw = R.Yaw;
	if (USpringArmComponent* Arm = Pawn->FindComponentByClass<USpringArmComponent>())
	{
		ArmLength = Arm->TargetArmLength;
	}
	return true;
}

// ==== Bino va qavat: sahnaning o'zidan o'qiladi ==============================
// Qavat soni hech qayerda yozilmagan - panel 12 tani qattiq ko'rsatardi, bu
// levelda esa atigi 3 ta qavat markeri bor va boshqa loyihada 22 ta bo'lishi
// mumkin. Shuning uchun ro'yxat har doim sahnadagi BP_FloorSectionMarker /
// BP_BuildingSectionMarker aktyorlaridan tuziladi.

// O'yin dunyosidagi aktyorni obyekt nomi bo'yicha topadi.
static AActor* LanessaFindActor(const FString& Name)
{
	UWorld* World = LanessaGameWorld();
	if (!World || Name.IsEmpty()) { return nullptr; }
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (IsValid(*It) && It->GetName() == Name) { return *It; }
	}
	return nullptr;
}

// Marker tasvirlaydigan qirqim qutisi: markerning SectionView_Volume i bo'lsa
// o'shaniki, bo'lmasa (bino markerida volume yo'q) markerning o'z chegarasi.
static bool LanessaMarkerBox(AActor* Marker, FVector& OutCentre, FVector& OutExtent, double& OutYaw)
{
	if (!Marker) { return false; }
	AActor* Src = Marker;
	for (TFieldIterator<FProperty> It(Marker->GetClass()); It; ++It)
	{
		if (!It->GetName().Equals(LanessaCfg().SectionVolumeProperty.ToString(), ESearchCase::IgnoreCase)) { continue; }
		if (FObjectProperty* OP = CastField<FObjectProperty>(*It))
		{
			if (AActor* Vol = Cast<AActor>(OP->GetObjectPropertyValue_InContainer(Marker)))
			{
				Src = Vol;
			}
		}
		break;
	}
	FVector Origin, Extent;
	Src->GetActorBounds(false, Origin, Extent);
	OutCentre = Origin;
	OutExtent = Extent;
	OutYaw = Src->GetActorRotation().Yaw;
	return true;
}

void ULanessaV2Widget::RemoteListBuildings(TArray<FString>& OutIds, TArray<FString>& OutNames)
{
	OutIds.Reset();
	OutNames.Reset();
	UWorld* World = LanessaGameWorld();
	if (!World) { return; }
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (!IsValid(A)) { continue; }
		if (!A->GetClass()->GetName().StartsWith(LanessaCfg().BuildingMarkerClassPrefix, ESearchCase::IgnoreCase)) { continue; }
		OutIds.Add(A->GetName());
		// POI_Info_Struct dagi nom ko'pincha to'ldirilmagan ("Name" deb qolgan),
		// unday holatda aktyor yorlig'i foydaliroq.
		FString N = LanessaPoiDisplayName(A);
		if (N.IsEmpty() || N.Equals(TEXT("Name"), ESearchCase::IgnoreCase))
		{
			N = A->GetActorNameOrLabel();
		}
		OutNames.Add(N);
	}
}

void ULanessaV2Widget::RemoteListFloors(const FString& BuildingId,
                                        TArray<FString>& OutIds, TArray<FString>& OutNames)
{
	OutIds.Reset();
	OutNames.Reset();
	UWorld* World = LanessaGameWorld();
	if (!World) { return; }

	// Bino markerining CHEGARASINI ishlatib bo'lmaydi: u Camera_Preview va
	// DrawFrustum komponentlarini ham qamraydi, shuning uchun markazi binodan
	// uzoqqa siljiydi (o'lchangan: marker joyi (3390,270), chegara markazi
	// (7253,571), qavat hajmlari esa (0,-1400)) - hech bir qavat ichiga tushmasdi.
	// Shuning uchun har bir qavatni ENG YAQIN bino markeriga biriktiramiz:
	// bu qo'shimcha sozlashsiz, nechta bino bo'lsa ham to'g'ri ishlaydi.
	TArray<AActor*> Buildings;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (IsValid(A) && A->GetClass()->GetName().StartsWith(LanessaCfg().BuildingMarkerClassPrefix, ESearchCase::IgnoreCase))
		{
			Buildings.Add(A);
		}
	}
	const bool bFilter = !BuildingId.IsEmpty() && Buildings.Num() > 0;

	TArray<TPair<double, AActor*>> Found;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (!IsValid(A)) { continue; }
		if (!A->GetClass()->GetName().StartsWith(LanessaCfg().FloorMarkerClassPrefix, ESearchCase::IgnoreCase)) { continue; }
		FVector C, E;
		double Y;
		if (!LanessaMarkerBox(A, C, E, Y)) { continue; }
		if (bFilter)
		{
			AActor* Nearest = nullptr;
			double Best = TNumericLimits<double>::Max();
			for (AActor* B : Buildings)
			{
				const FVector BL = B->GetActorLocation();
				const double D = FVector2D(C.X - BL.X, C.Y - BL.Y).SizeSquared();
				if (D < Best) { Best = D; Nearest = B; }
			}
			if (!Nearest || Nearest->GetName() != BuildingId) { continue; }
		}
		Found.Add(TPair<double, AActor*>(C.Z, A));
	}
	// Pastdan tepaga - 1-qavat eng pastda.
	Found.Sort([](const TPair<double, AActor*>& L, const TPair<double, AActor*>& R)
	{
		return L.Key < R.Key;
	});
	for (int32 i = 0; i < Found.Num(); ++i)
	{
		OutIds.Add(Found[i].Value->GetName());
		OutNames.Add(FString::Printf(TEXT("%d"), i + 1));
	}
}

// Qaysi bino qirqilayotganini YOZIB QO'YAMIZ. Buni odatda BP_Explorer_PC bino
// ikonkasi bosilganda qiladi; panel qavatni to'g'ridan tanlasa o'zgaruvchi bo'sh qolar,
// CHIQISH esa qaysi binoni tiklashni bilmay qolardi (o'lchangan: "[LanessaQirqim]
// CurrentQirqimBuilding bo'sh"). Markerning o'z binosidan olamiz, ya'ni ikonka
// bosilgandagi qiymat bilan bir xil.
static void LanessaRememberQirqimBuilding(AActor* Marker)
{
	UWorld* World = LanessaGameWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!Marker || !PC) { return; }
	int32 Floor = 0;
	FString Building;
	ULanessaV2Widget::GetOwnFloorAndBuilding(Marker, Floor, Building);
	if (Building.IsEmpty()) { return; }
	if (const FStrProperty* Prop = FindFProperty<FStrProperty>(PC->GetClass(), TEXT("CurrentQirqimBuilding")))
	{
		Prop->SetPropertyValue_InContainer(PC, Building);
	}
}

bool ULanessaV2Widget::RemoteSelectBuilding(const FString& MarkerId)
{
	AActor* Marker = LanessaFindActor(MarkerId);
	if (!Marker) { return false; }
	CallPOISelectPOI(Marker);
	return true;
}

bool ULanessaV2Widget::RemoteApplySection(const FString& MarkerId)
{
	AActor* Marker = LanessaFindActor(MarkerId);
	if (!Marker) { return false; }

	// Qavat ikonkasini ekranda bosish bilan AYNAN bir xil yo'l: markerning o'z
	// Select_POI i qirqimni qo'llaydi, kamerani qavat kamerasiga uchiradi va qavat
	// ikonkalarini ko'rsatadi. Avval bu yerda faqat MPC yozilardi - qirqim bo'lardi,
	// lekin kamera joyida qolar va ikonkalar chiqmasdi. O'lchangan: Select_POI yozgan
	// MPC (Bounds 4112.66, Rotation_Z 0.1595) pastdagi zaxira yo'l bilan bir xil.
	if (Marker->FindFunction(TEXT("Select_POI")))
	{
		CallPOISelectPOI(Marker);
		LanessaRememberQirqimBuilding(Marker);
		return true;
	}

	// Zaxira: Select_POI si yo'q marker (boshqa loyiha) - qirqimni o'zimiz qo'llaymiz.
	FVector C, E;
	double Y;
	if (!LanessaMarkerBox(Marker, C, E, Y)) { return false; }
	if (!ApplySectionBox(C, E, Y)) { return false; }
	LanessaRememberQirqimBuilding(Marker);
	return true;
}

bool ULanessaV2Widget::ApplySectionBox(FVector C, FVector E, double Y)
{
	UWorld* World = LanessaGameWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC) { return false; }
	// O'yinning o'z yo'lidan boramiz: SectionView_Mask MPC ni yozadi va o'tishni
	// animatsiya qiladi. MPC ni to'g'ridan yozish o'sha animatsiyani chetlab o'tardi.
	UFunction* Fn = PC->FindFunction(LanessaCfg().SectionMaskFunction);
	if (!Fn) { return false; }

	uint8* Buf = (uint8*)FMemory_Alloca(FMath::Max<int32>(Fn->ParmsSize, 1));
	FMemory::Memzero(Buf, Fn->ParmsSize);
	for (TFieldIterator<FProperty> P(Fn); P && (P->PropertyFlags & CPF_Parm); ++P)
	{
		const FString N = P->GetName();
		if (N.Equals(TEXT("Location_New"), ESearchCase::IgnoreCase))
		{
			const FLinearColor V((float)C.X, (float)C.Y, (float)C.Z, 1.f);
			P->CopyCompleteValue(P->ContainerPtrToValuePtr<void>(Buf), &V);
		}
		else if (N.Equals(TEXT("Bounds_New"), ESearchCase::IgnoreCase))
		{
			// BP_FloorSectionMarker BoxExtent ni 2 ga ko'paytirib uzatadi (MF_SectionMask
			// Bounds ni to'liq o'lcham deb o'qiydi). Avval extent o'zi yuborilardi - paneldan
			// qo'llangan quti ekrandagi tugmanikidan 2 barobar kichik chiqardi.
			// O'lchangan: extent 2056.33 -> BP yozgan Bounds 4112.66.
			const FLinearColor V((float)(E.X * 2.0), (float)(E.Y * 2.0), (float)(E.Z * 2.0), 1.f);
			P->CopyCompleteValue(P->ContainerPtrToValuePtr<void>(Buf), &V);
		}
		else if (N.Equals(TEXT("Rotation_New"), ESearchCase::IgnoreCase))
		{
			// MPC dagi Rotation_Z gradus emas, AYLANISH (0..1): material uni RotateAboutAxis ga
			// beradi. BP_FloorSectionMarker ham ClampAxis(Yaw)/360 yuboradi. Avval gradus
			// o'zi ketardi - 57.4 gradusli bino uchun material 57.4 aylanish, ya'ni 144 gradus
			// burardi va qirqim qutisi binodan qiyshayib chiqardi.
			// O'lchangan: yaw 57.404 -> BP yozgan Rotation_Z 0.1595.
			const double Turns = FRotator::ClampAxis(Y) / 360.0;
			if (FDoubleProperty* DP = CastField<FDoubleProperty>(*P))
			{
				DP->SetPropertyValue(DP->ContainerPtrToValuePtr<void>(Buf), Turns);
			}
			else if (FFloatProperty* FP = CastField<FFloatProperty>(*P))
			{
				FP->SetPropertyValue(FP->ContainerPtrToValuePtr<void>(Buf), (float)Turns);
			}
		}
	}
	PC->ProcessEvent(Fn, Buf);
	return true;
}

bool ULanessaV2Widget::RemoteTouchRotate(double X, double Y)
{
	APawn* Pawn = LanessaExplorerPawn();
	if (!Pawn) { return false; }
	UFunction* Fn = Pawn->FindFunction(LanessaCfg().TouchRotationFunction);
	if (!Fn) { return false; }

	uint8* Buf = (uint8*)FMemory_Alloca(FMath::Max<int32>(Fn->ParmsSize, 1));
	FMemory::Memzero(Buf, Fn->ParmsSize);
	for (TFieldIterator<FProperty> P(Fn); P && (P->PropertyFlags & CPF_Parm); ++P)
	{
		const FString N = P->GetName();
		const double V = N.Equals(TEXT("Location_X"), ESearchCase::IgnoreCase) ? X
		               : N.Equals(TEXT("Location_Y"), ESearchCase::IgnoreCase) ? Y
		               : TNumericLimits<double>::Lowest();
		if (V == TNumericLimits<double>::Lowest()) { continue; }
		if (FDoubleProperty* DP = CastField<FDoubleProperty>(*P))
		{
			DP->SetPropertyValue(DP->ContainerPtrToValuePtr<void>(Buf), V);
		}
		else if (FFloatProperty* FP = CastField<FFloatProperty>(*P))
		{
			FP->SetPropertyValue(FP->ContainerPtrToValuePtr<void>(Buf), (float)V);
		}
	}
	Pawn->ProcessEvent(Fn, Buf);

	// Barmoq boshqarayotgan ekan, attract-aylanish aralashmasin.
	LanessaWriteBool(Pawn, LanessaCfg().IdleFlag, false);
	return true;
}

bool ULanessaV2Widget::RemoteCameraMove(FVector Pivot, double ArmLength)
{
	APawn* Pawn = LanessaExplorerPawn();
	if (!Pawn) { return false; }

	double ArmMin = 0.0, ArmMax = 1000000.0;
	LanessaReadDouble(Pawn, LanessaCfg().ArmLengthMin, ArmMin);
	LanessaReadDouble(Pawn, LanessaCfg().ArmLengthMax, ArmMax);
	ArmLength = FMath::Clamp(ArmLength, FMath::Min(ArmMin, ArmMax), FMath::Max(ArmMin, ArmMax));

	Pawn->SetActorLocation(Pivot);
	if (USpringArmComponent* Arm = Pawn->FindComponentByClass<USpringArmComponent>())
	{
		Arm->TargetArmLength = (float)ArmLength;
	}
	// Pawn ning o'z joy hisobini ham yangilaymiz, aks holda keyingi Focus eski
	// joyga qaytaradi. Burilish o'zgaruvchilariga ATAYLAB tegmaymiz.
	for (TFieldIterator<FProperty> It(Pawn->GetClass()); It; ++It)
	{
		const FString N = It->GetName();
		if (N.Equals(TEXT("Location_New"), ESearchCase::IgnoreCase) ||
		    N.Equals(TEXT("Location_Current"), ESearchCase::IgnoreCase))
		{
			if (FStructProperty* SP = CastField<FStructProperty>(*It))
			{
				SP->Struct->CopyScriptStruct(SP->ContainerPtrToValuePtr<void>(Pawn), &Pivot);
			}
		}
		else if (N.Equals(TEXT("TargetArmLength_New"), ESearchCase::IgnoreCase))
		{
			if (FDoubleProperty* DP = CastField<FDoubleProperty>(*It)) { DP->SetPropertyValue_InContainer(Pawn, ArmLength); }
			else if (FFloatProperty* FP = CastField<FFloatProperty>(*It)) { FP->SetPropertyValue_InContainer(Pawn, (float)ArmLength); }
		}
	}
	LanessaWriteBool(Pawn, LanessaCfg().IdleFlag, false);
	return true;
}

// ==== Burilish, harakat, kategoriyalar, xonalar ==============================

// Burilish. Absolyut burchak qo'yib bo'lmaydi (spring arm da
// use_pawn_control_rotation=True), shuning uchun kontroller kirishini suramiz.
// DIQQAT: qiymat masshtablanmaydi - kirish kadrga bir marta yig'iladi va
// cheklanadi (o'lchangan: Val=3 va Val=300 bir xil ~1.3 gradus beradi).
// Ya'ni tezlik CHAQIRUVLAR SONIGA bog'liq: sekundiga 30 ta ~40 gradus/sek.
bool ULanessaV2Widget::RemoteRotate(double YawDir, double PitchDir)
{
	APawn* Pawn = LanessaExplorerPawn();
	if (!Pawn) { return false; }
	if (!FMath::IsNearlyZero(YawDir))
	{
		Pawn->AddControllerYawInput(YawDir > 0.0 ? 1.0f : -1.0f);
	}
	if (!FMath::IsNearlyZero(PitchDir))
	{
		Pawn->AddControllerPitchInput(PitchDir > 0.0 ? 1.0f : -1.0f);
	}
	LanessaWriteBool(Pawn, LanessaCfg().IdleFlag, false);
	return true;
}

// Jostik harakati: kamera yo'nalishiga NISBATAN suradi. Pawn ning o'z
// MoveForward i yaramaydi - u juda kuchli (o'lchangan: 20 chaqiruvda pawn
// 4 million birlik uchib ketdi), chunki kadr vaqtiga bog'lanmagan.
bool ULanessaV2Widget::RemoteMoveRelative(double Forward, double Right, double Up, double Speed)
{
	APawn* Pawn = LanessaExplorerPawn();
	if (!Pawn) { return false; }

	// Jostik "oldinga" ni ekranda ko'rinayotgan yo'nalish deb tushunishi kerak, ya'ni
	// boshqaruvchining burilishidan olinadi. Avval bu spring arm dan o'qilardi va u
	// kadrdan 130 gradusgacha farq qilishi o'lchangan - jostik yon tomonga surar edi.
	double Yaw = 0.0;
	if (AController* C = Pawn->GetController())
	{
		Yaw = C->GetControlRotation().Yaw;
	}
	else
	{
		Yaw = Pawn->GetActorRotation().Yaw;
	}
	const double R = FMath::DegreesToRadians(Yaw);
	const FVector Fwd(FMath::Cos(R), FMath::Sin(R), 0.0);
	const FVector Rgt(-FMath::Sin(R), FMath::Cos(R), 0.0);

	const FVector Delta = (Fwd * Forward + Rgt * Right + FVector(0, 0, Up)) * Speed;
	Pawn->SetActorLocation(Pawn->GetActorLocation() + Delta);

	// Pawn ning o'z joy hisobini ham yangilaymiz, aks holda keyingi Focus
	// eski joyga qaytaradi.
	const FVector NewLoc = Pawn->GetActorLocation();
	for (TFieldIterator<FProperty> It(Pawn->GetClass()); It; ++It)
	{
		const FString N = It->GetName();
		if (N.Equals(TEXT("Location_New"), ESearchCase::IgnoreCase) ||
		    N.Equals(TEXT("Location_Current"), ESearchCase::IgnoreCase))
		{
			if (FStructProperty* SP = CastField<FStructProperty>(*It))
			{
				SP->Struct->CopyScriptStruct(SP->ContainerPtrToValuePtr<void>(Pawn), &NewLoc);
			}
		}
	}
	LanessaWriteBool(Pawn, LanessaCfg().IdleFlag, false);
	return true;
}

// Kategoriyalar sahnadagi POI teglaridan topiladi. Ularni qattiq yozib bo'lmaydi:
// Atrofi panelida teglar INGLIZCHA (Education, Dining, Transportation, Shopping),
// Qulayliklarda esa O'ZBEKCHA (Ko'ngilochar, Transport, Xizmatlar) - loyihadan
// loyihaga o'zgaradi. PanelTag: "Surroundings" yoki "Amenities".
void ULanessaV2Widget::RemoteListCategories(const FString& PanelTag,
                                            TArray<FString>& OutTags, TArray<int32>& OutCounts)
{
	OutTags.Reset();
	OutCounts.Reset();
	UWorld* World = LanessaGameWorld();
	if (!World || PanelTag.IsEmpty()) { return; }
	const FName PanelName(*PanelTag);

	TMap<FString, int32> Counts;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (!IsValid(A) || !A->ActorHasTag(PanelName)) { continue; }
		for (const FName& T : A->Tags)
		{
			if (T == PanelName) { continue; }
			// Ikkinchi panel tegini kategoriya deb hisoblamaymiz.
			if (T == FName(TEXT("Surroundings")) || T == FName(TEXT("Amenities"))) { continue; }
			Counts.FindOrAdd(T.ToString())++;
		}
	}
	Counts.KeySort([](const FString& L, const FString& R) { return L < R; });
	for (const TPair<FString, int32>& P : Counts)
	{
		OutTags.Add(P.Key);
		OutCounts.Add(P.Value);
	}
}

void ULanessaV2Widget::RemoteListCategoryPois(const FString& PanelTag, const FString& CategoryTag,
                                              TArray<FString>& OutIds, TArray<FString>& OutNames)
{
	OutIds.Reset();
	OutNames.Reset();
	UWorld* World = LanessaGameWorld();
	if (!World) { return; }
	const FName PanelName(*PanelTag);
	const FName CatName(*CategoryTag);
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (!IsValid(A)) { continue; }
		if (!PanelTag.IsEmpty() && !A->ActorHasTag(PanelName)) { continue; }
		if (!CategoryTag.IsEmpty() && !A->ActorHasTag(CatName)) { continue; }
		if (!A->GetClass()->GetName().StartsWith(LanessaCfg().PoiClassPrefix, ESearchCase::IgnoreCase)) { continue; }
		OutIds.Add(A->GetName());
		FString N = LanessaPoiDisplayName(A);
		OutNames.Add(N.IsEmpty() ? A->GetActorNameOrLabel() : N);
	}
}

// Interyer xonalari: oqimli yuklangan ichki leveldagi PlayerStart lar.
// Ro'yxat PlayerStartTag bo'yicha tuziladi ("spalniy", "zal" kabi).
// Interyer xonalari VA progulka nuqtalari - bitta funksiya, farqi faqat tegda.
// Bu ULanessaInteriorTourWidget/ULanessaWalkPointsWidget::PopulateFromPlayerStarts bilan
// AYNAN bir xil qoidaga bo'ysunadi, aks holda telefondagi ro'yxat ekrandagisidan farq
// qilib qolardi: filtr AKTYOR TEGI bo'yicha ("room" / "walk"), ko'rinadigan nom esa
// PlayerStartTag xossasi ("spalniy", "zal"), aktyor nomi emas.
// RequiredTag bo'sh bo'lsa hammasi olinadi - faqat shu turdagi startlari bor level uchun.
void ULanessaV2Widget::RemoteListRooms(const FString& RequiredTag,
                                       TArray<FString>& OutIds, TArray<FString>& OutNames)
{
	OutIds.Reset();
	OutNames.Reset();
	UWorld* World = LanessaGameWorld();
	if (!World) { return; }
	const FName Want(*RequiredTag);

	// Nom bo'yicha saralaymiz: TActorIterator tartibi ro'yxatga olish tartibi bo'lib,
	// har yuklashda o'zgarishi mumkin - saralamasak telefondagi tugmalar joyini
	// almashtirib turardi.
	TArray<TPair<FString, FString>> Rows;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		APlayerStart* PS = *It;
		if (!IsValid(PS)) { continue; }
		if (!RequiredTag.IsEmpty() && !PS->ActorHasTag(Want)) { continue; }
		const FString Label = PS->PlayerStartTag.IsNone() ? FString() : PS->PlayerStartTag.ToString();
		Rows.Emplace(PS->GetName(), Label.IsEmpty() ? PS->GetActorNameOrLabel() : Label);
	}
	Rows.Sort([](const TPair<FString, FString>& L, const TPair<FString, FString>& R)
	          { return L.Value < R.Value; });
	for (const TPair<FString, FString>& P : Rows)
	{
		OutIds.Add(P.Key);
		OutNames.Add(P.Value);
	}
}

// Tanlangan PlayerStart ga o'tish. Id ham, PlayerStartTag ham qabul qilinadi.
bool ULanessaV2Widget::RemoteGotoRoom(const FString& RoomId)
{
	UWorld* World = LanessaGameWorld();
	APawn* Pawn = LanessaExplorerPawn();
	if (!World || !Pawn) { return false; }
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		APlayerStart* PS = *It;
		if (!IsValid(PS)) { continue; }
		const bool bMatch = PS->GetName() == RoomId ||
		                    (!PS->PlayerStartTag.IsNone() && PS->PlayerStartTag.ToString() == RoomId);
		if (!bMatch) { continue; }
		// TeleportTo, oddiy SetActorLocation emas: progulka pawn ida harakat komponenti
		// bor va u tezlikni saqlab qoladi - ko'chirilgandan keyin eski tezlik bilan
		// uchib ketardi. TeleportTo to'qnashuvni ham hisobga oladi.
		Pawn->TeleportTo(PS->GetActorLocation(), Pawn->GetActorRotation());
		if (UMovementComponent* MC = Pawn->FindComponentByClass<UMovementComponent>())
		{
			MC->Velocity = FVector::ZeroVector;
		}
		LanessaWriteBool(Pawn, LanessaCfg().IdleFlag, false);
		return true;
	}
	return false;
}

// Analog burilish: jostik markazdan qancha uzoq bo'lsa, shuncha tez buradi.
// RemoteRotate dan farqi - u AddControllerYawInput ga tayanadi va u qiymatga
// qarab masshtablanmaydi (o'lchangan: Val=3 va Val=300 bir xil ~1.3 gradus),
// ya'ni tezlikni faqat chaqiruvlar sonini oshirib berish mumkin edi. Bu yerda
// burchakning o'zini qo'shamiz, shuning uchun bitta chaqiruv yetarli va tezlik
// uzluksiz o'zgaradi - o'yindagi jostik shunday ishlaydi.
// Yozishda BARCHA uchta manba birga yangilanadi (boshqaruvchi + _Current + _New),
// chunki pawn har kadrda o'z _Current/_New qiymatlariga qaytarib tortadi va
// faqat boshqaruvchini yozsak burilish orqaga sirg'alib ketardi.
bool ULanessaV2Widget::RemoteRotateBy(double YawDelta, double PitchDelta)
{
	APawn* Pawn = LanessaExplorerPawn();
	if (!Pawn) { return false; }
	AController* C = Pawn->GetController();
	if (!C) { return false; }

	const FRotator Cur = C->GetControlRotation().GetNormalized();
	double NewYaw   = Cur.Yaw + YawDelta;
	double NewPitch = Cur.Pitch + PitchDelta;

	// Pitch ni pawn ning o'z chegaralarida ushlaymiz, aks holda kamera tepadan
	// oshib ag'darilib ketadi.
	double PitchMin = -89.0, PitchMax = 89.0;
	LanessaReadDouble(Pawn, LanessaCfg().PitchLimitMin, PitchMin);
	LanessaReadDouble(Pawn, LanessaCfg().PitchLimitMax, PitchMax);
	NewPitch = FMath::Clamp(NewPitch, FMath::Min(PitchMin, PitchMax), FMath::Max(PitchMin, PitchMax));

	C->SetControlRotation(FRotator(NewPitch, NewYaw, 0.0));
	// FAQAT _Current. _New ga TEGMAYMIZ: ular pawn ning Focus timeline ining
	// nishonlari va har yangi qiymat animatsiyani QAYTA BOSHLAYDI. Bir marta
	// yozib ko'rildi va jostik butunlay boshqarib bo'lmas holga keldi: 1 gradus
	// so'ralganda kamera 129 gradus aylanib ketdi, pitch esa umuman teskari
	// tomonga qimirladi. Viewpointga sakrash uchun _New to'g'ri, tirik surish
	// uchun esa faqat _Current.
	LanessaWriteDouble(Pawn, LanessaCfg().PitchCurrent, NewPitch);
	LanessaWriteDouble(Pawn, LanessaCfg().YawCurrent,   NewYaw);
	LanessaWriteBool(Pawn, LanessaCfg().IdleFlag, false);
	return true;
}

// Panel sozlamalarni SHUNDAN oladi: kategoriya panellarining teglari, interyer va
// progulka teglari, karta tugmalari. Shu tufayli operator.html da birorta teg ham
// qattiq yozilmaydi - boshqa loyihada sozlama sahifasini o'zgartirish kifoya,
// panel keyingi ochilishida yangisini oladi.
FString ULanessaV2Widget::RemoteGetConfig()
{
	return LanessaCfg().ToJson();
}

// ==== POI kartasidagi amallar va galereya =====================================
// Karta tugmalari ("3D TUR" = level, "VR TUR" = level2, "REJA" = media) vidjetda
// faqat OnPoiCardAction ni uzatadi; ularni BP_Explorer_PC eshitadi. Telefon ham
// xuddi shu uzatishni qilishi kerak, boshqa yo'l bilan emas - aks holda ekrandagi
// tugma bilan telefondagi tugma har xil ish qilib qolardi.
bool ULanessaV2Widget::RemoteCardAction(const FString& ActionId)
{
	ULanessaV2Widget* W = GetLiveWidget();
	if (!W || ActionId.IsEmpty()) { return false; }
	W->OnPoiCardAction.Broadcast(ActionId);
	return true;
}

// Jonli BP_Gallery_Widget nusxasini topadi. Galereya C++ da emas, Blueprint da
// (BP_Gallery_Widget_C), shuning uchun uni sinf nomi bo'yicha qidiramiz.
//
// Galereya BITTA emas: menyudagi "Galereya" o'zining nusxasini ochadi, xonadon
// kartasidagi REJA esa YANA BITTA nusxa yaratadi va ikkalasi bir vaqtda ko'rinib
// turadi (o'lchangan: _C_0 da 8 rasm "IMG_8910", _C_1 da 7 rasm "plan"). Avval
// bu yerda TObjectIterator topgan birinchisi olinardi - tartibi esa tasodifiy,
// shuning uchun panel ba'zan REJA o'rniga menyu galereyasini boshqarib qo'yardi.
//
// To'g'ri tanlov - ekranda USTIDA turgani, ya'ni eng OXIRGI ochilgani. Obyekt
// nomidagi raqam (BP_Gallery_Widget_C_0 / _1) UE ning yaratilish hisoblagichi,
// shuning uchun ko'rinadiganlar orasidan eng kattasi olinadi. REJA yopilsa u
// Collapsed bo'ladi va tanlov o'z-o'zidan menyu galereyasiga qaytadi.
static UUserWidget* LanessaGalleryWidget()
{
	UWorld* World = LanessaGameWorld();
	if (!World) { return nullptr; }

	UUserWidget* Best = nullptr;
	int32 BestRank = -1;
	UUserWidget* AnyHidden = nullptr;
	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		UUserWidget* W = *It;
		if (!IsValid(W) || W->GetWorld() != World) { continue; }
		if (!W->GetClass()->GetName().StartsWith(LanessaCfg().GalleryWidgetClassPrefix, ESearchCase::IgnoreCase))
		{
			continue;
		}
		if (!W->IsVisible())
		{
			if (!AnyHidden) { AnyHidden = W; }
			continue;
		}
		const int32 Rank = W->GetFName().GetNumber();
		if (Rank > BestRank) { BestRank = Rank; Best = W; }
	}
	// Ko'rinadigani bo'lmasa ham bittasini qaytaramiz: RemoteGalleryState uni
	// "ochiq emas" deb belgilaydi, panel esa tugmalarni o'chirib qo'yadi.
	return Best ? Best : AnyHidden;
}

// Galereyadagi hodisani chaqiradi. Arg berilsa, hodisaning birinchi obyekt
// parametriga o'sha qo'yiladi.
static bool LanessaCallGalleryFn(UUserWidget* W, FName FnName, UObject* Arg = nullptr)
{
	if (!W || FnName.IsNone()) { return false; }
	UFunction* Fn = W->FindFunction(FnName);
	if (!Fn) { return false; }
	uint8* Buf = (uint8*)FMemory_Alloca(FMath::Max<int32>(Fn->ParmsSize, 1));
	FMemory::Memzero(Buf, Fn->ParmsSize);
	if (Arg)
	{
		for (TFieldIterator<FProperty> P(Fn); P && (P->PropertyFlags & CPF_Parm); ++P)
		{
			if (FObjectProperty* OP = CastField<FObjectProperty>(*P))
			{
				OP->SetObjectPropertyValue(OP->ContainerPtrToValuePtr<void>(Buf), Arg);
				break;
			}
		}
	}
	W->ProcessEvent(Fn, Buf);
	return true;
}

// GalleryPreview_Widgets massividan Index dagi preview vidjetini oladi.
static UObject* LanessaGalleryPreviewAt(UUserWidget* G, int32 Index)
{
	if (!G) { return nullptr; }
	for (TFieldIterator<FProperty> It(G->GetClass()); It; ++It)
	{
		if (!It->GetName().Equals(LanessaCfg().GalleryPreviewArray.ToString(), ESearchCase::IgnoreCase)) { continue; }
		FArrayProperty* AP = CastField<FArrayProperty>(*It);
		if (!AP) { return nullptr; }
		FObjectProperty* Inner = CastField<FObjectProperty>(AP->Inner);
		if (!Inner) { return nullptr; }
		FScriptArrayHelper H(AP, AP->ContainerPtrToValuePtr<void>(G));
		if (Index < 0 || Index >= H.Num()) { return nullptr; }
		return Inner->GetObjectPropertyValue(H.GetRawPtr(Index));
	}
	return nullptr;
}

bool ULanessaV2Widget::RemoteGalleryClose()
{
	UUserWidget* G = LanessaGalleryWidget();
	if (!G) { return false; }
	// Sozlamadagi nomlar navbat bilan sinaladi - birinchi topilgani ishlatiladi.
	for (const FName& Fn : LanessaCfg().GalleryCloseFunctions)
	{
		if (LanessaCallGalleryFn(G, Fn)) { return true; }
	}
	return false;
}

// Rasmni almashtirish: indeksni o'zgartirib, galereyaning o'z yangilash hodisasini
// chaqiramiz. Indeksni to'g'ridan-to'g'ri yozish kifoya emas - rasm faqat
// Update_Gallery ishlaganda almashadi.
bool ULanessaV2Widget::RemoteGalleryStep(int32 Delta)
{
	UUserWidget* G = LanessaGalleryWidget();
	if (!G) { return false; }

	// Sanoq FAQAT GalleryPreview_Widgets dan olinadi. Media massivi galereya
	// yopiqligida ham to'la turadi (o'lchangan: PREV=0 MEDIA=7), shuning uchun
	// undan sanasak yo'q preview ga indeks qo'yib, tugma jim o'lardi.
	int32 Count = 0, Index = 0;
	FIntProperty* IdxProp = nullptr;
	for (TFieldIterator<FProperty> It(G->GetClass()); It; ++It)
	{
		const FString N = It->GetName();
		if (N.Equals(LanessaCfg().GalleryIndexProperty.ToString(), ESearchCase::IgnoreCase))
		{
			IdxProp = CastField<FIntProperty>(*It);
			if (IdxProp) { Index = IdxProp->GetPropertyValue_InContainer(G); }
		}
		else if (N.Equals(LanessaCfg().GalleryPreviewArray.ToString(), ESearchCase::IgnoreCase))
		{
			if (FArrayProperty* AP = CastField<FArrayProperty>(*It))
			{
				FScriptArrayHelper H(AP, AP->ContainerPtrToValuePtr<void>(G));
				Count = H.Num();
			}
		}
	}
	if (!IdxProp || Count <= 0) { return false; }

	// Aylanma: oxirgidan keyin birinchisiga qaytadi, chunki panelda "oxiri" degan
	// holat ko'rsatilmaydi va tugma o'lik bo'lib qolmasligi kerak.
	const int32 NewIndex = ((Index + Delta) % Count + Count) % Count;

	// Yangilash hodisasi PREVIEW VIDJETINI parametr sifatida oladi, indeksni emas:
	//   Update_Gallery_Event(UBP_Gallery_Preview_Widget_C BP_Gallery_Preview)
	// Avval bu yerga bo'sh (null) parametr yuborilardi - indeks o'zgarardi-yu
	// katta rasm joyida qolaverardi.
	UObject* Preview = LanessaGalleryPreviewAt(G, NewIndex);
	if (!Preview) { return false; }          // indeksga TEGMAYMIZ, aks holda adashadi

	IdxProp->SetPropertyValue_InContainer(G, NewIndex);

	// Blueprint da bir xil imzoli IKKI nom bor (Update_Gallery_Event va
	// UpdateGallery_Event). Qaysi biri haqiqiy ishlovchi ekani tashqaridan
	// bilinmaydi - biri topilgani bilan rasm almashmagani o'lchangan. Shuning
	// uchun ikkalasi ham chaqiriladi; bo'shi zarar qilmaydi.
	bool bAny = false;
	for (const FName& Fn : LanessaCfg().GalleryUpdateFunctions)
	{
		// HAMMASI chaqiriladi, birinchi topilganda to'xtamaydi.
		bAny |= LanessaCallGalleryFn(G, Fn, Preview);
	}
	return bAny;
}

// Panel tugmalarni faol/o'lik qilib ko'rsatishi uchun: galereya ochiqmi, nechanchi
// rasmda turibdi.
bool ULanessaV2Widget::RemoteGalleryState(int32& OutIndex, int32& OutCount)
{
	OutIndex = 0;
	OutCount = 0;
	UUserWidget* G = LanessaGalleryWidget();
	if (!G) { return false; }

	// "Ochiq" = KO'RINADIGAN. Vidjet yopilgandan keyin ham mavjud bo'lib turadi
	// (o'lchangan: yopilgach Visibility=Collapsed, Media hamon 7 ta), shuning
	// uchun faqat mavjudligiga qarab "ochiq" desak, panel tugmalarni behuda
	// yoqib qo'yardi.
	if (!G->IsVisible()) { return false; }

	for (TFieldIterator<FProperty> It(G->GetClass()); It; ++It)
	{
		const FString N = It->GetName();
		if (N.Equals(LanessaCfg().GalleryIndexProperty.ToString(), ESearchCase::IgnoreCase))
		{
			if (FIntProperty* IP = CastField<FIntProperty>(*It))
			{
				OutIndex = IP->GetPropertyValue_InContainer(G);
			}
		}
		else if (N.Equals(LanessaCfg().GalleryPreviewArray.ToString(), ESearchCase::IgnoreCase))
		{
			if (FArrayProperty* AP = CastField<FArrayProperty>(*It))
			{
				FScriptArrayHelper H(AP, AP->ContainerPtrToValuePtr<void>(G));
				OutCount = H.Num();
			}
		}
	}
	return true;
}
