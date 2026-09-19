#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LanessaDayNight.generated.h"

class ULocalLightComponent;

/**
 * Drives the scene's day/night lighting state from Ultra Dynamic Sky's own "Time of Day".
 *
 * Two things change together:
 *   - every LOCAL light in the level (point/spot/rect) is hidden by day, shown by night
 *   - the Emissive_MPC "StreetLights" scalar is dimmed by day, full by night
 *
 * Why ULocalLightComponent and not ULightComponent: UDirectionalLightComponent and
 * USkyLightComponent do NOT derive from it, so filtering on this base class automatically
 * leaves Ultra_Dynamic_Sky's own Sun/Moon/SkyLight alone - there is no name-based exclude
 * list to keep in sync. Confirmed on the Toshekent level: 952 spot + 121 point + 9 rect are
 * local, the only 3 directional + 2 sky components all live inside the UDS/UDW actors.
 *
 * Why SetVisibility and not Intensity=0: visibility removes the light from FScene entirely,
 * so Virtual Shadow Map pages, shadow rendering and Lumen's direct-light gathering all drop
 * to zero for it. Intensity=0 leaves the proxy in the scene, still culled/sorted every frame,
 * and would also require remembering 9 different original intensity values to restore.
 */
UCLASS()
class LANESSA_API ULanessaDayNight : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Applies the day/night state for a given Ultra Dynamic Sky "Time of Day".
	 *
	 * TimeOfDay is UDS's own 0-2400 scale, which is hundredths of an hour, NOT HHMM - UDS's own
	 * tooltip says "9:30 AM would be 950". So hours = ToD/100 and 07:00 = 700, 17:00 = 1700.
	 *
	 * The four threshold/emissive arguments default to -1, meaning "read ULanessaDayNightSettings"
	 * (Project Settings -> Plugins -> Lanessa Day/Night). That is what makes the schedule editable
	 * without a rebuild: leave the pins alone and the .ini value wins; pass a real number and this
	 * one call overrides it. -1 is safe as the sentinel because times are 0-2400 and emissive is >= 0.
	 *
	 * Edge-triggered: the 1082-component loop only runs when the day/night state actually flips,
	 * so this is safe to call every frame from a slider drag. Pass bForce to re-apply anyway
	 * (e.g. once at BeginPlay, when nothing has been applied yet).
	 */
	UFUNCTION(BlueprintCallable, Category = "Lanessa|DayNight", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "DayStart,NightStart,DayEmissive,NightEmissive"))
	static void ApplyDayNight(UObject* WorldContextObject, float TimeOfDay, bool bForce = false,
		float DayStart = -1.f, float NightStart = -1.f,
		float DayEmissive = -1.f, float NightEmissive = -1.f);

	/**
	 * Same as ApplyDayNight, but reads "Time of Day" off the level's own Ultra_Dynamic_Sky actor
	 * instead of taking it as a parameter - for callers that have no Pct/ToD value of their own
	 * (a BeginPlay sync, or a timer that keeps up with UDS's own animated day cycle).
	 * Returns false if no UDS actor with a readable "Time of Day" was found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lanessa|DayNight", meta = (WorldContext = "WorldContextObject"))
	static bool ApplyDayNightFromUDS(UObject* WorldContextObject, bool bForce = false);

	/** Reads "Time of Day" off the level's Ultra_Dynamic_Sky actor by property name (reflection). -1 if not found. */
	UFUNCTION(BlueprintCallable, Category = "Lanessa|DayNight", meta = (WorldContext = "WorldContextObject"))
	static float GetUDSTimeOfDay(UObject* WorldContextObject);

	/** True when DayStart <= TimeOfDay < NightStart. -1 on either bound reads it from settings. */
	UFUNCTION(BlueprintPure, Category = "Lanessa|DayNight")
	static bool IsDayTime(float TimeOfDay, float DayStart = -1.f, float NightStart = -1.f);

	/**
	 * Overrides the configured hours for this session, without touching the Project Settings page.
	 * For a schedule that changes while running - a season that shortens the day, a level that wants
	 * its own timing - where editing the .ini would be the wrong scope.
	 *
	 * bSaveToConfig writes the new values back to DefaultGame.ini, making them the project default.
	 * Left off, the override lasts until the editor/game is restarted.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lanessa|DayNight")
	static void SetDayNightHours(float DayStart, float NightStart, bool bSaveToConfig = false);

	/** Reads back whatever hours are currently in effect, config or runtime override. */
	UFUNCTION(BlueprintPure, Category = "Lanessa|DayNight")
	static void GetDayNightHours(float& DayStart, float& NightStart);

	/**
	 * Drops the cached light list so the next ApplyDayNight rebuilds it. The Toshekent level is a
	 * single persistent level with no World Partition and no Blueprint that spawns lights at
	 * runtime, so the list never changes on its own - this exists for the editor (where actors
	 * can be added by hand) and for a future streaming setup.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lanessa|DayNight")
	static void InvalidateLightCache();

	/** How many local light components the cache currently holds - for verifying the wiring. */
	UFUNCTION(BlueprintCallable, Category = "Lanessa|DayNight", meta = (WorldContext = "WorldContextObject"))
	static int32 GetCachedLightCount(UObject* WorldContextObject);

private:
	/** Fills CachedLights for World if it is empty or belongs to a different world. */
	static void EnsureCache(UWorld* World);

	static TArray<TWeakObjectPtr<ULocalLightComponent>> CachedLights;
	static TWeakObjectPtr<UWorld> CachedWorld;
	/** -1 = nothing applied yet, 0 = night applied, 1 = day applied. Drives the edge trigger. */
	static int8 LastState;
};
