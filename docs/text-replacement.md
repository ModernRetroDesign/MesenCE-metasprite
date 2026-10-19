## Remaining text replacement goal checklist - 2026-10-18

This is the current remaining-work list for the text replacement goal and supersedes older pending lists below. The cached overlay, regular tile adapters across all seven systems, configurable source cells (including SMS 6x8), visual region selectors, effects and optional word-aware wrapping are implemented. Supported-mode fixture checks already cover RAM and tilemap save-state restoration; broader lifecycle validation remains open.

- [ ] GBA: recognize affine/bitmap backgrounds and mosaic where stable source-cell identity is available.
- [ ] SNES: add text recognition for Mode 7, hires/interlace and mosaic.
- [ ] Assess compatible special paths: SuperGrafx, SGB/linked consoles, WonderSwan LCD icons/blending and guarded video filters; retain explicit guards for unsupported combinations.
- [ ] Add graphics-content/bank identity safeguards when unrelated text reuses a mapped tile index or address; validate mapper changes, raster transitions and dynamic palettes.
- [ ] Expand inherited palette ink beyond entries 1-3.
- [ ] Validate states across scenes/banks/palettes and states saved before profile activation; verify rewind, run-ahead and presentation changes.
- [ ] Expand real-game correctness and performance validation across all supported systems, using anonymous fixture names and hardware/mode/mapper descriptions in reports.
- [ ] Add ligatures, complex-script and right-to-left shaping, and broader language-specific line breaking.
- [ ] Add sprite-based text recognition.
- [ ] Handle partially visible source cells through cross-system screen-edge continuity (deferred).

These tasks extend the existing text editor and cached overlay. PNG/metasprite renderer support for a hardware mode does not by itself complete text recognition for that mode. The overall replacement goal remains unfinished. See docs/text-replacement.md for the implemented workflow and support matrix.
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
| SMS | Mode 4; Graphics I/II; Multicolor (address matching); 40-column text (6x8); native ink | Broader raster mode transitions and real-game validation |
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
   multiples of the source tile width horizontally and 8 vertically. The source tile width is 6 for SMS 40-column text and 8 otherwise. New profiles detect it from Tilemap Viewer; old profiles keep their saved grid. Maximum cells are 128x128, or 126x128 on a 6-pixel grid. For 8x16 characters, map both original tiles;
   for 16x16, map all four. Font size remains independent of this source size.
   Each profile uses one source-cell size. **Source grid origin X/Y** sets the
   tilemap phase within one cell: an 8x16 font starting at tilemap Y=8 uses Y=8,
   while one starting at Y=16 uses Y=0. Capture sets the origin automatically.
5. Optionally enable a **display region** to position and clip the new text.
   Its X/Y/width/height use uncropped gameplay-screen native pixels. Source
   tilemap-region settings independently restrict which tiles are recognized.
   Draw the orange rectangle on the gameplay snapshot, then move it or drag
   its corner/edge handles. **Draw new region** starts another rectangle;
   Alt+drag also draws a new rectangle. The cyan source selector snaps to the selected 6x8 or 8x8
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
runs. Source rows become text lines. With a display region and wrapping enabled, **Prefer word boundaries when wrapping** moves whole words to the next line. Map spaces as characters. Oversized words fall back to character wrapping; spaces at an automatic break are suppressed. Nonbreaking spaces remain inside a word until fallback is necessary. New editor profiles enable this option; older profiles retain character wrapping until selected.
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
publication; font buffers and container overhead are additional. MXT6 packets
carry word-wrap options/separator IDs, source tile width, complete source-cell grids, glyph bounds/advances, separate effects, layout and kerning. Native readers
also accept MXT1/MXT2 fixed-cell, MXT3 text-run, MXT4 source-grid and MXT5 source-width packets. All asset sets publish together, and one
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
- [x] Add SMS legacy Graphics I/II and Multicolor overlays in the verified fixture scopes.
- [x] Add SMS 40-column text mode with proper 6x8 source-cell support in the verified fixture scopes.
- [ ] Add SNES Mode 7, hires/interlace and mosaic support.
- [ ] Assess and support compatible special hardware/presentation paths: SuperGrafx, SGB/linked consoles, WonderSwan LCD icons/blending and currently guarded video filters. Keep unsupported combinations explicitly guarded.
- [ ] Add bank/content identity safeguards against unrelated tiles reusing mapped indices or graphics addresses; expand mapper, raster and dynamic-palette coverage.
- [ ] Expand inherited palette ink beyond entries 1-3.
- [ ] Validate save-state loading, rewind, run-ahead, presentation changes and real-game correctness/performance across the supported systems.
- [x] Add optional word-aware wrapping for current text overlays and preview.
- [ ] Add ligatures, complex-script/RTL shaping and broader language-specific line breaking.
- [ ] Add sprite-based text recognition.
- [ ] Include partially visible source cells in the existing cross-system screen-edge continuity work. This remains deferred.

These items extend the existing text editor and cached overlay workflow. See docs/text-replacement.md for implemented behavior and the support matrix.

## Text overlay save-state restoration checks - 2026-10-18

Verified first-captured-gameplay-frame restoration of active and inactive RAM-triggered text in all seven systems: 24 NES mapper-0 cases plus 48 GB/GBC, SMS Mode 4, single-VDC PCE, GBA regular tile BG, normal-resolution SNES BG1-BG4 and WonderSwan BG/FG cases. Both Debug and Release suites pass. Each case saves both RAM-trigger states, changes the live state, then loads the saved states without reapplying profiles or rewriting RAM. The first captured frame must equal the complete expected active overlay or inactive native image, including the correct output scale. This adds lifecycle evidence; no runtime fix was needed for these fixtures.

- [x] Validate first-frame RAM-trigger restoration from save states in the generated supported-mode text fixtures, in Debug and Release.
- [ ] Validate restoration across different scenes, tile banks/content, palettes and raster configurations, including states saved before profile activation.
- [ ] Validate text during rewind, run-ahead and broader presentation changes and real-game sessions. These remain part of the unfinished text goal.

Evidence: Tests/MetaSprites/obj/text-state-live-{debug,release}.log (48 gameplay cases per configuration), text-state-nes-{debug,release}.log (24 gameplay cases per configuration), build-text-state-core-{debug,release}.log and build-text-state-harness.log. The harness builds with zero warnings/errors. Tests also retain the existing activation, RAM toggling, snapshots and editor checks. See InteropDLL/TestApiWrapper.cpp, RunMetaTextSystemTest and RunMetaTextNesTest.

## Text source-identity save-state checks - 2026-10-18

Extended the live text suites beyond RAM toggling: mutate actual native tilemap indices while keeping the configured RAM trigger enabled, verify the overlay disappears, save the changed tilemap, and restore original/changed/original states without reapplying the pack. Every restore compares the first captured gameplay frame against the complete expected overlay/native image and output scale. Tilemap palette/flip/priority bits are preserved; the WonderSwan fixture's shared-RAM trigger byte is excluded from tilemap mutation. Different tile indices may contain identical blank graphics, so native artwork need not visibly change for the identity test to be meaningful.

All 24 NES mapper-0 and 48 other-system cases pass in Debug and Release, using the supported regular tile modes described in the current matrix. This verifies tilemap source-identity restoration, including index/address mappings and multi-tile cells. No production runtime change was needed.

- [x] Verify source identity and first captured frame after loading different native tilemap states across all seven systems in the generated supported-mode fixtures.
- [ ] Verify graphics/bank changes that reuse the same tile IDs, dynamic palettes/raster changes, states saved before profile activation, rewind, run-ahead and real-game sessions. Content/bank safeguards and unsupported-mode adapters remain unfinished.

Evidence: Tests/MetaSprites/obj/text-content-state-{debug,release}.log (48 cases/configuration), text-content-state-nes-{debug,release}.log (24 cases/configuration), build-text-content-state-{debug,release}.log and build-text-content-state-harness.log. Final native builds and the managed harness succeed; the final harness build has zero warnings/errors. See RunMetaTextSystemTest and RunMetaTextNesTest in InteropDLL/TestApiWrapper.cpp. This extends the preceding RAM-trigger-only state evidence; it does not complete the broader state/rewind goal.

## SMS legacy graphics text overlays - 2026-10-18

Implemented gameplay text capture for legacy SMS Graphics I (mode 0), Graphics II (mode 2) and Multicolor (mode 3). Capture follows the native fetch/shifter pipeline, including transparent/background-colored texels and fetched row colors. Graphics II exposes the expanded banked tile index and masked graphics address, matching the Tilemap Viewer. Multicolor exposes its row-dependent graphics address; its conventional tile index is unavailable. The editor now retains address-only source cells and rejects unavailable index mappings with a clear message. Native single/multi-tile matching also rejects unavailable identity sentinels.

Legacy ink 1 inherits the fetched foreground/left-block color, ink 2 the background/right-block color, and ink 3 aliases ink 1. Color zero resolves through the native backdrop at output time. Scanline resets clear optional text samples. No glyph rasterization is added to gameplay frames.

Verified: 84 cross-system actual-font gameplay cases pass in Debug and Release, including 36 new legacy SMS cases. These cover 8x8/8x16 source cells, Graphics I/II index/address matching, all three Graphics II bank regions, Multicolor address matching, inherited/custom ink, effects/fill/display regions, RAM gating and first-frame RAM/tilemap-state restoration. Each legacy graphics mode also passes actual editor Save and apply, failed-asset retention/retry and disabling profiles. Address-only authoring, the full native compositor suite and 24 NES gameplay regressions pass. Debug/Release UI builds and the harness build have zero warnings/errors. Packaged native core copies match the built DLLs; Release Mesen.exe is present.

- [x] SMS legacy Graphics I, Graphics II and Multicolor text overlays in the verified fixture scopes.
- [ ] SMS 40-column text mode requires proper 6x8 source-cell authoring, selection and matching-grid support; it remains guarded. Broader raster mode transitions and real-game coverage remain open. This does not complete the overall replacement goal.

Evidence: Tests/MetaSprites/obj/text-sms-legacy-{debug,release,editor,apply,native,nes-regression}.log and build-text-sms-legacy-*.log. Current support and workflow: docs/text-replacement.md.

## SMS 40-column text and native source grids - 2026-10-18

Implemented SMS 40-column text-mode gameplay capture with genuine 6x8 source tiles. Fetched identities, graphics addresses and foreground/background ink follow the native shifters; the eight-pixel side borders carry no text identity. Profiles persist SourceTileWidth (6 or 8), with old packs defaulting to 8. The native matcher checks the captured tile width, supports complete 6x8/12x8/6x16/12x16 patterns and larger grids, and preserves per-character origins and proportional adjacency. Source cells still use eight-pixel rows; the maximum six-pixel-grid cell is 126x128 with 336 component tiles.

The Tilemap Viewer detects the native width, captures each source tile at its actual column spacing and seeds new profiles correctly. The source selector supports separate X/Y snapping and keyboard steps. Selection reuse, numeric cell widths/origins and full-pattern capture follow the selected grid. Font size, display-region coordinates and outline/shadow bounds remain independent. Existing profiles are not silently converted when a game changes mode; use separate 6x8 and 8x8 profiles. Mixed raster-mode transitions remain broader validation work.

MXT5 transports the source tile width. Native readers retain MXT1-MXT4 compatibility; managed/native tests cover old JSON defaults, MXT4 patterns, malformed widths, maximum grids and incompatible source-width rejection. No new per-frame glyph rasterization or enlarged text-pixel allocation is introduced.

Verified: 100 cross-system actual-font gameplay cases in Debug and Release, including 16 new SMS text-mode cases for 6x8/12x8/6x16/12x16 source cells, index/address matching, inherited/custom ink, effects/fill/display regions, RAM gating and first-frame RAM/tilemap-state restoration. All 24 NES gameplay regressions also pass in both configurations. Actual 40-column Tilemap Viewer opening/detection, 12x16 capture, Save and apply, failed-asset retention/retry and disabling profiles pass against Debug and Release cores. Editor selector/capture tests, maximum 336-tile capture, the managed suite and full native compositor suite pass. Debug/Release UI and harness builds have zero warnings/errors.

- [x] SMS 40-column text-mode capture, source authoring, matching grids and pack transport in the verified fixture scopes.
- [ ] Broader raster transitions, same-index bank/content guards, dynamic palettes, rewind/run-ahead, advanced layout and real-game validation remain unfinished. Other guarded system modes and cross-system screen-edge continuity retain their prior status. The overall replacement goal remains active.

Evidence: Tests/MetaSprites/obj/text-six-{debug,release,nes-debug,nes-release,editor,apply,managed,native}.log and build-text-six-*.log. Release Mesen.exe is present, with packaged native DLL copies matching the built cores. See docs/text-replacement.md for the current workflow and support matrix.

## Optional word-aware font wrapping - 2026-10-18

Implemented the per-profile Prefer word boundaries when wrapping option for cached text overlays and the editor preview. It applies when a display region and wrapping are enabled. New editor profiles enable it; existing JSON profiles and older runtime packets retain character wrapping until the option is selected. Mapped breaking spaces delimit words. Standard and narrow nonbreaking spaces remain part of a word; a word wider than the region falls back to character wrapping. Trailing spaces at an automatic break are suppressed without adding an empty line before an explicit source row break. Source rows remain hard line breaks.

Word widths use cached glyph advances, internal kerning and letter spacing. Reverse suffix accumulation makes lookahead linear in the matched text length, including long unbroken strings. Glyphs/effects still clip to the configured display region and screen; font rasterization stays at activation. Both 6x8 and 8x8 source grids use the shared native layout. The preview follows the same rules using its cached font metrics. The wrapping control updates when the main wrapping toggle changes.

MXT6 carries the option flag and breakable-space glyph IDs. Native readers retain MXT1-MXT5 compatibility, reject invalid separator IDs and preserve older source-grid semantics. Word-wrap settings round-trip through pack saving and independent editor merges.

Verified: the full native compositor suite covers whole-word movement, character fallback, internal kerning, letter spacing, nonbreaking spaces, trailing-space/source-row handling and legacy character wrapping on both source grids. Actual-font preview and checkbox/persistence tests pass. Two new NES mapper-0 cases compare complete real-font gameplay frames against independently specified positions, including effects, alpha/fill, fine source scrolling, RAM/state restoration and paused snapshots. All 26 NES plus 100 cross-system gameplay cases pass in both Debug and Release. Managed packet/persistence tests and legacy MXT4/MXT5 transport checks pass. Final Debug/Release UI and harness builds have zero warnings/errors. Release Mesen.exe is present, and packaged native copies match the built DLLs.

- [x] Optional word-aware wrapping for current cached text-overlay adapters and preview.
- [ ] Ligatures, RTL/complex-script shaping, broader language-specific line breaking/hyphenation and the other outstanding text recognition/system tasks remain future work. This does not complete the overall replacement goal.

Evidence: Tests/MetaSprites/obj/text-words-{native,managed,editor,nes-debug,nes-release,systems-debug,systems-release}.log and build-text-words-*.log. Current workflow and support matrix: docs/text-replacement.md.
