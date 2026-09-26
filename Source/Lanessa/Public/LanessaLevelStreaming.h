#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Subsystems/WorldSubsystem.h"
#include "LanessaLevelStreaming.generated.h"

class ULevelStreaming;

/**
 * Menyudagi sahifalar. Nomlari ULanessaV2Widget::BuildRail dagi sahifa id lari bilan bir xil
 * (katta-kichik harf farqi hisobga olinmaydi): "home", "atrofi", "vr" va h.k. Sozlamada satr
 * emas, ro'yxat bo'lishi uchun enum - id ni qo'lda yozib xato qilib bo'lmaydi.
 */
UENUM(BlueprintType)
enum class ELanessaPage : uint8
{
	Home     UMETA(DisplayName = "Bosh sahifa"),
	Atrofi   UMETA(DisplayName = "Atrofi"),
	Qulay    UMETA(DisplayName = "Qulayliklar"),
	Qidiruv  UMETA(DisplayName = "Qidiruv"),
	Qirqim   UMETA(DisplayName = "Qirqim"),
	Galereya UMETA(DisplayName = "Galereya"),
	VR       UMETA(DisplayName = "VR progulka"),
	Sayr     UMETA(DisplayName = "Progulka"),
	Interyer UMETA(DisplayName = "Interyer"),
	// QURILISH sahifasi va uning panelidagi bosqich tugmalari. Id lari ham shu nomlarning
	// kichik harfdagi ko'rinishi: "qurilish", "kotlovan", "yerosti", "qurilishetapi"...
	Qurilish      UMETA(DisplayName = "Qurilish"),
	Kotlovan      UMETA(DisplayName = "Qurilish: Kotlovan"),
	YerOsti       UMETA(DisplayName = "Qurilish: Yer osti qavati"),
	Karkas        UMETA(DisplayName = "Qurilish: Karkas"),
	Devor         UMETA(DisplayName = "Qurilish: Devor"),
	Fasad         UMETA(DisplayName = "Qurilish: Fasad"),
	QurilishEtapi UMETA(DisplayName = "Qurilish: Qurilish etapi"),
	// Interyer rejimi (POI -> 3D TUR). Bu ikkalasi QO'SHIMCHA: ro'yxatdagilar interyer leveliga
	// qo'shib ochiladi, boshqa levellar yopilmaydi. Interyerning o'zini BP yuklaydi.
	Interyer3DTur  UMETA(DisplayName = "Interyer: 3D tur (qo'shimcha levellar)"),
	InteryerQirqim UMETA(DisplayName = "Interyer: Qirqim (qo'shimcha levellar)"),
};

/** Bitta sahifa va unda ochiq turadigan sub levellar. */
USTRUCT(BlueprintType)
struct FLanessaPageLevels
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Lanessa")
	ELanessaPage Page = ELanessaPage::Home;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Lanessa")
	TArray<TSoftObjectPtr<UWorld>> Levels;

	/** Kamida bitta level tanlanganmi. Bo'sh sahifa sozlanmagan hisoblanadi. */
	bool HasLevels() const
	{
		for (const TSoftObjectPtr<UWorld>& L : Levels) { if (!L.IsNull()) { return true; } }
		return false;
	}
};

/**
 * Project Settings -> Plugins -> Lanessa Level Streaming.
 *
 * Har bir sahifa uchun qaysi sub levellar ochiq turishi. Sahifa bosilganda uning ro'yxatidagi
 * levellar ko'rsatiladi, ro'yxatda YO'Q har qanday sub level yopiladi - kiritilmagan level
 * ochilmaydi. Jadvalda umuman yo'q sahifa bosilsa levellarga tegilmaydi: hali sozlanmagan
 * sahifa sahnani bo'shatib qo'ymasin.
 *
 * Levels oynasida "Always Loaded" qilib qo'yilgan sub levellarga tegilmaydi - doim turishi
 * kerak bo'lgan narsalar (yorug'lik, landshaft) uchun shu usul. Boshqarilishi kerak bo'lgan
 * sub levellar "Blueprint" streaming usulida bo'lishi kerak.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Lanessa Level Streaming"))
class LANESSA_API ULanessaLevelStreamingSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	/** O'chirilsa plagin levellarga umuman tegmaydi. */
	UPROPERTY(EditAnywhere, config, Category = "Streaming")
	bool bEnabled = true;

	/** O'yin boshlanganda qaysi sahifaning levellari ochiladi. */
	UPROPERTY(EditAnywhere, config, Category = "Streaming")
	ELanessaPage StartPage = ELanessaPage::Home;

	/**
	 * Yoqilgan bo'lsa jadvaldagi HAMMA levellar o'yin boshida xotiraga yuklanadi va keyin faqat
	 * ko'rsatiladi/yashiriladi. Sahifa almashishi shunda sezilmaydi, chunki diskdan o'qish
	 * yo'q. Evaziga ko'proq xotira (RAM/VRAM) ketadi. O'chirilsa yashirilgan level xotiradan
	 * ham chiqariladi va keyingi safar diskdan qayta yuklanadi.
	 *
	 * Jadvalda yo'q levellar (masalan POI orqali ochiladigan interyerlar) bunga kirmaydi -
	 * ular sahifa almashganda doim xotiradan chiqariladi.
	 */
	UPROPERTY(EditAnywhere, config, Category = "Streaming")
	bool bKeepLoadedInMemory = true;

	/**
	 * Yoqilgan bo'lsa almashish bitta kadrda bajariladi: yangi levellar to'liq chiqadi va
	 * eskilari shu kadrning o'zida yo'qoladi - bo'lak-bo'lak paydo bo'lish yoki bo'sh sahna
	 * ko'rinmaydi. Evaziga bosish paytida qisqa qotish bo'lishi mumkin; levellar xotirada
	 * turgan bo'lsa (yuqoridagi sozlama) u odatda sezilmaydi.
	 *
	 * O'chirilsa almashish fonda ketadi: eski levellar yangilari to'liq chiqmaguncha
	 * turaveradi, keyin yopiladi.
	 */
	UPROPERTY(EditAnywhere, config, Category = "Streaming")
	bool bInstantSwitch = true;

	/**
	 * Sahifa -> shu sahifada ochiq turadigan sub levellar. Oddiy ro'yxat, Map emas: Map da yangi
	 * qator doim "Bosh sahifa" kaliti bilan qo'shilar va u allaqachon bo'lsa Unreal qo'shishni
	 * rad etardi ("Duplicate keys are not allowed"). Bitta sahifa ikki marta kiritilsa birinchisi
	 * olinadi. Levellari bo'sh qator sozlanmagan hisoblanadi - bosilganda levellarga tegilmaydi.
	 */
	UPROPERTY(EditAnywhere, config, Category = "Streaming", meta = (DisplayName = "Page Levels", TitleProperty = "Page"))
	TArray<FLanessaPageLevels> Pages;

	/**
	 * Plagin HECH QACHON ochmaydigan va yopmaydigan levellar - ularni boshqa narsa boshqaradi.
	 * BP_Explorer_PC ning asosiy leveli (MainLevelName o'zgaruvchisi) bu ro'yxatsiz ham
	 * avtomatik tanib olinadi: uni BP interyerga kirganda yopib, chiqqanda qayta ochadi, va
	 * plagin ham unga tegsa ikkalasi bir-birining ishini buzardi.
	 */
	UPROPERTY(EditAnywhere, config, Category = "Streaming")
	TArray<TSoftObjectPtr<UWorld>> UnmanagedLevels;

	static const ULanessaLevelStreamingSettings& Get();

	/** Sahifaning qatori - faqat kamida bitta level tanlangan bo'lsa, aks holda null. */
	const FLanessaPageLevels* FindPage(ELanessaPage InPage) const
	{
		for (const FLanessaPageLevels& Entry : Pages)
		{
			if (Entry.Page == InPage && Entry.HasLevels()) { return &Entry; }
		}
		return nullptr;
	}
};

/**
 * Sahifa bo'yicha sub levellarni ochib-yopadigan qism. Har bir o'yin dunyosida bittadan
 * bo'ladi; ULanessaV2Widget::SetActiveView har bir sahifa bosilganda shuni chaqiradi.
 */
UCLASS()
class LANESSA_API ULanessaLevelStreamingSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * PageId - menyudagi sahifa id si ("home", "sayr"...). Sahifa jadvalda bo'lmasa yoki
	 * streaming o'chirilgan bo'lsa false qaytaradi va hech narsani o'zgartirmaydi.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lanessa|Streaming")
	bool ApplyPageId(const FString& PageId);

	UFUNCTION(BlueprintCallable, Category = "Lanessa|Streaming")
	bool ApplyPage(ELanessaPage Page);

	/** Sahifa id sini enum ga o'giradi. Sahifa bo'lmasa false. */
	static bool PageFromId(const FString& PageId, ELanessaPage& OutPage);

	/**
	 * Boshqa levellarga tegmasdan faqat shularni ochadi yoki yopadi - qurilish animatsiyasi
	 * levellarni birin-ketin qo'shishi uchun. Instant Switch yoqilgan bo'lsa shu kadrning
	 * o'zida bajariladi.
	 */
	void SetLevelsVisible(const TArray<TSoftObjectPtr<UWorld>>& Levels, bool bVisible);

	/**
	 * FAQAT shu levellarni ko'rsatadi, qolgan boshqariladigan sub levellarni yashiradi (xotirada
	 * qoladi - qaytish tez bo'lsin). KeepShortNames dagilar (masalan BP yuklagan interyer) va
	 * Unmanaged / Always Loaded levellarga tegilmaydi. Qaytish uchun avvalgi ko'rinayotgan
	 * levellar ro'yxatini qaytaradi - uni RestoreVisible ga bering.
	 */
	TArray<TWeakObjectPtr<ULevelStreaming>> ShowOnly(const TArray<TSoftObjectPtr<UWorld>>& Levels, const TSet<FString>& KeepShortNames);

	/** ShowOnly dan oldingi holatni qaytaradi: o'shanda ko'ringanlar ko'rinadi, qolganlari yashiriladi. */
	void RestoreVisible(const TArray<TWeakObjectPtr<ULevelStreaming>>& Snapshot, const TSet<FString>& KeepShortNames);

	/** Level xaritada bo'lsa va xotiraga yuklangan bo'lsa - uning ULevel i, aks holda null. */
	class ULevel* GetLoadedLevel(const TSoftObjectPtr<UWorld>& Level) const;

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

private:
	// PIE da paket nomi "UEDPIE_0_" bilan boshlanadi - solishtirishdan oldin olib tashlanadi.
	static FName NormalizedPackage(const ULevelStreaming* Streaming);

	// Plagin tegmaydigan levellarning QISQA nomlari (UnmanagedLevels + BP ning MainLevelName i).
	// Qisqa nom, chunki BP levelni "Load Stream Level (by name)" bilan faqat nomi bo'yicha ochadi.
	TSet<FString> UnmanagedShortNames() const;

	// Jadvalning istalgan sahifasida uchraydigan levellar.
	static TSet<FName> AllListedPackages();

	// Fonda almashish: yangilari to'liq chiqqach eskilari yopiladi.
	TArray<TWeakObjectPtr<ULevelStreaming>> PendingShow;
	TArray<TWeakObjectPtr<ULevelStreaming>> PendingHide;
	float PendingTime = 0.f;

	// O'yin boshidagi yuklashni birinchi kadrda bajarish uchun (BeginPlay ichida emas).
	bool bFlushOnFirstTick = false;
	// BeginPlay davomida ApplyPage flush qilmasin - buni birinchi Tick bajaradi.
	bool bSuppressFlush = false;

	void FinishPendingHide();
};
