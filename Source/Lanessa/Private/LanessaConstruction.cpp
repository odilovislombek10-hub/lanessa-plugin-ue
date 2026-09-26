#include "LanessaConstruction.h"
#include "LanessaLevelStreaming.h"
#include "LanessaRemoteSettings.h"
#include "LanessaV2Widget.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"

// Kanal 0 bo'lgan obyektga material tegmaydi, shuning uchun bu balandlik faqat "hech
// kim qirqilmasin" degan xavfsiz holat - kanal tozalanmay qolgan obyekt ham to'liq ko'rinadi.
static constexpr double LanessaRevealOff = 1.0e7;

// ==== Sozlamalar ====================================================================

const ULanessaConstructionSettings& ULanessaConstructionSettings::Get()
{
	const ULanessaConstructionSettings* S = GetDefault<ULanessaConstructionSettings>();
	check(S);
	return *S;
}

TArray<TSoftObjectPtr<UWorld>> ULanessaConstructionSettings::AllAnimationLevels() const
{
	TArray<TSoftObjectPtr<UWorld>> Out;
	Out.Append(KotlovanLevels);
	Out.Append(YerOstiLevels);
	Out.Append(UpperFloorLevels);
	Out.Append(WallLevels);
	Out.Append(FacadeLevels);
	return Out;
}

void ULanessaConstructionSettings::CaptureCurrentCamera()
{
	FVector Pivot;
	double Pitch, Yaw, Arm;
	if (!ULanessaV2Widget::RemoteCameraGet(Pivot, Pitch, Yaw, Arm))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LanessaQurilish] Kamera yozilmadi: o'yin (Play) ishlamayapti"));
		return;
	}

	FLanessaCameraView& View = Cameras.FindOrAdd(CaptureFor);
	View.bMoveCamera = true;
	View.Pivot = Pivot;
	View.Pitch = Pitch;
	View.Yaw = Yaw;
	View.ArmLength = Arm;

#if WITH_EDITOR
	TryUpdateDefaultConfigFile();
#endif
	UE_LOG(LogTemp, Log, TEXT("[LanessaQurilish] %s kamerasi yozildi: %s, pitch %.1f, yaw %.1f, masofa %.0f"),
		*StaticEnum<ELanessaPage>()->GetNameStringByValue((int64)CaptureFor), *Pivot.ToString(), Pitch, Yaw, Arm);
}

// ==== Subsystem =====================================================================

static ULanessaLevelStreamingSubsystem* LanessaStreaming(UWorld* World)
{
	return World ? World->GetSubsystem<ULanessaLevelStreamingSubsystem>() : nullptr;
}

bool ULanessaConstructionSubsystem::MoveCameraForPageId(const FString& PageId)
{
	ELanessaPage Page;
	if (!ULanessaLevelStreamingSubsystem::PageFromId(PageId, Page)) { return false; }
	// Qolgan sahifalarning kamerasi Blueprint da (OnNavClicked) - ikki joy bir kamerani
	// talashmasin.
	if ((uint8)Page < (uint8)ELanessaPage::Qurilish) { return false; }

	const FLanessaCameraView* View = ULanessaConstructionSettings::Get().Cameras.Find(Page);
	if (!View || !View->bMoveCamera) { return false; }

	APawn* Pawn = GetWorld() ? UGameplayStatics::GetPlayerPawn(GetWorld(), 0) : nullptr;
	if (!Pawn) { return false; }
	ULanessaV2Widget::SetExplorerPawnCameraTarget(Pawn, View->Pivot, View->Pitch, View->Yaw, View->ArmLength);
	return true;
}

bool ULanessaConstructionSubsystem::FloorMarkersRange(double& OutBottom, double& OutTop) const
{
	UWorld* World = GetWorld();
	if (!World) { return false; }

	const ULanessaRemoteSettings& Remote = ULanessaRemoteSettings::Get();
	// Floor >= 1 - tepa qavatlar. Qavat raqamlari umuman qo'yilmagan bo'lsa (hammasi 0),
	// hamma qavat markeri olinadi.
	double AboveBottom = TNumericLimits<double>::Max(), AboveTop = -TNumericLimits<double>::Max();
	double AllBottom = AboveBottom, AllTop = AboveTop;
	bool bAbove = false, bAny = false;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Marker = *It;
		if (!IsValid(Marker) || !Marker->GetClass()->GetName().StartsWith(Remote.FloorMarkerClassPrefix, ESearchCase::IgnoreCase))
		{
			continue;
		}

		// Qirqim qutisi - markerning SectionView_Volume i (qirqim ham shundan foydalanadi).
		AActor* Box = nullptr;
		if (const FObjectProperty* Prop = FindFProperty<FObjectProperty>(Marker->GetClass(), Remote.SectionVolumeProperty))
		{
			Box = Cast<AActor>(Prop->GetObjectPropertyValue_InContainer(Marker));
		}
		if (!Box) { continue; }

		FVector Origin, Extent;
		Box->GetActorBounds(false, Origin, Extent);
		const double Bottom = Origin.Z - Extent.Z;
		const double Top = Origin.Z + Extent.Z;

		int32 Floor = 0;
		FString Building;
		ULanessaV2Widget::GetOwnFloorAndBuilding(Marker, Floor, Building);

		bAny = true;
		AllBottom = FMath::Min(AllBottom, Bottom);
		AllTop = FMath::Max(AllTop, Top);
		if (Floor >= 1)
		{
			bAbove = true;
			AboveBottom = FMath::Min(AboveBottom, Bottom);
			AboveTop = FMath::Max(AboveTop, Top);
		}
	}

	if (bAbove) { OutBottom = AboveBottom; OutTop = AboveTop; return true; }
	if (bAny)   { OutBottom = AllBottom;   OutTop = AllTop;   return true; }
	return false;
}

bool ULanessaConstructionSubsystem::PlayConstruction()
{
	StopConstruction();

	UWorld* World = GetWorld();
	ULanessaLevelStreamingSubsystem* Streaming = LanessaStreaming(World);
	if (!World || !Streaming) { return false; }
	const ULanessaConstructionSettings& Cfg = ULanessaConstructionSettings::Get();

	// Sahifaning o'z levellari (landshaft, atrof va h.k.) - jadvalda bo'lsa.
	Streaming->ApplyPage(ELanessaPage::QurilishEtapi);

	// Boshlang'ich holat: faqat kotlovan. Qolganlari jadvaldan qat'i nazar yopiladi,
	// aks holda oldingi sahifadan qolgan fasad animatsiyadan oldin ko'rinib turardi.
	TArray<TSoftObjectPtr<UWorld>> Later = Cfg.YerOstiLevels;
	Later.Append(Cfg.UpperFloorLevels);
	Later.Append(Cfg.WallLevels);
	Later.Append(Cfg.FacadeLevels);
	// Boshqa bosqich sahifalarining levellari ham (Karkas, Devor...) - Qurilish etapi sahifasi
	// jadvalda bo'lmasa ApplyPage hech narsani yopmaydi va KARKAS dan keyin bosilgan etapda
	// karkas animatsiya ustida turib qolardi. Etap sahifasining o'z ro'yxatidagilar bundan mustasno.
	const ULanessaLevelStreamingSettings& StreamCfg = ULanessaLevelStreamingSettings::Get();
	TSet<FName> EtapOwn;
	if (const FLanessaPageLevels* Etap = StreamCfg.FindPage(ELanessaPage::QurilishEtapi))
	{
		for (const TSoftObjectPtr<UWorld>& L : Etap->Levels) { EtapOwn.Add(FName(L.GetLongPackageName())); }
	}
	for (ELanessaPage Stage : { ELanessaPage::Kotlovan, ELanessaPage::YerOsti, ELanessaPage::Karkas, ELanessaPage::Devor, ELanessaPage::Fasad })
	{
		if (const FLanessaPageLevels* Entry = StreamCfg.FindPage(Stage))
		{
			for (const TSoftObjectPtr<UWorld>& L : Entry->Levels)
			{
				if (!L.IsNull() && !EtapOwn.Contains(FName(L.GetLongPackageName()))) { Later.Add(L); }
			}
		}
	}
	Streaming->SetLevelsVisible(Later, false);
	Streaming->SetLevelsVisible(Cfg.KotlovanLevels, true);

	double Bottom = 0.0, Top = 0.0;
	const bool bMarkers = FloorMarkersRange(Bottom, Top);
	if (!bMarkers)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LanessaQurilish] Qavat section volume lari topilmadi - balandlik levelning o'zidan olinadi"));
	}

	auto InitGroup = [&](FRevealGroup& G, const TArray<TSoftObjectPtr<UWorld>>& Levels, FName Param, float Channel, float Start)
	{
		G = FRevealGroup();
		G.Levels = Levels;
		G.HeightParam = Param;
		G.Channel = Channel;
		G.StartTime = Start;
		// Marker bo'lmasa GatherComponents obyektlar chegarasidan to'ldiradi.
		G.BottomZ = bMarkers ? Bottom : TNumericLimits<double>::Max();
		G.TopZ = bMarkers ? Top : -TNumericLimits<double>::Max();
	};
	InitGroup(Upper, Cfg.UpperFloorLevels, Cfg.UpperHeightParam, 1.f, Cfg.UpperStart);
	// Kanal raqamlari materialdagi MF_LanessaBuildReveal bilan bir xil: 1 karkas, 2 fasad, 3 devor.
	InitGroup(Wall, Cfg.WallLevels, Cfg.WallHeightParam, 3.f, Cfg.UpperStart + Cfg.WallDelay);
	InitGroup(Facade, Cfg.FacadeLevels, Cfg.FacadeHeightParam, 2.f, Cfg.UpperStart + Cfg.WallDelay + Cfg.FacadeDelay);

	Time = 0.f;
	bYerOstiShown = false;
	bPlaying = true;
	UE_LOG(LogTemp, Log, TEXT("[LanessaQurilish] Qurilish etapi boshlandi (balandlik %.0f .. %.0f)"), Bottom, Top);
	return true;
}

void ULanessaConstructionSubsystem::StopConstruction()
{
	if (Upper.bStarted && !Upper.bFinished) { ClearGroup(Upper); }
	if (Wall.bStarted && !Wall.bFinished) { ClearGroup(Wall); }
	if (Facade.bStarted && !Facade.bFinished) { ClearGroup(Facade); }
	Upper = FRevealGroup();
	Wall = FRevealGroup();
	Facade = FRevealGroup();
	bPlaying = false;
}

void ULanessaConstructionSubsystem::Deinitialize()
{
	// Dunyo yopilyapti - obyektlarga yozishning ma'nosi yo'q, faqat holatni tashlaymiz.
	Upper = FRevealGroup();
	Wall = FRevealGroup();
	Facade = FRevealGroup();
	bPlaying = false;
	Super::Deinitialize();
}

void ULanessaConstructionSubsystem::SetHeight(FName Param, double Z)
{
	UMaterialParameterCollection* Collection = ULanessaConstructionSettings::Get().RevealCollection.LoadSynchronous();
	if (!Collection)
	{
		static bool bWarned = false;
		if (!bWarned)
		{
			bWarned = true;
			UE_LOG(LogTemp, Warning, TEXT("[LanessaQurilish] RevealCollection (MPC) topilmadi - levellar tiklanishsiz chiqadi"));
		}
		return;
	}
	UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), Collection, Param, (float)Z);
}

bool ULanessaConstructionSubsystem::GatherComponents(FRevealGroup& Group)
{
	ULanessaLevelStreamingSubsystem* Streaming = LanessaStreaming(GetWorld());
	if (!Streaming) { return false; }

	// Hammasi yuklangandagina yig'amiz - yarmi yig'ilib qolsa, qolgani kanalsiz, ya'ni
	// qirqilmasdan birdan chiqib ketardi.
	TArray<ULevel*> Loaded;
	for (const TSoftObjectPtr<UWorld>& Level : Group.Levels)
	{
		if (Level.IsNull()) { continue; }
		ULevel* L = Streaming->GetLoadedLevel(Level);
		if (!L)
		{
			if (!Group.bWarnedNotLoaded)
			{
				Group.bWarnedNotLoaded = true;
				UE_LOG(LogTemp, Warning, TEXT("[LanessaQurilish] kanal %.0f: '%s' leveli yuklanmagan - tiklanish kutib turibdi"),
					Group.Channel, *Level.GetLongPackageName());
			}
			return false;
		}
		Loaded.Add(L);
	}

	const int32 Index = ULanessaConstructionSettings::Get().ChannelDataIndex;
	double BoundsBottom = TNumericLimits<double>::Max(), BoundsTop = -TNumericLimits<double>::Max();
	for (ULevel* L : Loaded)
	{
		for (AActor* Actor : L->Actors)
		{
			if (!IsValid(Actor)) { continue; }
			TInlineComponentArray<UPrimitiveComponent*> Prims(Actor);
			for (UPrimitiveComponent* Prim : Prims)
			{
				if (!Prim) { continue; }
				Prim->SetCustomPrimitiveDataFloat(Index, Group.Channel);
				Group.Components.Add(Prim);
				const FBoxSphereBounds& B = Prim->Bounds;
				BoundsBottom = FMath::Min(BoundsBottom, B.Origin.Z - B.BoxExtent.Z);
				BoundsTop = FMath::Max(BoundsTop, B.Origin.Z + B.BoxExtent.Z);
			}
		}
	}

	// Pastki chegara - section volume dan (siz qo'ygan joydan). Tepasi esa levelning o'zi
	// balandroq bo'lsa (tom, antenna) o'shancha ko'tariladi, aks holda eng tepasi hech
	// qachon chiqmay qolardi.
	if (Group.BottomZ == TNumericLimits<double>::Max()) { Group.BottomZ = BoundsBottom; }
	Group.TopZ = FMath::Max(Group.TopZ, BoundsTop);
	Group.bGathered = true;
	return true;
}

void ULanessaConstructionSubsystem::ClearGroup(FRevealGroup& Group)
{
	const int32 Index = ULanessaConstructionSettings::Get().ChannelDataIndex;
	for (const TWeakObjectPtr<UPrimitiveComponent>& Weak : Group.Components)
	{
		if (UPrimitiveComponent* Prim = Weak.Get()) { Prim->SetCustomPrimitiveDataFloat(Index, 0.f); }
	}
	Group.Components.Reset();
	SetHeight(Group.HeightParam, LanessaRevealOff);
}

void ULanessaConstructionSubsystem::TickGroup(FRevealGroup& Group)
{
	if (Group.bFinished) { return; }

	if (!Group.bStarted)
	{
		if (Time < Group.StartTime) { return; }
		Group.bStarted = true;
		if (Group.Levels.Num() == 0) { Group.bFinished = true; return; }

		// Avval balandlik (kanal hali qo'yilmagan obyektlarga ta'sir qilmaydi), keyin ko'rsatish,
		// keyin kanal. Kanal level KO'RINGANDAN KEYIN qo'yiladi: yashirin (ro'yxatdan chiqarilgan)
		// komponentga yozilgan kanal level qayta ko'rsatilganda yo'qolib qolardi - birinchi
		// ishga tushirish ishlab, keyingilari qirqilmasdan chiqardi. Instant Switch da ko'rsatish
		// shu kadrning o'zida tugaydi, ya'ni level qirqilmagan holda bir kadr ham chizilmaydi.
		SetHeight(Group.HeightParam, Group.BottomZ);
		if (ULanessaLevelStreamingSubsystem* Streaming = LanessaStreaming(GetWorld()))
		{
			Streaming->SetLevelsVisible(Group.Levels, true);
		}
		GatherComponents(Group);
		UE_LOG(LogTemp, Log, TEXT("[LanessaQurilish] kanal %.0f: %d ta obyekt, balandlik %.0f .. %.0f%s"),
			Group.Channel, Group.Components.Num(), Group.BottomZ, Group.TopZ,
			Group.bGathered ? TEXT("") : TEXT(" (level hali yuklanmagan - keyingi kadrda)"));
	}

	if (!Group.bGathered)
	{
		// Level xotirada bo'lmagan - endi yuklanib bo'lgandir.
		if (!GatherComponents(Group)) { return; }
	}

	const float Alpha = FMath::Clamp((Time - Group.StartTime) / FMath::Max(ULanessaConstructionSettings::Get().RevealDuration, 0.1f), 0.f, 1.f);
	SetHeight(Group.HeightParam, FMath::Lerp(Group.BottomZ, Group.TopZ, (double)Alpha));

	if (Alpha >= 1.f)
	{
		// To'liq tiklandi - kanal olib tashlanadi, bino shu holatda qoladi.
		UE_LOG(LogTemp, Log, TEXT("[LanessaQurilish] kanal %.0f to'liq tiklandi"), Group.Channel);
		ClearGroup(Group);
		Group.bFinished = true;
	}
}

void ULanessaConstructionSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bPlaying) { return; }

	Time += DeltaTime;
	const ULanessaConstructionSettings& Cfg = ULanessaConstructionSettings::Get();

	if (!bYerOstiShown && Time >= Cfg.YerOstiDelay)
	{
		bYerOstiShown = true;
		if (ULanessaLevelStreamingSubsystem* Streaming = LanessaStreaming(GetWorld()))
		{
			Streaming->SetLevelsVisible(Cfg.YerOstiLevels, true);
		}
	}

	TickGroup(Upper);
	TickGroup(Wall);
	TickGroup(Facade);

	if (Upper.bFinished && Wall.bFinished && Facade.bFinished)
	{
		bPlaying = false;
		UE_LOG(LogTemp, Log, TEXT("[LanessaQurilish] Qurilish etapi tugadi"));
	}
}

TStatId ULanessaConstructionSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULanessaConstructionSubsystem, STATGROUP_Tickables);
}
