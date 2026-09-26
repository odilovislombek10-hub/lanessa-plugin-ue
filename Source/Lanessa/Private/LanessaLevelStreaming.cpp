#include "LanessaLevelStreaming.h"
#include "LanessaConstruction.h"
#include "Engine/World.h"
#include "Engine/LevelStreaming.h"
#include "Engine/LevelStreamingAlwaysLoaded.h"
#include "Misc/PackageName.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UnrealType.h"

// Fonda almashishda yangi levellar shuncha vaqtda chiqmasa, eskilari baribir yopiladi -
// topilmagan yoki buzilgan level sahnani abadiy ikki holat orasida qoldirmasin.
static constexpr float LanessaMaxPendingSeconds = 10.f;

const ULanessaLevelStreamingSettings& ULanessaLevelStreamingSettings::Get()
{
	const ULanessaLevelStreamingSettings* S = GetDefault<ULanessaLevelStreamingSettings>();
	check(S);
	return *S;
}

FName ULanessaLevelStreamingSubsystem::NormalizedPackage(const ULevelStreaming* Streaming)
{
	if (!Streaming) { return NAME_None; }
	return FName(UWorld::RemovePIEPrefix(Streaming->GetWorldAssetPackageName()));
}

TSet<FName> ULanessaLevelStreamingSubsystem::AllListedPackages()
{
	TSet<FName> Out;
	for (const FLanessaPageLevels& Entry : ULanessaLevelStreamingSettings::Get().Pages)
	{
		for (const TSoftObjectPtr<UWorld>& Level : Entry.Levels)
		{
			if (!Level.IsNull()) { Out.Add(FName(Level.GetLongPackageName())); }
		}
	}
	// Qurilish animatsiyasi levellari jadvalda bo'lmasa ham boshqariladi va xotirada turadi.
	for (const TSoftObjectPtr<UWorld>& Level : ULanessaConstructionSettings::Get().AllAnimationLevels())
	{
		if (!Level.IsNull()) { Out.Add(FName(Level.GetLongPackageName())); }
	}
	return Out;
}

TSet<FString> ULanessaLevelStreamingSubsystem::UnmanagedShortNames() const
{
	TSet<FString> Out;
	for (const TSoftObjectPtr<UWorld>& Level : ULanessaLevelStreamingSettings::Get().UnmanagedLevels)
	{
		if (!Level.IsNull()) { Out.Add(FPackageName::GetShortName(Level.GetLongPackageName())); }
	}

	// BP_Explorer_PC.MainLevelName - nom bo'yicha, turi String yoki Name bo'lishi mumkin.
	APlayerController* PC = GetWorld() ? UGameplayStatics::GetPlayerController(GetWorld(), 0) : nullptr;
	if (PC)
	{
		if (const FStrProperty* SP = FindFProperty<FStrProperty>(PC->GetClass(), TEXT("MainLevelName")))
		{
			const FString V = SP->GetPropertyValue_InContainer(PC);
			if (!V.IsEmpty()) { Out.Add(FPackageName::GetShortName(V)); }
		}
		else if (const FNameProperty* NP = FindFProperty<FNameProperty>(PC->GetClass(), TEXT("MainLevelName")))
		{
			const FName V = NP->GetPropertyValue_InContainer(PC);
			if (!V.IsNone()) { Out.Add(FPackageName::GetShortName(V.ToString())); }
		}
	}
	return Out;
}

bool ULanessaLevelStreamingSubsystem::PageFromId(const FString& PageId, ELanessaPage& OutPage)
{
	// Sahifa id si enum nomining o'zi, faqat kichik harfda ("vr" <-> VR).
	const UEnum* Enum = StaticEnum<ELanessaPage>();
	for (int32 i = 0; i < Enum->NumEnums() - 1; ++i)   // oxirgisi avtomatik _MAX
	{
		if (Enum->GetNameStringByIndex(i).Equals(PageId, ESearchCase::IgnoreCase))
		{
			OutPage = static_cast<ELanessaPage>(Enum->GetValueByIndex(i));
			return true;
		}
	}
	return false;
}

void ULanessaLevelStreamingSubsystem::SetLevelsVisible(const TArray<TSoftObjectPtr<UWorld>>& Levels, bool bVisible)
{
	UWorld* World = GetWorld();
	if (!World || Levels.Num() == 0) { return; }

	TSet<FName> Wanted;
	for (const TSoftObjectPtr<UWorld>& Level : Levels)
	{
		if (!Level.IsNull()) { Wanted.Add(FName(Level.GetLongPackageName())); }
	}
	const TSet<FName> KeepInMemory = ULanessaLevelStreamingSettings::Get().bKeepLoadedInMemory ? AllListedPackages() : TSet<FName>();

	bool bChanged = false;
	for (ULevelStreaming* Streaming : World->GetStreamingLevels())
	{
		if (!Streaming || Streaming->IsA<ULevelStreamingAlwaysLoaded>()) { continue; }
		const FName Package = NormalizedPackage(Streaming);
		if (!Wanted.Contains(Package)) { continue; }
		Streaming->SetShouldBeVisible(bVisible);
		Streaming->SetShouldBeLoaded(bVisible || KeepInMemory.Contains(Package));
		bChanged = true;
	}
	if (bChanged && ULanessaLevelStreamingSettings::Get().bInstantSwitch)
	{
		World->FlushLevelStreaming(EFlushLevelStreamingType::Full);
	}
}

TArray<TWeakObjectPtr<ULevelStreaming>> ULanessaLevelStreamingSubsystem::ShowOnly(const TArray<TSoftObjectPtr<UWorld>>& Levels, const TSet<FString>& KeepShortNames)
{
	TArray<TWeakObjectPtr<ULevelStreaming>> Snapshot;
	UWorld* World = GetWorld();
	if (!World) { return Snapshot; }

	TSet<FName> Wanted;
	for (const TSoftObjectPtr<UWorld>& Level : Levels)
	{
		if (!Level.IsNull()) { Wanted.Add(FName(Level.GetLongPackageName())); }
	}
	const TSet<FString> Unmanaged = UnmanagedShortNames();

	for (ULevelStreaming* Streaming : World->GetStreamingLevels())
	{
		if (!Streaming || Streaming->IsA<ULevelStreamingAlwaysLoaded>()) { continue; }
		const FName Package = NormalizedPackage(Streaming);
		const FString Short = FPackageName::GetShortName(Package.ToString());
		if (Unmanaged.Contains(Short) || KeepShortNames.Contains(Short)) { continue; }

		if (Streaming->GetShouldBeVisibleFlag()) { Snapshot.Add(Streaming); }
		if (Wanted.Contains(Package))
		{
			Streaming->SetShouldBeLoaded(true);
			Streaming->SetShouldBeVisible(true);
		}
		else if (Streaming->GetShouldBeVisibleFlag())
		{
			Streaming->SetShouldBeVisible(false);   // xotirada qoladi: qaytishda darhol ko'rinadi
		}
	}
	World->FlushLevelStreaming(EFlushLevelStreamingType::Full);
	UE_LOG(LogTemp, Log, TEXT("[LanessaStreaming] faqat %d ta level ko'rsatildi, %d ta avvalgi holat saqlandi"), Wanted.Num(), Snapshot.Num());
	return Snapshot;
}

void ULanessaLevelStreamingSubsystem::RestoreVisible(const TArray<TWeakObjectPtr<ULevelStreaming>>& Snapshot, const TSet<FString>& KeepShortNames)
{
	UWorld* World = GetWorld();
	if (!World) { return; }

	TSet<ULevelStreaming*> WasVisible;
	for (const TWeakObjectPtr<ULevelStreaming>& Weak : Snapshot) { if (Weak.IsValid()) { WasVisible.Add(Weak.Get()); } }
	const TSet<FString> Unmanaged = UnmanagedShortNames();
	const TSet<FName> KeepInMemory = ULanessaLevelStreamingSettings::Get().bKeepLoadedInMemory ? AllListedPackages() : TSet<FName>();

	for (ULevelStreaming* Streaming : World->GetStreamingLevels())
	{
		if (!Streaming || Streaming->IsA<ULevelStreamingAlwaysLoaded>()) { continue; }
		const FName Package = NormalizedPackage(Streaming);
		const FString Short = FPackageName::GetShortName(Package.ToString());
		if (Unmanaged.Contains(Short) || KeepShortNames.Contains(Short)) { continue; }

		if (WasVisible.Contains(Streaming))
		{
			Streaming->SetShouldBeLoaded(true);
			Streaming->SetShouldBeVisible(true);
		}
		else if (Streaming->GetShouldBeVisibleFlag())
		{
			Streaming->SetShouldBeVisible(false);
			Streaming->SetShouldBeLoaded(KeepInMemory.Contains(Package));
		}
	}
	World->FlushLevelStreaming(EFlushLevelStreamingType::Full);
}

ULevel* ULanessaLevelStreamingSubsystem::GetLoadedLevel(const TSoftObjectPtr<UWorld>& Level) const
{
	UWorld* World = GetWorld();
	if (!World || Level.IsNull()) { return nullptr; }
	const FName Want(Level.GetLongPackageName());
	for (ULevelStreaming* Streaming : World->GetStreamingLevels())
	{
		if (Streaming && NormalizedPackage(Streaming) == Want) { return Streaming->GetLoadedLevel(); }
	}
	return nullptr;
}

bool ULanessaLevelStreamingSubsystem::ApplyPageId(const FString& PageId)
{
	ELanessaPage Page;
	if (PageFromId(PageId, Page)) { return ApplyPage(Page); }
	return false;   // "none" va boshqa sahifa bo'lmagan holatlar - levellarga tegilmaydi
}

bool ULanessaLevelStreamingSubsystem::ApplyPage(ELanessaPage Page)
{
	const ULanessaLevelStreamingSettings& Cfg = ULanessaLevelStreamingSettings::Get();
	UWorld* World = GetWorld();
	if (!Cfg.bEnabled || !World) { return false; }

	const FLanessaPageLevels* Entry = Cfg.FindPage(Page);
	if (!Entry)
	{
		UE_LOG(LogTemp, Log, TEXT("[LanessaStreaming] '%s' sahifasi jadvalda yo'q yoki bo'sh - levellarga tegilmadi"),
			*StaticEnum<ELanessaPage>()->GetNameStringByValue((int64)Page));
		return false;
	}

	TSet<FName> Wanted;
	for (const TSoftObjectPtr<UWorld>& Level : Entry->Levels)
	{
		if (!Level.IsNull()) { Wanted.Add(FName(Level.GetLongPackageName())); }
	}
	// Oldingi fonda almashish tugamagan bo'lsa, u bekor - holat quyida noldan hisoblanadi.
	PendingShow.Reset();
	PendingHide.Reset();
	PendingTime = 0.f;

	TSet<FName> Found;
	const TSet<FString> Unmanaged = UnmanagedShortNames();
	for (ULevelStreaming* Streaming : World->GetStreamingLevels())
	{
		if (!Streaming || Streaming->IsA<ULevelStreamingAlwaysLoaded>()) { continue; }

		const FName Package = NormalizedPackage(Streaming);
		if (Unmanaged.Contains(FPackageName::GetShortName(Package.ToString()))) { continue; }
		if (Wanted.Contains(Package))
		{
			Found.Add(Package);
			Streaming->SetShouldBeLoaded(true);
			Streaming->SetShouldBeVisible(true);
			PendingShow.Add(Streaming);
		}
		else if (Streaming->ShouldBeLoaded() || Streaming->GetShouldBeVisibleFlag())
		{
			PendingHide.Add(Streaming);
		}
	}

	for (const FName& Package : Wanted)
	{
		if (!Found.Contains(Package))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[LanessaStreaming] '%s' bu xaritaning sub leveli emas (Levels oynasiga qo'shilmagan) - o'tkazib yuborildi"),
				*Package.ToString());
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[LanessaStreaming] %s: %d ochiladi, %d yopiladi"),
		*StaticEnum<ELanessaPage>()->GetNameStringByValue((int64)Page), PendingShow.Num(), PendingHide.Num());

	if (Cfg.bInstantSwitch)
	{
		// Yopish ham shu yerda belgilanadi va bitta flush hammasini bir kadrda bajaradi:
		// yangi levellar to'liq chiqadi, eskilari yo'qoladi, oraliq holat ekranga chizilmaydi.
		FinishPendingHide();
		PendingShow.Reset();
		if (!bSuppressFlush) { World->FlushLevelStreaming(EFlushLevelStreamingType::Full); }
	}
	else if (PendingShow.Num() == 0)
	{
		FinishPendingHide();
	}
	// Aks holda Tick yangilari chiqishini kutadi.
	return true;
}

void ULanessaLevelStreamingSubsystem::FinishPendingHide()
{
	const ULanessaLevelStreamingSettings& Cfg = ULanessaLevelStreamingSettings::Get();
	const TSet<FName> KeepInMemory = Cfg.bKeepLoadedInMemory ? AllListedPackages() : TSet<FName>();

	for (const TWeakObjectPtr<ULevelStreaming>& Weak : PendingHide)
	{
		ULevelStreaming* Streaming = Weak.Get();
		if (!Streaming) { continue; }
		Streaming->SetShouldBeVisible(false);
		// Jadvaldagi level xotirada qoladi (keyingi safar darhol chiqishi uchun),
		// jadvalda yo'qlari - interyerlar va boshqalar - to'liq chiqariladi.
		Streaming->SetShouldBeLoaded(KeepInMemory.Contains(NormalizedPackage(Streaming)));
	}
	PendingHide.Reset();
}

void ULanessaLevelStreamingSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	const ULanessaLevelStreamingSettings& Cfg = ULanessaLevelStreamingSettings::Get();
	if (!Cfg.bEnabled || Cfg.Pages.Num() == 0) { return; }

	// Bu xaritada jadvaldagi levellardan birortasi ham bo'lmasa (masalan bosh menyu xaritasi),
	// uning o'z sub levellariga tegilmaydi.
	const TSet<FName> Listed = AllListedPackages();
	bool bAnyListedHere = false;
	for (ULevelStreaming* Streaming : InWorld.GetStreamingLevels())
	{
		if (Listed.Contains(NormalizedPackage(Streaming))) { bAnyListedHere = true; break; }
	}
	if (!bAnyListedHere) { return; }

	if (Cfg.bKeepLoadedInMemory)
	{
		// Hamma jadvaldagi levellar yashirin holda xotiraga - keyin sahifa almashishi faqat
		// ko'rsatish/yashirishdan iborat bo'ladi.
		for (ULevelStreaming* Streaming : InWorld.GetStreamingLevels())
		{
			if (Streaming && Listed.Contains(NormalizedPackage(Streaming))) { Streaming->SetShouldBeLoaded(true); }
		}
	}

	bSuppressFlush = true;
	ApplyPage(Cfg.StartPage);
	bSuppressFlush = false;
	// Birinchi kadrgacha hammasi yuklanib bo'lsin - mijoz levellar birin-ketin paydo
	// bo'lishini ko'rmasin. BeginPlay ichida emas, birinchi Tick da: shu payt dunyo
	// to'liq ishga tushgan bo'ladi.
	bFlushOnFirstTick = true;
}

void ULanessaLevelStreamingSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bFlushOnFirstTick)
	{
		bFlushOnFirstTick = false;
		if (UWorld* World = GetWorld()) { World->FlushLevelStreaming(EFlushLevelStreamingType::Full); }
	}

	if (PendingShow.Num() == 0) { return; }

	PendingTime += DeltaTime;
	bool bAllVisible = true;
	for (const TWeakObjectPtr<ULevelStreaming>& Weak : PendingShow)
	{
		const ULevelStreaming* Streaming = Weak.Get();
		if (Streaming && !Streaming->IsLevelVisible()) { bAllVisible = false; break; }
	}

	if (bAllVisible || PendingTime > LanessaMaxPendingSeconds)
	{
		if (!bAllVisible)
		{
			UE_LOG(LogTemp, Warning, TEXT("[LanessaStreaming] yangi levellar %.0f s da chiqmadi - eskilari baribir yopildi"),
				LanessaMaxPendingSeconds);
		}
		PendingShow.Reset();
		FinishPendingHide();
	}
}

TStatId ULanessaLevelStreamingSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULanessaLevelStreamingSubsystem, STATGROUP_Tickables);
}
