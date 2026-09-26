#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LanessaInteriorTourWidget.generated.h"

class UTexture2D;

// Broadcast when a room pill (bottom bar) or a room marker (plan popup) is clicked. RoomId is
// whatever id string BP_Explorer_PC pushed in via SetRooms() for that room (its own naming
// convention - e.g. a BP_POI actor name or a stable room key) - not shown on screen.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLanessaInteriorOnRoomSelected, const FString&, RoomId);
// Broadcast when the top-right "X" is clicked - BP_Explorer_PC should run its full interior-exit
// cleanup (unload the streamed level, un-hide/re-enable-collision the main level's actors, destroy
// the walkthrough pawn, re-possess ExplorerMainPawn).
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLanessaInteriorOnExitClicked);

/**
 * Interior-walkthrough HUD chrome, shown while the player free-walks a streamed-in interior level
 * (BP_Explorer_PC::StreamToInteriorLevel). Ported for VISUAL DESIGN ONLY from the reference 360-tour
 * product's screenshots (room-jump pill bar + floor-plan corner popup + top-right icon cluster) -
 * this widget does not drive panorama navigation itself. That's BP_360Menu_Widget/BP_360Sphere's job
 * for the separate, unrelated 360 Tour feature; here, clicking a room is meant to teleport the
 * ThirdPersonCharacter to that room's spawn point, not swap a sphere texture.
 */
UCLASS()
class LANESSA_API ULanessaInteriorTourWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "LanessaInterior")
	FLanessaInteriorOnRoomSelected OnRoomSelected;

	UPROPERTY(BlueprintAssignable, Category = "LanessaInterior")
	FLanessaInteriorOnExitClicked OnExitClicked;

	// Populates the bottom room-jump bar and the plan popup's room list, in order. RoomIds/RoomLabels
	// must be the same length (RoomIds[i] is what OnRoomSelected reports back for RoomLabels[i]).
	UFUNCTION(BlueprintCallable, Category = "LanessaInterior")
	void SetRooms(const TArray<FString>& InRoomIds, const TArray<FString>& InRoomLabels);

	// Highlights RoomId in both the bottom bar and the plan popup, and updates the top-left room label.
	UFUNCTION(BlueprintCallable, Category = "LanessaInterior")
	void SetCurrentRoom(const FString& RoomId);

	// The architectural floor-plan raster shown in the plan popup - content authoring's own asset,
	// left as a placeholder panel until one is assigned.
	UFUNCTION(BlueprintCallable, Category = "LanessaInterior")
	void SetPlanImage(UTexture2D* Texture);

	/**
	 * Fills the room bar straight from the level's PlayerStarts, keeping only those whose Actor Tags
	 * contain RequiredTag. The visible label is each start's "Player Start Tag" property (the
	 * APlayerStart field, NOT the Tags array), falling back to the actor name when unset.
	 *
	 * Added because the BP path that used to build this list does not actually apply its tag test -
	 * its own SPAWN-DEBUG log prints "count=2" on a level whose two PlayerStarts carry different
	 * tags, so an untagged walk start ends up in the room list. Filtering here makes the rule one
	 * call, and keeps it identical to the walk bar's (see ULanessaWalkPointsWidget).
	 *
	 * Returns how many rooms were kept - 0 means the tag is missing or misspelled on every start.
	 */
	UFUNCTION(BlueprintCallable, Category = "LanessaInterior", meta = (WorldContext = "WorldContextObject"))
	int32 PopulateFromPlayerStarts(UObject* WorldContextObject, FName RequiredTag = TEXT("Room"));

	/** Moves the local player's pawn to RoomId's PlayerStart. False for an unknown or unloaded id. */
	UFUNCTION(BlueprintCallable, Category = "LanessaInterior", meta = (WorldContext = "WorldContextObject"))
	bool TeleportToRoom(UObject* WorldContextObject, const FString& RoomId);

	/**
	 * When true (default), clicking a room pill or plan marker teleports immediately. OnRoomSelected
	 * still fires either way - turn this off if BP needs to run its own move (fade, camera blend).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LanessaInterior")
	bool bTeleportOnClick = true;

	/**
	 * Tag that an entry pushed through SetRooms() must carry to survive, matched against the
	 * PlayerStart the entry names. None disables the check and takes the list exactly as given.
	 *
	 * This exists because BP_Explorer_PC builds the room list itself and does not apply its own tag
	 * test - its SPAWN-DEBUG prints "count=2" on a level whose two PlayerStarts carry different
	 * tags, and the walk start duly shows up in the room bar. Filtering on the way in fixes that
	 * without needing the BP graph rewired, and also gives SetRooms the actor map that
	 * TeleportToRoom needs - BP's own ids are labels, which resolve to no actor at all.
	 *
	 * Entries that match no PlayerStart are kept: the list may legitimately come from somewhere
	 * else (room markers, a data table), and silently dropping those would be worse than the leak.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LanessaInterior")
	FName AutoFilterTag = TEXT("Room");

	// ---- QIRQIM rejimi ------------------------------------------------------------------------
	// Yurish o'rniga kvartira tepadan, bosh sahifadagi orbit kamera bilan ko'rinadi va shu
	// tegli volume bo'yicha qirqiladi. Kamera nuqtasi - SectionCameraTag tegli aktyor (odatda
	// CameraActor): u turgan joydan volume markaziga qaraladi. Bunday aktyor bo'lmasa kamera
	// avtomatik: tepadan 60 gradus, volume o'lchamiga qarab masofa.

	/** Qirqim qutisi - interyer levelidagi shu Actor Tag li aktyorning chegarasi. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LanessaInterior|Qirqim")
	FName SectionTag = TEXT("InteriorSection");

	/** Qirqim kamerasi turadigan joy (ixtiyoriy). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LanessaInterior|Qirqim")
	FName SectionCameraTag = TEXT("InteriorSectionCamera");

	/** true - qirqim rejimiga o'tadi, false - yurish rejimiga, oxirgi turgan joyga qaytadi. */
	UFUNCTION(BlueprintCallable, Category = "LanessaInterior|Qirqim")
	void SetSectionMode(bool bEnable);

	UFUNCTION(BlueprintPure, Category = "LanessaInterior|Qirqim")
	bool IsSectionMode() const { return bSectionMode; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	TSharedRef<SWidget> BuildSectionButton();

	// Yuqori o'ngdagi X: qirqimdan chiqish, qo'shimcha levellarni yopish, BP ga xabar,
	// keyingi kadrda QIDIRUV sahifasiga qaytish.
	void HandleExit();

	// Xona tanlanganda: qirqimda bo'lsak avval yurishga qaytamiz - teleport yuruvchi
	// personajga tegishli, orbit pawn ga emas.
	void SelectRoom(const FString& RoomId);

	bool bSectionMode = false;
	// POI dagi 3D TUR orqali kirilganmi (menyudagi INTERYER emas). Qo'shimcha levellar va
	// chiqishda QIDIRUV ga qaytish faqat shu yo'l uchun.
	bool bPoiTour = false;
	// Qirqimga o'tishdan oldingi yuruvchi personaj va uning qarash yo'nalishi - qaytishda
	// o'sha holatning o'zi tiklanadi.
	TWeakObjectPtr<class APawn> WalkPawn;
	FRotator WalkControlRotation = FRotator::ZeroRotator;
	// Qirqimga kirishdan oldin ko'rinib turgan sub levellar - chiqishda aynan shu holat qaytadi.
	TArray<TWeakObjectPtr<class ULevelStreaming>> PreSectionVisible;
	// Qirqimda yashirilgan POI markerlari (xonadon filtrlari va h.k.) - faqat o'zimiz
	// yashirganlari, chiqishda aynan shular qaytariladi.
	TArray<TWeakObjectPtr<class AActor>> HiddenPois;

	TSharedRef<SWidget> BuildRoomBar();
	/**
	 * Refills RoomRow from the current RoomIds/RoomLabels. RebuildWidget() runs once, at construction
	 * - a bar built before the data arrived would stay empty, and Invalidate() only marks the existing
	 * tree dirty rather than re-running the builder. Today this happens to work because BP calls
	 * SetRooms() before the widget reaches the viewport; this makes it independent of that ordering.
	 */
	void RefreshRoomRow();
	TSharedRef<SWidget> BuildTopRightCluster();
	TSharedRef<SWidget> BuildPlanPopup();
	TSharedRef<SWidget> BuildRoomLabel();

	void TogglePlan();
	FText GetCurrentRoomLabel() const;

	TArray<FString> RoomIds;
	TArray<FString> RoomLabels;
	FString CurrentRoomId;
	bool bPlanOpen = false;

	// Filled by PopulateFromPlayerStarts so a click can reach its actor without re-scanning. Weak,
	// because interiors are streamed levels: a start that unloaded must fail, not dangle.
	TMap<FString, TWeakObjectPtr<AActor>> RoomActors;

	// Live handle on the pill row so RefreshRoomRow() can refill it after the tree is already built.
	TSharedPtr<class SHorizontalBox> RoomRow;

	UPROPERTY()
	TObjectPtr<UTexture2D> PlanTexture = nullptr;
};
