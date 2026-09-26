#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "LanessaRemoteSettings.generated.h"

/** POI kartasidagi bitta tugma: ichki amal nomi va ekranda ko'rinadigan yozuv. */
USTRUCT(BlueprintType)
struct FLanessaCardAction
{
	GENERATED_BODY()

	/** ULanessaV2Widget::OnPoiCardAction orqali uzatiladigan nom ("level", "media"). */
	UPROPERTY(EditAnywhere, config, Category = "Lanessa")
	FString ActionId;

	/** Operator panelidagi tugma yozuvi ("3D TUR", "REJA"). */
	UPROPERTY(EditAnywhere, config, Category = "Lanessa")
	FString Label;
};

/**
 * Project Settings -> Plugins -> Lanessa Remote.
 *
 * Masofadan boshqarish sahnadan ko'p narsani O'ZI topadi: kategoriyalar aktyor teglaridan,
 * POI lar va markerlar sahnadagi aktyorlardan, xonalar APlayerStart lardan, galereyadagi
 * rasmlar soni vidjetning massividan. Ammo bularga YETIB BORISH uchun bir necha nom kerak
 * bo'ladi - pawn ning o'zgaruvchilari, Blueprint sinflarining nomlari, galereyaning ichki
 * hodisalari. Ular ilgari C++ ichida TEXT("...") bo'lib yozilgan edi, ya'ni plaginni boshqa
 * loyihaga ko'chirish manbani tahrirlash va qayta qurishni talab qilardi.
 *
 * Bu yerda ularning hammasi sozlama bo'lib turibdi va DefaultGame.ini ga saqlanadi. Standart
 * qiymatlar hozirgi loyihanikiga teng, shuning uchun bu sahifaga tegmasangiz hech nima
 * o'zgarmaydi; boshqa loyihada esa faqat mos kelmaganini almashtirish kifoya.
 *
 * Nom topilmasa tegishli funksiya `false` qaytaradi va panelda "BAJARILMADI" chiqadi -
 * hech nima ishdan chiqmaydi, shuning uchun noto'g'ri nom xavfsiz va darhol ko'rinadi.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Lanessa Remote"))
class LANESSA_API ULanessaRemoteSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	// ---- Pawn o'zgaruvchilari ------------------------------------------------------------
	// Pawn ning o'zi UGameplayStatics::GetPlayerPawn bilan topiladi, ya'ni sinf nomi kerak
	// emas. Quyidagilar esa uning ichidagi o'zgaruvchilar: nomlari Blueprint da qanday
	// yozilgan bo'lsa shundayligicha.

	/** Attract rejimidagi avto-aylanishni o'chiradigan bayroq. Qo'lda boshqarilayotganda false qilinadi. */
	UPROPERTY(EditAnywhere, config, Category = "Pawn")
	FName IdleFlag = TEXT("Allow_Idle?");

	/** Pawn har kadrda transformini SHULARDAN qayta quradi - tirik surish uchun shular yoziladi. */
	UPROPERTY(EditAnywhere, config, Category = "Pawn")
	FName PitchCurrent = TEXT("Pitch_Current");

	UPROPERTY(EditAnywhere, config, Category = "Pawn")
	FName YawCurrent = TEXT("Yaw_Current");

	/**
	 * Focus animatsiyasining NISHONLARI. Viewpointga sakrash uchun to'g'ri, ammo tirik
	 * surish paytida yozilsa animatsiya qayta boshlanadi va kamera boshqarib bo'lmas
	 * holga keladi (o'lchangan: 1 gradus so'ralganda 129 gradus aylangan).
	 */
	UPROPERTY(EditAnywhere, config, Category = "Pawn")
	FName PitchNew = TEXT("Pitch_New");

	UPROPERTY(EditAnywhere, config, Category = "Pawn")
	FName YawNew = TEXT("Yaw_New");

	UPROPERTY(EditAnywhere, config, Category = "Pawn")
	FName LocationNew = TEXT("Location_New");

	UPROPERTY(EditAnywhere, config, Category = "Pawn")
	FName LocationCurrent = TEXT("Location_Current");

	UPROPERTY(EditAnywhere, config, Category = "Pawn")
	FName TargetArmLengthNew = TEXT("TargetArmLength_New");

	/** Kamera qay darajada tepaga/pastga qaray olishi. Topilmasa -89..89 ishlatiladi. */
	UPROPERTY(EditAnywhere, config, Category = "Pawn")
	FName PitchLimitMin = TEXT("PitchLimit_Min");

	UPROPERTY(EditAnywhere, config, Category = "Pawn")
	FName PitchLimitMax = TEXT("PitchLimit_Max");

	/** Zoom chegaralari. Topilmasa cheklanmaydi. */
	UPROPERTY(EditAnywhere, config, Category = "Pawn")
	FName ArmLengthMin = TEXT("SpringArm_Length_Min");

	UPROPERTY(EditAnywhere, config, Category = "Pawn")
	FName ArmLengthMax = TEXT("SpringArm_Length_Max");

	/** Pawn ning sichqoncha/barmoq burilishini qabul qiladigan funksiyasi (nomida bo'sh joy bor). */
	UPROPERTY(EditAnywhere, config, Category = "Pawn")
	FName TouchRotationFunction = TEXT("Touch Rotation");

	// ---- Blueprint sinflari --------------------------------------------------------------
	// Sinflar nom BOSHLANISHI bo'yicha topiladi, shuning uchun "BP_POI" "BP_POI_Unit_C" ga
	// ham tushadi.

	UPROPERTY(EditAnywhere, config, Category = "Sinflar")
	FString PoiClassPrefix = TEXT("BP_POI");

	UPROPERTY(EditAnywhere, config, Category = "Sinflar")
	FString BuildingMarkerClassPrefix = TEXT("BP_BuildingSectionMarker");

	UPROPERTY(EditAnywhere, config, Category = "Sinflar")
	FString FloorMarkerClassPrefix = TEXT("BP_FloorSectionMarker");

	UPROPERTY(EditAnywhere, config, Category = "Sinflar")
	FString GalleryWidgetClassPrefix = TEXT("BP_Gallery_Widget");

	// ---- POI ma'lumoti -------------------------------------------------------------------

	/** POI aktyoridagi struktura o'zgaruvchisi; ko'rinadigan nom shundan o'qiladi. */
	UPROPERTY(EditAnywhere, config, Category = "POI")
	FString PoiInfoStructPrefix = TEXT("POI_Info_Struct");

	/**
	 * Struktura ichidagi nom maydoni. Turi String, Text yoki Name bo'lishi mumkin -
	 * uchalasi ham o'qiladi, chunki faqat String o'qilganda ro'yxat bo'sh chiqqan edi.
	 */
	UPROPERTY(EditAnywhere, config, Category = "POI")
	FName PoiNameField = TEXT("Name");

	// ---- Qirqim ---------------------------------------------------------------------------

	/** Marker aktyoridagi qirqim hajmi (box). */
	UPROPERTY(EditAnywhere, config, Category = "Qirqim")
	FName SectionVolumeProperty = TEXT("SectionView_Volume");

	/** PlayerController dagi qirqim niqobini qo'llaydigan funksiya. */
	UPROPERTY(EditAnywhere, config, Category = "Qirqim")
	FName SectionMaskFunction = TEXT("SectionView_Mask");

	// ---- Galereya --------------------------------------------------------------------------
	// Galereya C++ da emas, Blueprint da. Bir nechta nusxasi bir vaqtda ochiq bo'lishi
	// mumkin (menyudagi "Galereya" va xonadon kartasidagi REJA) - shunda ko'rinadiganlar
	// orasidan eng oxirgi ochilgani boshqariladi.

	/** Ochilganda quriladigan preview vidjetlari massivi. Rasmlar soni shundan sanaladi. */
	UPROPERTY(EditAnywhere, config, Category = "Galereya")
	FName GalleryPreviewArray = TEXT("GalleryPreview_Widgets");

	/** Hozir ko'rsatilayotgan rasmning tartib raqami. */
	UPROPERTY(EditAnywhere, config, Category = "Galereya")
	FName GalleryIndexProperty = TEXT("Current_GalleryPreview_Index");

	/** Yopish hodisasi. Ro'yxatdagilar navbat bilan sinaladi. */
	UPROPERTY(EditAnywhere, config, Category = "Galereya")
	TArray<FName> GalleryCloseFunctions = { TEXT("Close_Gallery"), TEXT("Close Gallery") };

	/**
	 * Rasmni almashtirish hodisasi. Parametr sifatida PREVIEW VIDJETINI oladi, indeksni emas.
	 * Ro'yxatdagilarning HAMMASI chaqiriladi: bu Blueprint da bir xil imzoli ikkita nom bor
	 * va faqat bittasi haqiqiy ish qiladi - qaysi biri ekani tashqaridan bilinmaydi.
	 */
	UPROPERTY(EditAnywhere, config, Category = "Galereya")
	TArray<FName> GalleryUpdateFunctions = { TEXT("Update_Gallery_Event"), TEXT("UpdateGallery_Event") };

	// ---- Panel -----------------------------------------------------------------------------
	// Bularni operator paneli RemoteGetConfig orqali o'qiydi, ya'ni telefondagi tugmalar ham
	// shu sahifadan boshqariladi va panel HTML ida hech narsa qattiq yozilmaydi.

	/**
	 * Kategoriya panellarining teglari. Aktyorda bulardan tashqari qanday teg bo'lsa,
	 * o'sha kategoriya deb hisoblanadi - shuning uchun ro'yxat kategoriyalarni emas,
	 * ULARNI AJRATIB olish uchun kerak bo'lgan panel teglarini sanaydi.
	 */
	UPROPERTY(EditAnywhere, config, Category = "Panel")
	TArray<FName> PanelTags = { TEXT("Surroundings"), TEXT("Amenities") };

	/** Interyer xonalari sifatida qaraladigan PlayerStart larning AKTYOR tegi. */
	UPROPERTY(EditAnywhere, config, Category = "Panel")
	FName InteriorTag = TEXT("room");

	/** Progulka nuqtalari sifatida qaraladigan PlayerStart larning AKTYOR tegi. */
	UPROPERTY(EditAnywhere, config, Category = "Panel")
	FName WalkTag = TEXT("walk");

	/** Xonadon kartasidagi tugmalar. */
	UPROPERTY(EditAnywhere, config, Category = "Panel")
	TArray<FLanessaCardAction> CardActions;

	// ---- Ko'p ekranli rejim (nDisplay) ---------------------------------------------------

	/**
	 * 180/360 yoki ko'p ekranli rejimda ekrandagi interfeysni butunlay o'chiradi -
	 * ekranda faqat sahna qoladi, boshqaruv esa operator panelida bo'ladi.
	 *
	 * Rejim -dc_cluster bayrog'i bo'yicha aniqlanadi, ya'ni oddiy bitta ekranli
	 * ishga tushirishga TA'SIR QILMAYDI. O'chirib qo'ysangiz, menyu ekranlar
	 * orasiga bo'linib chiqadi: o'lchangan - logotip va qavat tugmalari birinchi
	 * ekranda, soat kartasi va POI kartasi ikkinchisida qolgan.
	 */
	UPROPERTY(EditAnywhere, config, Category = "Ko'p ekran")
	bool bHideUiInClusterMode = true;

	// ---- Editor ------------------------------------------------------------------------------

	/**
	 * Editorning "Use Less CPU when in Background" sozlamasini o'chirib qo'yadi.
	 *
	 * Remote Control buyruqni KADRGA BITTA bajaradi, ya'ni editorning kadr tezligi -
	 * API ning o'tkazuvchanlik chegarasi. Telefondan boshqarilganda editor oynasi hech
	 * qachon fokusda bo'lmaydi, shuning uchun u ~3 FPS ga tushadi va aynan boshqarish
	 * paytida qotadi: o'lchangan - bitta chaqiruv 333 ms, o'chirilgandan keyin 16 ms.
	 *
	 * Plagin buni ishga tushganda o'zi qo'yadi, ya'ni EditorSettings.ini ni qo'lda
	 * tahrirlash kerak emas. Noutbukda batareya tejash muhim bo'lsa - shu yerda
	 * o'chiring, sozlama o'z holiga qaytadi.
	 */
	UPROPERTY(EditAnywhere, config, Category = "Editor")
	bool bDisableBackgroundThrottle = true;

	ULanessaRemoteSettings();

	/** Hech qachon null emas - UDeveloperSettings doim xotirada turadi. */
	static const ULanessaRemoteSettings& Get();

	/** Operator paneli uchun JSON. Panel shundan teglar va tugmalarni oladi. */
	FString ToJson() const;
};
