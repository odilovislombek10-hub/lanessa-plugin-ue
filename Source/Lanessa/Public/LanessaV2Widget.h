#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LanessaV2Widget.generated.h"

class SVerticalBox;
class UUserDefinedStruct;
class UDataTable;

/**
 * Direct, section-by-section C++/Slate port of the approved "Lanessa - Master Menu HUD v2"
 * HTML/CSS mockup (published artifact 46c1b5b3-b961-4725-b312-62cb482b6140). Every static value
 * (color/position/size/icon path) is taken from that source's CSS - see the comment above each
 * block. Interaction (nav clicks switching panels, marker clicks opening the POI card, hover
 * highlight, the tour-start pulse) is implemented with real Slate state/Tick, not stubbed.
 * Kept as a separate class/file from ULanessaMasterMenuWidget so neither affects the other.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLanessaV2OnResetSectionView);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLanessaV2OnNavClicked, const FString&, ViewId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FLanessaV2OnCategoryToggled, const FString&, PanelTag, const FString&, CategoryTag, bool, bEnabled);
// Broadcast when an individual POI row (inside a category's dynamic entry list) is clicked - mirrors
// the real BP_Entry_Widget.OnReleased(Button_01) -> Select_POI(GetBP_POI) chain. PoiId is the value
// BP_Explorer_PC supplied via SetCategoryPois() for that row (its BP_POI actor's own object Name,
// stable at runtime, used purely to find the matching actor back in GI.BP_POIs - not shown on screen).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLanessaV2OnPoiEntryClicked, const FString&, PoiId);
// Broadcast when a button on the open POI card is clicked. ActionId is one of "level"/"level2"/"360"/
// "media" - mirrors the real BP_Info_Widget's Border_Level/Button_FR/Border_360/Border_MediaGallery,
// each of which does something different (OpenLevel, OpenLevel2, show 360 viewer, open media gallery).
// BP_Explorer_PC should bind this and dispatch to the real per-action logic on the selected BP_POI.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLanessaV2OnPoiCardAction, const FString&, ActionId);
// Broadcast when the Qidiruv panel's "QO'LLASH" button is clicked - MatchingPoiIds is every unit (from
// the data pushed in via SetSearchUnits) whose Surface/Price/BedroomsCount/BathroomsCount/Availability
// currently satisfy the panel's sliders/chips. BP_Explorer_PC should bind this and Show_POI every actor
// whose id is in the array, Hide_POI every other actor in GI.BP_POIs - mirrors the real product's
// Custom|Filter Show_POI/Hide_POI loop (see lanessa-real-product-behavior memory), except this actually
// evaluates every criterion the panel exposes, not just Surface.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLanessaV2OnSearchApplied, const TArray<FString>&, MatchingPoiIds);
// Broadcast whenever the weather-row chip selection changes (SetWeather). WeatherId is one of
// "quyosh"/"bulut"/"aralash"/"yomgir"/"qor" - BP_Explorer_PC should map each to the matching
// Ultra_Dynamic_Weather preset asset (/Game/UltraDynamicSky/Blueprints/Weather_Effects/Weather_Presets/
// Clear_Skies, Cloudy, Partly_Cloudy, Rain, Snow) and apply it on the level's Ultra_Dynamic_Weather actor.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLanessaV2OnWeatherChanged, const FString&, WeatherId);
// Broadcast whenever the season-row chip selection changes (SetSeason). SeasonId is one of
// "bahor"/"yoz"/"kuz"/"qish" - BP_Explorer_PC should map each to the matching Ultra_Dynamic_Weather
// season value (0=Spring, 1=Summer, 2=Autumn, 3=Winter), set Season Mode to Manual Setting, and call
// the actor's own Set Season / Update Season API. Setting the plain Season variable is NOT enough at
// runtime - UDS only recomputes Individual Seasons (and pushes UDW Seasons into its material
// parameter collection) inside Update Season, which nothing else calls during play.
// Separate from OnWeatherChanged on purpose: weather and season are independent axes, so a chip in
// one row must never silently move the other.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLanessaV2OnSeasonChanged, const FString&, SeasonId);
// Broadcast whenever the time-of-day track is dragged (SetTimePct). Pct is 0-1 across the track's
// 06:00-22:00 display range (see BuildChipCard's "06:00"/"14:00"/"22:00" labels) - BP_Explorer_PC should
// convert to hours (6 + Pct*16) and set it on the level's Ultra_Dynamic_Sky actor's "Time of Day".
// double, not float - a Blueprint-authored handler function's "Float" param reflects as an
// FDoubleProperty in this engine version, so the delegate's own param must match exactly (FFloatProperty
// vs FDoubleProperty are NOT considered signature-compatible by set_create_event_function/CreateDelegate).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLanessaV2OnTimePctChanged, double, Pct);
// Broadcast from SelectFloor every time a floor-rail number is clicked (the .floor-rail row shown in
// the Qirqim side panel) - previously SelectFloor only updated the highlighted number, with no
// functional effect at all. BP_Explorer_PC should bind this and find/Select_POI the matching
// BP_FloorSectionMarker (by its own Floor, via GetOwnFloorAndBuilding), exactly like clicking that
// marker's 3D floor icon does - the two entry points are meant to behave identically.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLanessaV2OnFloorSelected, int32, Floor);

// One row of real DataTable-backed filter data for the Qidiruv panel, pushed in via SetSearchUnits.
// PoiId is the same "actor's own object Name" convention used everywhere else in this API (see
// FLanessaV2OnPoiEntryClicked's comment) - not shown on screen, only used to report matches back out.
struct FLanessaV2SearchUnit
{
	FString PoiId;
	int32 Surface = 0;
	int32 Bedrooms = 0;
	int32 Bathrooms = 0;
	FString Availability; // one of "Sotilmagan"/"Band Qilingan"/"Sotilgan" - exact POI_Filter_Struct enum value text
	int32 Price = 0;
};

// One category row's static definition (tag/label/default toggle state) for BuildSidePanelCatListContent.
// The actual POI entries under it are NOT part of this - they're pushed in later, dynamically, via
// SetCategoryPois(), and render as an empty list until then (no POI tagged = no rows, not a placeholder).
struct FLanessaV2CategoryDef
{
	FString Tag;
	FString Label;
	bool bDefaultOn = true;
};

UCLASS()
class LANESSA_API ULanessaV2Widget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Lanessa")
	static ULanessaV2Widget* CreateForTest(UObject* WorldContextObject);

	// One-off content-authoring tool: adds a new Integer field to an existing UserDefinedStruct (e.g. a
	// DataTable's row struct) and gives it the requested display name. Not exposed anywhere else -
	// FStructureEditorUtils (the only engine API that can do this) is UnrealEd-only and has no Python/
	// Blueprint-node wrapper. Editor-only (no-ops outside WITH_EDITOR); call once via execute_python_script
	// or a throwaway Blueprint node, not meant for runtime/gameplay use.
	UFUNCTION(BlueprintCallable, Category = "Lanessa|EditorTools")
	static bool AddIntFieldToStruct(UUserDefinedStruct* Struct, const FString& FieldName);

	// Debug/verification helper for the above - lists every field currently on a UserDefinedStruct via
	// raw TFieldIterator, bypassing any Blueprint-node-spawner caching that might lag behind real state.
	UFUNCTION(BlueprintCallable, Category = "Lanessa|EditorTools")
	static TArray<FString> GetStructFieldNames(UUserDefinedStruct* Struct);

	// Re-broadcasts FStructureEditorUtils::OnStructureChanged without adding a field - use this to force
	// the Blueprint editor's node-spawner cache to refresh for a struct already edited (e.g. via a prior
	// AddIntFieldToStruct call whose own OnStructureChanged call didn't fully propagate).
	UFUNCTION(BlueprintCallable, Category = "Lanessa|EditorTools")
	static void RefreshStructActions(UUserDefinedStruct* Struct);

	// DataTable.RowStruct is read-only via Python reflection (no exposed setter) - this bypasses that by
	// writing the UPROPERTY directly from C++. DELIBERATELY also empties the table (EmptyTable()): once
	// RowStruct changes, all existing row memory is laid out for the OLD struct and unsafe to read/write
	// through the new one - the caller must re-populate every row from a backup after calling this.
	UFUNCTION(BlueprintCallable, Category = "Lanessa|EditorTools")
	static bool SetDataTableRowStructAndClear(UDataTable* Table, UScriptStruct* NewRowStruct);

	// Bound by BP_Explorer_PC (AssignDelegate on the old BP_MasterMenu_Widget's "Reset_SectionView"
	// dispatcher) to trigger its section-view cutaway reset. Property name must stay exactly
	// "Reset_SectionView" - Blueprint's AssignDelegate binds to it by that name. Nothing in the v2
	// Slate UI currently broadcasts this (v2 has no section-view control yet); the hook exists so
	// BP_Explorer_PC keeps compiling and is ready to wire up if/when v2 gets that control.
	UPROPERTY(BlueprintAssignable, Category = "Lanessa")
	FLanessaV2OnResetSectionView Reset_SectionView;

	// Broadcast from SetActiveView with the clicked nav button's view id ("home","atrofi","qulay",
	// "qidiruv","qirqim","galereya","none","vr","sayr","interyer") every time it's called, including
	// repeat clicks on the already-active view (matches the real BP_MasterMenu_Widget: OnClicked
	// always re-runs its full camera/visibility logic, it does not early-out on same-view). v2's own
	// Slate side only handles which side-panel is visible; the real per-button game logic (camera
	// move via BP_Explorer_Pawn.Focus(), POI/Route show-hide, ResetSectionView, closing info/gallery)
	// lives in BP_Explorer_PC, bound to this delegate - see BP_Explorer_PC's Create_MasterMenu event.
	UPROPERTY(BlueprintAssignable, Category = "Lanessa")
	FLanessaV2OnNavClicked OnNavClicked;

	// Broadcast when a category row in the Atrofi/Qulayliklar side-panel is toggled on/off. PanelTag
	// is the panel-level marker tag ("Surroundings" for Atrofi, "Amenities" for Qulayliklar - matches
	// the real product's own two-tag POI convention, see BP_Explorer_PC's real Button_Surroundings/
	// Button_Amenities investigation), CategoryTag is the specific category (a sanitized version of
	// the displayed Uzbek label, e.g. "Maktablar", "Basseyn" - single source of truth, no separate
	// translation table to drift out of sync). BP_Explorer_PC should bind to this and call the real
	// Enable_POI/Disable_POI + Enable_Route/Disable_Route on every BP_POI/BP_Route actor that has BOTH
	// tags (ActorHasTag(PanelTag) AND ActorHasTag(CategoryTag)).
	UPROPERTY(BlueprintAssignable, Category = "Lanessa")
	FLanessaV2OnCategoryToggled OnCategoryToggled;

	UPROPERTY(BlueprintAssignable, Category = "Lanessa")
	FLanessaV2OnPoiEntryClicked OnPoiEntryClicked;

	UPROPERTY(BlueprintAssignable, Category = "Lanessa")
	FLanessaV2OnPoiCardAction OnPoiCardAction;

	UPROPERTY(BlueprintAssignable, Category = "Lanessa")
	FLanessaV2OnSearchApplied OnSearchApplied;

	UPROPERTY(BlueprintAssignable, Category = "Lanessa")
	FLanessaV2OnWeatherChanged OnWeatherChanged;

	UPROPERTY(BlueprintAssignable, Category = "Lanessa")
	FLanessaV2OnSeasonChanged OnSeasonChanged;

	UPROPERTY(BlueprintAssignable, Category = "Lanessa")
	FLanessaV2OnTimePctChanged OnTimePctChanged;

	UPROPERTY(BlueprintAssignable, Category = "Lanessa")
	FLanessaV2OnFloorSelected OnFloorSelected;

	// current .navitem "data-view" equivalent: "home","atrofi","qulay","qidiruv","qirqim","galereya","none"
	FString ActiveView = TEXT("home");
	// currently selected .marker "data-poi", empty = none (POI card closed)
	FString SelectedPoiId;
	// .floor-rail selected floor (mirrors `if (view==='qirqim') selectFloor(9)`)
	int32 SelectedFloor = 9;
	// .plan-backdrop.open
	bool bPlanOpen = false;
	// Nav rail dropdown state: when false, only the "home" item is visible in BuildRail() and every
	// other nav item (atrofi/qulay/qidiruv/qirqim/galereya/vr/sayr/interyer) is Collapsed. Toggled by
	// clicking "home" itself (see BuildRail()'s per-item OnClicked), which also still calls
	// SetActiveView("home") as before - home now does both jobs. Starts closed per user's explicit
	// request (only "Bosh sahifa" visible until first clicked).
	bool bNavExpanded = false;
	// Tracks whether the "home" nav/camera action has already fired during the current open session
	// of the dropdown (reset to false whenever the dropdown re-opens from closed, or whenever a
	// different nav item navigates away from home). Lets repeated clicks on "home" while the dropdown
	// is open cycle: 1st click (dropdown closed) -> open, no nav; 2nd click (open, not yet activated)
	// -> nav fires, dropdown STAYS open; 3rd click (open, already activated) -> dropdown closes, no nav.
	bool bHomeActivatedThisSession = false;
	// .range-track (kvadratura/narx) 0-1 handle positions: {fieldId -> {a,b}}
	TMap<FString, FVector2D> RangeHandles;
	// .time-track 0-1 position (default .63 per mockup)
	float TimePct = 0.63f;
	// Real floors to show in the floor-rail, pushed in via SetAvailableFloors() - starts empty (rail
	// shows nothing) until BP_Explorer_PC populates it from the level's actual placed markers, rather
	// than the old hardcoded 12-1 placeholder range.
	TArray<int32> AvailableFloors;
	// .cat-item.on / .chip.on toggle state, keyed by "panel:label"
	TMap<FString, bool> ToggleState;
	// .weather-row button.on - which weather chip is selected (default "quyosh" per mockup)
	FString SelectedWeather = TEXT("quyosh");
	// .season-row button.on - which season chip is selected. Defaults to "yoz" (summer) because that
	// is the neutral state: the tree materials leave foliage at its authored colour when the season
	// vector reads pure summer, so an untouched panel shows the scene exactly as it was built.
	FString SelectedSeason = TEXT("yoz");
	// .tour-toast - transient notification text/timer, mirrors launchTourToast()'s 1.8s auto-hide
	FString ToastLabel;
	float ToastTimeLeft = 0.f;

	// Nav rail button labels (BuildRail()) - Blueprint-editable so content authors can retext the main
	// menu without a C++ rebuild. Defaults match the original hardcoded v2 mockup text; set these (e.g.
	// from BP_Explorer_PC, before AddToViewport) to override. Read once when BuildRail() runs.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lanessa|Nav Labels")
	FString Label_Home = TEXT("BOSH SAHIFA");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lanessa|Nav Labels")
	FString Label_Atrofi = TEXT("ATROFI");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lanessa|Nav Labels")
	FString Label_Qulay = TEXT("QULAYLIKLAR");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lanessa|Nav Labels")
	FString Label_Qidiruv = TEXT("QIDIRUV");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lanessa|Nav Labels")
	FString Label_Qirqim = TEXT("QIRQIM");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lanessa|Nav Labels")
	FString Label_Galereya = TEXT("GALEREYA");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lanessa|Nav Labels")
	FString Label_VRProgulka = TEXT("VR PROGULKA");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lanessa|Nav Labels")
	FString Label_Progulka = TEXT("PROGULKA");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lanessa|Nav Labels")
	FString Label_Interyer = TEXT("INTERYER");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lanessa|Nav Labels")
	FString Label_Qurilish = TEXT("QURILISH");

	// QURILISH panelidagi tanlangan bosqich: "kotlovan" "yerosti" "karkas" "devor" "fasad"
	// "qurilishetapi", bo'sh = hech biri. Boshqa sahifaga o'tilganda tozalanadi.
	FString ActiveBuildStage;

	// Real POI card data (POI_Info_Struct-derived), set via SetPoiData() - replaces the old mockup-only
	// Area/Beds/Baths/Price fields, which had no real-product equivalent (see lanessa-real-product-behavior
	// memory: no price field exists anywhere in the real data model).
	FString PoiName;
	FString PoiInformation;
	FString PoiFooter;
	bool bPoiHasLevel = false;
	FString PoiLevelButtonText;
	bool bPoiHasLevel2 = false;
	FString PoiLevel2ButtonText;
	bool bPoiHas360 = false;
	bool bPoiHasMedia = false;
	FString PoiMediaButtonText;

	// Apartment (POI_FILTER) spec-sheet data, read directly off the actor's own Filter struct via
	// PopulatePoiFilterFields() - drives BuildPoiCard's alternate stat-grid layout (per user's approved
	// "Xonadon N214" mockup) instead of the plain PoiInformation text every other POI type uses.
	bool bPoiIsFilterCard = false;
	int32 PoiFloor = 0;       // 0 = not yet in the DataTable (see PopulatePoiFilterFields)
	int32 PoiRoomsCount = 0;
	int32 PoiSurfaceVal = 0;
	int32 PoiBathroomsVal = 0;
	FString PoiAvailabilityShort;
	bool bPoiHasPlan = false; // true once PopulatePoiFilterFields finds a "FLOOR PLAN" Media entry

	UFUNCTION(BlueprintCallable, Category = "Lanessa")
	void SetActiveView(const FString& ViewId);

	/**
	 * QURILISH panelidagi bosqich tugmasi. StageId: "kotlovan" "yerosti" "karkas" "devor"
	 * "fasad" "qurilishetapi". Bosqich levellarini ochadi (Lanessa Level Streaming jadvali),
	 * kamerani uchiradi (Lanessa Qurilish -> Cameras); "qurilishetapi" animatsiyani boshlaydi.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lanessa")
	void SelectBuildStage(const FString& StageId);
	// BP_POI/BP_Info_Widget should call this directly instead of drilling into the old
	// BP_MasterMenu_Widget.BP_Info_Menu_01/BP_InfoGallery_Widget sub-widget references - v2 builds
	// its POI card inline via Slate (BuildPoiCard), there's no separate sub-widget to reach into.
	UFUNCTION(BlueprintCallable, Category = "Lanessa")
	void SelectPoi(const FString& PoiId);

	// Pushes the real POI_Info_Struct-derived data for the currently-selected POI into the card - call
	// this from BP_Explorer_PC/BP_POI right before/after SelectPoi(id), reading the fields off a
	// Break POI_Info_Struct node. bHasLevel/bHasLevel2/bHas360/bHasMedia mirror the real BP_Info_Widget's
	// own Update_LevelButton/Update_LevelButton_0/Update_360Button/Update_MediaButton visibility checks
	// (TextIsEmpty(Level)/IsValid(Texture_360)/etc.) - compute them in Blueprint and pass the bools
	// straight through rather than re-deriving "emptiness" here, so the one real behavior stays in sync.
	UFUNCTION(BlueprintCallable, Category = "Lanessa")
	void SetPoiData(const FString& Name, const FString& Information, const FString& Footer, UTexture2D* Image,
		bool bHasLevel, const FString& LevelButtonText, bool bHasLevel2, const FString& Level2ButtonText,
		bool bHas360, bool bHasMedia, const FString& MediaButtonText);

	void SelectFloor(int32 Floor);
	// Replaces the floor-rail's number list (previously a hardcoded 12-down-to-1 range) with the real
	// floors that actually have a placed BP_FloorSectionMarker - call from BP_Explorer_PC (e.g. in
	// Create_MasterMenu) with every marker's own Floor, sorted however they should read left-to-right.
	// Rebuilds the rail's row in place if it's already on screen (mirrors SetPanelCategories/
	// SetCategoryPois' already-built-widget-tree rebuild pattern).
	UFUNCTION(BlueprintCallable, Category = "Lanessa")
	void SetAvailableFloors(const TArray<int32>& Floors);
	void SetPlanOpen(bool bOpen);
	void SetWeather(const FString& WeatherId);
	void SetSeason(const FString& SeasonId);
	void ShowTourToast(const FString& Label);
	void ToggleChip(const FString& Key);
	bool IsToggled(const FString& Key, bool bDefault) const;
	void SetTimePct(float Pct);
	// Handle 0 = low, 1 = high; drags whichever handle is nearer to Pct.
	void SetRangeHandlePct(const FString& FieldId, float Pct);
	FVector2D GetRangeHandle(const FString& FieldId, float DefaultA, float DefaultB) const;

	// Builds (or completely replaces) the category-row list itself for the Atrofi/Qulayliklar side panel
	// identified by PanelTag ("Surroundings"/"Amenities"). CategoryTags/CategoryLabels/CategoryDefaultsOn
	// must be the same length, paired by index - Tag is the internal ActorHasTag() value, Label is what's
	// displayed. Renaming/adding/removing a category here (e.g. to mirror BP_Amenities_Widget's real
	// NameOfList values) takes effect immediately, no C++ recompile - this is the single source of truth
	// for what categories exist, not a hardcoded C++ array. Any existing category rows for this PanelTag
	// are discarded and rebuilt from scratch (their previously-set POI lists go with them - call
	// SetCategoryPois again per category after this).
	UFUNCTION(BlueprintCallable, Category = "Lanessa")
	void SetPanelCategories(const FString& PanelTag, const TArray<FString>& CategoryTags, const TArray<FString>& CategoryLabels, const TArray<bool>& CategoryDefaultsOn);

	// Populates (or replaces) the dynamic POI-entry list under one category row of the Atrofi/Qulayliklar
	// side panel. Called by BP_Explorer_PC after scanning GI.BP_POIs for actors tagged with BOTH PanelTag
	// ("Surroundings"/"Amenities") and CategoryTag. PoiIds/PoiLabels must be the same length, paired by
	// index. Calling this with empty arrays clears the category back to an empty list.
	UFUNCTION(BlueprintCallable, Category = "Lanessa")
	void SetCategoryPois(const FString& PanelTag, const FString& CategoryTag, const TArray<FString>& PoiIds, const TArray<FString>& PoiLabels);

	// Pushes the real per-unit filter data for the Qidiruv panel (one entry per placed BP_POI whose
	// RowName resolves in the level's DataTable). Call once from BP_Explorer_PC (e.g. right after
	// SetPanelCategories, in Create_MasterMenu) - all six arrays must be the same length, paired by
	// index. Replaces whatever was previously set. The panel's sliders/chips and its live "Topildi: N"
	// count read this data directly; nothing here touches the level - see OnSearchApplied for that.
	UFUNCTION(BlueprintCallable, Category = "Lanessa")
	void SetSearchUnits(const TArray<FString>& PoiIds, const TArray<int32>& Surfaces, const TArray<int32>& Bedrooms,
		const TArray<int32>& Bathrooms, const TArray<FString>& Availabilities, const TArray<int32>& Prices);

	// Same effect as SetSearchUnits, but reads Surface/BedroomsCount/BathroomsCount/Availability/narx
	// straight off Table via generic FProperty reflection instead of taking them as arrays - lets
	// BP_Explorer_PC populate the Qidiruv panel with a plain (PoiId, RowName) loop, no DataTable-Get +
	// Break-struct nodes required on the Blueprint side. Exists because the Blueprint editor's node
	// spawner cannot disambiguate two same-named UserDefinedStructs' "Break" nodes (New_Explorer's own
	// POI_Filter_Struct copy vs ArchVizExplorer's original one it was duplicated from) - it always
	// resolves to the wrong (original, narx-less) one, so building this in Blueprint isn't possible.
	// PoiIds/RowNames/POIActors must be the same length, paired by index; a RowName with no matching table
	// row is skipped (not an error - mirrors the real content gap already known to exist for some placed
	// POIs). Surface/Bedrooms/Bathrooms/narx (price) come from Table - real listings, not CSV-driven.
	// Availability comes from POIActors[i]'s own live Filter.Availability instead of Table's row, when
	// that actor is valid - the DataTable is a static snapshot (starts every unit "Sotilmagan"), while a
	// seller's CSV/Google-Sheets update only ever touches each POI actor's own Filter struct at runtime
	// (see RefreshPOIFilterColor, EventUpdate_CSV) - reading Availability from Table would keep showing a
	// sold unit as "Sotilmagan" in search forever, even after its marker has already turned red.
	UFUNCTION(BlueprintCallable, Category = "Lanessa")
	void PopulateSearchUnitsFromTable(UDataTable* Table, const TArray<FString>& PoiIds, const TArray<FName>& RowNames, const TArray<AActor*>& POIActors);

	// Sets a BP_POI actor's Holo_DynMat "Color" parameter from its own POI_Type/Filter.Availability
	// properties (read via reflection), reproducing Update_POI_Color's own NewEnumerator0/1->static,
	// NewEnumerator2(POI_FILTER)->availability-color logic (see lanessa-real-product-behavior memory)
	// entirely in C++. Exists because BP_POI's own Update_POI_Color cannot be fixed in Blueprint: its
	// POI_Type getter node is bound to a stale/wrong-class same-name-collision type no matter how it's
	// recreated (ArchVizExplorer's BP_POI vs New_Explorer's own copy), so the node graph itself can
	// never be made to read the real value. Takes a generic AActor* (not BP_POI*) specifically so a
	// Blueprint's own Self-Reference wildcard pin can feed it without hitting that same collision.
	// No-ops (returns without doing anything) if POIActor's POI_Type isn't POI_FILTER(2), or if any of
	// POI_Type/Filter/Holo_DynMat can't be found by reflection - safe to call on any actor.
	UFUNCTION(BlueprintCallable, Category = "Lanessa")
	static void RefreshPOIFilterColor(AActor* POIActor);

	// Reads a BP_POI actor's own POI_Type property (0=POI, 1=POI_CENTER, 2=POI_FILTER) as a plain int
	// via reflection - same reason as RefreshPOIFilterColor: BP_POI's own POI_Type_Enum getter nodes
	// are permanently bound to the wrong (stale/ArchVizExplorer) same-name-collision enum asset no
	// matter how they're recreated, so EventGraph's own EventBeginPlay switch on POI_Type can't be
	// fixed by rewiring Blueprint nodes - only by routing through a plain int instead.
	UFUNCTION(BlueprintCallable, Category = "Lanessa")
	static int32 GetPOITypeAsInt(AActor* POIActor);

	// Builds the POI info-card "Information" text for POIActor. For POI_FILTER(2) actors, ignores
	// FallbackInformation entirely and instead builds a Surface/Bedroom/Bathroom/Availability spec sheet
	// from the actor's own Filter struct (read via reflection - same same-name-collision reasoning as
	// RefreshPOIFilterColor/GetPOITypeAsInt), reproducing the real BP_Info_Widget's Update_Text
	// POI_Type-branched FormatText logic (see lanessa-real-product-behavior memory: real product shows a
	// completely different spec-sheet text for apartment markers than for amenity markers, not the same
	// POI_Info_Struct.Information text for both - New_Explorer's port was missing that branch entirely).
	// For any other POI_Type, or if Filter can't be found, returns FallbackInformation unchanged.
	UFUNCTION(BlueprintCallable, Category = "Lanessa")
	static FString BuildPoiInfoText(AActor* POIActor, const FString& FallbackInformation);

	// Reads Surface/Bedroomscount/Bathroomscount/Availability straight off POIActor's own Filter
	// struct (RuntimeDataTable-populated, same reflection approach as BuildPoiInfoText/GetPOITypeAsInt -
	// see same-name-collision reasoning there) into the PoiRoomsCount/PoiSurfaceVal/PoiBathroomsVal/
	// PoiAvailabilityShort/bPoiIsFilterCard fields BuildPoiCard reads for its apartment stat-grid
	// layout (per user's approved "Xonadon N214" mockup). Sets bPoiIsFilterCard=false and leaves the
	// other fields untouched for any non-POI_FILTER actor. Call from BP_POI's Select_POI right
	// alongside SetPoiData.
	// PoiFloor is the one exception: unlike Availability, which floor a unit sits on never changes
	// via a seller's live CSV sync, so it's read from Table (by POIActor's own RowName) instead of
	// off the actor's live Filter struct - there is currently no supported way to write a per-instance
	// struct-property override back onto an already-placed POI actor (ObjectTools.set_properties
	// rejects it), so Table is the only place a Floor value can practically live for existing content.
	// Table may be null (e.g. a non-POI_FILTER actor never reaches the lookup) - PoiFloor then stays 0.
	UFUNCTION(BlueprintCallable, Category = "Lanessa")
	void PopulatePoiFilterFields(AActor* POIActor, UDataTable* Table);

	// Reads Floor/Building for POIActor straight off Table (by POIActor's own RowName), same
	// reflection approach and same reason as PopulatePoiFilterFields' PoiFloor (see its comment) -
	// exists as its own function because BP_FloorSectionMarker's Select_POI override needs these
	// values without going through a Break POI_Filter_Struct node, which resolves to the wrong
	// (stale, pre-Floor/Building) same-name-collision struct copy no matter how it's spawned (see
	// unreal_mcp_workflow memory - this is the same dead end documented there for Break/Make-struct
	// nodes, just newly hit for POI_Filter_Struct specifically). OutFloor/OutBuilding are 0/empty if
	// POIActor or Table is null, or POIActor's RowName has no matching row.
	UFUNCTION(BlueprintCallable, Category = "Lanessa")
	static void GetPOIFloorAndBuilding(AActor* POIActor, UDataTable* Table, int32& OutFloor, FString& OutBuilding);

	// Convenience wrapper around GetPOIFloorAndBuilding for BP_FloorSectionMarker's Select_POI
	// override's per-candidate filter loop - avoids needing two local variables + two comparisons
	// wired per iteration in the Blueprint graph.
	UFUNCTION(BlueprintCallable, Category = "Lanessa")
	static bool DoesPOIMatchFloorBuilding(AActor* POIActor, UDataTable* Table, int32 Floor, const FString& Building);

	// Calls POIActor's own "Show_POI"/"Hide_POI" custom event via UFunction reflection (FindFunction
	// + ProcessEvent) instead of a Blueprint "Class|BPPOI|ShowPOI/HidePOI" cross-class call node -
	// those spawner strings resolve to the wrong (stale ArchVizExplorer) same-name-collision BP_POI
	// class no matter how they're created (declaring_class doesn't fix it either, unlike ordinary
	// CallFunction spawners - confirmed while wiring BP_FloorSectionMarker's Select_POI override).
	// Reflection dispatches on POIActor's ACTUAL runtime class, sidestepping the static Blueprint
	// node-type resolution entirely. No-ops if POIActor is null or has no such event (e.g. not a
	// BP_POI/subclass).
	UFUNCTION(BlueprintCallable, Category = "Lanessa")
	static void CallPOIShowHide(AActor* POIActor, bool bShow);

	// Calls POIActor's own "Select_POI" custom event via UFunction reflection - same reason and
	// pattern as CallPOIShowHide. Used by BP_3D_Widget_FloorIcon's own Button_Root OnClicked (the
	// icon widget is NOT set to HitTestInvisible like the stock BP_3D_Widget is, since it's swapped
	// in at runtime via SetWidget after EventBeginPlay's HitTestInvisible call already ran on the
	// OLD widget instance - so the icon itself intercepts the click instead of it reaching
	// POI_Geometry's mesh collision underneath. Wiring the click here directly is more robust than
	// fighting that anyway - no dependency on mesh position/occlusion at all.
	UFUNCTION(BlueprintCallable, Category = "Lanessa")
	static void CallPOISelectPOI(AActor* POIActor);

	// Sets BP_Explorer_Pawn's own Location_New/Pitch_New/Yaw_New/TargetArmLength_New (found by exact
	// name via reflection, same reason as the other Get/CallPOI* helpers - a plain Blueprint
	// "Class|BPExplorerPawn|SetLocation_New" etc. spawner can't be created via the MCP tooling, these
	// are auto-generated variable setters, not a UFUNCTION the normal spawner search can resolve) and
	// then calls the pawn's own "Focus" custom event (also via reflection) so it animates from wherever
	// it currently is to these new values, exactly like every other nav button's camera move. Used by
	// the "qirqim" nav case, which previously had no camera move of its own at all.
	UFUNCTION(BlueprintCallable, Category = "Lanessa")
	static void SetExplorerPawnCameraTarget(AActor* PawnActor, FVector Location, double Pitch, double Yaw, double TargetArmLength);

	// Generic reflection-based String variable setter for when a specific Blueprint variable's own
	// setter node (e.g. SetCurrentQirqimBuilding) refuses to spawn/connect cleanly from another
	// Blueprint's graph (a recurring MCP tooling quirk with cross-class self pins) - finds the FIRST
	// FString property on Target whose name matches VariableName (case-insensitive) and assigns Value.
	// No-ops if Target is null or has no such property.
	UFUNCTION(BlueprintCallable, Category = "Lanessa")
	static void SetActorStringVariable(AActor* Target, const FString& VariableName, const FString& Value);

	// Generic reflection-based zero-argument function/custom-event caller for the same recurring
	// self-pin-refusal quirk as SetActorStringVariable - finds Target's UFunction named FunctionName
	// (custom events included) and invokes it via ProcessEvent. No-ops if Target is null or has no
	// such function. Only for parameterless calls (matches every case this has been needed for so far,
	// e.g. Reset_SectionView) - extend with params the same way SetExplorerPawnCameraTarget does if a
	// future call needs them.
	UFUNCTION(BlueprintCallable, Category = "Lanessa")
	static void CallActorFunction(AActor* Target, const FString& FunctionName);

	// Calls FunctionName as declared on Target's IMMEDIATE PARENT class (Target->GetClass()->GetSuperClass()),
	// not on Target's own most-derived class - i.e. Super::FunctionName(), reachable from reflection. Needed
	// because BP_BuildingSectionMarker/BP_FloorSectionMarker each define their own Custom Event literally
	// named "Select_POI", which shadows BP_POI's real "Select_POI" function (the one with the Focus Settings/
	// camera-look-at logic) for any FindFunction()-based caller (e.g. CallPOISelectPOI) - the child's event
	// is always found first. This walks straight to the superclass so the parent's own implementation runs
	// instead. No-ops if Target is null, has no superclass, or the superclass has no such function.
	UFUNCTION(BlueprintCallable, Category = "Lanessa")
	static void CallParentFunction(AActor* Target, const FString& FunctionName);

	// Reads Floor/Building straight off POIActor's OWN Filter struct (not off a DataTable row, unlike
	// GetPOIFloorAndBuilding) - for a BP_FloorSectionMarker, which has no DataTable row of its own
	// (it isn't a real apartment unit), the content author sets its Filter.Floor/Filter.Building by
	// hand in the Details panel at placement time (a handful of markers, unlike the 174 real units
	// GetPOIFloorAndBuilding exists for - those can't be hand-edited one by one, see that function's
	// comment). Same reflection approach as PopulatePoiFilterFields' Surface/Bedroomscount/etc.
	// OutFloor/OutBuilding are 0/empty if POIActor is null or has no Filter struct.
	UFUNCTION(BlueprintCallable, Category = "Lanessa")
	static void GetOwnFloorAndBuilding(AActor* POIActor, int32& OutFloor, FString& OutBuilding);

	// Temporary diagnostic helper - lists every top-level FProperty name on POIActor's class, so the
	// exact (possibly GUID-suffixed) names RefreshPOIFilterColor should match against can be confirmed
	// instead of guessed. Remove once RefreshPOIFilterColor is verified working.
	UFUNCTION(BlueprintCallable, Category = "Lanessa|EditorTools")
	static TArray<FString> DebugListActorProperties(AActor* POIActor);

	// Broadcasts OnSearchApplied with the currently-matching PoiIds - same effect as clicking "QO'LLASH"
	// in the panel. The real product's Button_UnitSearch calls its own Filter() immediately on open (see
	// lanessa-real-product-behavior memory: Show_POI/Hide_POI per unit based on Surface/Budget, no wait
	// for a button); BP_Explorer_PC should call this once right after switching to the "qidiruv" nav view
	// so units are already shown/hidden correctly before the user touches a single control.
	UFUNCTION(BlueprintCallable, Category = "Lanessa")
	void ApplySearch();

	// ---- Operator remote control (phone/tablet panel over Remote Control) --------------------
	// Remote Control can only reach UFUNCTIONs, and the live widget's own object path carries a
	// GameInstance index (".BP_Explorer_GameInstance_LN_C_9.LanessaV2Widget_0") that differs on every
	// run, so nothing outside the engine can address the instance directly. Everything below is
	// therefore static: the panel calls it on the class default object, whose path
	// "/Script/Lanessa.Default__LanessaV2Widget" never changes, and each call resolves the live widget
	// itself - so the same request works unchanged in the editor, in PIE and in a packaged build.
	// Each returns false rather than silently doing nothing when no game is running, because Remote
	// Control answers HTTP 200 either way and the panel otherwise cannot tell the two apart.

	// The live (non-CDO, non-archetype) widget, preferring one that belongs to a real game/PIE world
	// over an editor preview world. Null when nothing is playing.
	UFUNCTION(BlueprintCallable, Category = "Lanessa|Remote")
	static ULanessaV2Widget* GetLiveWidget();

	// SeasonId: "bahor" "yoz" "kuz" "qish" - the same ids the season chips pass to SetSeason.
	UFUNCTION(BlueprintCallable, Category = "Lanessa|Remote")
	static bool RemoteSetSeason(const FString& SeasonId);

	// WeatherId: "quyosh" "bulut" "aralash" "yomgir" "qor".
	UFUNCTION(BlueprintCallable, Category = "Lanessa|Remote")
	static bool RemoteSetWeather(const FString& WeatherId);

	// Pct is 0-1 across the time track, the same units the widget's own drag produces.
	UFUNCTION(BlueprintCallable, Category = "Lanessa|Remote")
	static bool RemoteSetTimePct(float Pct);

	UFUNCTION(BlueprintCallable, Category = "Lanessa|Remote")
	static bool RemoteSelectFloor(int32 Floor);

	// ViewId: "home" "atrofi" "qulay" "qidiruv" "qirqim" "galereya" "vr" "sayr" "interyer", or "none"
	// to close every side panel.
	UFUNCTION(BlueprintCallable, Category = "Lanessa|Remote")
	static bool RemoteSetActiveView(const FString& ViewId);

	// Broadcasts OnPoiEntryClicked, which is what a POI row click does - BP_Explorer_PC answers it by
	// running the actor's own Select_POI and flying the camera there. Deliberately NOT SelectPoi(),
	// which only opens the card and leaves the camera where it was.
	UFUNCTION(BlueprintCallable, Category = "Lanessa|Remote")
	static bool RemoteSelectPoi(const FString& PoiId);

	UFUNCTION(BlueprintCallable, Category = "Lanessa|Remote")
	static bool RemoteResetSectionView();

	// Every POI in the live world, as parallel arrays the panel turns into a list. OutIds are the
	// actor object names RemoteSelectPoi expects; OutNames are the POI_Info_Struct display names
	// ("Shopping Mall" and so on). bFilterUnits true keeps only the apartment (POI_FILTER) markers,
	// false only the plain named viewpoints - which is what an operator showing the building wants.
	UFUNCTION(BlueprintCallable, Category = "Lanessa|Remote")
	static void RemoteListPois(bool bFilterUnits, TArray<FString>& OutIds, TArray<FString>& OutNames);

	// ---- Bino va qavat ------------------------------------------------------------------------
	// Qavat soni hech qayerda yozilmagan: panel 12 tani qattiq ko'rsatardi, bu levelda esa 3 ta
	// qavat markeri bor va boshqa loyihada 22 ta bo'lishi mumkin. Ro'yxat har doim sahnadagi
	// BP_FloorSectionMarker / BP_BuildingSectionMarker aktyorlaridan tuziladi.

	UFUNCTION(BlueprintCallable, Category = "Lanessa|Remote")
	static void RemoteListBuildings(TArray<FString>& OutIds, TArray<FString>& OutNames);

	// BuildingId bo'sh bo'lsa - hamma qavat. Aks holda faqat shu binoning qavatlari
	// (markazi bino qutisi ichida bo'lganlari). Pastdan tepaga tartiblangan, nomlari 1..N.
	UFUNCTION(BlueprintCallable, Category = "Lanessa|Remote")
	static void RemoteListFloors(const FString& BuildingId,
	                             TArray<FString>& OutIds, TArray<FString>& OutNames);

	// Markerning o'z SectionView_Volume ini qirqim qutisi qilib qo'yadi. Bu qavatni
	// raqam bilan tanlashdan aniqroq: 3 ta bino bo'lganda raqam qaysi binoniki ekani
	// noma'lum bo'lib qolardi va hammasi birdan qirqilardi.
	UFUNCTION(BlueprintCallable, Category = "Lanessa|Remote")
	static bool RemoteApplySection(const FString& MarkerId);

	// Qirqim qutisini to'g'ridan qo'llaydi - markersiz. Centre/Extent dunyo koordinatasida
	// (Extent - yarim o'lcham, GetActorBounds beradigani), Yaw gradusda. BP_Explorer_PC ning
	// SectionView_Mask i orqali, ya'ni qavat qirqimi bilan aynan bir xil animatsiya va MPC.
	// Interyer qirqimi (InteriorSection tegli volume) shundan foydalanadi.
	UFUNCTION(BlueprintCallable, Category = "Lanessa|Remote")
	static bool ApplySectionBox(FVector Centre, FVector Extent, double Yaw);

	// Bino ikonkasini ekranda bosish bilan AYNAN bir xil: markerning o'z Select_POI i
	// chaqiriladi - kamera bino kamerasiga uchadi, qavat ikonkalari chiqadi va
	// CurrentQirqimBuilding yoziladi. Avval panel bino tugmasida faqat ro'yxat so'rardi,
	// sahnada hech narsa bo'lmasdi.
	UFUNCTION(BlueprintCallable, Category = "Lanessa|Remote")
	static bool RemoteSelectBuilding(const FString& MarkerId);

	// ---- Operator camera ---------------------------------------------------------------------
	// BP_Explorer_Pawn is an orbit rig: Location_New is the pivot, Pitch_New/Yaw_New the angles,
	// TargetArmLength_New the distance. SetExplorerPawnCameraTarget above writes those four and then
	// runs the pawn's Focus timeline, which is right for jumping to a viewpoint but wrong for a live
	// finger drag - each new value restarts the animation, so the move fights itself. bAnimate=false
	// instead writes the pawn transform and spring arm directly, which is safe to send every frame.
	// Rotation goes through the controller because bUseControllerRotationYaw/Pitch overwrite the
	// pawn's own actor rotation every tick and a plain SetActorRotation would not stick. Pitch and
	// arm length are clamped to the pawn's own PitchLimit_Min/Max and SpringArm_Length_Min/Max, so
	// the operator cannot drive the camera anywhere the mouse could not.
	UFUNCTION(BlueprintCallable, Category = "Lanessa|Remote")
	static bool RemoteCameraSet(FVector Pivot, double Pitch, double Yaw, double ArmLength, bool bAnimate);

	// Attract-mode auto-rotation. RemoteCameraSet(bAnimate=false) switches it off, because a manually
	// aimed camera otherwise drifts on its own; call this with true to hand the scene back to it.
	UFUNCTION(BlueprintCallable, Category = "Lanessa|Remote")
	static bool RemoteSetIdleRotation(bool bEnabled);

	// Kamerani BURISH uchun yagona ishlaydigan yo'l: barmoq koordinatasini pawn ning
	// o'z "Touch Rotation" iga uzatish - o'yin ichidagi barmoq ham shundan buradi.
	// Absolyut burchak qo'yib bo'lmaydi: spring arm da use_pawn_control_rotation=True
	// va rotation lag 5.0, ya'ni kamera burilishni faqat pawn ning kirish mantig'idan
	// oladi. Pitch_Current/Yaw_Current yozish aktyorni buradi-yu kamerani emas,
	// SpringArm->SetWorldRotation keyingi kadrda bekor bo'ladi (ikkalasi ham
	// surat bilan tasdiqlangan). Sezgirlik pawn ning Rotation_Speed_Touch idan keladi.
	UFUNCTION(BlueprintCallable, Category = "Lanessa|Remote")
	static bool RemoteTouchRotate(double X, double Y);

	// Faqat joy va masofa - burilishga UMUMAN tegmaydi. Ikki barmoq bilan surish va
	// chimdish shundan foydalanadi. RemoteCameraSet ni ishlatib bo'lmaydi: u burilish
	// o'zgaruvchilarini ham yozadi va RemoteTouchRotate qo'ygan burilishni bekor
	// qilib, kamerani noto'g'ri buradi (o'lchangan regressiya).
	UFUNCTION(BlueprintCallable, Category = "Lanessa|Remote")
	static bool RemoteCameraMove(FVector Pivot, double ArmLength);

	// ---- Burilish va jostik harakati -----------------------------------------------------------
	// Burilish faqat kontroller kirishi orqali bo'ladi. Qiymat masshtablanmaydi: kirish kadrga
	// bir marta yig'iladi va cheklanadi (o'lchangan: Val=3 ham, Val=300 ham ~1.3 gradus).
	// Tezlik CHAQIRUVLAR SONIGA bog'liq - sekundiga 30 ta taxminan 40 gradus/sek beradi.
	UFUNCTION(BlueprintCallable, Category = "Lanessa|Remote")
	static bool RemoteRotate(double YawDir, double PitchDir);

	// Kamera yo'nalishiga nisbatan suradi. Pawn ning MoveForward i yaramaydi - kadr vaqtiga
	// bog'lanmagani uchun juda kuchli (o'lchangan: 20 chaqiruvda 4 million birlik).
	UFUNCTION(BlueprintCallable, Category = "Lanessa|Remote")
	static bool RemoteMoveRelative(double Forward, double Right, double Up, double Speed);

	// ---- Kategoriyalar va xonalar --------------------------------------------------------------
	// Kategoriyalar sahnadagi POI teglaridan topiladi, qattiq yozilmaydi: Atrofi panelida teglar
	// inglizcha (Education/Dining/Transportation/Shopping), Qulayliklarda o'zbekcha
	// (Ko'ngilochar/Transport/Xizmatlar). PanelTag: "Surroundings" yoki "Amenities".
	UFUNCTION(BlueprintCallable, Category = "Lanessa|Remote")
	static void RemoteListCategories(const FString& PanelTag,
	                                 TArray<FString>& OutTags, TArray<int32>& OutCounts);

	UFUNCTION(BlueprintCallable, Category = "Lanessa|Remote")
	static void RemoteListCategoryPois(const FString& PanelTag, const FString& CategoryTag,
	                                   TArray<FString>& OutIds, TArray<FString>& OutNames);

	// Interyer xonalari va progulka nuqtalari. RequiredTag - AKTYOR tegi: "room"
	// interyer uchun, "walk" progulka uchun (bo'sh = hammasi). OutNames - startlarning
	// "Player Start Tag" xossasi, ya'ni ekrandagi ro'yxat bilan bir xil yozuvlar.
	// Ataylab ULanessaInteriorTourWidget/ULanessaWalkPointsWidget::PopulateFromPlayerStarts
	// bilan bir xil qoida: ikki joyda ikki xil filtr bo'lsa telefon va ekran ajralib ketadi.
	UFUNCTION(BlueprintCallable, Category = "Lanessa|Remote")
	static void RemoteListRooms(const FString& RequiredTag,
	                            TArray<FString>& OutIds, TArray<FString>& OutNames);

	UFUNCTION(BlueprintCallable, Category = "Lanessa|Remote")
	static bool RemoteGotoRoom(const FString& RoomId);

	// Analog burilish: berilgan GRADUS miqdoricha buradi, shuning uchun jostikni
	// markazdan qancha uzoqqa surilsa shuncha tez aylanadi. RemoteRotate faqat
	// yo'nalishni biladi va tezligi o'zgarmaydi - jostik uchun shu ishlatiladi.
	UFUNCTION(BlueprintCallable, Category = "Lanessa|Remote")
	static bool RemoteRotateBy(double YawDelta, double PitchDelta);

	// Panel uchun sozlamalar (JSON): panel teglari, interyer/progulka teglari va
	// karta tugmalari. Project Settings -> Plugins -> Lanessa Remote dan boshqariladi.
	// 180/360 yoki ko'p ekranli rejimda ishlayapmizmi (-dc_cluster bayrog'i).
	// Shu rejimda ekran interfeysi chizilmaydi - boshqaruv operator panelida.
	UFUNCTION(BlueprintPure, Category = "Lanessa|Remote")
	static bool IsClusterDisplayMode();

	UFUNCTION(BlueprintCallable, Category = "Lanessa|Remote")
	static FString RemoteGetConfig();

	// POI kartasidagi tugmalar: "level" = 3D TUR, "level2" = VR TUR, "media" = REJA.
	// Vidjetning o'z OnPoiCardAction uzatmasini ishga soladi, ya'ni ekrandagi tugmani
	// bosgan bilan aynan bir xil yo'ldan ketadi.
	UFUNCTION(BlueprintCallable, Category = "Lanessa|Remote")
	static bool RemoteCardAction(const FString& ActionId);

	// Galereya (BP_Gallery_Widget - Blueprint da, C++ da emas).
	UFUNCTION(BlueprintCallable, Category = "Lanessa|Remote")
	static bool RemoteGalleryClose();

	// Delta = +1 keyingi rasm, -1 oldingisi. Oxiridan boshiga aylanadi.
	UFUNCTION(BlueprintCallable, Category = "Lanessa|Remote")
	static bool RemoteGalleryStep(int32 Delta);

	// false qaytsa galereya umuman ochiq emas - panel tugmalarni o'chirib qo'yadi.
	UFUNCTION(BlueprintCallable, Category = "Lanessa|Remote")
	static bool RemoteGalleryState(int32& OutIndex, int32& OutCount);

	// Current pivot/angles/distance, so the panel's gestures can start from where the camera actually
	// is rather than snapping to the panel's own last known values.
	UFUNCTION(BlueprintCallable, Category = "Lanessa|Remote")
	static bool RemoteCameraGet(FVector& Pivot, double& Pitch, double& Yaw, double& ArmLength);

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	TSharedRef<SWidget> BuildBrand();
	TSharedRef<SWidget> BuildSpine();
	TSharedRef<SWidget> BuildRail();
	TSharedRef<SWidget> BuildRailFoot();
	TSharedRef<SWidget> BuildSidePanel(const FString& ViewId, TSharedRef<SWidget> Content);
	TSharedRef<SWidget> BuildSidePanelQidiruvContent();
	TSharedRef<SWidget> BuildSidePanelCatListContent(const FString& Title, const FString& Sub, const FString& PanelTag);
	TSharedRef<SWidget> BuildCategoryRow(const FString& PanelTag, const FString& Title, const FLanessaV2CategoryDef& Cat);
	TSharedRef<SWidget> BuildChipCard();
	void RefreshEnvironmentCaption();
	// UDS faqat kesh eskirganda qidiriladi; sana va vaqtga yozilmaydi.
	TWeakObjectPtr<class AActor> EnvironmentSky;
	float EnvironmentCaptionRefreshLeft = 0.f;
	FText EnvironmentCaption = FText::FromString(TEXT("—"));
	TSharedRef<SWidget> BuildFloorRail();
	// QURILISH -> Yer osti qavati: qirqimdagi qavat paneli, faqat Floor < 1 qavatlar bilan.
	TSharedRef<SWidget> BuildUndergroundRail();
	TSharedRef<SWidget> BuildSidePanelQurilishContent();
	// Qavat raqamlari qatorini qayta to'ldiradi (qirqim va yer osti panellari uchun umumiy).
	void PopulateFloorRow(const TSharedPtr<class SHorizontalBox>& Row, bool bUnderground);
	TSharedRef<SWidget> BuildPoiCard();
	TSharedRef<SWidget> BuildPlanModal();
	TSharedRef<SWidget> BuildUtility();
	TSharedRef<SWidget> BuildTourToast();

	// Real per-unit data for the Qidiruv panel, set via SetSearchUnits(). Plain C++ struct (not exposed
	// to Blueprint directly) since it's only ever consumed inside the panel's own filtering/counting.
	TArray<FLanessaV2SearchUnit> SearchUnits;
	// Evaluates the panel's current RangeHandles (kvadratura/narx) + ToggleState (yotoqxona/sanuzel/
	// holati chips) against SearchUnits and returns every matching unit's PoiId. Empty SearchUnits (not
	// yet pushed in by BP_Explorer_PC) safely yields an empty result rather than matching everything.
	TArray<FString> ComputeMatchingPoiIds() const;
	// Real min/max for "kvadratura" (Surface) or "narx" (Price) across SearchUnits - shared by
	// BuildSidePanelQidiruvContent (range labels) and ComputeMatchingPoiIds (actual filtering), so the
	// displayed range and the filter it drives can never drift apart. Falls back to the v2 mockup's own
	// static numbers (52-118 / 420-980) when SearchUnits hasn't been pushed in yet.
	void ComputeFieldRange(const FString& FieldId, int32& OutMin, int32& OutMax) const;

	// Hard UPROPERTY reference: a plain local UTexture2D* in BuildBrand() is invisible to the GC's
	// reference collector, so the asset gets collected after ~60s (first GC pass) even while the
	// Slate brush is still rendering it, crashing on next paint. This keeps it rooted for the widget's lifetime.
	// The "sayr" (Progulka) spawn-point bar, created on demand the first time that view is entered
	// and hidden again on leaving it. Owned here rather than by BP_Explorer_PC because the BP path
	// that would have created it does not exist: pressing Progulka currently shows no bar at all,
	// and the same graph's room list demonstrably skips its own tag test (SPAWN-DEBUG "count=2").
	// Driving it from SetActiveView keeps the mode self-contained and needs no Blueprint wiring.
	UPROPERTY()
	TObjectPtr<class ULanessaWalkPointsWidget> WalkWidget;

	// Bound to WalkWidget->OnExitClicked. Must be a UFUNCTION for AddDynamic; routes back through
	// SetActiveView("none") so leaving Progulka restores the rail exactly like any other exit.
	UFUNCTION()
	void HandleWalkExit();

	UPROPERTY()
	TObjectPtr<UTexture2D> BrandLogoTexture;
	// Rooted the same way as BrandLogoTexture (see comment above) - the POI card's real image, set
	// per-selection via SetPoiData(), needs the same GC-safety treatment as any long-lived texture ref.
	UPROPERTY()
	TObjectPtr<UTexture2D> PoiImageTexture;
	// SImage needs a stable-address FSlateBrush* to bind to - this member is that brush, kept in sync
	// with PoiImageTexture inside SetPoiData().
	FSlateBrush PoiImageBrush;

	// Live, persistent per-category POI-entry list boxes, keyed "PanelTag:CategoryTag", so
	// SetCategoryPois() can repopulate the right one after the widget tree is already built.
	TMap<FString, TSharedPtr<SVerticalBox>> CategoryPoiListBoxes;
	// Live, persistent outer category-row list box per panel, keyed "PanelTag", so SetPanelCategories()
	// can rebuild it after the widget tree is already built. Value.Key = the box, Value.Value = the
	// side-panel's Title (needed to keep the ToggleState "Title:Label" key scheme unchanged on rebuild).
	TMap<FString, TPair<TSharedPtr<SVerticalBox>, FString>> PanelCategoryListBoxes;
	// Live floor-rail row box, so SetAvailableFloors() can clear and repopulate it after the widget
	// tree is already built - same reason/pattern as CategoryPoiListBoxes above.
	TSharedPtr<class SHorizontalBox> FloorRailRow;
	TSharedPtr<class SHorizontalBox> UndergroundRailRow;

	// False until the first NativeTick has pushed the current Ultra Dynamic Sky "Time of Day" through
	// ULanessaDayNight once. Without it the scene starts in whatever light/emissive state the level was
	// saved in - wrong roughly half the time, and only corrected once the user actually drags the
	// slider. Done from Tick rather than from widget construction because the UDS actor is not
	// guaranteed to be findable while the widget tree is still being built.
	bool bDayNightSynced = false;
};
