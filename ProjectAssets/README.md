# Project assets the plugin works with

Nothing here is part of the plugin. These are host-project assets the widgets either load by
absolute path or that carry the Blueprint side of the wiring. They are kept in the repository so the
UI can be reconstructed, and the folder is deliberately **not** named `Content`, so Unreal does not
mount it and the plugin keeps `CanContainContent: false`.

Copy each file to the matching path under the project's `Content`, keeping the folder structure
below.

## Blueprints — the binding layer

The plugin broadcasts; these Blueprints listen and change the scene. Without them the widgets draw
and click but nothing in the world reacts.

| File | Goes to | What it wires |
|---|---|---|
| `BP_Explorer_PC.uasset` | `/Game/New_Explorer/Blueprints/` | Binds every `ULanessaV2Widget` delegate. `OnTimePctChanged` → Ultra Dynamic Sky's *Time of Day*, `OnSeasonChanged` → the UDW season + `Update Season`, `OnWeatherChanged` → the weather presets, plus nav/POI/search/floor handling and `LanessaInteriorOnRoomSelected` |
| `BP_POI.uasset` | `/Game/New_Explorer/Blueprints/` | The POI actor the widget addresses by object `Name`; `Show_POI` / `Hide_POI` / `Select_POI` live here |
| `BP_FloorSectionMarker.uasset` | `/Game/New_Explorer/Blueprints/` | Per-floor section marker driven by `OnFloorSelected` |
| `BP_BuildingSectionMarker.uasset` | `/Game/New_Explorer/Blueprints/` | Per-building section marker |
| `Widgets/BP_Info_Widget.uasset` | `/Game/New_Explorer/Blueprints/Widgets/` | The original POI card the C++ card mirrors; still the target of `OnPoiCardAction` |
| `Widgets/BP_3D_Widget_FloorIcon.uasset` | `/Game/New_Explorer/Blueprints/Widgets/` | The in-world floor icon; clicking it and clicking the floor rail are meant to behave identically |
| `New_widgets/WBP_MasterMenu_Lanessa.uasset` | `/Game/New_Explorer/Blueprints/New_widgets/` | The earlier UMG master menu, kept for levels still on it |

Lights are **not** wired here. `ULanessaV2Widget::SetTimePct` calls
`ULanessaDayNight::ApplyDayNightFromUDS` directly on every drag frame, so the day/night switch is
pure C++ and travels with the plugin.

## The section cut

These two are the whole section mechanism on the material side. Every master material that should
be cuttable wires the function in; the collection is what `BP_Explorer_PC` writes the box into.
See [`../docs/SECTION_MATERIALS.md`](../docs/SECTION_MATERIALS.md) for how to wire a material and
for the four ways it silently fails to cut.

| File | Goes to | What it is |
|---|---|---|
| `Materials/MF/MF_SectionMask.uasset` | `/Game/New_Explorer/Materials/MF/` | Reads the collection, returns `Mask` (dithered) and `Mask Hard` (clean 0/1 — use this one) |
| `Materials/MPC/SectionMask_MPC.uasset` | `/Game/New_Explorer/Materials/MPC/` | `Location`, `Bounds`, `Rotation_Z`, `Mask_Intensity`, `Mask_Falloff` |

Unlike `Emissive_MPC` this collection is **not** repointable: `MF_SectionMask` references it
directly, so both files travel together. The project also contains an older `/Game/ArchVizExplorer/`
copy of each — a material wired to those compiles, shows no error and never cuts.

### The parked volume — a level step, not an asset

`Reset_SectionView` does not disable the cut. Nothing ever writes `Mask_Intensity`, so the cut is
permanently armed and "off" means moving the box somewhere empty: whatever `BP_Explorer_Pawn →
SectionView_Initial_Volume` points at.

That reference is a level actor, so it cannot be shipped here. Recreate it:

1. Place a `TriggerVolume` over the building, sized to cover it in X and Y.
2. Raise it until `Z − Bounds` clears **every** piece of geometry in its XY footprint — remember the
   Blueprint passes `Bounds` as the full extent, so the box is `Location ± Bounds`, twice the
   actor's half-extent.
3. Assign it to `SectionView_Initial_Volume` on the level's `BP_Explorer_Pawn`.

Parking it directly above the building, rather than off to the side, is also what makes the reveal
animate top-to-bottom — the box travels from here to the selected floor, so a volume parked at the
world origin sends it flying sideways across the map instead.

This needs revisiting whenever the building gets taller: added floors are exactly what pushes
geometry up into a volume that used to clear it, and the symptom is the cut never coming back on
CHIQISH or on any nav button. The shipped defaults in the collection park it at
`Z 20000` with `Bounds Z 2152`, clearing this project's tallest geometry at `Z 15029` — reposition
for your own level.

## Textures loaded by absolute path

| File | Goes to | Loaded by |
|---|---|---|
| `T_Lanessa_Logo_Horizontal.uasset` | `/Game/ArchVizExplorer/Blueprints/New_widgets/Textures/` | `LanessaV2Widget`, `LanessaMasterMenuWidget` |
| `T_Weather_Sun.uasset` | same folder | `LanessaMasterMenuWidget` weather row |
| `T_Weather_Cloud.uasset` | same folder | " |
| `T_Weather_Partly.uasset` | same folder | " |
| `T_Weather_Rain.uasset` | same folder | " |
| `T_Weather_Snow.uasset` | same folder | " |

The paths in the code are absolute; renaming a folder breaks the lookup silently — the widget draws
nothing rather than erroring.

## Not included

- **The level.** `/Game/New_Explorer/Maps/Demonstration_01` is ~7 MB and changes constantly. It
  matters because the interior room bar and the walk bar fill themselves from its `APlayerStart`
  actors — membership comes from the actor tag (`Room` / `Walk`), the label from *Player Start Tag*.
  Recreate those tags rather than shipping the map. The section's parked `TriggerVolume` and the
  `BP_Explorer_Pawn` reference to it are level actors too — see *The parked volume* above.
- **`/Game/New_Explorer/Materials/MPC/Emissive_MPC`** — the collection `LanessaDayNight` writes its
  `StreetLights` scalar into. Repointable in `Project Settings → Plugins → Lanessa Day/Night`, so a
  different project can use its own.
- **`/Game/UltraDynamicSky/Blueprints/Weather_Effects/Weather_Presets/*`** — ship with Ultra Dynamic
  Sky. The plugin only names them in a comment; `BP_Explorer_PC` applies them.
- **Fonts.** Read straight from `C:/Windows/Fonts/georgia.ttf` and `georgiab.ttf`, not from an asset.
  Without Georgia installed the widgets fall back to the engine default.
