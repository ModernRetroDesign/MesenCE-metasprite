# TrueType font overlays

Cached font overlays support native tile backgrounds on NES, GB/GBC, GBA, PCE,
SMS, SNES and WonderSwan. Use compatible default video filters. Initial adapter
coverage is listed below; transformed modes and broader bank/raster combinations
remain unfinished.

| System | Initial gameplay capture | Guarded/pending |
| --- | --- | --- |
| NES | Background, default PPU | VS/HD PPU variants and screen-edge continuity |
| GB/GBC | Background and window, signed/unsigned addressing and CGB bank selection | SGB and linked consoles |
| GBA | Regular tile BG0-BG3; 4/8bpp and native windows | Affine/bitmap text recognition, mosaic and stereoscopic output |
| PCE | VDC1 background and native palette/grayscale | SuperGrafx and mixed-resolution/forced-scale filters |
| SMS | Mode 4 background, native palette and scroll locks | Legacy graphics/text modes |
| SNES | Normal-resolution tile modes, BG1-BG4, main/subscreen windows | Mode 7, hires/interlace and mosaic text |
| WonderSwan | BG/FG tile backgrounds, native modes/palettes and FG window | LCD icon output and incompatible blending/filter modes |

## Editor workflow

1. Load a game, open Tilemap Viewer and select a hardware background map and
   text tile. Open **TrueType text replacements...** in the settings panel.
2. Add/name a profile and import a TTF/OTF font. Files already at the pack root
   are reused; external filename collisions receive short numbered suffixes.
3. Map tile indices or logical graphics addresses to characters. The captured
   viewer tile can seed a mapping. Exact palette entries override wildcards.
4. Enable **Proportional text runs with font kerning** for natural font spacing.
   New profiles enable it by default; existing profiles retain their setting.
   Font size is independent of source-cell size. Larger glyphs and
   effects extend beyond those cells rather than being cropped to them.
   **Source cell width/height** describe one original character and can be any
   multiples of 8 up to 128x128. For 8x16 characters, map both original tiles;
   for 16x16, map all four. Font size remains independent of this source size.
   Each profile uses one source-cell size. **Source grid origin X/Y** sets the
   tilemap phase within one cell: an 8x16 font starting at tilemap Y=8 uses Y=8,
   while one starting at Y=16 uses Y=0. Capture sets the origin automatically.
5. Optionally enable a **display region** to position and clip the new text.
   Its X/Y/width/height use uncropped gameplay-screen native pixels. Source
   tilemap-region settings independently restrict which tiles are recognized.
   Draw the orange rectangle on the gameplay snapshot, then move it or drag
   its corner/edge handles. **Draw new region** starts another rectangle;
   Alt+drag also draws a new rectangle. The cyan source selector snaps to 8x8
   tiles. **Use selection from Tilemap Viewer** copies the rectangle selected
   before opening the editor. **Match source size / position** copies the source
   rectangle into the display rectangle. Numeric fields and rectangles stay
   synchronized. Arrow keys move, Ctrl+arrows resize, and Shift increases the
   step tenfold. Display steps are one native pixel; source steps are one tile.
   The display selector previews the sample text, including wrapping and effects;
   it does not decode a new string from the snapshot. Changes to display bounds
   refresh the font preview after a short delay. Use **Refresh font preview**
   after changing font/style options. **Refresh gameplay snapshot** captures the
   latest frame, including when paused, without saving a screenshot file. Its
   coordinates exclude decorative widescreen margins and screen rotation;
   cropped overscan is shown as a dark border. If no frame has rendered yet,
   advance/resume the game before refreshing. PCE display coordinates include the native capture porch; the gameplay snapshot shows its dark border. Source preview is a snapshot of
   the hardware layer selected when opening the editor; reopen after changing it.
6. Set optional outline and drop shadow, each with its own RRGGBBAA color.
   Outline width and shadow X/Y offsets accept quarter-pixel increments.
7. Optional fill covers the display rectangle, or matched source cells when
   no display region is configured. It defaults off. A background PNG can
   cover original text instead; the overlay leaves original native tiles intact.
8. Add RAM conditions if needed, refresh preview, then **Save and apply** for
   gameplay. Save profiles alone is also available.

Mappings use hexadecimal identities and optional hexadecimal palettes:

```text
0010 * = A
0011 02 = V
0012 * = U+0020
```

For a profile with an 8x16 source cell:

```text
0010,0020 * = A
0010,0021 * = B
```

The two letters can share their top tile because the whole pattern must match.
Patterns use reading order (left to right, then top to bottom); a 16x16 example
is `0010,0011,0020,0021 * = A`. No automatic consecutive-address assumption is
made. A palette number applies to every component; `*` permits mixed palettes
and inherits the first tile's ink color when palette inheritance is enabled.
Exact-palette patterns take precedence over wildcard patterns.

To capture patterns visually, select one complete letter in the cyan preview,
click **Cell size from selection**, enter its character, and click **Add selected
source cell as character**. Move the source selection to other letters and
repeat. Afterwards select the full dialogue matching region or disable that
region limit. **Add captured source cell** instead starts at the tile selected
in Tilemap Viewer before opening the editor. Capture uses the frozen tilemap
snapshot; refresh the viewer and reopen after switching a bank or layer.

Recognition requires evidence for every tile in the visible source cell and
does not guess missing/offscreen halves. Screen-edge continuity remains future
work. Unmatched or incomplete patterns emit nothing and do not upscale output.

Each mapping contains one printable Unicode scalar. Codepoint notation supports
spaces and supplementary characters. Map spaces as well as letters for text
runs. Source rows become text lines; display-region wrapping breaks at its edge.
Line height 0 uses font metrics. Letter spacing is an optional adjustment on top
of font advances and kerning. Preview sample text uses the same cached glyphs,
font metrics, effect colors and kerning data; native-palette preview uses white.

## Rendering and placement

The overlay runs after native graphics and PNG composition, above sprites and
all PNG insertion bands. Native tiles are retained. Transparent glyph pixels
show the completed image beneath them. Fill can cover old text, including its
transparent edges, and supports alpha. Moving the new text does not erase its
old source location; use the intended background PNG or other artwork there.

Adapters capture visible fetched tile identities, native graphics addresses,
attribute palettes and local coordinates only while text profiles are installed.
The source-region option filters recognition in tilemap coordinates. It does
not constrain a larger font to native source-cell bounds. A display region
positions the output independently and clips overhang, outlines and shadows at
its edges; without it, output is anchored to source text rows and clipped to the
gameplay viewport. Decorative widescreen moves the overlay with that viewport.
Only currently captured visible source tiles contribute characters.

Proportional runs use actual font advances and HarfBuzz-derived pair kerning,
cached at activation. They support left-to-right character layout. Ligatures,
right-to-left/complex-script shaping, word-aware wrapping
and sprite-based text recognition remain future work. Wrapping currently breaks
at a character boundary. Fixed-cell mode remains available for native spacing.

Ink color can inherit the source palette's selected entry (1-3) or use a
custom RGBA value. Outline and shadow have separate cached RGBA layers and are
independent of the ink color. Shadow is an offset silhouette. Font and effect
sizes use native pixels; raster scale controls cached artwork detail.

Graphics addresses currently identify logical PPU locations rather than physical
CHR bank/content identities. Reused identities may match in another context;
use source layer/region and RAM conditions until explicit bank/content guards
are implemented. All RAM conditions must match; absent snapshot bytes fail
closed. Only configured CPU addresses are read with each video frame.

## Persistence and resources

Profiles live under TextProfiles in the ROM's existing groups.json. Version-1
JSON packs without text profiles/new fields continue to load. Saves preserve
independent sprite/background/camera edits and reject stale game/editor contexts.
Closing discards unsaved profile changes but retains imported font assets.

Font glyphs, metrics, kerning and effect images are cached when applied; playback
performs no font rasterization. Duplicate character mappings share glyphs within
a profile. Cached glyph blending and optional simple pixel expansion run in the
shared CPU video path. Text does not trigger native background/OBJ reconstruction.
No matching tiles means no text-driven upscale or output copy.

Bounds: 64 profiles; 4096 mappings/profile; up to 256 distinct characters/profile
for proportional kerning; 32 MiB/font; font size 1-128 native pixels; raster scale
1-8; and an 8-million-pixel preview limit. Glyph coverage/effects and text transport
each have a 64 MiB activation limit. Old/new resources can coexist during
publication; font buffers and container overhead are additional. MXT4 packets
carry glyph bounds/advances, separate effects, layout and kerning. Native readers
also accept MXT1/MXT2 fixed-cell and MXT3 text-run packets. All asset sets publish together, and one
immutable definition snapshot is used throughout each frame's composition.

## Performance and verification

The previous text path forced full-screen native composition at the font scale,
even without matching letters. At 4x this meant 983,040 output pixels per frame.
The cached overlay only locates matches, expands finished pixels when needed,
and blends matched glyphs. A Release CPU benchmark with 32 letters and eight
native sprites measured:

| Case | Median composition time |
| --- | --- |
| Previous 4x tile replacement | 18.66 ms/frame |
| Cached 4x fixed-cell overlay | 0.59 ms/frame |
| Cached 4x proportional overlay | 0.58 ms/frame |
| No matching 4x font tiles | 0.14-0.15 ms/frame, output stays 1x |

These are synthetic compositor measurements, not complete game FPS. Background
PNGs retain their independent composition cost. Evidence is recorded under
Tests/MetaSprites/obj/text-profile-release.log and
text-layout-profile[-proportional]-release.log.

Twenty-four generated mapper-0 actual-font cases pass in Debug and Release:
static/fine X/Y scrolling, tile/address matching, palette overrides, custom alpha,
fill off/on, original-pixel retention, RAM activation/reactivation and restoration,
plus larger proportional fonts in moved display regions with colored effects.
Six cases cover 8x16, 16x8 and 16x16 source patterns, each with fill off/on,
fine scrolling, RAM reactivation and paused/widened/rotated snapshot comparison.
Native tests verify PNG combination, clipping, kerning positions, glyph overhang,
wrapping/line height, fill, crops, missing snapshots and legacy packet parsing.
The editor suite checks real A/V kerning, uncropped glyph bounds, separate effect
colors, preview and persisted controls. Existing NES/SNES authoring regressions
and managed transport tests pass. Debug/Release UI builds pass without warnings.

The cross-system suite adds 48 actual-font full-frame comparisons in Debug and
Release: DMG, CGB BG/window, SMS Mode 4, PCE VDC1, GBA regular BG0, SNES BG1-BG4
and WonderSwan BG/FG window. Cases cover 8x8/8x16 source cells, index/address
patterns, fill, colored effects, display-region placement, RAM deactivation/
reactivation, clearing and native-coordinate gameplay snapshots. Six isolated
editor processes also verify Save and apply, missing-font-glyph rollback/retry,
malformed/wrong-ROM retention and disabling all profiles through the native API.
Native tests cover simultaneous text planes, mixed scales, inherited ink,
transparent source pixels, missing RAM and capture reset. Evidence is in
Tests/MetaSprites/obj/text-systems-*.log. Broader real-game coverage is pending.

Remaining: transformed/guarded adapter modes, bank/content guards, broader dynamic palette/
raster/mapper coverage, state/rewind/run-ahead and presentation coverage, wider
real-game validation, and the advanced text-layout extensions listed above.

## Remaining text replacement goals - 2026-10-17

The initial cached text overlay and regular tile-background adapters are implemented across all seven systems. The following work remains planned and is not marked complete:

- [ ] Extend GBA recognition to affine/bitmap backgrounds and mosaic where a stable source-cell identity can be defined.
- [ ] Add SMS legacy graphics/text modes.
- [ ] Add SNES Mode 7, hires/interlace and mosaic support.
- [ ] Assess and support compatible special hardware/presentation paths: SuperGrafx, SGB/linked consoles, WonderSwan LCD icons/blending and currently guarded video filters. Keep unsupported combinations explicitly guarded.
- [ ] Add bank/content identity safeguards against unrelated tiles reusing mapped indices or graphics addresses; expand mapper, raster and dynamic-palette coverage.
- [ ] Expand inherited palette ink beyond entries 1-3.
- [ ] Validate save-state loading, rewind, run-ahead, presentation changes and real-game correctness/performance across the supported systems.
- [ ] Add word-aware wrapping, ligatures and complex-script/RTL shaping.
- [ ] Add sprite-based text recognition.
- [ ] Include partially visible source cells in the existing cross-system screen-edge continuity work. This remains deferred.

These items extend the existing text editor and cached overlay workflow. See docs/text-replacement.md for implemented behavior and the support matrix.
