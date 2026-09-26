#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Subsystems/WorldSubsystem.h"
#include "LanessaLevelStreaming.h"
#include "LanessaConstruction.generated.h"

class UMaterialParameterCollection;
class UPrimitiveComponent;

/** Sahifa yoki qurilish bosqichi tugmasi bosilganda kamera qayerga uchadi (orbit pawn). */
USTRUCT(BlueprintType)
struct FLanessaCameraView
{
	GENERATED_BODY()

	/** O'chirilgan bo'lsa kamera joyida qoladi. */
	UPROPERTY(EditAnywhere, config, BlueprintReadWrite, Category = "Kamera")
	bool bMoveCamera = false;

	/** Aylanish markazi (BP_Explorer_Pawn ning Location_New i). */
	UPROPERTY(EditAnywhere, config, BlueprintReadWrite, Category = "Kamera", meta = (EditCondition = "bMoveCamera"))
	FVector Pivot = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, config, BlueprintReadWrite, Category = "Kamera", meta = (EditCondition = "bMoveCamera"))
	double Pitch = -25.0;

	UPROPERTY(EditAnywhere, config, BlueprintReadWrite, Category = "Kamera", meta = (EditCondition = "bMoveCamera"))
	double Yaw = 0.0;

	/** Kameraning markazdan masofasi (spring arm uzunligi). */
	UPROPERTY(EditAnywhere, config, BlueprintReadWrite, Category = "Kamera", meta = (EditCondition = "bMoveCamera"))
	double ArmLength = 6000.0;
};

/**
 * Project Settings -> Plugins -> Lanessa Qurilish.
 *
 * QURILISH sahifasi va uning bosqich tugmalari (Kotlovan, Yer osti qavati, Karkas, Devor,
 * Fasad, Qurilish etapi). Har bir bosqichning LEVELLARI bu yerda emas, "Lanessa Level
 * Streaming" jadvalida - o'sha sahifalar ro'yxatida Kotlovan, YerOsti va h.k. ham bor.
 * Bu yerda faqat kamera nuqtalari va "Qurilish etapi" animatsiyasi.
 *
 * Animatsiya qanday ishlaydi:
 *   0 s                         - Kotlovan levellari chiqadi
 *   YerOstiDelay                - Yer osti levellari chiqadi
 *   UpperStart                              - Karkas (tepa qavatlar) pastdan tepaga "tiklanadi"
 *   UpperStart + WallDelay                  - Devor ham shunday tiklanadi
 *   UpperStart + WallDelay + FacadeDelay    - Fasad ham shunday tiklanadi
 * Tiklanish balandligi qavat section volume laridan (BP_FloorSectionMarker) olinadi.
 * Tugagach bino to'liq holda qoladi.
 *
 * Tiklanish MATERIALDA bo'ladi: har bir obyektga Custom Primitive Data orqali "kanal"
 * yoziladi (1 = karkas, 2 = fasad, 3 = devor), material esa o'sha kanalning balandligini
 * RevealCollection dan o'qib, undan yuqorisini yashiradi. Kanal 0 bo'lgan (ya'ni
 * animatsiyadan tashqaridagi) obyektlarga hech narsa ta'sir qilmaydi.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Lanessa Qurilish"))
class LANESSA_API ULanessaConstructionSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	// ---- Kamera --------------------------------------------------------------------------------

	/**
	 * QURILISH sahifasi va har bir bosqich tugmasi uchun kamera. Faqat qurilish sahifalari
	 * (Qurilish, Kotlovan, YerOsti, Karkas, Devor, Fasad, QurilishEtapi) o'qiladi - qolgan
	 * sahifalarning kamerasini avvalgidek Blueprint boshqaradi.
	 */
	UPROPERTY(EditAnywhere, config, Category = "Kamera")
	TMap<ELanessaPage, FLanessaCameraView> Cameras;

	/** "Hozirgi kamerani yozish" tugmasi qaysi sahifaga yozadi. */
	UPROPERTY(EditAnywhere, Transient, Category = "Kamera")
	ELanessaPage CaptureFor = ELanessaPage::Qurilish;

	/**
	 * O'yin (PIE) ishlab turganda kamerani kerakli joyga olib boring, keyin shu tugmani
	 * bosing - hozirgi kamera holati CaptureFor sahifasiga yoziladi va saqlanadi.
	 */
	UFUNCTION(CallInEditor, Category = "Kamera", meta = (DisplayName = "Hozirgi kamerani yozish"))
	void CaptureCurrentCamera();

	// ---- Qurilish etapi animatsiyasi ------------------------------------------------------------

	UPROPERTY(EditAnywhere, config, Category = "Qurilish etapi")
	TArray<TSoftObjectPtr<UWorld>> KotlovanLevels;

	UPROPERTY(EditAnywhere, config, Category = "Qurilish etapi")
	TArray<TSoftObjectPtr<UWorld>> YerOstiLevels;

	UPROPERTY(EditAnywhere, config, Category = "Qurilish etapi")
	TArray<TSoftObjectPtr<UWorld>> UpperFloorLevels;

	/** Karkasdan keyin tiklanadigan devorlar. */
	UPROPERTY(EditAnywhere, config, Category = "Qurilish etapi", meta = (DisplayName = "Devor Levels"))
	TArray<TSoftObjectPtr<UWorld>> WallLevels;

	UPROPERTY(EditAnywhere, config, Category = "Qurilish etapi")
	TArray<TSoftObjectPtr<UWorld>> FacadeLevels;

	/** Kotlovandan keyin necha soniyada yer osti chiqadi. */
	UPROPERTY(EditAnywhere, config, Category = "Qurilish etapi", meta = (ClampMin = "0", Units = "s"))
	float YerOstiDelay = 1.5f;

	/** Boshidan necha soniyada tepa qavatlar tiklana boshlaydi. */
	UPROPERTY(EditAnywhere, config, Category = "Qurilish etapi", meta = (ClampMin = "0", Units = "s"))
	float UpperStart = 3.f;

	/** Karkas tiklana boshlagandan necha soniya keyin devor boshlanadi. */
	UPROPERTY(EditAnywhere, config, Category = "Qurilish etapi", meta = (ClampMin = "0", Units = "s", DisplayName = "Devor Delay"))
	float WallDelay = 2.f;

	/** Devor tiklana boshlagandan necha soniya keyin fasad boshlanadi. */
	UPROPERTY(EditAnywhere, config, Category = "Qurilish etapi", meta = (ClampMin = "0", Units = "s"))
	float FacadeDelay = 2.f;

	/** Bitta levelning pastdan tepagacha tiklanish vaqti. */
	UPROPERTY(EditAnywhere, config, Category = "Qurilish etapi", meta = (ClampMin = "0.1", Units = "s"))
	float RevealDuration = 10.f;

	// ---- Material ---------------------------------------------------------------------------------

	/**
	 * Tiklanish balandliklari yoziladigan Material Parameter Collection. Qirqimning o'z MPC si,
	 * alohida emas: material ko'pi bilan 2 ta MPC ishlata oladi va ko'p materiallar ikkalasini
	 * allaqachon band qilgan - uchinchisi qo'shilganda ular Default Material ga tushib qolgan.
	 */
	UPROPERTY(EditAnywhere, config, Category = "Material")
	TSoftObjectPtr<UMaterialParameterCollection> RevealCollection =
		TSoftObjectPtr<UMaterialParameterCollection>(FSoftObjectPath(TEXT("/Game/New_Explorer/Materials/MPC/SectionMask_MPC.SectionMask_MPC")));

	UPROPERTY(EditAnywhere, config, Category = "Material")
	FName UpperHeightParam = TEXT("UpperZ");

	UPROPERTY(EditAnywhere, config, Category = "Material")
	FName FacadeHeightParam = TEXT("FacadeZ");

	UPROPERTY(EditAnywhere, config, Category = "Material")
	FName WallHeightParam = TEXT("WallZ");

	/**
	 * Obyektning kanali yoziladigan Custom Primitive Data indeksi. Materialdagi parametrning
	 * "Primitive Data Index" i bilan bir xil bo'lishi shart.
	 */
	UPROPERTY(EditAnywhere, config, Category = "Material", meta = (ClampMin = "0", ClampMax = "35"))
	int32 ChannelDataIndex = 20;

	static const ULanessaConstructionSettings& Get();

	/** Animatsiyadagi hamma levellar - streaming ularni ham xotirada ushlab turishi uchun. */
	TArray<TSoftObjectPtr<UWorld>> AllAnimationLevels() const;
};

/** "Qurilish etapi" animatsiyasini o'ynatadigan va qurilish kameralarini qo'llaydigan qism. */
UCLASS()
class LANESSA_API ULanessaConstructionSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Lanessa|Qurilish")
	bool PlayConstruction();

	/** Animatsiyani to'xtatadi va hamma obyektni to'liq holatga qaytaradi. */
	UFUNCTION(BlueprintCallable, Category = "Lanessa|Qurilish")
	void StopConstruction();

	UFUNCTION(BlueprintPure, Category = "Lanessa|Qurilish")
	bool IsPlaying() const { return bPlaying; }

	/** Sahifaning kamerasi sozlangan bo'lsa (Lanessa Qurilish -> Cameras) kamerani o'sha yerga uchiradi. */
	UFUNCTION(BlueprintCallable, Category = "Lanessa|Qurilish")
	bool MoveCameraForPageId(const FString& PageId);

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual void Deinitialize() override;

private:
	/** Pastdan tepaga tiklanadigan bitta guruh (tepa qavatlar yoki fasad). */
	struct FRevealGroup
	{
		TArray<TSoftObjectPtr<UWorld>> Levels;
		FName HeightParam;
		float Channel = 0.f;
		float StartTime = 0.f;
		bool bStarted = false;
		bool bFinished = false;
		bool bGathered = false;
		// "level hali yuklanmagan" ogohlantirishi bir marta yozilsin, har kadrda emas.
		bool bWarnedNotLoaded = false;
		double BottomZ = 0.0;
		double TopZ = 0.0;
		TArray<TWeakObjectPtr<UPrimitiveComponent>> Components;
	};

	bool bPlaying = false;
	bool bYerOstiShown = false;
	float Time = 0.f;
	FRevealGroup Upper;
	FRevealGroup Wall;
	FRevealGroup Facade;

	void TickGroup(FRevealGroup& Group);
	bool GatherComponents(FRevealGroup& Group);
	void SetHeight(FName Param, double Z);
	void ClearGroup(FRevealGroup& Group);

	// Qavat section volume lari (Floor >= 1) bo'yicha tepa qavatlar diapazoni.
	bool FloorMarkersRange(double& OutBottom, double& OutTop) const;
};
