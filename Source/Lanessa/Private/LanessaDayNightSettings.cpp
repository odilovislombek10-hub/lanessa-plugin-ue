#include "LanessaDayNightSettings.h"

ULanessaDayNightSettings::ULanessaDayNightSettings()
{
	// Set in the constructor rather than as an inline member initialiser: FSoftObjectPath's
	// string constructor is not constexpr-friendly as a default member init in a UCLASS, and this
	// also keeps the "which of the two same-named collections" decision next to its explanation.
	EmissiveCollection = FSoftObjectPath(TEXT("/Game/New_Explorer/Materials/MPC/Emissive_MPC.Emissive_MPC"));
}

const ULanessaDayNightSettings& ULanessaDayNightSettings::Get()
{
	const ULanessaDayNightSettings* Settings = GetDefault<ULanessaDayNightSettings>();
	check(Settings);
	return *Settings;
}
