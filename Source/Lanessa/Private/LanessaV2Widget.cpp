#include "LanessaV2Widget.h"
#include "LanessaDayNight.h"
#include "LanessaDayNightSettings.h"
#include "LanessaWalkPointsWidget.h"
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
#include "Kismet/GameplayStatics.h"
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
	if (!FloorRailRow.IsValid()) { return; }

	FloorRailRow->ClearChildren();
	using namespace V2;
	for (int32 Floor : AvailableFloors)
	{
		TAttribute<FLinearColor> BgColor = TAttribute<FLinearColor>::Create([this, Floor]() { return SelectedFloor == Floor ? V2::Olive : V2::Transparent; });
		TAttribute<FSlateColor> TxtColor = TAttribute<FSlateColor>::Create([this, Floor]() { return FSlateColor(SelectedFloor == Floor ? V2::IconInk : V2::TextDim); });
		FloorRailRow->AddSlot().AutoWidth()
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

static bool LanessaReadDouble(AActor* Actor, const TCHAR* Prefix, double& Out);

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

TSharedRef<SWidget> ULanessaV2Widget::RebuildWidget()
{
	using namespace V2;

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

		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(0.f, 36.f, 48.f, 0.f)[BuildChipCard()]

		// NOTE: v2 mockup's .tour-start (play button + pulsing ring) is not drawn here - confirmed
		// with the user (2026-07-18) it has no equivalent in the real ArchVizExplorer product and was
		// only ever a decorative mockup element, not real functionality worth porting.

		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, 40.f)
		[
			SNew(SBox).Visibility(TAttribute<EVisibility>::Create([this]() { return ActiveView == TEXT("qirqim") ? EVisibility::Visible : EVisibility::Collapsed; }))
			[BuildFloorRail()]
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

// Re-shows the floor icons of the building the user was cutting - see the definition, below, for why
// resetting the cut is not enough on its own.
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
				// Undoing the cut leaves the floor icons hidden - restore them too, see the function's comment.
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
TSharedRef<SWidget> ULanessaV2Widget::BuildUtility()
{
	using namespace V2;
	using namespace LanessaIcons;
	auto Btn = [](const TArray<FLanessaIconPrim>& Icon)
	{
		return SNew(SBox).WidthOverride(50.f).HeightOverride(50.f)
		[
			SNew(SLanessaCutBorder).CutSize(0.f).FillColor(PanelSolid)
			.HoverColor(FLinearColor(0.078f, 0.078f, 0.063f, 1.f)) // #141410
			.bAnimateHover(true)
			.Content()
			[
				SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
				[SNew(SLanessaLineIcon).Primitives(Icon).IconSize(16.f).StrokeWidth(1.3f).StrokeColor(FLinearColor(Paper.R,Paper.G,Paper.B,0.8f))]
			]
		];
	};
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.f,0.f,1.f,0.f)[Btn(UtilitySettings())]
		+ SHorizontalBox::Slot().AutoWidth()[Btn(Exit())];
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

// Reads a double/float property by name prefix - BP-generated variables can carry a GUID suffix, so
// prefix matching is the same approach BuildPoiInfoText and GetPOITypeAsInt already use here.
static bool LanessaReadDouble(AActor* Actor, const TCHAR* Prefix, double& Out)
{
	if (!Actor) { return false; }
	for (TFieldIterator<FProperty> It(Actor->GetClass()); It; ++It)
	{
		if (!It->GetName().StartsWith(Prefix, ESearchCase::IgnoreCase)) { continue; }
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
		if (!It->GetName().StartsWith(TEXT("POI_Info_Struct"), ESearchCase::IgnoreCase)) { continue; }
		const FStructProperty* InfoProp = CastField<FStructProperty>(*It);
		if (!InfoProp) { continue; }
		const void* InfoPtr = InfoProp->ContainerPtrToValuePtr<void>(POIActor);
		for (TFieldIterator<FProperty> F(InfoProp->Struct); F; ++F)
		{
			if (!F->GetName().StartsWith(TEXT("Name"), ESearchCase::IgnoreCase)) { continue; }
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
 * CHIQISH has to undo two separate things, and only one of them is anyone's job today. The cut is
 * reset by BP_Explorer_PC answering Reset_SectionView. The floor icons are not: they are
 * BP_FloorSectionMarker actors whose show/hide lives entirely in BP_Explorer_PC's nav handlers, so
 * cutting a 22-floor tower at floor 9 hides every icon above 9 and nothing brings them back -
 * CHIQISH deliberately does not navigate, so OnNavClicked never fires.
 *
 * What C++ can do is repeat the one action that already restores them: Select_POI on the building
 * marker, exactly as clicking that building's own 3D icon does. Which building that is comes from
 * BP_Explorer_PC's CurrentQirqimBuilding, read by name for the same reason every other Blueprint
 * value here is. It has to be that one building: a level holds several towers, and restoring all of
 * them would light up icons for buildings the user never opened.
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
		// Named rather than linked, so a rename shows up here instead of at compile time.
		UE_LOG(LogTemp, Warning, TEXT("[LanessaQirqim] %s has no CurrentQirqimBuilding - floor icons cannot be restored"),
			*PC->GetClass()->GetName());
		return;
	}

	const FString TargetBuilding = Prop->GetPropertyValue_InContainer(PC);
	if (TargetBuilding.IsEmpty())
	{
		// Either no building was opened - in which case no icon was hidden - or BP_Explorer_PC never
		// wrote the variable when the building icon was clicked. The log tells the two apart.
		UE_LOG(LogTemp, Warning, TEXT("[LanessaQirqim] CurrentQirqimBuilding is empty - nothing restored"));
		return;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor) || !Actor->GetClass()->GetName().StartsWith(TEXT("BP_BuildingSectionMarker"))) { continue; }

		int32 Floor = 0;
		FString Building;
		ULanessaV2Widget::GetOwnFloorAndBuilding(Actor, Floor, Building);
		if (!Building.Equals(TargetBuilding, ESearchCase::IgnoreCase)) { continue; }

		ULanessaV2Widget::CallPOISelectPOI(Actor);
		UE_LOG(LogTemp, Log, TEXT("[LanessaQirqim] restored building '%s' via %s"), *TargetBuilding, *Actor->GetName());
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[LanessaQirqim] no BP_BuildingSectionMarker matches '%s'"), *TargetBuilding);
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
		if (!IsValid(A) || !A->GetClass()->GetName().StartsWith(TEXT("BP_POI"), ESearchCase::IgnoreCase)) { continue; }
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
	LanessaReadDouble(Pawn, TEXT("PitchLimit_Min"), PitchMin);
	LanessaReadDouble(Pawn, TEXT("PitchLimit_Max"), PitchMax);
	LanessaReadDouble(Pawn, TEXT("SpringArm_Length_Min"), ArmMin);
	LanessaReadDouble(Pawn, TEXT("SpringArm_Length_Max"), ArmMax);
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

	Pawn->SetActorLocation(Pivot);
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

bool ULanessaV2Widget::RemoteCameraGet(FVector& Pivot, double& Pitch, double& Yaw, double& ArmLength)
{
	Pivot = FVector::ZeroVector;
	Pitch = 0.0;
	Yaw = 0.0;
	ArmLength = 0.0;

	APawn* Pawn = LanessaExplorerPawn();
	if (!Pawn) { return false; }

	Pivot = Pawn->GetActorLocation();
	// Normalized so pitch comes back as -180..180 rather than the 0..360 the controller may hold -
	// the panel does arithmetic on these and would otherwise jump a full turn mid-drag.
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
