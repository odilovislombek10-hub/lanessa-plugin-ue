# Wiring order, and why it is built this way

How the Lanessa widgets reach the scene, and the reasoning behind each decision.
Uzbek version: [`ULANISH_TARTIBI.md`](ULANISH_TARTIBI.md).

---

## 1. The chain

Control flows in exactly **one** direction:

```
  on-screen menu               operator.html (phone)
         |                              |
         |                              v
         |                    Remote*  (static, addressed through the CDO)
         |                              |
         +--------------+---------------+
                        v
              ULanessaV2Widget::SetXxx()
                        |
              1. updates its own state   (SelectedSeason, TimePct, ...)
              2. Invalidate(Paint)       -> repaints
              3. delegate Broadcast      -> OnSeasonChanged, OnTimePctChanged, ...
                        |
                        v
              handler in BP_Explorer_PC
                        |
                        v
        Ultra_Dynamic_Sky / Ultra_Dynamic_Weather / BP_POI / MPC
```

**Why one direction.** The phone panel and the on-screen menu enter through the same `SetXxx()`,
so whichever one is used, the widget and the scene update together. Had the panel written to
actors directly, the on-screen chip would go stale — the operator picks "winter" while the menu
still reads "summer".

The earlier `panel.html` did exactly that (wrote to the UDS actor directly) and was abandoned over
that mismatch. `operator.html` is the corrected version routed through this chain.

---

## 2. Setup order

These must happen **in this order**. Getting it wrong raises no error — things simply do not
appear, which is harder to track down.

| # | Who | What | Why here |
|---|---|---|---|
| 1 | engine | `RebuildWidget()` | Builds the Slate tree and registers `PanelCategoryListBoxes` |
| 2 | `BP_Explorer_PC` | bind the delegates | Binding after the first `SetXxx()` loses that first broadcast |
| 3 | `BP_Explorer_PC` | `SetPanelCategories(panel, tags, labels, defaults)` | `BuildCategoryRow` fills `CategoryPoiListBoxes` |
| 4 | `BP_Explorer_PC` | `SetCategoryPois(panel, tag, poiIds, labels)` | Without the box registered in step 3 it **returns silently** |
| 5 | `BP_Explorer_PC` | `SetSearchUnits(...)` or `PopulateSearchUnitsFromTable(...)` | Data for the search filter |
| 6 | `BP_Explorer_PC` | `SetAvailableFloors([...])` | Fills the floor rail |
| 7 | widget | first `NativeTick` | `bDayNightSynced` runs `ApplyDayNightFromUDS(bForce=true)` once |

**Steps 3 and 4 are order-dependent.** `SetCategoryPois` looks its box up in
`CategoryPoiListBoxes` under the key `PanelTag + ":" + CategoryTag`, and those keys only exist once
`BuildCategoryRow` has run inside `SetPanelCategories`. Called the other way round, no POI rows are
added and nothing warns.

`SetPanelCategories` also clears keys beginning with `PanelTag + ":"` when re-run: the boxes
belonged to category rows about to be destroyed, and stale entries would otherwise leak and be
mistaken for live ones.

**Why step 7 lives in Tick.** The UDS actor may not be loaded when the widget is constructed, so
the initial sync retries every tick until it succeeds, then latches off via `bDayNightSynced`.

---

## 3. Channels

### Time → UDS → lights

The most involved path, because it has three stages:

```
SLanessaDragTrack dragged
    -> SetTimePct(Pct)
         1. TimePct = Clamp(Pct, 0, 1)
         2. Invalidate(Paint)
         3. OnTimePctChanged.Broadcast(TimePct)
              -> BP_Explorer_PC:  UDS."Time of Day" = 600 + Pct * 1600
         4. ULanessaDayNight::ApplyDayNightFromUDS(this, bForce=false)
              -> READS "Time of Day" BACK off the UDS actor
              -> flips the lights if a day/night boundary was crossed
```

**Why step 4 comes after the broadcast, and why it re-reads.** The percent-to-hour mapping
(`600 + Pct*1600`) lives in the Blueprint handler. Had the C++ repeated that formula, one rule
would live in two places and the two could drift. By the time the broadcast returns, UDS already
holds the authoritative value, so the C++ just reads it. **The UDS actor is the single source of
truth.**

The track is not a 24-hour interpolation: it slides the raw HHMM **number** linearly from 600 to
2200. `0.25` → 1000 (10:00), `0.50` → 1400 (14:00). Hence the chip card's `06:00 / 14:00 / 22:00`
labels.

### Lights — why C++ and not Blueprint

`ULanessaDayNight` caches every `ULocalLightComponent` in the level, toggles their visibility with
the day/night state, and writes the `StreetLights` scalar in `Emissive_MPC`.

Three reasons:

1. **Volume.** The scene holds ~1082 light components. A per-frame `Get All Actors of Class` plus
   loop in Blueprint is expensive; C++ caches once.
2. **Edge trigger.** `LastState` remembers the last applied state. During a slider drag the
   function is called every frame but stops at a single `float` compare until the 700 / 1700
   boundary is actually crossed. Without it, every frame would issue 1082
   `MarkRenderStateDirty()` calls.
3. **Blueprint cannot express that cache** — no `TWeakObjectPtr` arrays, no static state.

So there are **no light nodes at all** in `BP_Explorer_PC`. That is deliberate, not an omission.

Cache invalidation: on a world swap (editor ↔ PIE) `EnsureCache` resets `LastState = -1`, since
state applied to the old world's components means nothing in the new one.

### Season and weather

`SetSeason` and `SetWeather` are separate delegates on purpose. Season and weather are independent
axes, so a chip in one row must never silently move the other.

**Why the Blueprint side must call `Update Season`.** Setting UDW's `Season` variable is not
enough. UDS only recomputes Individual Seasons — and pushes `UDW Seasons` into the material
parameter collection — inside `Update Season`, which nothing else calls during play. So the
handler runs: `Season Mode = Manual Setting` → `Season = 0..3` → `Update Season`.

### POI, floors, search

| Delegate | What the Blueprint does |
|---|---|
| `OnPoiEntryClicked(PoiId)` | Finds the actor in `GI.BP_POIs` and calls `Select_POI` |
| `OnPoiCardAction(ActionId)` | `"level"` / `"level2"` / `"360"` / `"media"` — each a different action |
| `OnSearchApplied(MatchingPoiIds)` | `Show_POI` for every id in the array, `Hide_POI` for the rest |
| `OnFloorSelected(Floor)` | Finds the matching `BP_FloorSectionMarker` and calls `Select_POI` |
| `OnCategoryToggled(...)` | Shows/hides by category |
| `OnResetSectionView()` | Returns the section box to `Bounds = 0` |

**What `PoiId` is.** The actor's own object `Name` (e.g. `BP_POI_C_12`). Never shown on screen,
used only to find the actor again. It was chosen because it is stable at runtime, unique, and
obtainable without loading the actor.

**Why `OnFloorSelected` exists.** Clicking a floor number used to only highlight it, with no
functional effect. It now behaves identically to clicking the 3D floor icon — the rule being that
two entry points to the same action must produce the same result.

### Interior and walk bars

`LanessaInteriorTourWidget` and `LanessaWalkPointsWidget` fill themselves from the level's
`APlayerStart` actors:

- membership comes from the actor **tag** — `Room` for interior, `Walk` for free-walk
- the pill label comes from the `Player Start Tag` field

```
PopulateFromPlayerStarts(WorldContext, "Room")   // or "Walk"
```

**Why PlayerStart.** The point list could have lived in a DataTable or a Blueprint array, but then
moving a point would mean updating two places. A `PlayerStart` already carries location and
rotation, and the teleport reuses that same transform. One source.

Tag matching is case-insensitive but otherwise exact: `Rooms` does **not** match `Room`. Passing
`None` disables filtering and takes every `PlayerStart`.

---

## 4. The Remote Control layer

The phone panel addresses `/Script/Lanessa.Default__LanessaV2Widget` — the **class default
object**, not the live widget.

**Why the CDO.** The live widget's path changes every run:
`...BP_Explorer_GameInstance_LN_C_9.LanessaV2Widget_0` — the trailing index is not knowable in
advance. So every `Remote*` function is `static`, and `GetLiveWidget()` finds the live instance
itself with a `TObjectIterator` (skipping CDOs and archetypes, preferring the one in the game
world).

**Why they return `false`.** Remote Control answers HTTP 200 either way, so the return value is
the only way to distinguish "PIE is not running" from "the command was rejected".

**Why `bAllowAnyRemoteFunctionCall=True` is mandatory.** In 5.8 the web server rejects function
calls by default, and does so **silently** — the panel log stays green while the scene never
moves.

---

## 5. Why reflection instead of linking

The plugin does not **link** against the Blueprint classes it drives. Everything goes by name:
`FindFProperty`, `CallActorFunction`, `CallParentFunction`, class-name prefix scans.

Because:

1. **A C++ module cannot depend on a Blueprint.** `BP_POI` is a Blueprint actor; there is nothing
   to link against at compile time.
2. **It keeps the plugin portable.** In another project where `BP_POI` is named differently, the
   plugin still builds — it simply does not find that function and stays quiet.
3. **The UDS prefix is configurable.** If the `Ultra_Dynamic_Sky` blueprint is duplicated or
   renamed, the settings page supplies the new prefix. Without that the lights would just stop
   switching, silently.

There is a cost: a mistyped name is not a compile error, it is runtime silence. That is why the
important paths log through `UE_LOG` under a `[LanessaDayNight]` prefix.

---

## 6. Traps

**`double`, not `float`.** A Blueprint-authored handler's "Float" parameter reflects as an
`FDoubleProperty` in this engine version. `FFloatProperty` and `FDoubleProperty` are **not**
considered signature-compatible, so `CreateDelegate` refuses to bind — and does not error, it just
does not bind. Hence `FLanessaV2OnTimePctChanged` takes a `double`.

**Live Coding is not enough for a new `UFUNCTION`.** Ctrl+Alt+F11 reports success and the function
does not exist. Close the editor and do a full build.

**UDS property names contain spaces.** `"Time of Day"`, `"Simulated Sunrise Time"` — no GUID
suffix, spaces included. Reflection lookups must spell them exactly.

**Settings are read per call.** Changing `DayStart` / `NightStart` in the `.ini` takes effect on
the very next call — no rebuild, no restart. `SetDayNightHours()` changes them at runtime and
resets `LastState = -1`, because otherwise a new schedule would not show until the time crossed a
boundary on its own.

---

## 7. Defaults

`Project Settings → Plugins → Lanessa Day/Night`, saved to `DefaultGame.ini`:

| Setting | Default | Meaning |
|---|---|---|
| Day Start | 700 | lights switch **off** |
| Night Start | 1700 | lights switch **on** |
| Day / Night Emissive | 0.1 / 1.0 | value written into the collection scalar |
| Emissive Scalar Name | `StreetLights` | which scalar |
| Emissive Collection | `/Game/New_Explorer/Materials/MPC/Emissive_MPC` | which collection |
| Uds Actor Class Prefix | `Ultra_Dynamic_Sky` | how the UDS actor is found |

Times use Ultra Dynamic Sky's own 0–2400 scale — hundredths of an hour, **not** HHMM. 17:30 is
`1750`, not `1730`.
