# 180/360 va ko'p ekran (nDisplay) — reja va holat

Tiklangan: 2026-09-28. Manba: 2026-09-18 dagi tadqiqot va 2026-09-21 dagi ish
(ANIMA_Character 5.8 loyihasida qilingan, u papka endi mavjud emas).

## Maqsad

Explorer 180/360 ekranda va 2-3 ekranda **butun ekranni egallab, faqat sahna**
bo'lib ko'rinadi. Boshqaruv — telefon/planshetdagi operator panelida.

Ikki holatda ham ishlashi shart:

1. **180/360 ekran — bitta HDMI** orqali ulangan.
2. **2-3 ekran — alohida HDMI / DisplayPort** orqali ulangan.

## Arxitektura

```
planshet (operator.html, WebSocket) -> Remote Control (30010/30020) -> Lanessa Remote* API
                                                                        |
                                            bitta jarayon, bitta oyna, N ta nDisplay viewport
```

### Asosiy qaror: bitta jarayon, bitta oyna, N ta viewport

- Bitta pawn va bitta vidjet qoladi, shuning uchun operator panelining `Remote*`
  API si **o'zgarishsiz** ishlaydi. Remote Control nDisplay bilan yonma-yon
  ishlashi tasdiqlangan (`RemoteSetWeather`, `RemoteCameraSet`, `RemoteSetSeason`,
  `RemoteRotateBy`).
- Ikki holat orasidagi farq **faqat `.ndisplay` faylida**, kodda emas:
  bitta build, bir nechta konfiguratsiya.
- Har monitorga alohida node (bir nechta jarayon) qo'yilsa, buyruqlar nDisplay
  Cluster Event orqali tarqalishi kerak bo'ladi. Ikkala holat uchun ham kerak emas.
  Faqat keyinchalik bir nechta kompyuter/GPU ga bo'linsa kerak bo'ladi.

## Bosqichlar

| # | Bosqich | Holat (Amanat_Oxirgi) |
|---|---|---|
| 1 | Operator paneli (statik HTML + WebSocket -> Remote Control) | ✅ `OperatorPanel/` |
| 2 | `bAllowAnyRemoteFunctionCall=True` (`DefaultRemoteControl.ini`), ishga tushirishda `-RCWebControlEnable` | ✅ |
| 3 | `.uproject` da `nDisplay` plaginini yoqish | ❌ qilinmagan |
| 4 | `DefaultEngine.ini` ga `GameEngine` / `GameViewportClientClassName` (pastga qarang) | ❌ qilinmagan |
| 5 | `.ndisplay` konfiguratsiyalari — `nDisplay/make_configs.py` bilan yaratiladi | ❌ fayllar yo'qolgan, generator tiklandi |
| 6 | Lumen chokini sinash | ✅ chok yo'q (pastga qarang) |
| 7 | Har xil o'lchamli ekranlar: har viewport o'z `region`i, burchak fizik kenglikdan | ✅ usul tayyor, konfiguratsiya qayta yaratilishi kerak |
| 8 | Cluster rejimida ekrandagi UI ni yashirish (`bHideUiInClusterMode`) | ✅ `LanessaV2Widget::RebuildWidget` |
| 9 | Unumdorlik: nur kuzatish byudjeti `769,566 MiB / 400 MiB` oshgan, 6 viewportda ko'payadi | ⏳ keyingi qadam |
| 10 | Chokni ho'l/yaltiroq sahnada va harakatdagi kamerada qayta sinash | ⏳ |
| 11 | Landshaft materiali — katta ekranda ko'proq ko'rinadi | ⏳ |

## Sozlash

### 3. `.uproject`

```json
{ "Name": "nDisplay", "Enabled": true }
```

Keyin loyihani qayta build qilish.

### 4. `Config/DefaultEngine.ini`

```ini
[/Script/Engine.Engine]
GameEngine=/Script/DisplayCluster.DisplayClusterGameEngine
GameViewportClientClassName=/Script/DisplayCluster.DisplayClusterViewportClient
```

Bu ikki sinf bo'lmasa `-dc_cluster` **umuman o'qilmaydi**: uni
`UDisplayClusterGameEngine::DetectOperationMode` o'qiydi. O'lchangan: sinflarsiz
o'yin normal ishga tushdi, chizdi, birorta viewport yaratmadi va log jim qoldi.

Oddiy bitta ekranli rejimni **buzmaydi**: `-dc_cluster` berilmasa rejim `Disabled`
bo'ladi va `UGameEngine::Init` odatdagidek chaqiriladi.

### 5. Konfiguratsiyalar

```
python nDisplay/make_configs.py
```

| Fayl | Viewport | Vazifasi |
|---|---|---|
| `ANIMA_360.ndisplay` | 6 x 60° | 360, bitta chiqish |
| `ANIMA_180.ndisplay` | 3 x 60° | 180, bitta chiqish |
| `ANIMA_3ekran.ndisplay` | 3 | 3 tekis 1920x1080 ekran |
| `ANIMA_2MONITOR.ndisplay` | 2 | har xil o'lchamli ikki ekran, fizik o'lcham bilan |
| `ANIMA_SINOV_4vp.ndisplay` | 4 | chokni temirsiz sinash |

### Ishga tushirish

```
<Loyiha>.exe -dc_cluster -dc_cfg="<yo'l>\ANIMA_360.ndisplay" -dc_node=node_main -RCWebControlEnable
```

Uchala `-dc_*` bayrog'i ham shart.

## 2-3 ekran holati

Windows monitorlarni **uzluksiz ish stoli** deb ko'rishi kerak — bitta oyna
hammasini qoplaydi va bu 180/360 holatiga aylanadi.

- Bir xil ruxsatli ekranlar: NVIDIA Surround / AMD Eyefinity.
- Har xil ruxsatli ekranlar (Surround qila olmaydi): Windows da oddiy yonma-yon
  joylashuv yetarli, bitta oyna ularni baribir qoplaydi.
- Viewport `region` lari bir xil balandlikda bo'lishi **shart emas**. O'lchangan juftlik:
  3440x1440 + 2194x1234 -> oyna 5634x1440, regionlar `0,0,3440,1440` va `3440,0,2194,1234`.
- Har ekranning `size` nisbati o'z viewportining piksel nisbatiga teng bo'lishi kerak,
  aks holda tasvir cho'ziladi.

### Burchak FIZIK kenglikdan olinadi, pikseldan emas

Ikki ekranga "ikkita bo'lgani uchun" bir xil burchak berilsa, kengrog'ida hamma
narsa yirikroq ko'rinadi. O'lchangan: ultrawide (79.4 x 33.7 sm) va 55" TV
(120 x 68 sm) ikkalasi 45° da — TV da yirikroq edi. 1.2 m masofada to'g'risi:
ultrawide **36.6°**, TV **53.1°**. nDisplay ga har ekranning haqiqiy `size` ini
metrda bering — frustumni o'zi hisoblaydi.

Fizik o'lchamni Windows beradi:

```powershell
Get-CimInstance -Namespace root\wmi WmiMonitorBasicDisplayParams   # MaxHorizontal/VerticalImageSize, sm
Get-CimInstance -Namespace root\wmi WmiMonitorID                   # model
```

Ba'zi EDID larda o'lcham yo'q (0x0) — unda diagonal va nisbatdan hisoblanadi.

## Lumen choki — natija

Eng katta xavf edi: loyiha Lumen GI + Lumen reflections + TSR da
(`r.DynamicGlobalIlluminationMethod=1`, `r.ReflectionMethod=1`, `r.AntiAliasingMethod=4`).

**Chok topilmadi.** 4x kattalashtirishda, eng og'ir sharoitda sinalgan: ochiq osmon,
tik quyosh, oq binolar, qattiq soyalar, 3440x1440 va 5634x1440, ikki monitor
tutashgan aniq joyda ham. Yo'l, bordyur, maysa, devor, soyalar — yorqinlik va
rangda sakrash yo'q.

Hali sinalmagan: yaltiroq/ho'l aks ettirish, harakatdagi kamera (TSR tarixi
chegarada), ko'proq viewport, ko'p GPU / ko'p kompyuter. Zaxira yo'l:
`r.AllowStaticLighting=True`, ya'ni pishirilgan yorug'likka o'tish mumkin.

## Ekrandagi UI

`ULanessaV2Widget::RebuildWidget` cluster rejimida `SNullWidget` qaytaradi
(`IsClusterDisplayMode()` — `-dc_cluster` bayrog'i, va `bHideUiInClusterMode`,
**Project Settings -> Plugins -> Lanessa Remote -> Ko'p ekran**).

Busiz HUD ekranlar orasiga bo'linadi: o'lchangan — logotip va qavat tugmalari
1-ekranda, soat va POI kartasi 2-ekranda qolgan.

Vidjet obyekti tirik qoladi, delegatlari ishlaydi — UI yashirin holda sinalgan:
174 xonadon ro'yxati, `RemoteSetSeason`, `RemoteRotateBy` ishladi. Sahnadagi qavat
markerlari (`BP_3D_Widget_FloorIcon`) bunga kirmaydi — ular world-space aktyorlar.

## UE 5.8 tuzoqlari

- `.ndisplay` fayli 5.8 da ham **4.27 JSON sxemasida**: dvijokda faqat `JSON426`/`JSON427`
  parserlari bor, `Version_500` — asset formati. Kalitlar camelCase. Logda:
  `Detected (.ndisplay 4.27) config format`.
- **`simple` proyeksiya 5.8 da yo'q** — `RedirectSimpleToMeshPolicy` uni jimgina `mesh`
  ga aylantiradi, `screen` parametri `Component` bo'ladi. `simple` yozish hozircha shu
  moslik yo'li orqali ishlaydi.
- Oyna o'lchami config dagi `window` dan emas, `-ResX`/`-ResY` dan olinadi va UE
  windowed oynani asosiy monitorga qisadi. Bir nechta monitorni qoplash uchun oyna
  ramkasini (`WS_CAPTION`/`WS_THICKFRAME`) olib, `SetWindowPos` bilan majburlash kerak.
- 360 uchun kamida 4 viewport (bitta warp mesh <= 90° amaliy, 179° qattiq chegara).
  Havola: Ars Electronica Deep Space 8K starter kit (GitHub, UE 5.7) — fixed
  framerate ni o'chirish, DX11 va DX12 ni sinash.

## Keyingi qadamlar

1. 3, 4, 5-bosqichlarni Amanat_Oxirgi ga qaytarish, ishga tushirish `.bat` ini yozish.
2. Unumdorlik — 6 viewport uchun nur kuzatish byudjeti.
3. Chokni ho'l/yaltiroq sahnada va harakatda qayta sinash.
4. Landshaft materiali.
