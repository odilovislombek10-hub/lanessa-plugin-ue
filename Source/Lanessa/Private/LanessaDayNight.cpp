#include "LanessaDayNight.h"
#include "LanessaDayNightSettings.h"

#include "Components/LocalLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "UObject/UnrealType.h"

TArray<TWeakObjectPtr<ULocalLightComponent>> ULanessaDayNight::CachedLights;
TWeakObjectPtr<UWorld> ULanessaDayNight::CachedWorld;
int8 ULanessaDayNight::LastState = -1;

namespace
{
	// The emissive collection and its scalar moved to ULanessaDayNightSettings - they are content
	// choices (the project ships two same-named collections, and the same collection also holds
	// "Buildings"/"Effects"), so they belong on the settings page, not compiled in here.

	// UDS exposes its properties under display names with spaces and no GUID suffix ("Time of Day").
	// This one stays hardcoded: it is UDS's own published API, not a project decision.
	const TCHAR* GUDSTimeOfDayProperty = TEXT("Time of Day");

	/**
	 * The level's Ultra_Dynamic_Sky actor, found by class-name prefix so no UDS header/dependency is
	 * needed. The prefix is configurable because a duplicated or renamed UDS blueprint would
	 * otherwise silently stop being found, with the lights simply never switching.
	 */
	AActor* FindUDSActor(UWorld* World, const FString& ClassPrefix)
	{
		if (!World || ClassPrefix.IsEmpty()) { return nullptr; }
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!IsValid(Actor)) { continue; }
			if (Actor->GetClass()->GetName().StartsWith(ClassPrefix))
			{
				return Actor;
			}
		}
		return nullptr;
	}

	/**
	 * Keshda o'lik yozuv bormi.
	 *
	 * Lampo_1..4, Stalba_1, Sham_1/2, Table_2 chiroq Blueprint'lari konstruksiya skripti qayta
	 * ishlaganda (aktyorni ko'chirish, tahrirlash, BP rekompilyatsiyasi, PIE boshlanishi)
	 * o'z chiroq komponentini O'CHIRIB, o'rniga yangisini yaratadi. Eski zaif ko'rsatkich
	 * yaroqsiz bo'ladi, ApplyDayNight uni keshdan tashlaydi - lekin YANGI komponent keshga
	 * qaytmaydi, chunki EnsureCache faqat kesh bo'sh bo'lsagina qayta quradi. Natijada
	 * Toshekent levelida kesh 1087 dan 886 ga tushib qolardi va o'sha 201 ta soya tashlovchi
	 * chiroq kun bo'yi yonib turardi.
	 *
	 * Shuning uchun bitta o'lik yozuv ham "kesh eskirgan" degani: butunlay qayta quramiz.
	 * Narxi - 1087 ta zaif ko'rsatkichni tekshirish, bu slayder tortilayotganda ham sezilmaydi.
	 */
	bool HasStaleEntry(const TArray<TWeakObjectPtr<ULocalLightComponent>>& Lights)
	{
		for (const TWeakObjectPtr<ULocalLightComponent>& Light : Lights)
		{
			if (!Light.IsValid()) { return true; }
		}
		return false;
	}
}

void ULanessaDayNight::EnsureCache(UWorld* World)
{
	if (!World) { return; }

	if (CachedWorld.Get() == World && CachedLights.Num() > 0 && !HasStaleEntry(CachedLights))
	{
		return;
	}

	CachedLights.Reset();
	CachedWorld = World;
	// A world swap (editor <-> PIE) invalidates whatever was applied to the old world's components.
	LastState = -1;

	TArray<ULocalLightComponent*> Found;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor)) { continue; }

		Found.Reset();
		// bIncludeFromChildActors: the lamp Blueprints (Lampo_*, Stalba_1, Sham_*, Table_2) hold their
		// light as an ordinary component, but child-actor nesting is cheap to cover and costs nothing here.
		Actor->GetComponents<ULocalLightComponent>(Found, /*bIncludeFromChildActors=*/true);
		for (ULocalLightComponent* Light : Found)
		{
			if (IsValid(Light))
			{
				CachedLights.Add(Light);
			}
		}
	}
}

bool ULanessaDayNight::IsDayTime(float TimeOfDay, float DayStart, float NightStart)
{
	const ULanessaDayNightSettings& Settings = ULanessaDayNightSettings::Get();
	if (DayStart   < 0.f) { DayStart   = Settings.DayStart; }
	if (NightStart < 0.f) { NightStart = Settings.NightStart; }
	return TimeOfDay >= DayStart && TimeOfDay < NightStart;
}

void ULanessaDayNight::SetDayNightHours(float DayStart, float NightStart, bool bSaveToConfig)
{
	ULanessaDayNightSettings* Settings = GetMutableDefault<ULanessaDayNightSettings>();
	if (!Settings) { return; }

	Settings->DayStart = DayStart;
	Settings->NightStart = NightStart;
	if (bSaveToConfig) { Settings->SaveConfig(); }

	// The edge trigger remembers which state it last applied; leaving it alone would make the very
	// next call a no-op whenever the new hours happen to agree with the old state, so a schedule
	// change would not show until the time crossed a boundary on its own.
	LastState = -1;

	UE_LOG(LogTemp, Log, TEXT("[LanessaDayNight] hours set to day %.0f-%.0f%s"),
		DayStart, NightStart, bSaveToConfig ? TEXT(" (saved to config)") : TEXT(""));
}

void ULanessaDayNight::GetDayNightHours(float& DayStart, float& NightStart)
{
	const ULanessaDayNightSettings& Settings = ULanessaDayNightSettings::Get();
	DayStart = Settings.DayStart;
	NightStart = Settings.NightStart;
}

float ULanessaDayNight::GetUDSTimeOfDay(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	AActor* UDS = FindUDSActor(World, ULanessaDayNightSettings::Get().UdsActorClassPrefix);
	if (!UDS) { return -1.f; }

	// UDS authors "Time of Day" as a Blueprint variable; its UPROPERTY name carries the spaces.
	for (TFieldIterator<FProperty> It(UDS->GetClass()); It; ++It)
	{
		if (!It->GetName().Equals(GUDSTimeOfDayProperty, ESearchCase::IgnoreCase)) { continue; }
		if (const FDoubleProperty* DP = CastField<FDoubleProperty>(*It))
		{
			return static_cast<float>(DP->GetPropertyValue_InContainer(UDS));
		}
		if (const FFloatProperty* FP = CastField<FFloatProperty>(*It))
		{
			return FP->GetPropertyValue_InContainer(UDS);
		}
	}
	return -1.f;
}

void ULanessaDayNight::ApplyDayNight(UObject* WorldContextObject, float TimeOfDay, bool bForce,
	float DayStart, float NightStart, float DayEmissive, float NightEmissive)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World) { return; }

	// -1 on any of these means "whatever the settings page says" - see the header's note on why that
	// sentinel is safe. Resolved here rather than in the defaults so a change to the .ini takes
	// effect on the very next call, with no rebuild and no restart.
	const ULanessaDayNightSettings& Settings = ULanessaDayNightSettings::Get();
	if (DayStart      < 0.f) { DayStart      = Settings.DayStart; }
	if (NightStart    < 0.f) { NightStart    = Settings.NightStart; }
	if (DayEmissive   < 0.f) { DayEmissive   = Settings.DayEmissive; }
	if (NightEmissive < 0.f) { NightEmissive = Settings.NightEmissive; }

	EnsureCache(World);

	const bool bDay = IsDayTime(TimeOfDay, DayStart, NightStart);
	const int8 NewState = bDay ? 1 : 0;
	if (NewState == LastState && !bForce)
	{
		// Edge trigger: re-running the loop on every frame of a slider drag would mean 1082
		// MarkRenderStateDirty() calls per frame.
		return;
	}
	LastState = NewState;

	int32 Applied = 0;
	for (int32 i = CachedLights.Num() - 1; i >= 0; --i)
	{
		ULocalLightComponent* Light = CachedLights[i].Get();
		if (!IsValid(Light))
		{
			CachedLights.RemoveAtSwap(i, EAllowShrinking::No);
			continue;
		}
		// bPropagateToChildren=false: these light components have no child components, and true
		// would recurse for nothing across 1082 of them.
		Light->SetVisibility(!bDay, /*bPropagateToChildren=*/false);
		++Applied;
	}

	// Soft path resolved per call: the collection is content, and which one/which scalar the lamp
	// masters use is a settings decision. TryLoad rather than LoadObject so an unset or renamed
	// path is a warning, not a hard failure inside the light loop.
	const FString ScalarName = Settings.EmissiveScalarName.ToString();
	if (UMaterialParameterCollection* Collection = Cast<UMaterialParameterCollection>(Settings.EmissiveCollection.TryLoad()))
	{
		UKismetMaterialLibrary::SetScalarParameterValue(World, Collection, Settings.EmissiveScalarName, bDay ? DayEmissive : NightEmissive);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LanessaDayNight] Emissive collection not found: %s"),
			*Settings.EmissiveCollection.ToString());
	}

	UE_LOG(LogTemp, Log, TEXT("[LanessaDayNight] ToD=%.1f (day %.0f-%.0f) -> %s | lights=%d | %s=%.2f"),
		TimeOfDay, DayStart, NightStart,
		bDay ? TEXT("DAY (lights off)") : TEXT("NIGHT (lights on)"),
		Applied, *ScalarName, bDay ? DayEmissive : NightEmissive);
}

bool ULanessaDayNight::ApplyDayNightFromUDS(UObject* WorldContextObject, bool bForce)
{
	const float ToD = GetUDSTimeOfDay(WorldContextObject);
	if (ToD < 0.f) { return false; }
	ApplyDayNight(WorldContextObject, ToD, bForce);
	return true;
}

void ULanessaDayNight::InvalidateLightCache()
{
	CachedLights.Reset();
	CachedWorld.Reset();
	LastState = -1;
}

int32 ULanessaDayNight::GetCachedLightCount(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World) { return 0; }
	EnsureCache(World);
	return CachedLights.Num();
}
