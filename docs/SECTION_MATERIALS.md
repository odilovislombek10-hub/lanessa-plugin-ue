# The section cut, on the material side

`WIRING.md` covers how the section panel drives the cut — the floor markers, `Reset_SectionView`,
what CHIQISH undoes. This file covers the other half: how the cut actually reaches the geometry,
and the four ways it silently fails to.

Uzbek version: [`QIRQIM_MATERIALLARI.md`](QIRQIM_MATERIALLARI.md).

---

## 1. The chain

```
BP_Explorer_PC  --Location/Bounds/Rotation_Z-->  SectionMask_MPC
                                                      |
                                        MF_SectionMask reads all five params
                                                      |
                              Masked material: -> OpacityMask   (pixel discarded)
                          Translucent material: -> Opacity      (pixel invisible)
```

The cut is **per-pixel and world-space**. It has no notion of which actor belongs to which building:
every pixel whose world position falls inside the box is cut, in every material that wired the
function in. Actor membership plays no part.

**The canonical assets** — the project ships two copies of each, and only one is live:

| Asset | Use this one | The other one |
|---|---|---|
| `MF_SectionMask` | `/Game/New_Explorer/Materials/MF/` | `/Game/ArchVizExplorer/…` — legacy, 2 references |
| `SectionMask_MPC` | `/Game/New_Explorer/Materials/MPC/` | `/Game/ArchVizExplorer/…` — legacy |

`BP_Explorer_PC` and `BP_Explorer_Pawn` write to the New_Explorer collection, and the project's
GameMode is `BP_Explorer_GameMode_LN`, also New_Explorer. A material wired to the ArchVizExplorer
copy compiles, shows no error, and never cuts.

## 2. The mask

`MF_SectionMask` has no inputs and two outputs. Internally:

```
WorldPosition -> RotateAboutAxis(Z, Rotation_Z, pivot=Location) -> BoxMask-3D(Location, Bounds, Mask_Falloff)
                                                                        |
                        Mask Hard = (1 - BoxMask) + (1 - Mask_Intensity)
                        Mask      = DitherTemporalAA(Mask Hard)
```

So with `Mask_Intensity = 1`:

| Where | Value | Result |
|---|---|---|
| outside the box | `1` | visible |
| inside the box | `0` | cut |

**`Mask_Intensity = 1` means the cut is ON.** Nothing in Blueprint ever writes this parameter —
verified by searching both `BP_Explorer_PC` and `BP_Explorer_Pawn`, which write only `Location`,
`Bounds` and `Rotation_Z`. The cut is therefore *always* armed; "off" is implemented by parking the
box somewhere with no geometry. That single fact explains the reset behaviour in §6.

`Bounds` is used as a **full extent**, not a half-extent: the Blueprint passes `2 × actor half-extent`,
so the effective box is `Location ± Bounds`.

## 3. Use `Mask Hard`, not `Mask`

`Mask` passes through `DitherTemporalAA`, which discards pixels stochastically. TSR does not fully
resolve that, so the cut edge — and on glass the whole surface — comes out as a speckled, blurry
field that reads like unintended roughness rather than a cut.

`Mask Hard` is the same value without the dither: a clean 0/1.

Across the level, 84 masters (4,537 slots) were on `Mask`; all are now on `Mask Hard`.

## 4. Two wiring schemes, one of which does not work

**Opaque / masked materials — use material attributes.**

```
existing chain -> MakeMaterialAttributes -> MP_MATERIAL_ATTRIBUTES
MF_SectionMask . Mask Hard -> Make.OpacityMask       (× the existing mask, if there is one)
use_material_attributes = true        blend = MASKED
```

**Connecting `MF_SectionMask` straight to `MP_OPACITY_MASK` does not cut**, even though the graph
reads back correctly, the blend mode is `MASKED`, the MPC path is right and the material compiles.
This was isolated by putting one mesh in a level where the cut worked, swapping only its material:
with a direct-wired master it stayed solid; with an attribute-wired master it cut. 17 masters
(1,302 slots) were found on the direct scheme and converted.

**Translucent materials — through `Opacity`.**

```
MF_Weather_Glass . Opacity  ×  MF_SectionMask . Mask Hard  ->  Make.Opacity
blend stays TRANSLUCENT
```

## 5. Glass needs two settings changed, not just the graph

A translucent surface at `Opacity = 0` is invisible but **not gone**: it still refracts the
background and still catches specular. On glass that leaves a ghost pane hanging in the air where
the wall was cut away.

The project's own working glass (`New_Explorer/Materials/Glass/M_Glass`, and the POI materials
`M_Holo` / `M_Route`) all share the same two settings, and the ones that ghosted did not:

| Setting | Ghosts | Clean |
|---|---|---|
| `translucency_lighting_mode` | `TLM_SURFACE_PER_PIXEL_LIGHTING` | `TLM_SURFACE` |
| `refraction_method` | `RM_INDEX_OF_REFRACTION` | `RM_PIXEL_NORMAL_OFFSET` or `RM_NONE` |

Note what the working materials do *not* do: they never mask `Specular`. The fix is in the material
settings, not the graph. Changing them also made `M_Corona_Glass` cheaper — 2224 → 1566 PS
instructions.

## 6. The reset parks the box above the level

Because nothing turns `Mask_Intensity` off, `Reset_SectionView` "clears" the cut by moving the box
to `BP_Explorer_Pawn → SectionView_Initial_Volume`, a `TriggerVolume` placed in the level.

That volume must sit **above every piece of geometry in its own XY footprint**, or the reset leaves
whatever it still covers cut away. In this project the volume was at `Z 9676` (box `7524…11828`)
while geometry in that footprint reached `Z 15029`, so the upper floors stayed missing after every
exit. It now points at a duplicate parked at `Z 20000`.

This gets worse as the building grows: adding floors is exactly what pushes geometry up into a
volume that used to clear it. Symptom: the cut never comes back on CHIQISH or on any nav button.

The animation start point is this same volume, so parking it directly above the building also makes
the reveal read as top-to-bottom. Parked off to the side (for example at the world origin, which is
where the MPC default pointed), the box flies across the map and the cut appears to sweep sideways.

## 7. Two traps that cost the most time

**The 2-collection limit.** A material may reference at most **two** `MaterialParameterCollection`s.
Section (`SectionMask_MPC`) plus season/weather (`UltraDynamicWeather_Parameters`) already uses both,
so adding the day/night emissive collection to the same material makes it exceed the limit:

```
Material references too many MaterialParameterCollections!
A material may only reference 2 different collections.
```

It then renders as the **default material** — grey, with no error visible in the viewport. Two
masters hit this. `M_Corona_Master`'s emissive reference turned out to be doing nothing anyway (all
27 of its instances had emissive at 0), so it was removed rather than dropping the weather.

**Stale function-call nodes.** An `MF_SectionMask` node placed before the function gained its second
output keeps reporting only `['Mask']` — `Mask Hard` cannot be connected to it, and
`update_material_function` does not refresh it. The node has to be deleted and re-created. One
material also carried 15 copies of the node with only one connected, which made every scan report it
as broken.

## 8. Checking a material

```
blend            MASKED (opaque surfaces) or TRANSLUCENT (glass)
attr             use_material_attributes = true, section into Make.OpacityMask / Make.Opacity
output           Mask Hard, not Mask
collection       MF_SectionMask from /Game/New_Explorer/...
MPC count        ≤ 2 including the season collection
PS instructions  ≠ 0   (0 means it failed to compile and is rendering as the default material)
```

That last line is the cheapest check there is: a material whose statistics report `PS = 0` is not
being drawn at all as authored, whatever the graph looks like.
