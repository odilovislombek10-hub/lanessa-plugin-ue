# Project assets the plugin loads by path

These `.uasset` files are **not** part of the plugin — the widgets load them from the host
project's content at fixed paths. They are kept here so the repository carries everything the UI
needs, but the folder is deliberately **not** named `Content`, so Unreal does not mount it and the
plugin keeps `CanContainContent: false`.

To set up a project, copy each file to the matching path under the project's `Content`, keeping the
folder structure below. The engine paths in the code are absolute; renaming a folder breaks the
lookup silently (the widget draws nothing rather than erroring).

| File | Goes to | Loaded by |
|---|---|---|
| `T_Lanessa_Logo_Horizontal.uasset` | `/Game/ArchVizExplorer/Blueprints/New_widgets/Textures/` | `LanessaV2Widget`, `LanessaMasterMenuWidget` |
| `T_Weather_Sun.uasset` | same folder | `LanessaMasterMenuWidget` weather row |
| `T_Weather_Cloud.uasset` | same folder | " |
| `T_Weather_Partly.uasset` | same folder | " |
| `T_Weather_Rain.uasset` | same folder | " |
| `T_Weather_Snow.uasset` | same folder | " |

## Not included here

Three more dependencies live in the host project and are too entangled with it to copy standalone:

- `/Game/New_Explorer/Materials/MPC/Emissive_MPC` — the emissive collection `LanessaDayNight`
  writes its `StreetLights` scalar into. Configurable in `Project Settings → Plugins → Lanessa
  Day/Night`, so a different project can point at its own collection instead.
- `/Game/UltraDynamicSky/Blueprints/Weather_Effects/Weather_Presets/*` — Ultra Dynamic Sky's own
  presets (`Clear_Skies`, `Cloudy`, `Partly_Cloudy`, `Rain`, `Snow`). They ship with UDS; the plugin
  only names them in a comment, the Blueprint side applies them.
- Fonts are read straight from `C:/Windows/Fonts/georgia.ttf` (and `georgiab.ttf`), not from any
  asset. On a machine without Georgia installed the widgets fall back to the engine default.
