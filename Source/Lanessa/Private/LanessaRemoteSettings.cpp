#include "LanessaRemoteSettings.h"

ULanessaRemoteSettings::ULanessaRemoteSettings()
{
	// Konstruktorda, inline boshlang'ich qiymat sifatida emas: USTRUCT massivining
	// ichidagi satrlarni UCLASS a'zosining o'zida to'ldirib bo'lmaydi.
	// Nomlar ULanessaV2Widget dagi karta tugmalari bilan bir xil bo'lishi shart -
	// ekrandagi tugma va telefondagi tugma bitta yo'ldan ketsin.
	CardActions.Add({ TEXT("level"),  TEXT("3D TUR") });
	CardActions.Add({ TEXT("level2"), TEXT("VR TUR") });
	CardActions.Add({ TEXT("media"),  TEXT("REJA")   });
}

const ULanessaRemoteSettings& ULanessaRemoteSettings::Get()
{
	const ULanessaRemoteSettings* S = GetDefault<ULanessaRemoteSettings>();
	check(S);
	return *S;
}

FString ULanessaRemoteSettings::ToJson() const
{
	// Qo'lda yig'amiz: bu yerda bir nechta satr va ikkita massiv bor, xolos -
	// butun JSON modulini tortib kelishga arzimaydi.
	auto Esc = [](const FString& In)
	{
		FString Out = In;
		Out.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
		Out.ReplaceInline(TEXT("\""), TEXT("\\\""));
		return Out;
	};

	FString PanelArr;
	for (const FName& T : PanelTags)
	{
		if (!PanelArr.IsEmpty()) { PanelArr += TEXT(","); }
		PanelArr += FString::Printf(TEXT("\"%s\""), *Esc(T.ToString()));
	}

	FString CardArr;
	for (const FLanessaCardAction& A : CardActions)
	{
		if (A.ActionId.IsEmpty()) { continue; }
		if (!CardArr.IsEmpty()) { CardArr += TEXT(","); }
		CardArr += FString::Printf(TEXT("{\"id\":\"%s\",\"label\":\"%s\"}"),
			*Esc(A.ActionId), *Esc(A.Label.IsEmpty() ? A.ActionId : A.Label));
	}

	return FString::Printf(
		TEXT("{\"panelTags\":[%s],\"interiorTag\":\"%s\",\"walkTag\":\"%s\",\"cardActions\":[%s]}"),
		*PanelArr, *Esc(InteriorTag.ToString()), *Esc(WalkTag.ToString()), *CardArr);
}
