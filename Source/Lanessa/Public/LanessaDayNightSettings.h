#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "LanessaDayNightSettings.generated.h"

/**
 * Project Settings page for the day/night light switch (Project Settings -> Plugins -> Lanessa Day/Night).
 *
 * These used to be C++ default arguments on ULanessaDayNight::ApplyDayNight, which meant moving the
 * evening switch-on from 19:00 to 17:00 was a source edit plus a full rebuild plus an editor restart.
 * As config they are saved to DefaultGame.ini and read on every call, so the hour can be changed in
 * the editor and tested immediately - and, because the same values are BlueprintReadWrite through
 * ULanessaDayNight's accessors, a Blueprint can override them at runtime too (a seasonal schedule,
 * a per-level override) without touching this page at all.
 *
 * Times are on Ultra Dynamic Sky's own "Time of Day" scale: 0-2400 in hundredths of an hour, so
 * 7:00 is 700 and 17:30 is 1750. UDS's own tooltip states the same ("9:30 AM would be 950") - this
 * is NOT HHMM, which is why 1750 and not 1730.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Lanessa Day/Night"))
class LANESSA_API ULanessaDayNightSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	ULanessaDayNightSettings();

	/** Settings category shown in the Project Settings tree. */
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	/**
	 * Local time the lights switch OFF, on UDS's 0-2400 scale (700 = 07:00).
	 * Day is DayStart <= ToD < NightStart; anything outside that range is night.
	 */
	UPROPERTY(EditAnywhere, config, Category = "Hours", meta = (ClampMin = "0.0", ClampMax = "2400.0", UIMin = "0.0", UIMax = "2400.0"))
	float DayStart = 700.f;

	/** Local time the lights switch ON, on UDS's 0-2400 scale (1700 = 17:00). */
	UPROPERTY(EditAnywhere, config, Category = "Hours", meta = (ClampMin = "0.0", ClampMax = "2400.0", UIMin = "0.0", UIMax = "2400.0"))
	float NightStart = 1700.f;

	/** Emissive_MPC scalar value applied during the day - dims lamp/sign materials rather than hiding them. */
	UPROPERTY(EditAnywhere, config, Category = "Emissive", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "4.0"))
	float DayEmissive = 0.1f;

	/** Emissive_MPC scalar value applied at night. */
	UPROPERTY(EditAnywhere, config, Category = "Emissive", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "4.0"))
	float NightEmissive = 1.f;

	/**
	 * Scalar parameter inside the collection below that carries the emissive level.
	 * Configurable because the same collection also holds "Buildings"/"Effects", and which one the
	 * lamp masters are wired to is a content decision, not a code one.
	 */
	UPROPERTY(EditAnywhere, config, Category = "Emissive")
	FName EmissiveScalarName = TEXT("StreetLights");

	/**
	 * The Material Parameter Collection holding EmissiveScalarName.
	 *
	 * A soft path, not a hard reference: this is a content asset that must not force-load the
	 * collection (and everything it touches) just because the settings object exists. Note the
	 * project ships two collections with this name - /Game/New_Explorer/... is the live one that
	 * BP_Explorer_PC drives; /Game/ArchVizExplorer/... is the older copy.
	 */
	UPROPERTY(EditAnywhere, config, Category = "Emissive", meta = (AllowedClasses = "/Script/Engine.MaterialParameterCollection"))
	FSoftObjectPath EmissiveCollection;

	/**
	 * Class name prefix used to find the level's Ultra Dynamic Sky actor when reading "Time of Day".
	 * Kept as data so a renamed/duplicated UDS blueprint does not need a code change to be found.
	 */
	UPROPERTY(EditAnywhere, config, Category = "Advanced")
	FString UdsActorClassPrefix = TEXT("Ultra_Dynamic_Sky");

	/** Convenience accessor - never null, UDeveloperSettings are always resident. */
	static const ULanessaDayNightSettings& Get();
};
