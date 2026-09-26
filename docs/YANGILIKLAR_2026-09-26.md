# Yangiliklar — 2026-09-24 … 26

Bu hujjat shu kunlarda plaginga qo'shilgan funksiyalar, ularning sozlamalari va loyiha
asset'larida (material, MPC) ulangan nodelar haqida. Asset nusxalari `ProjectAssets/` da.

---

## 1. Progulka paneli va chiqish tugmasi

- **`LanessaWalkPointsWidget`** — yuqori chapdagi nuqta nomi ("SAYR") logotip ustiga tushib
  qolardi: olib tashlandi, joriy nuqta pastki panelda yoritiladi. Yuqori o'ngdagi **X** ham olib
  tashlandi (`OnExitClicked` Blueprint ulanishlari buzilmasligi uchun qoldirilgan).
- **`LanessaV2Widget::BuildUtility`** — pastki o'ngdagi ikkinchi (eshik) tugma endi dasturdan
  chiqadi: `UKismetSystemLibrary::QuitGame`. PIE da faqat o'yin sessiyasi to'xtaydi.

## 2. Level streaming — `LanessaLevelStreaming`

`Project Settings → Plugins → Lanessa Level Streaming`, `DefaultGame.ini` ga saqlanadi.

Har bir sahifa (menyu tugmasi) uchun qaysi sub levellar ochiq turishi:

| Sozlama | Ma'nosi |
|---|---|
| `Page Levels` (`Pages`) | Ro'yxat: har qatorda **Page** va **Levels**. Bosilganda ro'yxatdagilar ochiladi, ro'yxatda yo'q boshqariladigan sub levellar yopiladi |
| `Start Page` | O'yin boshida qo'llanadigan sahifa (standart: Bosh sahifa) |
| `Keep Loaded In Memory` | Jadvaldagi hamma level boshida xotiraga yuklanadi, keyin faqat ko'rsatiladi/yashiriladi — almashish sezilmaydi |
| `Instant Switch` | Almashish bitta kadrda (`FlushLevelStreaming`) — bo'lak-bo'lak chiqish yo'q |
| `Unmanaged Levels` | Plagin hech qachon tegmaydigan levellar |

Qoidalar:
- **Always Loaded** sub levellarga tegilmaydi (osmon, yorug'lik, POI lar shu yerda tursin).
- Jadvalda yo'q yoki **levellari bo'sh** sahifa bosilsa levellarga tegilmaydi.
- `BP_Explorer_PC.MainLevelName` (BP interyerga kirganda yopib, chiqqanda ochadigan level)
  avtomatik "unmanaged" hisoblanadi.
- Sahifa id lari enum nomining kichik harfdagi ko'rinishi: `home`, `atrofi`, `qulay`,
  `qidiruv`, `qirqim`, `galereya`, `vr`, `sayr`, `interyer`, `qurilish`, `kotlovan`, `yerosti`,
  `karkas`, `devor`, `fasad`, `qurilishetapi`.
- `ULanessaV2Widget::SetActiveView` har sahifada `ApplyPageId` ni chaqiradi — Blueprint shart emas.

> Eslatma: avval `Page Levels` `TMap` edi — yangi qator doim "Bosh sahifa" kaliti bilan qo'shilib,
> "Duplicate keys are not allowed" berardi. Endi oddiy `TArray<FLanessaPageLevels>`.

## 3. QURILISH sahifasi — `LanessaConstruction`

### Menyu va panel
- INTERYER tagida **QURILISH** tugmasi (`Label_Qurilish`, kran ikonkasi `LanessaIcons::Qurilish`).
- Yon panel: **Kotlovan, Yer osti qavati, Karkas, Devor, Fasad, Qurilish etapi**
  (`ULanessaV2Widget::SelectBuildStage(StageId)`).
- Har bosqich o'z sahifasining levellarini ochadi (streaming jadvalidagi `Qurilish: …` qatorlari).
- **Yer osti qavati** — pastda qirqimdagi kabi qavat paneli, faqat `Floor < 1` qavatlar
  (`BuildUndergroundRail`). Raqam bosilsa `OnFloorSelected` — qirqim bilan bir xil yo'l.
- Bosqich tugmalari ham **`OnNavClicked(StageId)`** ni chaqiradi — kamerani bosh sahifa / atrof
  kabi `BP_Explorer_PC.OnNavClicked_Event` da sozlash uchun (pastga qarang).

### "Qurilish etapi" animatsiyasi
`Project Settings → Plugins → Lanessa Qurilish → Qurilish etapi`:

| Vaqt | Nima |
|---|---|
| 0 s | `Kotlovan Levels` chiqadi |
| `Yer Osti Delay` (1.5 s) | `Yer Osti Levels` chiqadi |
| `Upper Start` (3 s) | `Upper Floor Levels` (karkas) pastdan tepaga tiklanadi |
| + `Devor Delay` (2 s) | `Devor Levels` ham shunday tiklanadi |
| + `Facade Delay` (2 s) | `Facade Levels` ham shunday tiklanadi |

- Har bir tiklanish `Reveal Duration` (10 s) davom etadi; tugagach bino to'liq holda qoladi.
- Tiklanish balandligi **`BP_FloorSectionMarker`** larning `SectionView_Volume` laridan olinadi
  (Floor ≥ 1: eng pastki volume pasti → eng yuqorisining tepasi; level balandroq bo'lsa tepasi
  uzaytiriladi). Markerlar Persistent yoki Always Loaded levelda bo'lishi kerak.
- Animatsiya paytida istalgan tugma bosilsa animatsiya to'xtaydi va bino to'liq holatga qaytadi.
- `Qurilish: Qurilish etapi` sahifasiga faqat **doim ko'rinadigan** levellar kiritiladi
  (landshaft, kotlovan) — karkas/devor/fasadni animatsiya o'zi chiqaradi.
- Kanal level **ko'rsatilgandan keyin** qo'yiladi — yashirin komponentga yozilgan kanal level
  qayta ko'rsatilganda yo'qolardi (birinchi bosish ishlab, keyingilari ishlamasdi).
- Log: `[LanessaQurilish] kanal N: X ta obyekt …`, `… to'liq tiklandi`, yuklanmagan level bo'lsa
  ogohlantirish.

### Qanday kesiladi (material)
Har bir obyektga **Custom Primitive Data** (indeks `Channel Data Index` = **20**) orqali kanal
yoziladi: **1 = karkas, 2 = fasad, 3 = devor**. Material o'sha kanalning balandligini
`SectionMask_MPC` dan o'qib, undan yuqorisini yashiradi. Kanal 0 (animatsiyadan tashqari) —
hech narsa o'zgarmaydi.

### Kamera
Ikki usul bor, bittasini ishlating:
1. **Blueprint (tavsiya):** `BP_Explorer_PC → OnNavClicked_Event` dagi `ViewId == "home"` /
   `"atrofi"` zanjiriga yangi tarmoq: `ViewId == "qurilish"`, `"kotlovan"`, `"yerosti"`,
   `"karkas"`, `"devor"`, `"fasad"`, `"qurilishetapi"` → `SetLocation_New`, `SetPitch_New`,
   `SetYaw_New`, `SetTargetArmLength_New` → `Focus`. **Bu tarmoqlarni Blueprint da qo'lda qo'shish
   kerak** — plagin faqat `OnNavClicked` ni chaqiradi.
2. **Settings:** `Lanessa Qurilish → Cameras` — Play paytida kamerani joyiga olib borib, `Capture For`
   ni tanlab **"Hozirgi kamerani yozish"**. Ikkala usul birga ishlatilsa kamera talashadi.

## 4. Interyer QIRQIM rejimi — `LanessaInteriorTourWidget`

POI (xonadon) → **3D TUR** bilan kirilgan interyer widgetida yuqori chapda **QIRQIM** tugmasi.

- Bosilganda: yuruvchi personaj yashirinadi, bosh sahifaning orbit pawn i
  (`GI.Explorer_Main_Pawn`) egallanadi — boshqaruv bosh sahifadagidek.
- **Faqat** kvartira leveli (`BP_Explorer_PC.PendingInteriorLevelName`), `Interyer: Qirqim`
  ro'yxatidagi levellar va Always Loaded levellar ko'rinadi (`ShowOnly`); POI markerlari
  yashiriladi. Chiqishda oldingi holat aynan qaytadi (`RestoreVisible`).
- Kesish: interyer levelidagi **`InteriorSection`** tegli volume (Trigger Volume). Volume ICHI
  yashiriladi — pasti poldan ~120–150 sm, tepasi shiftdan baland bo'lsin.
  `ULanessaV2Widget::ApplySectionBox` → `BP_Explorer_PC.SectionView_Mask` (qavat qirqimi bilan bir xil).
- Kamera: **`InteriorSectionCamera`** tegli CameraActor ning aniq joyi va burilishi (aylanish
  markazi uning qarash chizig'ida, volume markaziga eng yaqin nuqta). Bo'lmasa — tepadan 60°.
  Orbit pawn masofasi `SpringArm_Length_Min` (1500) dan yaqin bo'lmaydi.
- QIRQIM qayta bosilsa yoki xona tanlansa — yurishga, o'sha joyga qaytiladi.
- **X** — qirqimdan chiqish, qo'shimcha levellarni yopish, BP chiqishi, keyingi kadrda QIDIRUV.
- `Interyer: 3D tur` jadval qatori — 3D TUR da interyerga **qo'shib** ochiladigan levellar.

## 5. Boshqa tuzatishlar

- **Fasl ikonkalari** (`LanessaLineIcon.h`): SVG `C` (cubic Bézier) buyrug'i tanilmasdi —
  barglar siniq chiziq bo'lib chiqardi. `Tessellate` ga `C` qo'shildi.
- `ULanessaV2Widget::ApplySectionBox(Centre, Extent, Yaw)` — markersiz qirqim (BlueprintCallable).

---

## Loyiha asset'larida ulangan nodelar

Hammasi `ProjectAssets/` da nusxalangan (qanday joylashtirish — `ProjectAssets/README.md`).

### `SectionMask_MPC` (`/Game/New_Explorer/Materials/MPC/`)
Yangi scalar parametrlar (standart **10000000** = hech narsa kesilmaydi): **`UpperZ`**,
**`FacadeZ`**, **`WallZ`**. Alohida MPC ishlatilmaydi: material ko'pi bilan **2 ta** MPC o'qiy
oladi, uchinchisi qo'shilganda ko'p materiallar Default Material ga tushib qolgan.

### `MF_LanessaBuildReveal` (yangi, `/Game/Lanessa/`)
Chiqish: **`Mask`** (1 = ko'rinadi, 0 = yashirin).
```
Lanessa_Channel  = ScalarParameter, Use Custom Primitive Data, index 20, default 0
Threshold        = If(Channel, 1.5):  <,=  -> UpperZ
                                      >    -> If(Channel, 2.5): <,= -> FacadeZ,  > -> WallZ
Cut              = If(AbsoluteWorldPosition.Z, Threshold):  >  -> 0,  <,= -> 1
Mask             = If(Channel, 0.5):  >  -> Cut,  <,= -> 1
```

### `MF_SectionMask` (`/Game/New_Explorer/Materials/MF/`)
`Add_0` (qirqim natijasi) endi `Multiply(Add_0, MF_LanessaBuildReveal.Mask)` orqali o'tadi — shu
ko'paytma `DitherTemporalAA.Alpha Threshold` ga va **`Mask Hard`** chiqishiga boradi. Natijada
qirqim ishlatadigan **195 ta** material o'zgartirishsiz tiklanishni ham qo'llaydi.

### `M_MS_Surface_Material_VT` (`/Game/MSPresets/`)
Megascans materiallari qirqilmasdi (master da `MF_SectionMask` yo'q edi). SHABLON nusxasi kabi:
**Opaque → Masked**, `MF_MapAdjustments.Result → BreakMaterialAttributes → MakeMaterialAttributes`,
`MF_SectionMask.Mask Hard → Make.OpacityMask`.

### `M_Glass_Master` (`/Game/ArchvisDefault/Model/avto_skm/AutomotiveMaterials/Masters/`)
Thin Translucent shisha qirqilgan joyda tonirovka va yaltirashni saqlab qolardi:
- `Saturate(MF_SectionMask.Mask Hard)` → **ThinTranslucentMaterialOutput.SurfaceCoverage**
- `Specular = Lerp_2 × Saturate(Mask Hard)`
- `Refraction = Lerp(1, RayTracingQualitySwitch, Saturate(Mask Hard))`

---

## Build

Loyiha `.uproject` ida manbasi yo'q CitySample modullari yozilgan, shuning uchun loyihani o'zi
build bo'lmaydi va Live Coding ishlamaydi. Plagin faqat shu plagin yoqilgan bo'sh "host" loyiha
orqali build qilinadi (`Plugins/Lanessa` → haqiqiy plaginga junction), editor **yopiq** holda:

```
Build.bat UnrealEditor Win64 Development -Project=<host>\LanessaHost.uproject -WaitMutex
```
