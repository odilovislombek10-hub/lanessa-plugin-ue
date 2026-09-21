# Qirqim — material tomoni

`ULANISH_TARTIBI.md` qirqimning Blueprint tomonini yoritadi: qavat markerlari, `Reset_SectionView`,
CHIQISH nimani bekor qilishi. Bu fayl ikkinchi yarmi haqida — qirqim geometriyaga qanday yetib
boradi va qaysi to'rt holatda **jimgina** ishlamay qoladi.

Inglizcha versiyasi: [`SECTION_MATERIALS.md`](SECTION_MATERIALS.md).

---

## 1. Zanjir

```
BP_Explorer_PC  --Location/Bounds/Rotation_Z-->  SectionMask_MPC
                                                      |
                                    MF_SectionMask beshala parametrni o'qiydi
                                                      |
                            Masked material: -> OpacityMask   (piksel tashlanadi)
                        Translucent material: -> Opacity      (piksel ko'rinmaydi)
```

Qirqim **piksel darajasida va dunyo koordinatasida** ishlaydi. U qaysi aktyor qaysi binoga tegishli
ekanini bilmaydi: qutining ichiga tushgan har bir piksel kesiladi — funksiya ulangan har qanday
materialda. Aktyorga a'zolik hech qanday rol o'ynamaydi.

**Kanonik assetlar** — loyihada har biridan ikkitadan nusxa bor, faqat bittasi tirik:

| Asset | Shuni ishlating | Ikkinchisi |
|---|---|---|
| `MF_SectionMask` | `/Game/New_Explorer/Materials/MF/` | `/Game/ArchVizExplorer/…` — eski, 2 ta havola |
| `SectionMask_MPC` | `/Game/New_Explorer/Materials/MPC/` | `/Game/ArchVizExplorer/…` — eski |

`BP_Explorer_PC` va `BP_Explorer_Pawn` New_Explorer kolleksiyasiga yozadi, loyihaning GameMode'i ham
`BP_Explorer_GameMode_LN` — New_Explorer'niki. ArchVizExplorer nusxasiga ulangan material
kompilyatsiyadan o'tadi, xato bermaydi va hech qachon kesmaydi.

## 2. Maska

`MF_SectionMask` da kirish yo'q, ikkita chiqish bor. Ichida:

```
WorldPosition -> RotateAboutAxis(Z, Rotation_Z, pivot=Location) -> BoxMask-3D(Location, Bounds, Mask_Falloff)
                                                                        |
                        Mask Hard = (1 - BoxMask) + (1 - Mask_Intensity)
                        Mask      = DitherTemporalAA(Mask Hard)
```

`Mask_Intensity = 1` bo'lganda:

| Qayerda | Qiymat | Natija |
|---|---|---|
| quti tashqarisida | `1` | ko'rinadi |
| quti ichida | `0` | kesiladi |

**`Mask_Intensity = 1` — qirqim YONIQ degani.** Blueprint bu parametrni hech qachon yozmaydi —
`BP_Explorer_PC` va `BP_Explorer_Pawn` ikkalasini tekshirib chiqildi, ular faqat `Location`,
`Bounds` va `Rotation_Z` ni yozadi. Ya'ni qirqim **doim yoqilgan**; "o'chirish" esa qutini
geometriya yo'q joyga olib qo'yish orqali amalga oshiriladi. Shu bitta fakt §6 dagi reset
xatti-harakatini tushuntiradi.

`Bounds` **to'liq o'lcham** sifatida ishlatiladi, yarim emas: Blueprint `2 × aktyorning yarim
o'lchami` ni uzatadi, ya'ni haqiqiy quti `Location ± Bounds`.

## 3. `Mask` emas, `Mask Hard`

`Mask` chiqishi `DitherTemporalAA` orqali o'tadi, u piksellarni tasodifiy tashlaydi. TSR uni to'liq
tozalay olmaydi, natijada qirqim chekkasi — oynada esa butun sirt — nuqta-nuqta, xira maydon bo'lib
chiqadi va qirqimga emas, tasodifiy g'adir-budurlikka o'xshab ko'rinadi.

`Mask Hard` — o'sha qiymat, ditherziz: toza 0 yoki 1.

Levelda 84 ta master (4,537 slot) `Mask` da edi; hammasi `Mask Hard` ga o'tkazildi.

## 4. Ikkita ulanish sxemasi, biri ishlamaydi

**Opaque / masked materiallar — material atributlari orqali.**

```
mavjud zanjir -> MakeMaterialAttributes -> MP_MATERIAL_ATTRIBUTES
MF_SectionMask . Mask Hard -> Make.OpacityMask       (mavjud maska bo'lsa, unga ko'paytirib)
use_material_attributes = true        blend = MASKED
```

**`MF_SectionMask` ni to'g'ridan-to'g'ri `MP_OPACITY_MASK` ga ulash kesmaydi** — graf to'g'ri
o'qilsa ham, blend `MASKED` bo'lsa ham, MPC yo'li to'g'ri bo'lsa ham, material kompilyatsiyadan
o'tsa ham. Bu shunday aniqlandi: qirqim ishlaydigan levelga bitta mesh qo'yilib, faqat materiali
almashtirildi — to'g'ridan-to'g'ri ulangan master bilan mesh joyida qoldi, atribut sxemasidagi
master bilan kesildi. 17 ta master (1,302 slot) shu nuqsonli sxemada topilib, o'tkazildi.

**Translucent materiallar — `Opacity` orqali.**

```
MF_Weather_Glass . Opacity  ×  MF_SectionMask . Mask Hard  ->  Make.Opacity
blend TRANSLUCENT bo'lib qoladi
```

## 5. Oynada grafdan tashqari ikkita sozlama ham o'zgarishi kerak

`Opacity = 0` bo'lgan translucent sirt ko'rinmaydi, lekin **yo'qolmaydi**: u hamon fonni sindiradi
va yaltirashni ushlaydi. Natijada devor kesilgan joyda havoda osilgan sharpa oyna qoladi.

Loyihaning o'z ishlaydigan oynasi (`New_Explorer/Materials/Glass/M_Glass`, hamda POI materiallari
`M_Holo` / `M_Route`) — uchalasida ham bir xil ikkita sozlama, sharpa qoldiradiganlarida esa
boshqacha:

| Sozlama | Sharpa qoladi | Toza |
|---|---|---|
| `translucency_lighting_mode` | `TLM_SURFACE_PER_PIXEL_LIGHTING` | `TLM_SURFACE` |
| `refraction_method` | `RM_INDEX_OF_REFRACTION` | `RM_PIXEL_NORMAL_OFFSET` yoki `RM_NONE` |

E'tibor bering: ishlaydigan materiallar `Specular` ni **maskalamaydi**. Yechim grafda emas, material
sozlamalarida. Bu o'zgarish ustiga arzonlashtirdi ham — `M_Corona_Glass` 2224 → 1566 PS buyruq.

## 6. Reset qutini level ustiga parklaydi

`Mask_Intensity` hech qachon o'chirilmagani uchun, `Reset_SectionView` qirqimni "tozalash" uchun
qutini `BP_Explorer_Pawn → SectionView_Initial_Volume` ga — levelga qo'yilgan `TriggerVolume` ga —
ko'chiradi.

O'sha hajm **o'z XY hududidagi barcha geometriyadan baland** turishi shart, aks holda reset
qamrab qolgan narsasini kesilgan holda qoldiradi. Bu loyihada hajm `Z 9676` da edi (quti
`7524…11828`), o'sha hududdagi geometriya esa `Z 15029` gacha ko'tarilardi — shuning uchun har
chiqishda yuqori qavatlar yo'qolgancha qolardi. Endi u `Z 20000` da parklangan nusxaga ishora
qiladi.

Bino o'sgan sari bu yomonlashadi: qavat qo'shish aynan geometriyani ilgari bo'sh bo'lgan hajm ichiga
ko'taradi. Alomati: CHIQISH yoki biror nav tugmasidan keyin qirqim qaytmaydi.

Animatsiyaning boshlanish nuqtasi ham shu hajm, shuning uchun uni to'g'ridan-to'g'ri binoning ustiga
parklash qirqimni tepadan pastga tushadigan qiladi. Yon tomonda parklansa (masalan koordinata
boshida — MPC sukut qiymati o'sha yerni ko'rsatardi), quti butun xarita bo'ylab uchib o'tadi va
qirqim yonlamasiga surilgandek ko'rinadi.

## 7. Eng ko'p vaqt olgan ikkita tuzoq

**2 ta kolleksiya cheklovi.** Bitta material ko'pi bilan **ikkita** `MaterialParameterCollection` ga
murojaat qila oladi. Qirqim (`SectionMask_MPC`) va fasl/ob-havo (`UltraDynamicWeather_Parameters`)
ikkalasini band qiladi, shuning uchun o'sha materialga kecha/kunduz emissive kolleksiyasini qo'shish
chegaradan chiqaradi:

```
Material references too many MaterialParameterCollections!
A material may only reference 2 different collections.
```

Shundan keyin u **sukut materiali** bilan chiziladi — kulrang, viewportda esa hech qanday xato
ko'rinmaydi. Ikkita master shunga duch keldi. `M_Corona_Master` dagi emissive havolasi amalda hech
narsa qilmayotgani aniqlandi (27 ta instance'ning hammasida emissive 0 edi), shuning uchun ob-havoni
tashlash o'rniga o'sha olib tashlandi.

**Eskirgan funksiya tugunlari.** Funksiya ikkinchi chiqishga ega bo'lishidan oldin qo'yilgan
`MF_SectionMask` tuguni faqat `['Mask']` ni ko'rsatib turadi — unga `Mask Hard` ni ulab bo'lmaydi va
`update_material_function` ham uni yangilamaydi. Tugunni o'chirib, yangisini qo'yish kerak. Bitta
materialda esa shu tugunning 15 ta nusxasi bor edi, faqat bittasi ulangan — shu sababli har bir
skaner uni buzuq deb ko'rsatardi.

## 8. Materialni tekshirish

```
blend            MASKED (qattiq sirt) yoki TRANSLUCENT (oyna)
attr             use_material_attributes = true, qirqim Make.OpacityMask / Make.Opacity ga
chiqish          Mask Hard, Mask emas
kolleksiya       MF_SectionMask — /Game/New_Explorer/... dan
MPC soni         ≤ 2, fasl kolleksiyasi bilan birga
PS buyruqlar     ≠ 0   (0 bo'lsa kompilyatsiyadan o'tmagan va sukut materiali bilan chizilyapti)
```

Oxirgi qator — eng arzon tekshiruv: statistikasi `PS = 0` ko'rsatgan material, grafi qanday
ko'rinishidan qat'i nazar, umuman yozilganidek chizilmayapti.
