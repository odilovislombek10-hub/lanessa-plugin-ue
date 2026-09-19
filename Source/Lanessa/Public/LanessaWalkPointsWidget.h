#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LanessaWalkPointsWidget.generated.h"

// Broadcast when a point pill in the bottom bar is clicked. PointId is whatever id string
// BP_Explorer_PC pushed in via SetPoints() for that point (its own naming convention - typically the
// PlayerStart's actor name or a stable key) - never shown on screen, the label is what the user sees.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLanessaWalkOnPointSelected, const FString&, PointId);
// Broadcast when the top-right "X" is clicked - BP_Explorer_PC should run its full walk-exit
// cleanup (put the camera back, re-possess the main explorer pawn, remove this widget, and restore
// the v2 nav rail it hid on entering walk mode).
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLanessaWalkOnExitClicked);

/**
 * Bottom spawn-point bar for the "sayr" (free-walk) mode - the same pill row the interior tour uses
 * for rooms, but sourced from the level's walk PlayerStarts instead of an interior's room markers.
 *
 * Why its own widget and not a reuse of ULanessaInteriorTourWidget: that one owns interior-only
 * chrome (floor-plan popup, the top-right map/expand/close cluster, the "INTERYER" corner label) and
 * its delegate is named around rooms. Reusing it here would mean shipping a widget that is half
 * hidden and half mislabelled. This one is only the bar, so "sayr" stays free to keep the main v2
 * rail on screen - walk mode does not take over the HUD the way the interior walkthrough does.
 *
 * The API deliberately mirrors ULanessaInteriorTourWidget::SetRooms/OnRoomSelected one-for-one, so
 * the BP_Explorer_PC wiring for the two modes is the same shape and can be read side by side.
 */
UCLASS()
class LANESSA_API ULanessaWalkPointsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "LanessaWalk")
	FLanessaWalkOnPointSelected OnPointSelected;

	UPROPERTY(BlueprintAssignable, Category = "LanessaWalk")
	FLanessaWalkOnExitClicked OnExitClicked;

	/**
	 * Populates the bottom bar, in order. PointIds/PointLabels must be the same length -
	 * PointIds[i] is what OnPointSelected reports back for PointLabels[i].
	 *
	 * Mismatched lengths are rejected rather than half-applied: a shorter label array would
	 * otherwise index out of bounds while painting, and silently truncating would hide the
	 * authoring mistake that caused it.
	 */
	UFUNCTION(BlueprintCallable, Category = "LanessaWalk")
	void SetPoints(const TArray<FString>& InPointIds, const TArray<FString>& InPointLabels);

	/** Highlights PointId in the bar and updates the corner label. Unknown ids clear the highlight. */
	UFUNCTION(BlueprintCallable, Category = "LanessaWalk")
	void SetCurrentPoint(const FString& PointId);

	/** The id currently highlighted - for BP that wants to know where the pawn is without tracking it. */
	UFUNCTION(BlueprintPure, Category = "LanessaWalk")
	FString GetCurrentPoint() const { return CurrentPointId; }

	/** How many points the bar currently holds - lets BP verify SetPoints() actually received something. */
	UFUNCTION(BlueprintPure, Category = "LanessaWalk")
	int32 GetPointCount() const { return PointIds.Num(); }

	/**
	 * Fills the bar straight from the level's PlayerStarts, keeping only those whose Actor Tags
	 * contain RequiredTag. The visible label for each is its "Player Start Tag" property (the
	 * APlayerStart field, NOT the Tags array) - falling back to the actor's name when that is unset,
	 * so a half-authored start is still reachable instead of showing a blank pill.
	 *
	 * This lives in C++ rather than as a BP node graph because the existing BP path demonstrably
	 * skips the tag test: its own SPAWN-DEBUG log prints "count=2" on a level whose two PlayerStarts
	 * carry different tags, i.e. every start is taken regardless. Doing the filter here makes the
	 * rule one call and one place instead of a loop that can silently lose its Branch again.
	 *
	 * Returns how many points were kept, so BP can log/branch on an empty result rather than
	 * discovering the misconfigured tags only by looking at an empty bar.
	 */
	UFUNCTION(BlueprintCallable, Category = "LanessaWalk", meta = (WorldContext = "WorldContextObject"))
	int32 PopulateFromPlayerStarts(UObject* WorldContextObject, FName RequiredTag = TEXT("Walk"));

	/**
	 * Moves the local player's pawn to PointId's PlayerStart. Safe to call for an unknown id
	 * (returns false) - ids come from PopulateFromPlayerStarts, so a miss means the level changed
	 * under the widget and teleporting to a stale transform would drop the pawn somewhere arbitrary.
	 */
	UFUNCTION(BlueprintCallable, Category = "LanessaWalk", meta = (WorldContext = "WorldContextObject"))
	bool TeleportToPoint(UObject* WorldContextObject, const FString& PointId);

	/**
	 * When true (default), clicking a pill teleports immediately using the widget's own world, so the
	 * bar works with no BP wiring beyond creating it. OnPointSelected still fires either way - turn
	 * this off if BP needs to run its own move (a fade, a camera blend, a permission check) instead.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LanessaWalk")
	bool bTeleportOnClick = true;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	TSharedRef<SWidget> BuildPointBar();
	TSharedRef<SWidget> BuildCornerLabel();
	TSharedRef<SWidget> BuildExitButton();

	/**
	 * Refills PointRow from the current PointIds/PointLabels.
	 *
	 * Needed because RebuildWidget() runs once, when the widget is first constructed - which for a
	 * viewport widget is at AddToViewport(), before any data has arrived. Invalidate() only marks the
	 * existing Slate tree dirty; it does not re-run the builder, so a bar built while the list was
	 * empty stays empty forever. Keeping the row and refilling it is the same pattern the v2 HUD uses
	 * for its own late-populated lists (CategoryPoiListBoxes).
	 */
	void RefreshPointRow();

	FText GetCurrentPointLabel() const;

	TArray<FString> PointIds;
	TArray<FString> PointLabels;
	FString CurrentPointId;

	// Filled by PopulateFromPlayerStarts so a click can reach its actor without re-scanning the world.
	// Weak, because the interior/walk levels these can live in are streamed: a start that unloaded
	// must fail the teleport, not resurrect a dangling pointer.
	TMap<FString, TWeakObjectPtr<AActor>> PointActors;

	// Live handle on the pill row so RefreshPointRow() can refill it after the tree is already built.
	TSharedPtr<class SHorizontalBox> PointRow;
};
