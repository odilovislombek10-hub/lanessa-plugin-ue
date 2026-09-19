# Lanessa — Unreal Engine 5.8 plugin

Native C++/Slate UI for ArchViz Explorer–based projects: the master menu HUD, the interior tour
chrome, the free-walk spawn bar, and a configurable day/night light switch.

No Blueprint assets — every widget is built in C++/Slate, so the UI is version-controllable as text
and the Blueprint side only supplies data (categories, POIs, unit rows) through `BlueprintCallable`
entry points.

## Modules

| File | What it is |
|---|---|
| `LanessaV2Widget` | The master menu HUD: nav rail, side panels (Qidiruv / Atrofi / Qulayliklar), POI card, chip card, time/weather/season controls |
| `LanessaMasterMenuWidget` | Earlier menu variant, kept for the levels still using it |
| `LanessaInteriorTourWidget` | Interior walkthrough chrome: room pill bar, floor-plan popup, corner label, exit |
| `LanessaWalkPointsWidget` | Free-walk ("sayr") spawn-point bar, same visual language as the interior bar |
| `LanessaDayNight` | Switches every local light + an emissive MPC scalar between day and night |
| `LanessaDayNightSettings` | Project Settings page backing the above (hours, emissive levels, MPC path) |
| `LanessaCustomShapes`, `LanessaLineIcon` | Slate primitives: cut-corner borders, gradients, drag tracks, SVG-path line icons |

## Day/night configuration

`Project Settings → Plugins → Lanessa Day/Night`, saved to `DefaultGame.ini`:

| Setting | Default | Meaning |
|---|---|---|
| Day Start | 700 | lights switch **off** |
| Night Start | 1700 | lights switch **on** |
| Day / Night Emissive | 0.1 / 1.0 | value pushed into the emissive collection scalar |
| Emissive Scalar Name | `StreetLights` | which scalar in that collection |
| Emissive Collection | `/Game/New_Explorer/Materials/MPC/Emissive_MPC` | which collection |
| Uds Actor Class Prefix | `Ultra_Dynamic_Sky` | how the level's UDS actor is found |

Times use Ultra Dynamic Sky's own 0–2400 scale — hundredths of an hour, **not** HHMM, so 17:30 is
`1750`. Values are read on every call, so an edit takes effect immediately: no rebuild, no restart.
`ULanessaDayNight::SetDayNightHours()` overrides them at runtime for a schedule that changes while
playing, optionally writing back to the config.

## Spawn-point bars

Both the interior room bar and the walk bar fill themselves from the level's `APlayerStart` actors:

- **Actor Tags** decide membership — `Room` for interior rooms, `Walk` for walk points
- **Player Start Tag** is the label shown on the pill

```
PopulateFromPlayerStarts(WorldContext, "Room")   // or "Walk"
```

Tag matching is case-insensitive but exact otherwise: `Rooms` will not match `Room`. Passing `None`
disables filtering and takes every `PlayerStart`, for levels that contain nothing else. Clicking a
pill teleports the possessed pawn there (`bTeleportOnClick`, on by default) and broadcasts
`OnPointSelected` / `OnRoomSelected` for Blueprint to hook additional behaviour onto.

## Requirements

Unreal Engine 5.8. Module dependencies: `Core`, `CoreUObject`, `Engine`, `InputCore`, `UMG`,
`Slate`, `SlateCore`, `DeveloperSettings`, plus `UnrealEd` in editor builds only.

## Installing

Copy the folder to `<YourProject>/Plugins/Lanessa`, then regenerate project files and build.
`Config/DefaultEngine.ini` inside the plugin carries `CoreRedirects` so Blueprints that referenced
these classes before they were extracted into a plugin still resolve.

The widgets also load a logo and five weather icons from the host project's content by absolute
path. Those assets, and the rest of the project-side dependencies, are listed in
[`ProjectAssets/README.md`](ProjectAssets/README.md) — without them the UI builds and runs, but
draws no logo and no weather icons.
