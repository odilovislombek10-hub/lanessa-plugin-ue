# Ulanish tartibi va nega aynan shunday

Bu hujjat Lanessa vidjetlari sahnaga qanday ulanganini va har bir qarorning sababini yozadi.
Ingliz tilidagi nusxasi: [`WIRING.md`](WIRING.md).

---

## 1. Umumiy zanjir

Boshqaruvning **bitta** yo'nalishi bor:

```
  ekrandagi menyu              operator.html (telefon)
         |                              |
         |                              v
         |                    Remote*  (statik funksiya, CDO orqali)
         |                              |
         +--------------+---------------+
                        v
              ULanessaV2Widget::SetXxx()
                        |
              1. o'z holatini yangilaydi  (SelectedSeason, TimePct, ...)
              2. Invalidate(Paint)        -> ekran qayta chiziladi
              3. Delegat Broadcast        -> OnSeasonChanged, OnTimePctChanged, ...
                        |
                        v
              BP_Explorer_PC dagi handler
                        |
                        v
        Ultra_Dynamic_Sky / Ultra_Dynamic_Weather / BP_POI / MPC
```

**Nega bitta yo'nalish.** Telefondagi panel ham, ekrandagi menyu ham aynan bitta `SetXxx()` ga
kiradi. Shuning uchun qaysi biridan bosilsa ham vidjetning o'zi ham, sahna ham bir vaqtda
yangilanadi. Agar panel aktyorlarga to'g'ridan yozganida, ekrandagi chip eski holatda qolib
ketardi — foydalanuvchi "qish" ni tanlaydi, menyuda "yoz" yozilib turadi.

Eski `panel.html` aynan shunday ishlagan (UDS aktyoriga to'g'ridan yozgan) va o'sha muammo bilan
tashlab yuborilgan. `operator.html` — shu zanjirdagi to'g'rilangan variant.

---

## 2. Ishga tushish tartibi

Quyidagilar **shu ketma-ketlikda** bajarilishi shart. Tartib buzilsa xato chiqmaydi — shunchaki
hech narsa ko'rinmaydi, bu esa topilishi qiyinroq.

| # | Kim | Nima | Nega aynan bu joyda |
|---|---|---|---|
| 1 | dvigatel | `RebuildWidget()` | Slate daraxti qurilib, `PanelCategoryListBoxes` ro'yxatga olinadi |
| 2 | `BP_Explorer_PC` | delegatlarga bind | Birinchi `SetXxx()` dan oldin bog'lanmasa, o'sha birinchi broadcast bo'shga ketadi |
| 3 | `BP_Explorer_PC` | `SetPanelCategories(panel, taglar, yorliqlar, standartlar)` | `BuildCategoryRow` ichida `CategoryPoiListBoxes` to'ldiriladi |
| 4 | `BP_Explorer_PC` | `SetCategoryPois(panel, tag, poiIdlar, yorliqlar)` | 3-qadam ro'yxatga olgan quticha bo'lmasa, funksiya **jimgina qaytib ketadi** |
| 5 | `BP_Explorer_PC` | `SetSearchUnits(...)` yoki `PopulateSearchUnitsFromTable(...)` | Qidiruv filtri uchun ma'lumot |
| 6 | `BP_Explorer_PC` | `SetAvailableFloors([...])` | Qavat railini to'ldiradi |
| 7 | vidjet | birinchi `NativeTick` | `bDayNightSynced` bir marta `ApplyDayNightFromUDS(bForce=true)` chaqiradi |

**3 va 4 ning tartibi majburiy.** `SetCategoryPois` o'z quticha-sini `PanelTag + ":" + CategoryTag`
kaliti bo'yicha `CategoryPoiListBoxes` dan qidiradi. Bu kalitlar esa faqat `SetPanelCategories`
ichidagi `BuildCategoryRow` da yaratiladi. Teskari tartibda chaqirsangiz POI qatorlari umuman
qo'shilmaydi va hech qanday ogohlantirish chiqmaydi.

`SetPanelCategories` yana bir ish qiladi: o'zi qayta chaqirilganda `PanelTag + ":"` bilan
boshlanadigan eski kalitlarni tozalaydi. Aks holda yo'q qilingan kategoriya qatorlarining
quticha-lari xotirada qolib, jonli deb adashtirilardi.

**7-qadam nega Tick da.** Vidjet qurilganda UDS aktyori hali yuklanmagan bo'lishi mumkin.
Shuning uchun boshlang'ich sinxronizatsiya muvaffaqiyat qaytarmaguncha har tick urinadi, keyin
`bDayNightSynced` bilan o'chadi.

---

## 3. Kanallar

### Vaqt → UDS → chiroqlar

Eng chigal joyi shu, chunki uch bosqich bor:

```
SLanessaDragTrack sudraldi
    -> SetTimePct(Pct)
         1. TimePct = Clamp(Pct, 0, 1)
         2. Invalidate(Paint)
         3. OnTimePctChanged.Broadcast(TimePct)
              -> BP_Explorer_PC:  UDS."Time of Day" = 600 + Pct * 1600
         4. ULanessaDayNight::ApplyDayNightFromUDS(this, bForce=false)
              -> UDS dan "Time of Day" ni QAYTA O'QIYDI
              -> kun/tun chegarasi kesilgan bo'lsa chiroqlarni almashtiradi
```

**Nega 4-qadam broadcastdan keyin va nega qayta o'qiydi.** `Pct` ni soatga aylantirish formulasi
(`600 + Pct*1600`) Blueprint handlerida yozilgan. Agar C++ ham o'sha formulani takrorlaganida,
bitta qoida ikki joyda yashagan bo'lardi — birini o'zgartirib ikkinchisini unutish oson. Broadcast
qaytgan paytda UDS allaqachon haqiqiy qiymatni ushlab turadi, shuning uchun C++ uni shunchaki
o'qib oladi. Ya'ni **yagona haqiqat manbai — UDS aktyorining o'zi**.

Vaqt shkalasi 24 soatlik emas: u HHMM **sonini** 600 dan 2200 gacha chiziqli suradi. `0.25` → 1000
(10:00), `0.50` → 1400 (14:00). Chip kartasidagi `06:00 / 14:00 / 22:00` yorliqlari shundan.

### Chiroqlar — nega C++ da, Blueprint'da emas

`ULanessaDayNight` sahnadagi **barcha** `ULocalLightComponent` ni keshlaydi va kun/tun holatiga
qarab `SetVisibility` qiladi, yana `Emissive_MPC` dagi `StreetLights` skalyarini yozadi.

Uchta sabab:

1. **Miqdor.** Sahnada ~1082 ta chiroq komponenti bor. Blueprint'da har kadr `Get All Actors of
   Class` + sikl qilish qimmat; C++ da bir marta keshlanadi.
2. **Chekka trigger.** `LastState` oxirgi qo'llangan holatni eslaydi. Slider sudralayotganda
   funksiya har kadr chaqiriladi, lekin chegara (700 / 1700) kesilmaguncha bitta `float`
   solishtiruvdan nariga o'tmaydi. Busiz har kadr 1082 ta `MarkRenderStateDirty()` bo'lardi.
3. **Blueprint'da bunday keshni yozib bo'lmaydi** — `TWeakObjectPtr` massivi va statik holat
   Blueprint'da yo'q.

Shuning uchun chiroqlar `BP_Explorer_PC` da **umuman yo'q** — u yerda birorta light nodi izlab
topolmaysiz. Bu ataylab.

Keshning bekor qilinishi: dunyo almashganda (editor ↔ PIE) `EnsureCache` `LastState = -1` qiladi,
chunki eski dunyodagi komponentlarga qo'llangan holat yangi dunyoda ma'nosiz.

### Fasl va ob-havo

`SetSeason` va `SetWeather` — alohida delegatlar, ataylab. Fasl va ob-havo mustaqil o'qlar: birida
chip bosilganda ikkinchisi jimgina siljib ketmasligi kerak.

**Nega Blueprint tomonida `Update Season` chaqirish shart.** UDW da `Season` o'zgaruvchisini
yozishning o'zi yetmaydi. UDS `Individual Seasons` ni qayta hisoblashni va `UDW Seasons` ni
material parameter kolleksiyasiga uzatishni faqat `Update Season` ichida qiladi, o'yin paytida
esa uni boshqa hech kim chaqirmaydi. Shuning uchun handler: `Season Mode = Manual Setting` →
`Season = 0..3` → `Update Season`.

### POI, qavat, qidiruv

| Delegat | Blueprint nima qiladi |
|---|---|
| `OnPoiEntryClicked(PoiId)` | `GI.BP_POIs` dan mos aktyorni topib `Select_POI` |
| `OnPoiCardAction(ActionId)` | `"level"` / `"level2"` / `"360"` / `"media"` — har biri boshqa ish |
| `OnSearchApplied(MatchingPoiIds)` | Ro'yxatdagi har aktyorga `Show_POI`, qolganiga `Hide_POI` |
| `OnFloorSelected(Floor)` | Mos `BP_FloorSectionMarker` ni topib `Select_POI` |
| `OnCategoryToggled(...)` | Kategoriya bo'yicha ko'rsatish/yashirish |
| `Reset_SectionView` | Kesim qutisini boshlang'ich volume holatiga qaytarish |

**`PoiId` nima.** Bu aktyorning o'z obyekt `Name`i (`BP_POI_C_12` kabi). Ekranda ko'rinmaydi,
faqat aktyorni qaytib topish uchun. Nega shu tanlangan: ishga tushganda barqaror, takrorlanmas va
uni olish uchun aktyorni yuklash shart emas.

**`OnFloorSelected` nega bor edi.** Avval qavat rakamini bosish faqat raqamni yoritardi, boshqa
hech qanday ta'siri yo'q edi. Endi u 3D dagi qavat ikonkasini bosish bilan **bir xil** ishlaydi —
ikkala kirish nuqtasi bir natija berishi kerak degan qoida bo'yicha.

### CHIQISH ikkita ishni qiladi

Qirqim panelidagi CHIQISH tugmasi bosilganda ikki narsa buziladi, lekin ularni ikki xil egasi
tuzatadi:

```
CHIQISH bosildi
  ├─ Reset_SectionView.Broadcast()        -> BP_Explorer_PC kesim qutisini qaytaradi
  └─ LanessaRestoreQirqimBuilding()       -> qavat ikonkalarini qaytaradi
```

**Nega ikkinchisi kerak.** Qavat ikonkalari — `BP_FloorSectionMarker` aktyorlari, ularni
ko'rsatish/yashirish butunlay `BP_Explorer_PC` ning nav handlerlarida. CHIQISH esa ataylab hech
qayerga o'tmaydi, ya'ni `OnNavClicked` chaqirilmaydi. Natijada 22 qavatli binoni 9-qavatda
kesgandan keyin 9-dan yuqoridagi ikonkalar yashirinib qolardi va ularni hech kim qaytarmasdi.

**Nega aynan `Select_POI`.** C++ ikonkalarni o'zi yashirmaydi, demak o'zi ko'rsata ham olmaydi.
Uning o'rniga u ikonkalarni qaytaradigan yagona mavjud harakatni takrorlaydi — bino markerida
`Select_POI`, bu foydalanuvchi binoning 3D ikonkasini bosgani bilan aynan bir xil.

**Nega faqat bitta bino.** Qaysi bino ekani `BP_Explorer_PC` ning `CurrentQirqimBuilding` satridan
nom orqali o'qiladi. Levelda bir nechta bino bo'ladi; hammasini qaytarsa, foydalanuvchi umuman
ochmagan binolarning ikonkalari ham yonib ketardi. Birinchi mos marker topilgach sikl to'xtaydi.

Qidiruv nima qaytargani `[LanessaQirqim]` prefiksi bilan logga yoziladi — mos bino topilgani,
`CurrentQirqimBuilding` bo'shligi, o'zgaruvchi umuman yo'qligi va mos marker topilmagani alohida
xabarlar. Refleksiya nom bo'yicha ishlagani uchun bu yagona diagnostika vositasi.

### Interyer va sayr panellari

`LanessaInteriorTourWidget` va `LanessaWalkPointsWidget` o'z tugmalarini **leveldagi
`APlayerStart` aktyorlaridan** to'ldiradi:

- a'zolik — aktyorning **tegi** bo'yicha: interyer uchun `Room`, sayr uchun `Walk`
- tugmadagi yozuv — `Player Start Tag` maydonidan

```
PopulateFromPlayerStarts(WorldContext, "Room")   // yoki "Walk"
```

**Nega PlayerStart.** Nuqtalar ro'yxatini DataTable yoki Blueprint massivida saqlash mumkin edi,
lekin u holda dizayner nuqtani ko'chirganda ikki joyni yangilashi kerak bo'lardi. `PlayerStart`
esa allaqachon joylashuv + burilishni ushlab turadi, teleport uchun ham xuddi shu transform
ishlatiladi. Bitta manba.

Teg solishtiruvi katta-kichik harfga befarq, lekin aniq: `Rooms` `Room` ga **mos kelmaydi**.
`None` uzatilsa filtrlash o'chadi va hamma `PlayerStart` olinadi.

---

## 4. Remote Control qatlami

Telefondagi panel `/Script/Lanessa.Default__LanessaV2Widget` manziliga yozadi — bu **klassning
default obyekti (CDO)**, jonli vidjet emas.

**Nega CDO.** Jonli vidjetning yo'li har ishga tushganda o'zgaradi:
`...BP_Explorer_GameInstance_LN_C_9.LanessaV2Widget_0` — oxiridagi indeks oldindan bilinmaydi.
Shuning uchun barcha `Remote*` funksiyalari `static` qilingan; ular ichida `GetLiveWidget()`
`TObjectIterator` bilan jonli nusxani o'zi topadi (CDO va arxetiplarni chetlab o'tib, o'yin
dunyosiga tegishlisini afzal ko'rib).

**Nega `false` qaytaradi.** Remote Control ikkala holatda ham HTTP 200 beradi. Shuning uchun
"PIE ishlamayapti" ni "buyruq rad etildi" dan ajratishning yagona yo'li — qaytish qiymati.

**`bAllowAnyRemoteFunctionCall=True` nega shart.** 5.8 da web server funksiya chaqiruvlarini
standart holda rad etadi va buni **jimgina** qiladi — jurnalda hammasi yashil, sahna qimirlamaydi.

---

## 5. Nega refleksiya, nega to'g'ridan bog'lanmagan

Plagin o'zi boshqaradigan Blueprint klasslariga **link qilmaydi**. Hammasi nom orqali:
`FindFProperty`, `CallActorFunction`, `CallParentFunction`, klass nomi prefiksi bo'yicha qidirish.

Sabablari:

1. **Blueprint'dan C++ ga qaramlik qurib bo'lmaydi.** `BP_POI` — Blueprint aktyori; C++ modul unga
   kompilyatsiya vaqtida bog'lana olmaydi.
2. **Plagin ko'chma bo'lib qoladi.** Boshqa loyihada `BP_POI` boshqacha atalgan bo'lsa ham plagin
   quriladi — shunchaki o'sha funksiyani topmaydi va jim turadi.
3. **UDS aktyorining prefiksi sozlanadi.** `Ultra_Dynamic_Sky` blueprinti nusxalansa yoki qayta
   nomlansa, sozlamadan yangi prefiks beriladi. Busiz chiroqlar jimgina ishlamay qo'yardi.

Narxi ham bor: nom xato yozilsa kompilyator aytmaydi, ish paytida jim qoladi. Shuning uchun
muhim joylarda `UE_LOG` bilan diagnostika qoldirilgan (`[LanessaDayNight]` prefiksi bilan).

---

## 6. Tuzoqlar

**`double`, `float` emas.** Blueprint'da yozilgan handler funksiyasining "Float" parametri bu
dvigatel versiyasida `FDoubleProperty` bo'lib aks etadi. `FFloatProperty` bilan
`FDoubleProperty` imzo jihatidan **mos deb hisoblanmaydi**, ya'ni `CreateDelegate` bog'lamaydi —
va xato bermaydi, shunchaki bog'lanmaydi. Shuning uchun `FLanessaV2OnTimePctChanged` ning
parametri `double`.

**Yangi `UFUNCTION` qo'shsangiz Live Coding yetmaydi.** Ctrl+Alt+F11 "muvaffaqiyatli" deydi,
funksiya esa mavjud bo'lmaydi. Editorni yopib to'liq qurish shart.

**UDS xossalari nomida probel bor.** `"Time of Day"`, `"Simulated Sunrise Time"` — GUID
qo'shimchasiz, probel bilan. Reflekisyada aynan shunday yozish kerak.

**Sozlamalar har chaqiruvda o'qiladi.** `DayStart` / `NightStart` ni `.ini` dan o'zgartirsangiz
keyingi chaqiruvdayoq ishlaydi — qayta qurish ham, qayta ishga tushirish ham kerak emas.
`SetDayNightHours()` esa ish paytida o'zgartiradi va `LastState = -1` qiladi, aks holda yangi
jadval chegara o'z-o'zidan kesilmaguncha ko'rinmasdi.

---

## 7. Standart qiymatlar

`Project Settings → Plugins → Lanessa Day/Night`, `DefaultGame.ini` ga saqlanadi:

| Sozlama | Standart | Ma'nosi |
|---|---|---|
| Day Start | 700 | chiroqlar **o'chadi** |
| Night Start | 1700 | chiroqlar **yonadi** |
| Day / Night Emissive | 0.1 / 1.0 | kolleksiya skalyariga yoziladigan qiymat |
| Emissive Scalar Name | `StreetLights` | qaysi skalyar |
| Emissive Collection | `/Game/New_Explorer/Materials/MPC/Emissive_MPC` | qaysi kolleksiya |
| Uds Actor Class Prefix | `Ultra_Dynamic_Sky` | UDS aktyori qanday topiladi |

Vaqt UDS ning 0–2400 shkalasida — soatning yuzdan bir ulushlari, **HHMM emas**. Ya'ni 17:30 bu
`1750`, `1730` emas.
