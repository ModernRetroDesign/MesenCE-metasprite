# Metasprite replacement development

## Fixture aliases and hardware coverage

Hardware descriptions refer to the tested configuration, rather than implying
coverage of every mapper or video mode supported by a system.

| Fixture | System / mapper or hardware mode | Tested behavior |
| --- | --- | --- |
| Cedar | NES mapper 1 (MMC1); supplied ROM | Side-scrolling boundaries, player poses, PNG composition |
| Flint | NES mapper 0 (NROM); generated ROM | Sprite/background replacement, fine scroll, clipping, transparency |
| Quartz | SNES LoROM; generated ROM, BG modes 0–4 and 7 | Sprite ordering, main/subscreen backgrounds, windows, color math, identity Mode 7 coordinates |
| Reed | GB/GBC ROM-only cartridge; generated ROM | DMG/CGB palettes, CGB VRAM bank 1, window and scroll/wrap |
| Slate | GBA; generated ROM, modes 0, 2, 3, 4 and 5 | 4/8-bit OBJ artwork, windows, overlap, text background scrolling, wrapped affine BG2, indexed/direct-color bitmap BG2 |
| Ember | PCE; generated ROM | VDC sprites/backgrounds, wide sprites, dot-clock divider variants |
| Moss | SMS; generated ROM, VDP Mode 4 | Zoom, SMS1 variants, scroll locks and left-edge masking |
| Opal | WS; generated ROM, video modes 0x80/0xC0/0xE0 | Planar/packed artwork, backgrounds, windows and occlusion |

## Main workflow resumed; slowdown investigation deferred

The active goal was updated to contain the metasprite workflow, background layers
and explicitly configured CPU conditions. Slowdown investigation is now outside
that objective; earlier performance notes below are historical.

Tilemap Viewer now offers Replace selected PNG. It keeps the background ID,
name, native layer, placement, scale, scrolling/wrap settings and CPU conditions,
updates the PNG path/dimensions, and retains the previous asset. It can repair an
externally resized asset without creating an extra background layer. Successful
replacement/activation passes all eight authoring configurations; an injected
write failure in the NES configuration preserves the old path, preview and JSON
and removes the newly copied PNG. The chooser rejects changed ROM/editor selection.

Background editor saves now validate the actual PNG header dimensions and scale
before writing JSON, and reject a canvas that no longer matches the imported
dimensions. This prevents an externally resized PNG from committing definitions
that cannot activate. All eight authoring configurations replace a test background
with a 1x1 canvas, verify the saved pack remains unchanged, and restore the PNG.
The preflight checks dimensions/header validity; full pixel decoding still happens
when replacements are applied. Debug and Release UI builds pass after this change.

The current UI and native core also build successfully in Release. The final UI
build reports zero warnings/errors. This ordinary Release build uses MesenCore.dll
beside the executable; Dependencies.zip contains libSkiaSharp.dll and
libHarfBuzzSharp.dll (the core is embedded only with OptimizeUi=true). The GBA
affine/bitmap suite, including live page switching, passes against the Release
core. Release desktop interaction has not been separately verified.

The user verified the visible +/−/Previous/Next controls and preview origin click
in the Debug build. At their request, origin clicks are now always enabled on
both default and alternate PNG previews; the opt-in checkboxes and model flags
were removed. The routed-control suite clicks both canvases directly without an
enable step. Background CPU-condition drafts now also survive layer switching,
matching sprite/alternate draft handling; all eight authoring configurations pass
the background draft-retention and invalid-save checks.

Invalid CPU-condition text for groups and alternates is now retained in the
editor when switching group selection. Previously it silently reverted to the
last valid parsed conditions. Correcting the text clears the temporary draft;
Reload saved groups discards it explicitly. Invalid drafts are never serialized
as CPU checks. The actual-window suite switches away and back with invalid group
and alternate text and verifies both survive before correction.

The current Debug build passes both `--selection-suite` and `--pointer-suite`
for Snes, Nes, Gameboy, Gbc, Gba, Pce, Sms and Ws after the background support
validation change. This rechecks live snapshots, automatic Create PNG/JSON saves
and native activation, persisted names/pose origins, artwork import rollback,
masked alternate conditions, background PNG export/import and placement, stale
editor protection, and routed drag/Ctrl/origin/navigation/removal controls. These
tests exercise real editor models and hidden XAML windows; visible desktop hit
testing, rendering and interactive file dialogs still require verification.

SNES Mode 7 now has main/subscreen integration fixtures using an identity matrix,
tile 0 with a nonzero indexed color, and separate OBJ VRAM so sprite graphics do
not overlap the affine tile data. Both pass captured map-coordinate checks and
the existing PNG alpha/composition/disable-restoration checks. They are included
in both the full gameplay and SNES ordering suites. Horizontal 2x scale and
quarter-turn rotation also pass for main/subscreen placement. The tests independently
calculate coordinates for all visible pixels, including negative rotated coordinates
wrapped into the 1024x1024 map, and sample a patterned replacement using the
transformed position and configured placement offset. Extended BG2 fixtures now
pass with both low (index 1) and high (index 0x81) pixel priority bits on the
main and subscreens, checking coordinate capture, PNG sampling/composition and
restoration. The native tests now explicitly assert BG2 fallback priority 1 or 5
for every visible pixel, rather than inferring priority from fixture output color.
Same-screen BG2/OBJ priority transitions, out-of-map behavior and
hires/interlace remain incomplete.

Source audit identifies why SNES hires/interlace cannot currently use replacements:
`SnesPpu` acquires a fixed 256x239 capture, excludes OBJ-interlace decoded rows
and coverage, and skips detailed tiled BG capture in hires modes. Color-math
state capture is disabled for double-width output. `MetaSpriteCompositor::Apply`
returns the native buffer whenever the filtered output exceeds capture dimensions.
The earlier gate is in `SnesPpu::SendFrame`: high-resolution/interlaced frames
are sent without a metasprite capture attached. `RenderScreen` emits 512x478
output, places subscreen/main pixels in even/odd columns for hires, duplicates
main pixels for low-resolution lines within a high-resolution frame, and maps
rows using current-field parity and the selected deinterlace mode. Capture must
follow those per-line mappings, including mixed-resolution frames and bob/weave
handling; changing the SendFrame gate alone cannot enable correct replacements.
These are implementation gaps, not merely missing tests. Extending support must
carry both hires output phases and interlace field parity through capture, sprite
recognition, background samples and color math; simply enlarging the capture or
removing the OBJ-interlace guard would not preserve native rendering semantics.

Runtime background support validation now shares the packet builder's activation
policy: only enabled layers with configured CPU conditions are checked for native
layer support. An inactive legacy draft therefore cannot block sprite activation
because its native layer is unsupported. Targeted checks cover all seven CPU
types: drafts and disabled layers are ignored, active unsupported layers fail,
and active layer 0 passes. UI compilation and the bitmap/export/PNG helper suite
pass after this change.

GBA affine coverage now includes a Mode 2 BG2 fixture with a 256x256 wrapped
map, identity matrix and reference offsets (253, 250). It verifies captured
coordinates for every native screen pixel, rejects samples on disabled layers,
and checks every output pixel outside the metasprite against a striped replacement
PNG sampled at the captured affine coordinates. The same fixture checks BG2
alpha blending with the backdrop, transparent and partial-alpha metasprite pixels,
and restoration after disabling replacements. It passes through the native PPU
and video decoder using `--gba-affine`; the full gameplay suite includes it too.
The affine suite also passes a scale/shear matrix (PA=512, PB=256, PC=0,
PD=256) and a quarter-turn matrix (PA=0, PB=256, PC=-256, PD=0), in GBA
8.8 fixed-point units. Expected coordinates are independently computed for every
native pixel, including negative coordinates wrapped back into the map. Patterned
PNG output, backdrop blending and sprite alpha are verified for both transforms.
Mode 4 indexed bitmap BG2 now has dedicated coverage: identity transform and
all 240x160 captured sample coordinates, OBJ tile 512 in bitmap-safe VRAM,
BG2 priority and blend targets, replacement alpha, composition and disable
restoration. A second Mode 4 fixture fills page 1 with nonzero palette indices,
leaves page 0 transparent, and selects page 1 before display. Its captured BG2
opacity proves the selected page is used; the same alpha/composition/restoration
checks pass with that opaque bitmap. Mode 3 (240x160) and Mode 5 (160x128)
direct-color fixtures now pass with nonzero blue bitmap pixels. The native test
checks every sample's coordinates and opacity, every HD background output pixel,
sprite alpha and disable restoration. Mode 5 also verifies that pixels outside
the bitmap remain the native backdrop rather than receiving replacement artwork.
A live Mode 4 fixture now toggles DISPCNT's bitmap page bit every 16 vblanks,
using transparent page 0 and opaque page 1. With PNG replacements active, the
native hook observes BG2 opacity change in both directions while the composed
replacement stays correct. The fixture also passes the existing coordinate,
sprite-alpha and disable-restoration checks in the GBA affine/bitmap suite.

The shared artwork browser's Load replacement PNG action now replaces the PNG
currently being viewed. For an alternate it preserves the name, CPU conditions,
scale and origin, and retains its previous PNG file. Failed saves restore the
previous path and remove only the newly copied file. The chooser also rejects
an import if the selected artwork changes while it is open. All eight system
configurations pass the actual-window routed-control suite, including successful
alternate import and injected save-failure rollback. These windows remain hidden;
visible desktop painting and file-picker interaction are still unverified.

The full native gameplay fixture suite also passed: PNG alpha, clipping,
priority, background composition, ROM-driven CPU condition transitions and
activation/clear behavior. Hardware coverage is the fixture table above; this
does not establish coverage of advanced video modes absent from those fixtures.

Sprite group saves validate every known default PNG canvas against its selected
scale before writing JSON. Incompatible scale edits therefore preserve the
previous saved pack and active runtime settings. Actual window tests set the
numeric scale control to an incompatible value, invoke Save, verify unchanged
JSON, then correct the scale and verify saving/activation succeeds.
Validation now reads PNG header dimensions for every assigned default and
alternate image, including unselected alternates and older definitions without
cached dimensions. It does not allocate or decode full pixel buffers. Browsing
and the add-image action also require valid settings for the currently displayed
alternate. Tests verify an invalid alternate scale is preserved on attempted
navigation and prevents saving even when that alternate is no longer selected.

Artwork now uses one shared preview area. **Previous** and **Next** cycle through
the default PNG and every alternate, wrapping at either end. **+** opens PNG
import and selects the new alternate; **−** removes the displayed alternate
definition and retains its file. Removal is disabled for the default PNG.
Alternate name, scale, origin and memory conditions occupy the same editor area
as the default artwork instead of a separate expander below it. Earlier/Later
controls retain explicit matching priority independently of preview navigation.

New alternate PNGs may be saved as drafts before their memory checks are
configured. A blank condition list leaves them inactive: runtime serialization
excludes these drafts and does not decode their PNGs. Adding valid CPU conditions
and saving activates their existing conditional matching behavior. Window tests
cover browsing/wrapping, mutually exclusive editor panels, remove enablement,
last-alternate removal, retained files and draft-condition persistence.
Browsing also preserves invalid condition text by requiring correction before
switching away from that alternate. An injected file-write failure verifies that
removal restores the selected alternate and preserves saved definitions.

`Tests/ViewerLifecycle --pointer-suite <system>` loads the real Mesen application
styles/resources in an isolated configuration home, creates the actual Sprite
Viewer window and measures its XAML layout. Routed pointer events travel through
the real PictureViewer and window handlers. Tests cover forward/reverse drag at
zoom 1, 2 and 4 with the host DPI scale, Ctrl-click removal, wrapped-copy selection,
pointer release and capture-loss cancellation. These tests pass for NES, GB/GBC,
GBA, PCE, SMS, SNES and WS. Windows remain invisible; desktop hit testing, painting
and file-picker interaction are still separate verification requirements.
The same window suite now refreshes a native snapshot, invokes the actual
**Create meta group** button's routed click handler to save/enable the group PNG,
sends a routed press to the actual origin preview canvas and checks both
crosshair border bindings and persisted replacement coordinates after reload.
With origin editing disabled, the same click leaves the anchor unchanged.

Tilemap Viewer background saves likewise reject newer on-disk background
definitions from another editor. A stale remove restores the layer, and a
stale PNG import removes only its newly created unsaved copy and restores the
allocated ID. **Reload saved backgrounds** retrieves the latest layer settings.
Sprite-only edits do not conflict with background saves. Actual two-editor
tests cover save, remove, import cleanup and resumed editing for all systems.

Sprite Viewer saves compare the latest on-disk sprite definitions with the
definitions originally loaded or last saved by that viewer. If another editor
has saved different sprite groups, the stale save fails with a reload message
instead of overwriting newer definitions. Background-only edits do not cause
this conflict and continue to be preserved when saving sprite groups. **Reload
saved groups** retrieves current definitions and discards unsaved group edits
and selection. Actual two-viewer tests verify rejection, preservation of newer
JSON, reload and resumed editing across all target systems.

The Sprite Viewer group dropdown binds an observable display label, so editing
the optional group or pose name updates the numbered entry immediately.
Clearing both names retains the persistent `Meta NNN` label. The derived label
is excluded from saved JSON. An actual Avalonia text binding test verifies
renaming and clearing without reloading the group list.

Sprite Viewer now provides **Align PNG pose origins** alongside native animation
origin alignment. It uses the selected pose's replacement PNG anchor offset
from bottom center, measured in native pixels, and applies that offset to each
other replacement PNG in its pose family. Different PNG canvas sizes and scales
are handled independently. The source anchor, native group origins, other
families and conditional alternate anchors retain their values. The whole
family is validated before mutation; a failed save restores previous PNG anchors.
Managed tests cover different artwork sizes/scales, and the actual viewer suites
exercise persisted alignment and an injected filesystem save failure.

Enabled background PNGs require at least one explicitly configured CPU memory
condition, and all conditions must match. The Tilemap Viewer rejects saving an
enabled layer with no conditions. Runtime packet generation skips conditionless
layers without decoding their PNGs, including older saved definitions loaded at
startup. Such definitions remain editable. Blank background conditions no longer
mean unconditional gameplay replacement; older notes below describe prior behavior.

At the user's request, further slowdown investigation is deferred. Development
has returned to the Sprite Viewer metasprite authoring workflow; background PNG
layers remain a secondary goal and optional 3D artwork remains future work.

Base replacement PNG import now restores the previous artwork settings and
retains the existing preview when saving fails. Only the newly created unsaved
copy is removed, including when a copy operation fails partway through. Existing
and source artwork remain intact. The file dialog and automated authoring tests
use the same import method.

The actual Avalonia/native authoring suites pass for NES, GB, GBC, GBA, PCE,
SMS, SNES and WS, including successful base PNG import, origin preservation,
failed-save cleanup, pose creation, conditional alternate artwork and background
layer authoring. The UI builds with zero warnings and errors. These tests use
invisible windows; visible pointer interaction and advanced background video
modes still require verification.

## Configured Cedar fixture scrolling-boundary reproduction (NES mapper 1 / MMC1)

The supplied commercial-ROM reproduction is called **Cedar** in development
notes. Its iNES header identifies mapper 1 (MMC1), 128 KiB PRG ROM, 128 KiB
CHR ROM and battery-backed RAM. The test covers side-scrolling background
coordinates, scrolling boundaries, metasprite pose recognition and PNG
composition. This is a fixture alias; the original local ROM, save-state and
pack filenames are retained so existing reproduction commands remain usable.

The user clarified that walking to the edge after the camera stops scrolling,
then reversing direction, can resolve the slowdown. The viewer harness now
supports `--scroll-turn --settings <settings.json> --software-renderer` alongside
`--cold-start <home> preloaded|queued|disabled <rom> --state <state>`.

The supplied settings are read and applied to native configuration inside an
isolated test home. Output-folder overrides are disabled so screenshots, battery
saves and other test output stay isolated. The native test hook configures the
regular SoftwareRenderer without opening a window or audio device, and the
managed harness consumes its normal RefreshSoftwareRenderer callback through
the actual SoftwareRendererView bitmap-transfer code. Five-second PNG snapshots
contain the final composited replacement frame, rather than only the original
video-filter output. The decoder/compositor already ran in earlier headless
tests; this option adds the regular renderer and its Avalonia bitmap transfers.
Desktop window painting and GPU presentation remain excluded.

An important correction to earlier real-game measurements: those runs did not
apply NES video configuration. The native palette remained black, and the
supplied player-character groups did not match. Their FPS comparisons demonstrate capture
overhead, not verified use of the supplied PNG artwork. The earlier reported
8% capture optimization remains measured, but is not proof of that improvement
with matching real-game replacements. Configured runs now require at least one
recognized group before their result is accepted. The actual authoring suite
also applies native configuration, and all seven systems plus GBC still pass.

The first movement attempt continued too long after the camera stopped and
left the castle for the overworld; its later FPS rise is confounded by that
scene change. The revised script waits for 30 stable camera frames, continues
right for another 30, and then turns left. It abandons a suspected boundary if
scrolling resumes first. It releases left after 360 frames or a subsequent
stable camera stop. In the supplied state this reverses at movement frame 241
and releases at frame 601. Composited screenshots confirm the character stays
in the castle and the supplied PNG, including its opaque red background, is
actually replacing the player character. Meta 001 and walking poses are recognized.

Three fresh-process, configured Debug runs over 60 seconds:

| Activation | FPS observation | Asset activation |
| --- | --- | --- |
| Preloaded | Approximately 34–35 throughout | 52.364 ms at frame 1 |
| Queued | First interval 29.59, then approximately 34–35 | 27.962 ms at saved-state frame 1418 |
| Disabled in copied pack | Approximately 39 | 17.300 ms at frame 1 |

No run showed a lasting FPS improvement caused by the verified castle reversal,
or a recovery around 45 seconds while remaining in that scene. Preloading
removes delayed asset activation, but this comparison does not prove that it
fixes the user's recovery symptom. The next reproduction scope still needs
actual visible-window/GPU presentation and the user's renderer selection.

Authoritative reports and composited PNGs are under the viewer harness's
`bin/Debug/net10.0/artifacts`:

- Preloaded: `cold-preloaded-a0c2c6c5706e4555920f2ebe698e48f5`
- Queued: `cold-queued-6251cfbb18de4acc84035d3de89129be`
- Disabled: `cold-disabled-48c3f8fbfcc54d89b094deff946b4223`

## Cedar gameplay capture cost (NES mapper 1 / MMC1)

The supplied `Debug/SaveStates/realgamerom_1.mss` successfully loads through the
native StateLoaded notification. Fresh-process, 60-second Debug measurements in
that side-scrolling scene with the supplied nine-group pack and actual viewer
model refresh gave roughly 24.6–25.2 FPS with preloading, and 24.4–25.2 FPS with
queued loading. Disabling replacements only in an isolated pack copy, while
retaining recognition metadata and the viewer schedule, gave 36.2–36.8 FPS.
These tests expose sustained native replacement overhead, but do not reproduce
the reported recovery around 45 seconds. No player input or desktop presentation
is included; the scene advances from the supplied state with neutral controls.

NES background-coordinate capture previously shifted fifteen records every dot.
It now advances a circular buffer and uses the same logical coordinates when
loading fetched tile data and reading the visible sample. The same gameplay
measurement after this change gave 26.4–27.6 FPS (mean 26.94 versus 24.89 before,
about 8% higher throughput). Debug and Release core builds succeeded. All seven NES pixel fixtures
pass in both configurations, covering sprite replacement, backgrounds, fine scrolling,
masking, cropping, source holes, transparent artwork and disable restoration.
This measured improvement does not close the remaining performance goal.

Reports under `Tests/ViewerLifecycle/bin/Debug/net10.0/artifacts`:

- Before: `cold-preloaded-70ef26831e424861afbfff78455fb7d8/startup-timing.csv`
- Queued: `cold-queued-36025f3b2bdc4848adc2c2a0410438ad/startup-timing.csv`
- Disabled: `cold-disabled-7a20b3e8309d4dcfb2510710503ef2e1/startup-timing.csv`
- Circular buffer: `cold-preloaded-75272fdec4c34cd080086af44cd4ee64/startup-timing.csv`

`--cold-start <home> disabled <rom> --state <state>` changes only the copied pack,
disabling native sprites/backgrounds while keeping the same viewer recognition
workload. `--gameplay-nes <core-dll>` in the metasprite test harness runs the seven
NES native pixel fixtures without repeating unrelated-system tests.

## Cedar reproduction and viewer allocations (NES mapper 1 / MMC1)

The user supplied the Cedar fixture as `realgamerom.nes`
and its NES pack in `bin/win-x64/Debug`. The cold-start harness now accepts an
optional ROM path after the activation mode, copies the pack into an isolated
directory, and records allocated bytes and generation 0/1/2 collection counts
alongside five-second FPS intervals. `--state <file>` can start the measurement
from a supplied gameplay save state; successful state loading is checked through
the native StateLoaded notification.

Two fresh-process Debug runs using that ROM and pack at the title sequence gave
approximately 28–29 emulated FPS for both modes. Preloaded activation took
99.362 ms at frame 1, versus 33.929 ms at frame 14 for queued activation. Neither
showed a recovery around 45 seconds. The user clarified that the recovery occurs
only during side-scrolling gameplay, so these title-sequence results do not
reproduce or resolve the reported issue. Desktop presentation is excluded.
The reports are `cold-preloaded-f1ec9dba06de467285046bd864dc6315/startup-timing.csv`
and `cold-queued-91f489ded15b4c7989eb50e60a9f4ff8/startup-timing.csv` under the
viewer harness's `bin/Debug/net10.0/artifacts` directory.

Live recognition now reuses pixel buffers by hardware sprite index, updates
their contents on every refresh, reallocates when dimensions change, and clears
them when loading group definitions. Saved groups retain their independent
pixel arrays. `--selection-suite Nes --recognition-benchmark` measures the actual
paused viewer refresh after warming up, including native preview generation and
recognition. Allocation decreased from 142,424 to 124,504 bytes per refresh
(about 13%); measured time was 0.2300 versus 0.2211 ms per refresh. This isolated
allocation improvement is not evidence of a fix for the real-game recovery.
All seven systems and GBC passed actual authoring, PNG/background/alternate
imports, save-failure recovery, reload and pre-emulation activation regressions
after the buffer change. UI build completed with zero warnings and errors.

## Fresh-process preload versus queued activation

The harness accepts `--cold-start <saved-test-home> preloaded|queued`. It copies
the same saved HD pack into a new isolated directory, initializes a fresh emulator
process without authoring first, and compares synchronous GameLoaded activation
with queued UI activation. Recognition metadata is populated without reactivating
the pack; each mode decodes/activates only through its selected load path. Both
use the same actual Sprite Viewer model refresh workload for 60 seconds and save
five-second interval CSVs. Desktop presentation and a cold filesystem cache are
not part of this test.

For a SNES Debug fixture with two groups, one conditional alternate and a gated
background, preloaded activation took 57.678 ms at frame 0; queued activation
took 21.425 ms at frame 10. Preloaded throughput stayed approximately 26–27 FPS
through 55 seconds; queued throughput also stayed approximately 26–27 FPS.
Neither run showed the reported sustained recovery around 45 seconds. This
single comparison does not show a sustained FPS advantage from preload, and
does not establish the cause or resolution of the real-game symptom. Preload
does ensure asset decode/packet activation finishes before emulation begins.

## Actual conditional alternate artwork authoring

The actual viewer suite now imports an alternate PNG with two CPU-byte conditions
(one masked), edits its independent origin, saves and reloads the pack, and then
imports/saves a background in that same pack. All seven systems and GBC pass.
Saved-pack preload covers base groups, alternate artwork and the background before
emulated frame/clock progression. Native condition matching and configured-address
snapshot checks remain covered by the separate gameplay fixture suite.

Alternate PNG import now uses the same path-based code after the file dialog and
removes its new PNG on failed saving. It restores the prior alternate selection,
conditions and list. All-system checks force group-condition validation failure
after copying and verify unchanged prior JSON, retained existing assets and no
orphan alternate file. Background failure rollback now also passes across all
targets. Debug UI builds with zero warnings/errors.

These checks do not exercise visible-window dialogs or prove changing artwork
pixels for these newly authored alternate definitions.

## Actual tilemap authoring and background import recovery

The real viewer integration suite now constructs TilemapViewerViewModel for all
seven systems and GBC, refreshes a native snapshot, saves its frozen PNG snapshot,
imports it through the same path-based import code used after the file dialog,
and saves background placement/scroll/wrap settings with one masked CPU-byte
condition. Native activation, background reload and sprite/background shared-pack
preservation pass across all targets. Saved pack preload with sprites and the
background also completes without advancing the emulated frame/clock.

Background import now removes its newly copied PNG if definition saving fails,
restores the next layer ID/list, preserves existing artwork, and reports any
cleanup failure. A SNES integration check forces definition validation failure
after copying and verifies no extra background PNG, unchanged prior JSON and
retained source artwork. Debug UI builds with zero warnings/errors.

These checks use actual models and native emulation without a visible window.
They do not exercise file-dialog/pointer routing or prove rendered pixels for the
newly authored background; existing native gameplay fixture coverage is separate.

## Sixty-second viewer/model startup timing

The live model harness accepts `--selection-suite Snes --startup-timing`. After
its authoring checks and saved-pack preload, it resumes emulation, schedules
actual SpriteViewerViewModel snapshot refreshes, drains UI dispatcher work, and
records five-second emulated-FPS intervals for 60 seconds. It saves CSV under the
isolated test configuration directory. This includes debugger/model/bitmap work,
but no desktop presentation; the earlier authoring phase warms managed methods,
so it is not a fresh-process cold-start benchmark.

One Debug SNES run measured 30.60 FPS at 5 seconds, 25.38/19.20 at 10/15 seconds,
then mostly 30.39–31.00 through 55 seconds (26.99 at 45 seconds). It showed no
sustained throughput recovery at 45 seconds. PNG preload took 2.690 ms with no
emulated progress. This does not establish that preload fixes the reported real
game slowdown; a fresh-process comparison of queued and preloaded activation,
and actual desktop presentation, remain unverified.

## All-system actual viewer authoring checks

`Tests/ViewerLifecycle --selection-suite <system>` accepts Snes, Nes, Gameboy,
Gbc, Gba, Pce, Sms and Ws. Each run constructs the actual SpriteViewerViewModel
with its native emulator fixture and isolated workspace configuration/assets.
All targets pass snapshot refresh, group creation with automatic enabled PNG,
default-folder JSON/PNG persistence, naming, artwork-origin/crosshair editing,
pose creation with persistent family/IDs and consistent captured origins,
native activation status, and viewer reload.

All targets also preload saved groups synchronously in GameLoaded with both
frame counter and emulated master clock unchanged. NES starts its counter at 1
(`NesPpu.cpp` initialization), so a universal zero-frame assertion was incorrect;
the fixture now checks no emulated progress during decoding/activation instead.
The earlier SNES zero-frame result remains valid for that system.

These are actual model/native integration checks with no visible window. They
do not prove pointer routing, visually rendered crosshairs, replacement pixels
for these specific authored assets, or the reported 45-second FPS recovery.

## Pre-emulation saved artwork loading

The revised performance report is a roughly 20% FPS drop on load that clears
after about 45 seconds. Saved packs previously decoded through a queued UI job
after GameLoaded, allowing PNG decoding and packet construction to overlap with
gameplay. MainWindow now loads them synchronously in GameLoaded after applying
per-game settings. The core sends this notification while paused, before starting
emulation and video threads. The shared loader eagerly decodes enabled base PNGs,
alternate PNGs and background PNGs during activation, not during frame composition.

The actual SNES viewer integration test creates a saved pack and reloads its ROM
with the shared loader in a GameLoaded listener. Native activation succeeds and
the frame count remains zero before and after decoding. A one-group fixture
measured 2.679 ms for this preload. This proves ordering, not resolution of the
reported 45-second FPS dip; longer real-game cold-start timing remains required.

SMS assets can now be registered before its program selects Mode 4. Rendering
capture remains guarded by UseMode4, and the compositor requires valid captured
background samples, so preload does not add support for other VDP modes. Invalid
layer indices and unsupported console layouts remain rejected.
The Debug core and UI build successfully. The complete all-system Debug gameplay
suite and real bitmap/PNG/Create-save regressions pass after this change.

## Actual viewer model selection and Create integration

Run `Tests/ViewerLifecycle` with `--selection-suite` from the repository root.
It initializes native emulation and Avalonia with no visible window and keeps
configuration, saves and HD assets in a unique workspace artifact directory.
The fixture constructs the real SpriteViewerViewModel and verifies rectangle
selection, exclusion of disabled sprites, positive-overlap edge behavior,
wrapped-copy offsets, Ctrl-click addition/removal, and clearing selection.

It then refreshes a real SNES fixture snapshot, selects a hardware sprite, calls
the actual Create flow, verifies PNG/JSON in the default per-ROM/system folder,
checks gameplay enabled by default and successful native activation status, and
reloads the viewer's persistent group. These checks pass. They do not exercise
visible-window pointer routing, prove rendered replacement pixels, or establish
the corresponding Create integration on the other six systems.

## Create-group save failure recovery

Create now uses `SaveNewMetaGroup` to export and assign the default-folder PNG,
enable gameplay, and save the definition. If export or definition saving fails,
the helper removes only the PNG that it created and restores the group list and
next ID. The existing outer handler restores pose-family changes. If cleanup
also fails, the status reports the unsaved PNG's path and the cleanup error.
An activation failure after a successful save still retains the saved group/PNG.

The real Avalonia harness invokes this same helper and verifies automatic export,
assignment and enablement, preservation of a pre-existing filename, rollback on
an injected definition-save failure, unchanged prior JSON, and successful retry
with the rolled-back group ID. All bitmap/PNG checks pass, and the Debug UI builds
with zero warnings/errors. These helper tests do not prove live selection,
Create-button interaction or native gameplay activation.

## Separating debugger and viewer callback overhead

The live viewer test now measures debugger initialization without any viewer
schedule between its native and two-viewer phases. This distinguishes CPU/debugger
instrumentation from scheduled PPU callbacks; none of these phases draw desktop UI.
The remaining per-PPU-cycle debugger ownership-wrapper lookup and interrupt lookup
now use the already initialized, lifetime-stable `CpuInfo.Instance` pointer.
This produced only small timing differences, not a fix for the full slowdown.

One Debug run measured these mean delivered-frame intervals in milliseconds:

| System | Native | Debugger only | Debugger + two schedules |
| --- | ---: | ---: | ---: |
| GB | 5.20 | 9.69 | 10.75 |
| GBC | 5.25 | 9.66 | 10.77 |
| GBA | 11.37 | 25.72 | 30.30 |
| PCE | 3.64 | 10.31 | 12.35 |
| SMS | 4.00 | 8.45 | 9.99 |
| NES | 9.80 | 21.42 | 23.41 |
| SNES | 7.96 | 20.54 | 22.12 |
| WS | 3.68 | 8.03 | 8.74 |

Most measured overhead is already present with debugger instrumentation alone.
Both Debug and Release live callback delivery/removal suites pass for all seven
systems and GBC. Release SNES measured 1.27/1.90/1.96 ms and GBA 1.19/1.96/2.18 ms
for these same phases. These representative ROMs are not a substitute for real-game
desktop FPS verification or replacement-enabled timing.

## Sprite preview pixel comparison

The shared `SpritePreviewModel.Init` now uses span comparison/copy instead of a
scalar compare-and-copy loop. Unchanged artwork does not need to be copied back
into the cache. Newly allocated bitmaps still receive their pixels even when the
cache matches; changed artwork still invalidates the bitmap once.

The real Avalonia harness has an optional `--preview-benchmark` argument. In the
Debug UI, 128 sprites at 64x64 measured 0.8920 -> 0.1188 ms per unchanged refresh
and 0.9543 -> 0.1922 ms when every sprite's last pixel changes. At 8x8 the cost
remained small (0.0516 -> 0.0543 ms unchanged, 0.0589 -> 0.0818 ms changing).
These are model update timings, not complete viewer rendering or gameplay FPS.

Regression checks read the actual bitmap and verify first/last pixel updates,
restored transparent pixels, cache retention without redundant invalidations,
and dimension changes with unchanged cached pixels. Bitmap disposal and all PNG
round-trip checks pass. The Debug UI builds with zero warnings/errors.

## Straight-alpha gameplay PNG loading

The real bitmap harness found that export preserved half-transparent yellow as
RGBA (255,255,0,128), while the former gameplay loader changed RGB to (254,254,0)
through its premultiplied UI bitmap conversion. The shared sprite/background loader
now uses the existing Skia codec to decode BGRA with straight alpha directly.
Canvas dimensions/scale are validated before allocating the decoded bitmap, and
incomplete decode is rejected. The Debug UI builds with zero warnings/errors.

The harness passes exact gameplay pixel comparisons for relative placement and
partial alpha, a PNG round trip across all 256 alpha values using nontrivial RGB,
and rejection of corrupt PNG data and incompatible canvas scale. UI display
conversion may still round partial-alpha colors by one level; gameplay decoding
preserves the PNG's original bytes. Bitmap lifecycle checks also still pass.
These checks do not establish desktop FPS or full Create-button interaction.

## Real bitmap lifecycle and PNG verification

`Tests/ViewerLifecycle` uses the built Debug UI assemblies and initializes Avalonia
without creating a visible window. Restore it from its local directory with
`.tools/dotnet/dotnet.exe restore Tests/ViewerLifecycle/ViewerLifecycle.csproj
--source Tests/ViewerLifecycle`, then run it with `--no-restore` after building UI.
It requires no additional NuGet packages.

The harness verifies unchanged sprite dimensions retain the real bitmap, changed
dimensions replace and dispose it, cleanup clears the model property, and repeated
cleanup is safe. It also invokes the actual viewer group bitmap builder, saves a
PNG and reloads it through Avalonia. Every pixel of the 4x3 test canvas is checked:
transparent gaps, source transparency and relative member placement all survive
the round trip. Both checks pass. The output is in the harness's `artifacts` folder.
This proves these bitmap paths, not pointer interaction, automatic Create-button
file assignment, complete XAML rendering or desktop FPS.

## Sprite preview bitmap lifetime

Sprite Viewer now releases old per-sprite bitmaps after publishing a replacement
when sprite dimensions change. Rebuilding the sprite list releases the old models'
bitmap resources; replacing the screen preview releases its old bitmap as well.
Viewer disposal releases the current sprite and screen bitmaps. Property updates
precede disposal so bindings can move to the replacement. This addresses resource
retention during animation/layout changes and viewer closure. The Debug UI builds
with zero warnings/errors. Interactive lifecycle and desktop FPS measurements
remain unverified; this change has no measured performance claim.

## Live selection bounds

Sprite Viewer now calculates the selected hardware sprites' bounds in one pass,
without allocating a filtered list and repeating position lookups for each edge.
It still uses the selected wrapped-copy offsets, includes sprite dimensions and
returns an empty selection rectangle when no current sprite is selected. Selected
group PNG preview loading was confirmed to occur on group selection changes,
rather than every live refresh. The Debug UI builds with zero warnings/errors.
This small allocation reduction has no measured FPS claim; pointer interactions
and full desktop viewer timing still require end-to-end verification.

## Tilemap visibility reuse

SNES tiled backgrounds now compute capture presence once per render call. For
each pixel, native drawing and BG-layer capture share the main/subscreen window
results instead of evaluating masks twice. Transparent native texels still record
enabled layers when needed for PNG alpha; high-resolution subscreen drawing keeps
its existing path. The window helper only reads state. Both core builds and the
focused SNES suite pass after this change.

The Debug fixture measured 10.38 ms native, 15.89 ms capture and 16.20 ms with
one PNG (previously 17.41 ms). One-PNG p95 was 16.73 ms, with 8/120 intervals
above 16.67 ms, so this does not prove steady 60 FPS. Combined PNG/BG measured
46.62 ms. Release means were 1.55, 3.13, 3.66 and 9.28 ms, with no skipped
frames and every interval below 16.67 ms. Desktop presentation is excluded;
all-system Debug performance and the full workflow verification remain open.

## Four-layer background ordering

SNES background preparation now orders its at-most-four native layer records
directly with insertion, retaining source order for equal priorities. Debug and
Release core/native builds and native composition checks pass. The focused SNES
suite verifies scrolling, windows/subscreen behavior, transparency, memory gates
and BG modes 0..4 after this change.

The Debug combined PNG/background fixture measured 47.47 ms versus 47.55 ms
previously; this does not demonstrate a meaningful speedup. Native, capture and
one-PNG means were 10.96, 17.00 and 17.41 ms. These are core/headless timings,
and the full performance objective remains open.

## SNES color-math capture setup

Color-math capture eligibility, scanline selection and row-buffer lookup now run
once in ApplyColorMath rather than once per pixel. The pixel function receives
the local row pointer; per-pixel windows, subscreen priority and effect decisions
are unchanged. The focused SNES ordering suite passes, covering transparency,
priority, window/subscreen behavior, BG layers/modes and memory-driven selection.
Debug and Release core builds pass.

The latest Debug means were 10.99 ms native, 17.37 ms capture, 17.59 ms one PNG
and 47.55 ms combined PNG/BG. The previous one-PNG mean was 18.33 ms; native
timing also changed, so this is an incremental improvement. Release means were
1.53, 3.19, 3.80 and 9.26 ms, with every interval below 16.67 ms and no skipped
frames. Debug capture/composition and desktop viewer performance remain open.

## SNES capture-row access

SNES rendering now takes local pixel/background row pointers once per render
segment. Scanline selection and the background-capture presence check are also
outside the pixel loops. Pointers do not survive the call; frame buffers remain
fixed during rendering. The four captured BG layer records still copy in native
order. Both core configurations build and the focused SNES ordering suite passes,
including holes, overlapping sprites, rotation, subscreen/window cases, BG0..3
scrolling, modes 0..4 and memory-driven artwork/background gates.

Debug mean capture timing decreased from 19.66 to 17.81 ms; one PNG decreased
from 19.80 to 18.33 ms. Native measured 11.31 ms and combined PNG/BG 46.34 ms.
Release means were 1.42 ms native, 3.05 ms capture, 3.65 ms one PNG and 9.00 ms
combined, with every interval below 16.67 ms and no skipped frames. These results
exclude desktop presentation. Debug capture and combined composition still exceed
the 60 FPS budget, so the performance objective remains incomplete.

## Fetched-row allocation

SNES and WonderSwan capture reserve eight pixel entries before appending a fetched
row; SMS reserves its zoom-adjusted row width. GBA reserves the remaining width
when starting a contiguous fetched slice. This removes intermediate vector growth
without changing row contents or capture lifetime. Both core configurations build,
and the full seven-system Debug gameplay suite passes.

A row-buffer recycling experiment was removed after its Debug capture mean rose
to 20.83 ms versus 18.77 ms with reserved widths. The final Debug run measured
11.56 ms native, 19.66 ms capture, 19.80 ms one PNG and 47.36 ms combined PNG/BG.
These timings do not establish a substantial speedup from width reservation.
Final Release means were 1.50, 3.07, 3.65 and 9.15 ms, with no skipped frames
and every interval below 16.67 ms. The remaining capture overhead requires
further investigation; desktop presentation is excluded from these measurements.

## Alpha boundary fast paths

Shared sprite/background alpha composition now returns the foreground for alpha
255 and the background for alpha 0, retaining opaque output alpha. Partial alpha
uses the same rounding and channel blend as before. Direct background composition
takes stable pixel/output pointers after validating dimensions; it retains authored
layer priority even when the first matching layer is transparent. No native
visibility or priority checks are skipped. Native tests explicitly cover boundary
alpha with nonopaque input colors in addition to existing transparency/effects
verification. Both core builds, Debug/Release native checks and the full Debug
seven-system gameplay suite pass.

The SNES Debug fixture measured 11.26 ms native, 19.16 ms with capture, 19.34 ms
with one PNG and 47.36 ms with PNG plus background, versus 50.39 ms for the previous
combined case (about 6% less frame time). Release measured 1.42, 3.00, 3.61 and
9.04 ms respectively, with no skipped frames and all intervals below 16.67 ms.
These measurements exclude desktop drawing; the Debug performance goal remains
incomplete.

## Gameplay matching follow-up

The native compositor rejects unrelated positions and dimensions before scanning
matched candidate indices. Its matched-index vector retains capacity across
anchor attempts, and consumed flags use a fixed local byte buffer instead of
vector<bool> proxy access. Recognition order, memory gates and composition remain
unchanged. Debug/Release native verification and core builds pass. The full
seven-system Debug gameplay suite passed, including transparency, priority,
alternate-artwork transitions, background memory gates and activation rollback.

The SNES Debug timing fixture measured 11.36 ms native, 19.07 ms with capture,
19.73 ms with one PNG, and 50.39 ms with the PNG plus background replacement.
This does not establish a substantial gameplay improvement from the matching
change. Release measured 1.51, 3.24, 3.67 and 9.61 ms respectively, with every
interval below 16.67 ms and no skipped frames. Capture and combined-background
composition remain important Debug performance targets; desktop presentation
is excluded from these measurements.

## Live Sprite Viewer matching

The managed matcher now rejects candidates at unrelated coordinates before
checking whether they reuse a previously matched hardware sprite. That check
uses a plain loop instead of allocating a LINQ predicate/enumerator for every
candidate. Group member traversal is indexed. Graphic equality, relative layout,
origin placement, candidate order and exclusion of duplicate wrapped hardware
sprites retain their existing behavior. PNG decoding was already outside live
refresh; this change targets snapshot recognition.

`--matcher-benchmark` compares the previous algorithm with the current matcher
using repeated 8x8 graphics in a four-member pose. It verifies 420 identical
origins/member assignments for 512 candidates including wrapped copies, and
105 matches for 128 ordinary candidates. Over 20 calls after warm-up, the latest
Debug managed run measured:

| Snapshot | Previous mean | Current mean | Previous allocation/call | Current allocation/call |
| --- | ---: | ---: | ---: | ---: |
| 128 candidates | 3.298 ms | 0.217 ms | 2,735,922 bytes | 17,626 bytes |
| 512 candidates with wraps | 18.926 ms | 2.515 ms | 40,974,970 bytes | 70,210 bytes |

Existing matching/persistence/serialization checks pass; the Debug UI builds
with zero warnings and errors. This is a substantial improvement in this viewer
subsystem, not an end-to-end desktop FPS measurement. Gameplay capture/composition
and remaining CPU debugger overhead remain separate performance targets.

## Cross-system debugger timing and memory counters

CPU dispatch now caches each CPU debugger's pointer immediately after construction,
while its unique owner retains lifetime control. The typed dispatch helper and
frequent step-back checks use that stable pointer; no instruction, breakpoint,
counter or script processing is skipped. Debug and Release core builds and all
eight representative ROM viewer checks passed. Debug GBA scheduled-viewer mean
changed from 33.84 to 31.92 ms, with native baseline changing from 12.23 to
11.77 ms. Other systems showed small changes: this is an incremental result,
not a resolution of the slowdown. All Release viewer intervals remained below
16.67 ms with no skipped frames. Desktop drawing remains outside these tests.

Disassembler::BuildCache now takes a local pointer and size for the source buffer
once per call, retaining a Debug entry-range assertion. The pointer is never stored
across calls, so program-cache reset can still replace its buffer. Instruction
decoding, flag updates and overlapping-instruction clearing remain active. Both
core builds and the all-system viewer diagnostic passed after this change.
Debug scheduled-viewer means remained approximately 11.5 ms (GB/GBC), 33.8 ms
(GBA), 13.8 ms (PCE), 11.3 ms (SMS), 26.4 ms (NES), 25.8 ms (SNES), and 9.9 ms
(WS); this does not demonstrate a meaningful performance improvement. Release
means ranged from 0.58 to 2.22 ms, with every measured interval below 16.67 ms
and no skipped frames. CPU debugger instruction processing remains a profiling
target; these results still exclude desktop viewer generation and drawing.

The user reports the slowdown on every system in a Debug build, with one active
metasprite, and also when opening Sprite Viewer. The new `--viewer-refresh
<MesenCore.dll>` diagnostic measures representative ROMs before and after debugger
initialization with two viewer schedules. Generate its ROMs with `--gameplay`
first. It observes refresh notifications and verifies removal stops delivery.
Frame skipping is disabled for GBA, PCE and SNES so delivered-frame timing remains
comparable. These tests exclude desktop viewer snapshot generation and drawing.

All seven systems, plus GBC, passed delivery/removal checks in Debug and Release.
The Debug measurements below are mean milliseconds per delivered frame over 120
intervals; there were no skipped frames. They compare two operating modes of the
current build, rather than measuring an optimization against an earlier build.

| System | Native, no debugger | Debugger and two viewer schedules |
| --- | ---: | ---: |
| GB | 5.67 | 11.76 |
| GBC | 5.61 | 11.78 |
| GBA | 12.13 | 33.45 |
| PCE | 3.85 | 13.43 |
| SMS | 4.22 | 11.00 |
| NES | 11.49 | 26.08 |
| SNES | 9.05 | 25.45 |
| WS | 3.99 | 9.60 |

MemoryAccessCounter now caches pointers to its fixed-size counter buffers,
avoiding checked vector access per byte. Buffers are never resized; reset clears
their contents without invalidating cached pointers. Debug assertions retain
type/range checks. Copying the counter owner is disabled. Both build configurations
passed focused checks for mixed read/write/execute widths, timestamps, ignored
negative addresses, reset, and access after reset. No instrumentation is disabled.
The SNES Debug scheduled-viewer measurement was 27.55 ms versus the earlier
28.21 ms; this small difference does not establish a substantial improvement.

The rebuilt Release SNES diagnostic measured 2.10 ms with two schedules, 3.58 ms
with capture and the debugger, 3.80 ms with one PNG, and 9.73 ms with a PNG plus
background replacement. Every interval was below 16.67 ms with no skipped frames.
The Debug performance goal and full desktop viewer measurements remain open.

## Graphics viewer refresh scheduling

The shared PPU viewer scheduler now precomputes scanline and cycle membership
when a viewer is added, retimed or removed under the existing DebugBreakHelper.
Ordinary PPU ticks reject without traversing viewer entries; possible refresh
ticks still check the exact pair and send the same ViewerRefresh notification.
CPU debugging, breakpoints, logging, memory counters and scripting remain active.

Native tests cover multiple IDs at one timing, retiming an existing ID, removal,
unknown IDs, zero/65535 coordinates, bit-word boundaries and false-positive mask
pairs. The SNES live diagnostic now registers two schedules and observes actual
notifications. It received 121/121 notifications over 120 measured frame intervals
(boundary tolerance 118–123), with no unexpected IDs; removal stopped delivery.

Before this change, native Debug timing with the debugger initialized averaged
26.7684 ms/frame, and adding two schedules raised it to 35.0526 ms/frame.
The final run measured 26.1796 ms with no schedules and 28.2142 ms with two
schedules: about 20% less frame time than the prior scheduled-viewer case.
Capture with the debugger initialized measured 34.5456 ms. These are core and
notification measurements; desktop snapshot generation, matching and drawing
are excluded. Broader debugger overhead and the full performance goal remain
unresolved.

Debug and Release core builds and native schedule/compositor checks pass.
The Release live diagnostic also verified 121/121 notifications and removal,
measuring 2.00644 ms with the debugger initialized, 2.08275 ms with the two
schedules and 3.64894 ms with capture plus the debugger. Every Release stage
stayed below 16.67 ms with no skipped frames. Subsequent live scheduling checks
on all systems are recorded above; desktop performance remains unverified.

## Background traversal and debugger overhead

Background views now iterate immutable layer pointers directly, avoiding checked
STL iterator construction inside output-pixel loops. The shared compositor sizes
all staging arrays before taking fixed data pointers and verifies captured pixel
dimensions before use. Native tests cover empty views, preserved authored order
and malformed captured dimensions in addition to existing effects/alpha checks.

The Debug combined sprite/background fixture improved from 103.597 ms/frame to
68.9723 ms after view iteration changed, then to 50.2992 ms after staging access
changed. The final run measured 12.0523 ms native without capture, 20.2443 ms
with capture, and 20.5336 ms with one PNG. The full seven-system gameplay suite
passed after these changes; Debug/Release native verification and core builds pass.
The dense Release compositor benchmark measured 13.4462 ms mean, 14.2224 ms p95,
14.8188 ms p99, 17.3001 ms maximum, 1/120 intervals above 16.67 ms, with checksum
1048540628796416 unchanged.

`--snes-viewer-core-timing <MesenCore.dll>` adds two stages after ordinary timing:
native output with the debugger initialized, then capture with the debugger
initialized. The Debug run measured 12.3425 ms native without the debugger,
26.7532 ms native with the debugger, and 35.1697 ms capture with the debugger.
This reproduces roughly a 54% reduction in available native FPS from debugger
initialization alone. It excludes viewer snapshots, matching and desktop drawing.
No debugger work has been disabled: CPU instruction, call-stack, CDL and memory
instrumentation must be considered alongside viewer-only needs before changing
this behavior. The performance goal remains incomplete.

## Capture frame reuse and scanline coverage

All seven PPU adapters now acquire capture frames from a per-PPU pool capped at
three retained frames. Reuse requires sole pool ownership; frames still held by
the decoder or renderer remain unchanged. If every slot is held, capture uses
an uncached frame instead of blocking or mutating a published frame. Reset clears
all scalar pixel state, both coverage lists, backgrounds, fetched rows, effect
flags, frame number and configured-address snapshots. Coverage capacity is retained.

Native checks cover held references, pool overflow, reuse, default restoration,
dimensions, coverage growth/copy/move/swap and aliasing during growth. The full
seven-system generated gameplay suite passed with pooled frames. SNES scanline
coverage also uses the lightweight coverage storage, avoiding checked STL iterator
construction for each empty pixel list.

The Debug allocation diagnostic measured 6.81397 ms/frame for new frame allocation
and destruction versus 0.570086 ms/frame for reuse/reset. The final Debug live
timing fixture measured:

| Stage | Mean ms | p95 ms | p99 ms |
| --- | ---: | ---: | ---: |
| Native without capture | 11.5514 | 11.6294 | 22.6006 |
| Native with capture | 19.6662 | 19.8945 | 31.5423 |
| One metasprite PNG | 19.8162 | 20.0913 | 31.1885 |
| Metasprite and background PNG | 103.597 | 105.352 | 105.799 |

Both native test configurations pass and Debug/Release core builds succeed.
The one-PNG mean is about 59% lower than the initial 48.7681 ms measurement,
but capture remains slower than native output. Background composition and desktop
Sprite Viewer performance remain outstanding; these measurements do not establish
completion of the performance goal.

The focused SNES suite passed after the scanline storage change, including
priority rotation, overlap, windows, color math, background holes and scrolling,
BG0–BG3/modes 0–4, changing CPU-memory conditions and combined pack activation.
The rebuilt Release live fixture measured 1.48196 ms native without capture,
3.19807 ms with capture, 3.67955 ms with one PNG, and 9.64553 ms with both sprite
and background PNGs. Every Release stage had zero intervals above 16.67 ms and
zero skipped frames; desktop presentation and viewer refresh are excluded.

## Capture storage improvement

Per-pixel GBA/SNES coverage now uses lazily allocated storage with pointer
iteration instead of constructing two checked STL vector proxies per pixel.
It retains all coverage entries, order, deep-copy semantics, and storage reuse
after clear. Debug and Release native checks exercise growth through 128 entries,
copy/move independence, clear/reuse, and appending an existing entry during growth.

After this change, the same Debug timing fixture measured:

| Stage | Mean ms | p95 ms | p99 ms |
| --- | ---: | ---: | ---: |
| Native without capture | 12.1009 | 12.498 | 24.266 |
| Native with capture | 28.1616 | 32.0727 | 42.1067 |
| One metasprite PNG | 28.0328 | 29.7623 | 40.3708 |
| Metasprite and background PNG | 107.131 | 108.592 | 109.102 |

The one-PNG mean improved from 48.7681 to 28.0328 ms, but the Debug slowdown
remains unresolved. A new `--benchmark-capture-allocation` native diagnostic
measured 6.95258 ms per 256×239 frame for allocation and destruction alone in
Debug, identifying frame lifecycle cost as the next investigation target.

The full generated gameplay suite passed across NES, GB/GBC, GBA, PCE, SMS,
SNES and WonderSwan after the storage change. The final Release native dense
layer benchmark measured 14.6798 ms mean, 15.557 ms p95, 16.4531 ms p99,
17.0166 ms maximum and 1/120 intervals above 16.67 ms; checksum unchanged.

Sprite Viewer matching now keeps its secondary snapshot buffers across refreshes
and uses the existing snapshot when the viewer background is transparent.
The UI build passed without warnings or errors. Desktop viewer performance
has not yet been measured.

## Debug slowdown reproduction and background export

The reported slowdown affects all systems in Debug, with one active metasprite,
and also occurs when opening Sprite Viewer. The timing fixture now measures a
native stage without capture before enabling capture, to separate capture cost
from PNG composition. A Debug SNES run measured 120 intervals per stage:

| Stage | Mean ms | p95 ms | p99 ms |
| --- | ---: | ---: | ---: |
| Native without capture | 11.5591 | 11.723 | 22.8251 |
| Native with capture | 52.2163 | 57.656 | 63.1289 |
| One metasprite PNG | 48.7681 | 54.5466 | 60.1127 |
| Metasprite and background PNG | 118.772 | 119.78 | 120.591 |

This reproduces substantial capture overhead; it does not establish a fix or
measure desktop Sprite Viewer rendering. Per-pixel frame construction currently
includes GBA and SNES coverage vectors on every system. Viewer group recognition
also renders a separate transparent sprite snapshot per refresh; both remain
investigation targets.

The Release dense SNES layer benchmark after shared row preparation and
power-of-two background wrapping measured 14.0632 ms mean, 14.6288 ms p95,
15.0052 ms p99, and 15.1805 ms maximum, with no intervals above 16.67 ms.
The checksum remains 1048540628796416. Native compositor checks include negative
and extreme wrapping coordinates, power-of-two and arbitrary canvas sizes.
These Release results do not resolve the Debug slowdown.

Tilemap Viewer's background panel now offers **Export selected tilemap PNG**.
It freezes the current displayed bitmap before the save dialog and preserves
its alpha format; the export includes the current display settings. UI compilation
passed with no warnings or errors. Interactive export remains to be verified.

## Latest performance evidence: live Release timing

`--snes-frame-timing <Release MesenCore.dll>` now measures 120 delivered-frame
intervals in each stage using the actual emulator, capture, decoder, and headless
renderer readback. It checks that emulation is unthrottled, reports percentiles,
deadline overruns, and skipped frames. MaximumSpeed must be enabled after ROM
loading: creating the rewind manager clears that flag, so enabling it before
LoadRom measures paced output instead of available processing time.

The measured fixture has one 8×8 hardware sprite with 2× partial-alpha metasprite
artwork, and a scrolling-capable SNES background layer (zero scroll in this run).

| Stage | Mean ms | p95 ms | p99 ms | Maximum ms | Over 16.67 ms | Skipped |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Native with sprite capture | 4.93734 | 5.6546 | 6.2093 | 6.4155 | 0/120 | 0 |
| Metasprite PNG | 5.00106 | 5.5344 | 5.8308 | 7.2373 | 0/120 | 0 |
| Metasprite and background PNG | 12.0496 | 12.5144 | 12.6394 | 12.6913 | 0/120 | 0 |

These are delivered-frame intervals through the asynchronous headless pipeline,
not isolated per-frame latency or desktop GPU presentation measurements. They do
not establish the 16.67 ms requirement for dense groups, larger PNG scales, other
systems, or the interactive application. Those performance checks remain required.

The synthetic Release benchmark also now warms up eight frames and reports 120
individual timings, including checksum readback. The captured-layer workload
measured mean 15.2036 ms, p95 15.9056 ms, p99 16.2735 ms, maximum 16.6288 ms,
zero overruns, and unchanged checksum `1048540628796416`. That workload has
eight groups of sixteen members but excludes emulation, capture, filtering, and
presentation, so its narrow remaining margin still requires optimization.

## Latest implementation: SNES background layer capture

SNES now retains samples, native colors, opacity, priorities, screen enables, and
window eligibility for all enabled BG layers, including native transparent texels.
The compositor places each PNG overlay in its native layer, orders BGs with OBJ,
and applies captured main/subscreen color math afterward. PNG alpha continues to
overlay that layer's native color; at native holes, lower layers supply the alpha
fallback. Full layer capture uses a separate SNES-only frame vector and is allocated
only while background definitions are loaded. Metasprite-only playback and other
systems do not allocate the additional layer data.

Native tests cover transparent layers, partial alpha over a lower native layer,
OBJ/BG priority, window exclusion, and subscreen math. Two new live ROMs verify
that PNGs fill native BG holes and respect a masked stripe. The focused SNES suite
passed these cases alongside modes 0–4, scrolling, layer identities, and memory
conditions. Cross-system live cases passed through the initial SNES scenarios;
the first run stopped at an incorrect new-fixture expectation, corrected to account
for native backdrop math being disabled before loading a background PNG.

The new `--benchmark-snes-background-layers` Release benchmark measures this path
with eight sixteen-member groups, partial-alpha artwork, and a 256×239 frame at
2× output. An OBJ-priority early return reduced the measured run from 18.3358 to
15.8777 ms/frame with checksum `1048540628796416` unchanged. These are synthetic
composition measurements, excluding capture and presentation; gameplay performance
is still unfinished. Active offset-per-tile, mosaic, Mode 7, high resolution,
interlace, and interactive editor verification remain outstanding.

## Latest verification: changing CPU memory

A new SNES ROM fixture changes WRAM `$7E0000` between 0 and 1 every sixteen
vblanks and writes the related byte `$7E0002` as `$A0 | state`. The live test
observes at least four transitions in each of two stages. It checks every newly
observed frame against that frame's captured values: first alternate blue/white
metasprite PNGs and a gated green background, then group gating and restoration
to native output size. Expected colors include SNES main/subscreen half-add math.
Conditions exercise exact bytes, masked bytes, and multiple addresses together.
Each snapshot must contain exactly those two configured addresses, excluding the
ROM's separate vblank counter. The changing-memory fixture passed; other systems
still need equivalent ROM-driven transition coverage.

## Latest verification: SNES background modes 2–4

The focused `--snes-ordering` live suite now includes twelve additional ROMs:
BG0 and BG1 in modes 2, 3, and 4, each tested on the main and subscreen with
scrolling. Mode 3/4 BG0 uses all four plane pairs and palette color 85, so an
incorrect 4-bit decode cannot reproduce the expected native color. All cases
passed captured-coordinate checks and existing PNG placement, alpha, color-math,
memory-gating, and restoration checks. The combined pack activation checks passed.

These cases use ordinary scrolling with offset-per-tile entries inactive. They do
not verify active offset-per-tile, mosaic, Mode 7, high resolution, or interlace.
At the time of this earlier verification, capture retained only the winning native
background sample. The newer layer-capture implementation above supersedes that
limitation. The full cross-system goal stays active.

## Goal

The Sprite Viewer is the authoring tool for numbered groups of hardware sprites,
an editable origin in native pixels, transparent group PNG export, and replacement
PNG import. Runtime replacement must support NES, Game Boy/GBC, GBA, PCE, SMS,
SNES, and WonderSwan while retaining each system's clipping, sprite order,
background priority, and effects. Background PNG layers belong in the Tilemap Viewer.
CPU-memory conditions will select alternate metasprite PNGs and gate background
replacements.

Development priorities:

1. Main goal: complete the Sprite Viewer metasprite workflow across all seven
   systems. Select recurring sprite arrangements on screen, assign persistent
   group numbers and optional names, set their origins, export PNG artwork, and
   load replacement PNGs aligned to those origins during gameplay.
2. Secondary goal: complete PNG background replacement layers in the Tilemap
   Viewer across all seven systems, including PNG export, placement, scrolling, and native
   composition with sprites.
3. CPU-memory conditions: select alternate metasprite PNGs when all configured
   byte/mask conditions match, and gate background replacements the same way.
   Capture and check only user-defined addresses; never save full memory dumps.
4. Performance: investigate and fix slowdowns with PNG replacement active;
   reproduce the reported 50–70% FPS drop in Debug, including one active PNG
   and opening Sprite Viewer. Also track the 16.67 ms frame-time budget. Measure recognition,
   hashing, native capture, composition, filtering, and presentation together;
   report frame-time variation as well as averages. A compositor-only benchmark
   below 16.67 ms is insufficient to prove this requirement.

These are completion targets; the implementation notes below describe partial
progress and outstanding system-specific work.

## Current implementation

Group/pose creation now uses a shared exported-artwork assignment method that
sets the PNG path, native canvas dimensions, scale 1, matching origin and gameplay
enabled state. Managed tests directly verify these defaults for a differently
sized pose, even when its prior artwork scale differs, alongside pose-family
alignment and persistence tests. Creation checks the current ROM before exporting
artwork rather than waiting for pack-save rejection. Managed tests and UI build
pass; live viewer creation/export/enable verification remains outstanding.

Click-to-set default/group and alternate origins now use view-model save methods
that restore the previous coordinates on validation/persistence failure. Clicks
use the same -4096..4096 range as numeric origin controls. The window no longer
resets group selection after origin clicks, preserving invalid condition text
for correction. Existing property notifications update the crosshair during
edits and rollback. UI build passes with zero warnings/errors; interactive
pointer mapping and failed-save origin verification remain outstanding.

Background scale/X/Y properties now notify changes, and Tilemap Viewer preview
placement information refreshes immediately without reloading its PNG. Selection
changes and disposal detach listeners. Managed tests verify notifications and
unchanged-value suppression, alongside existing persistence/packet tests. UI
build passes without warnings/errors. Interactive preview verification remains
outstanding; this change updates placement information, not a gameplay overlay.

Sprite Viewer default replacement PNG imports now retain the requested pack/group
across the file dialog, reject changed games/groups, and copy to unique files
without overwriting earlier artwork. Failed definition saves restore the previous
PNG path, scale, dimensions and origin instead of presenting unsaved artwork as
active. Sprite pack persistence checks the current ROM hash, and alternate PNG
imports also reject a changed ROM. UI build passes with zero warnings/errors.
Interactive failed-save and game-change verification remain pending; imported
files are retained when saving fails, consistent with alternate/background imports.

Tilemap background imports now snapshot the target pack before the file dialog
and reject the result if the game or loaded pack changed while choosing artwork.
Each imported PNG gets a unique filename and is copied without overwriting, so
retrying after a failed save cannot clobber retained artwork. The selected SNES
hardware tab already supplied its BG index; import guidance now accurately names
SNES/GBA layer activation and current rendering requirements instead of saying
integration is pending. UI build passes without warnings/errors. Interactive
dialog/game-change and failed-save verification remain outstanding.

SNES background pack activation is now enabled in the UI loader and native
combined/background-only APIs for layers 0..3 on SFC consoles. Native activation
requests PPU capture just as metasprite activation does; unsupported layers and
non-SFC layouts are rejected. Tilemap Viewer status states the current low-
resolution/default-filter/neutral-color requirements and visible-native-texel
limitation. Combined and standalone activation/clearing, retention after every
truncated background packet, unsupported layer and wrong ROM pass in the SNES
interop fixture. Expanded live SNES suite and UI/core builds pass (UI zero
warnings/errors). Interactive import/save/preview verification, fuller per-layer
capture, originally transparent BG texels and remaining display modes are still
unfinished. Earlier notes saying activation is disabled describe previous stages.

Live Mode 0 inter-layer fixtures now enable opaque BG3 beneath BG0, BG1 or BG2
on the main screen. Every visible captured background sample retains the higher
layer identity/coordinates. Activating a PNG for hidden BG3 leaves the sampled
native foreground/color-math result unchanged; switching to the higher layer
passes combined metasprite/background alpha, patterned placement and restoration
checks. Core build and expanded focused suite pass. Priority-bit reversals,
inter-layer holes, sub-screen layer overlap, other modes and pack activation
remain unfinished.

NativeCompositor.vcxproj now has a separate optimized Release configuration,
with assertions explicitly retained and a compile-time guard against NDEBUG.
Release artifacts use bin/native/Release and obj/native/Release, leaving the
Debug harness separate. Optimized native correctness tests pass. On the same
synthetic workload, SNES background+alpha measured 10.5249 ms/frame (checksum
1048574437228544) and sprite alpha/effects alone measured 5.17944 ms/frame
(checksum 1047099651530752). The combined checksum matches Debug. Capture,
filtering, presentation and live gameplay costs are excluded, so slowdown
resolution remains unproven; the extra background composition cost needs work.

PNG sampling now wraps signed coordinates with one remainder per axis and
avoids subpixel division when artwork/output scales match. The checked Sample
entry point remains available; SNES uses SampleValidated only after immutable
active layers pass IsValid once per frame. Native tests and the complete
all-system gameplay suite pass, including the expanded SNES Mode 0/1 fixtures.
The combined Debug benchmark measured 101.209 ms/frame versus the earlier
104.508 ms/frame baseline with identical checksum 1048574437228544. This is a
small synthetic timing difference, not proof of resolved gameplay slowdown.

Added a reproducible combined SNES background/alpha benchmark:
`NativeCompositor.exe --benchmark-snes-background` uses eight groups of sixteen
members, 256×239 input, scale 2, and a partial-alpha background. Debug baseline
measured 104.508 ms/frame with checksum 1048574437228544. A proposed background
color reuse change measured 102.493–107.213 ms/frame with the same checksum and
was removed because it did not demonstrate a reliable improvement. Native
correctness tests passed during this investigation. These synthetic Debug
timings are not release/gameplay measurements; performance work remains open.

Live Mode 1 fixtures now verify BG0/BG1 4-bit tiles and BG2 2-bit tiles on both
screens with wraparound scroll. The 4-bit assets set planes 0 and 2 (palette color
5), making incorrect 2-bit interpretation observable. Each main-screen layer
also exercises simultaneous metasprite/background PNG alpha, color math,
patterned placement and condition-driven restoration. The expanded focused suite
passes, including existing Mode 0, window and sprite-order regressions. Other
modes, inter-layer overlap and SNES background pack activation remain unfinished.

Live Mode 0 fixtures now cover BG1, BG2 and BG3 in addition to BG0. Each layer
uses its own map/character registers, palette range and screen/color-math enable
bit. Main/sub captures verify every visible layer identity and coordinate under
wraparound scroll. Main-screen layer PNGs also run combined metasprite alpha,
background alpha, patterned offset/scale, native sub-screen half-add math, and
condition-driven restoration checks. Core build and the expanded focused suite
pass. Inter-layer overlap, other display modes and pack activation remain pending;
these fixtures enable one background layer at a time.

Generated SNES BG0 window-mask fixtures now check every visible capture sample
on either the main or sub screen: the 40..95 stripe has no eligible background
sample, while unmasked samples retain their source coordinates. The main-screen
replacement test additionally verifies every scaled pixel in the masked stripe
remains native blue and an unmasked pixel displays replacement artwork with
color math. Combined metasprite alpha, scrolling, and restoration checks also
run for this fixture. Core build and focused live suite pass. Inverted/two-window
logic, color windows, other BG layers/modes and pack activation remain pending.

Live SNES BG0-main/native-OBJ-sub fixtures now activate metasprite and background
PNGs together. Every pixel of the 16×16 replacement canvas is checked for opaque,
transparent, and partial-alpha sprite artwork, including source-transparent OBJ
texels, against the replacement background with native half-add math. A partial-
alpha background is checked beneath opaque sprite artwork. Patterned background
artwork verifies scale 2, placement (3,-2), wrapping and source scrolling at a
known sampled pixel in both zero-scroll and wraparound-scroll ROMs. Unavailable
memory conditions restore native dimensions and colors after combined activation.
Core build and focused live SNES tests pass. Broader layers, windows, Mode 7,
mosaic, and SNES background pack activation remain outstanding.

Combined SNES metasprite/background tests exposed and fixed a source-transparent
OBJ fallback bug: inserted sprite coverage previously prevented a transparent PNG
pixel from revealing a replacement background. Fallback selection now recognizes
when the original main/sub screen contained a background beneath that coverage.
Native regressions verify both screens with transparent and partial-alpha sprite
PNGs, including sub-screen half-add math. Core build, native tests, and focused
live SNES regressions pass. Additional combinations and SNES background pack
activation remain outstanding.

Initial SNES background PNG composition is implemented in the shared compositor.
It samples the captured winning main/sub background independently, retains native
OBJ priority, blends PNG alpha with the native background before SNES color math,
and uses those backgrounds when transparent metasprite pixels reveal a fallback.
Background-only packs can select output scale, and conditions filter backgrounds
before drawing. Native tests cover opaque/partial alpha, foreground OBJ priority,
sub-screen half-add math and missing/matching conditions. Live BG0-main ROMs
verify PNG replacement combined with native sub-screen OBJ half-add math, including
scrolling, and restoration to native dimensions/color for an unavailable address.
Core build, native compositor tests and focused live SNES tests pass. Native/UI
pack activation still excludes SNES backgrounds pending broader layer, window,
mode, combined metasprite/background, and alpha/color-math verification. Capture
currently identifies winning background texels; replacing originally transparent
or hidden background texels will need fuller per-layer capture.

SNES winning main/sub background samples now retain source map coordinates.
Regular tiles capture the fetched effective horizontal/vertical scroll and map
dimensions, including offset-per-tile scroll; Mode 7 records transformed source
coordinates. Mosaic retains the sample selected at its block origin. Generated
BG0 main/sub ROMs verify every visible coordinate across the 256×224 area with
zero scroll and wraparound scroll (253,250), alongside sprite ordering and alpha
regressions. The core build and focused SNES suite pass. Offset-per-tile, large
maps/tiles, Mode 7, and mosaic still need dedicated fixtures. PNG background
composition and UI activation on SNES remain unfinished.

SNES background fallback capture now retains the winning BG0..BG3 identity
separately for main and sub screens, before OBJ composition. Backdrop is marked
0xFF; identities change only when the existing background priority comparison
accepts a new winner and reset for each rendered span. Tilemap and Mode 7 paths
provide the identity. Generated low-resolution ROM tests verify BG0 on the main
screen, BG0 on the sub-screen, and backdrop identities; overlapping/rotated OBJ
and transparent artwork regressions also pass. SNES background PNG composition,
other layers/modes, and authoring activation remain
unfinished. This capture change alone does not enable SNES background packs.

Alternate PNG add/remove/reorder operations now restore their previous list
and selection when validation or pack persistence fails, preserving the reported
error. A successful pack save remains authoritative even if later activation
reports a runtime error. The UI compiles without warnings; interactive failed-save
verification remains outstanding. An imported PNG is retained if its definition
cannot be saved, so a failed operation never deletes artwork.

Live background condition tests now pass in generated-ROM fixtures for Game
Boy/GBC, NES, SMS, PCE, WonderSwan, and GBA fixtures with captured background
layers. Matching conditions activate distinct replacement artwork; failing one
of two conditions on the same address restores the native background while
retaining metasprite artwork. An unavailable address also suppresses the
background, and restoring a valid match restores replacement artwork. Snapshots
still contain only the configured valid address. SNES backgrounds are explicitly
excluded pending their capture/composition implementation. The core build and
complete gameplay regression suite pass.

Live generated-ROM tests now exercise alternate artwork transitions on NES,
Game Boy/GBC, GBA, PCE, SMS, SNES, and WonderSwan. Two alternates share a matching
condition; changing their order changes the rendered artwork, and making both
conditions mismatch restores the default artwork. Each frame snapshot contains
only the single configured address, including when that address is shared by
the group and both alternates. The fully occluded WonderSwan fixture instead
checks that switching artwork preserves occlusion. The core build and complete
gameplay suite pass. These tests change replacement definitions against a stable
ROM byte; ROM-driven changes to memory remain to be verified.

Alternate PNG previews now display an origin crosshair in native coordinates.
The click-to-set option saves the clicked origin; numeric origin and scale edits
update the crosshair and preview placement immediately through property-change
notifications. Selection changes and window disposal detach handlers. Managed
tests check notifications and unchanged-value suppression; the UI build passes
without warnings. Pointer-coordinate and interactive preview verification remain
outstanding, along with SNES backgrounds.

The Sprite Viewer now exposes an alternate-PNG authoring panel: enter nonempty
memory conditions, import a PNG into the per-game folder, name it, edit its
native origin/scale, preview it, change matching priority, or remove it. Save and
Apply validate condition text and the selected image's canvas/scale. Selection
survives the window's binding refresh, imported files use unique names, and
preview bitmaps are disposed on selection/window teardown. Numeric alternate
origins are available; alternate crosshair editing and interactive authoring
verification remain outstanding, as do dedicated live alternate transitions.
Managed persistence/serialization tests and the UI build pass without warnings.

The Tilemap Viewer background editor now exposes selected-layer CPU conditions
using the same hexadecimal text parser as metasprites. Valid edits remain with
the selected layer; Save and Apply revalidate text, persist conditions, and
activate MBG2 background gating. Blank text removes conditions. The shared
parser tests cover input/round-trip validation; the UI build passes without
warnings. Interactive background editing
and live background-condition transitions still require verification.

The Sprite Viewer now exposes selected-group CPU conditions as hexadecimal
`ADDRESS = VALUE / MASK` lines. The mask defaults to FF; blank input clears
conditions. All lines must match. Valid edits follow the selected group, and
Save/Apply revalidate the current text before persisting or activating it.
The parser accepts `$`/`0x` prefixes, 32-bit addresses, and byte values/masks;
malformed input and more than 64 conditions are rejected. Managed tests cover
parsing, formatting round trips, clearing, and invalid/oversized inputs; the UI
build passes with no warnings. Interactive
UI verification, background/alternate condition editors, and alternate PNG
authoring controls remain unfinished.

Saved alternate PNGs now load through the runtime loader and MSP3 packets. Each
image transfers its native origin, scale, canvas/pixels, and nonempty byte/mask
conditions in authored priority order. MSP1/MSP2 remain supported and packs emit
the oldest format needed. Alternates are limited to 64 per group; native parsing
validates origins, dimensions, scale, pixels, and condition counts/bytes.
Managed-to-native tests cover selected alternate rendering, default fallback,
origin/scale transfer, every truncated MSP3 packet, and trailing data rejection.
Managed/native tests and core/UI builds pass. Condition/alternate authoring UI,
dedicated live alternate transitions, and SNES background composition remain
unfinished.

Native metasprite definitions now select alternate artwork using sparse captured
memory: the first matching alternate wins, otherwise the default PNG is used.
Selected artwork controls its own origin, scale, canvas, and alpha without
copying the pixel buffer or changing group recognition. Alternate condition
addresses join capture deduplication. Native tests cover selection/fallback,
simultaneous matches, different origins/scales, transparent/partial-alpha pixels,
and duplicate addresses. Alternate packet transfer, PNG loading, authoring, and
dedicated live alternate transitions remain unfinished.
Native tests, the core build, and the full existing gameplay regression suite pass.

Saved background byte/mask conditions now pass through MBG2 packets and the UI
loader on supported background systems. Unconditioned packs retain MBG1; the
native parser accepts both formats and rejects truncated or non-byte conditions.
Managed/native transfer tests include a conditioned PNG rendered from the
transferred definition, sparse snapshot capture, mismatching data, alpha, and
native fallback. Managed/native tests and core/UI builds pass. Dedicated live
background condition-transition fixtures and authoring controls remain necessary;
SNES background composition and alternate metasprite PNGs remain unfinished.

Native background definitions now support byte/mask conditions. Composition
selects active layer references once per frame, preserving authored order and
avoiding PNG copies and condition checks inside HD pixel loops. Only matching
layers contribute output scale. Background condition addresses join the unique
capture list, including background-only activation. Native tests cover missing
bytes, matches/mismatches, direct-color and GBA composition, alpha, and scale.
The core build, native tests, and full existing gameplay regression suite pass.
Background condition packet transfer and UI loader activation remain unfinished.

Saved metasprite byte/mask conditions now pass through the UI loader and operate
against sparse CPU snapshots during gameplay. Live fixtures switch definitions
between an exact captured-byte match, a mismatch, an out-of-range address, and a
restored match; native output scale follows the condition state. Snapshot checks
require exactly the single requested address, and invalid-address snapshots must
be empty. These tests change the expected condition, not emulated RAM. Dedicated
RAM-changing ROMs, condition authoring controls, rewind persistence, alternate
PNG selection, and background gating remain unfinished. The loader continues
to reject alternate artwork and conditioned backgrounds explicitly.
Core/UI builds and the full gameplay suite, including these live condition
checks across all seven systems, pass.

Frame submission now captures configured CPU addresses through the side-effect-
free Game Boy, NES, SMS, PCE, GBA, WonderSwan, and SNES debug memory readers, before
asynchronous decoding. Out-of-range addresses remain absent rather than wrapping.
Rewind submission skips live memory reads; rewind snapshot persistence still
needs validation.
Only the compiled unique condition-address list is read; no full memory dumps
are taken. The core build passes. Gameplay fixtures now require a captured byte
through a zero-mask condition, exercising missing-snapshot suppression across
the console adapters; the full gameplay suite passes. Byte-value transitions
still need dedicated live fixtures.
The UI runtime loader still rejects conditioned packs pending broader validation;
alternate artwork and background gating remain unfinished.

Metasprite activation now compiles a sorted, unique list of memory-condition
addresses alongside the immutable group definitions. A capture helper reads each
address once into a frame snapshot, omits failed reads, and clears stale bytes
before capture. No reads occur for an unconditioned/cleared pack. Native tests
verify duplicate-address elimination, matching after capture, failed reads, and
clearing definitions. Console readers and frame-submission invocation remain
to be connected; this helper does not yet enable conditioned gameplay.

MSP2 packets now transfer each enabled metasprite's CPU address, expected byte,
and mask to native composition. Packs without conditions still emit MSP1, and the
native parser accepts both versions. Conditions are limited to 64 per group;
non-byte values/masks, truncated payloads, and trailing data are rejected.
Managed and native comparisons both mask actual and expected values. Transfer
tests cover masked values, legacy compatibility, every truncated MSP2 packet,
an overflowing mask, and native composition gated by transferred conditions.
Managed/native tests and core/UI builds pass.
Live CPU snapshot capture, alternate artwork, background conditions, and UI
authoring remain unfinished; enabled conditioned packs are still rejected by
the runtime loader until capture is connected.

Native metasprite definitions now accept byte/mask conditions evaluated against
a sparse CPU-memory snapshot attached to their captured video frame. All checks
must match; missing bytes suppress the group, including its HD scale contribution.
Native tests exercise masked matches, multiple-condition failure, missing data,
and changing snapshots across repeated composition. Empty condition lists preserve
existing behavior. This is runtime groundwork: live CPU capture, condition packet
transfer, alternate artwork, background gating, and authoring UI are not yet
connected. The UI loader still explicitly rejects enabled conditioned packs.
Native condition/scale tests, the core build, and full unconditioned gameplay
regressions pass; these do not establish live memory-conditioned gameplay.

Shared direct-color background composition now uses the original capture frame
when output has no crop. Previously every background replacement frame allocated
and copied a full capture-pixel array even at identical dimensions. Cropped
outputs still construct a cropped capture to retain their existing coordinate
and fallback semantics. Native compositor tests, the core build, and the full
generated-ROM gameplay regression suite pass;
throughput measurement and removal of cropped-frame copies remain outstanding.

Game Boy/GBC fetched sprite rows now retain their evaluated scanline slot. The
source-hole eligibility loop uses that slot directly instead of searching the
sprite list for each covered pixel (normally ten slots, up to forty with the
sprite-limit option disabled). Sprite evaluation fills the
list before pixel fetching, so DMG X/OAM ordering and CGB slot ordering continue
to use the same identities. This removes a nested search; throughput and dense
sprite workload profiling remain necessary. The core build and full generated-ROM
gameplay regression suite pass.

WonderSwan's two double-buffered scanline coverage arrays now store only their
low/high 64-bit sprite masks. They previously held full capture pixels, including
unused color/layer metadata and coverage vectors, which were reset every
scanline and copied when BG1 priority filtered sprites. The compact storage
preserves IDs 0..127 and the existing high-priority coverage selection. The core
build and full generated-ROM gameplay regression suite pass; no
emulator-throughput improvement is claimed without profiling.

GBA color effects also use compact scalar state, including inserted-object
coverage, secondary-layer removal, and background layer-pair composition. This
removes full capture-pixel copies and their coverage-vector allocations from
these effects paths. The GBA partial-alpha debug workload improved from 37.2224
to 30.84 ms/frame with unchanged checksum 1047029328791040. Native compositor
regressions, the core build, and the full generated-ROM gameplay suite pass;
release/runtime performance validation
remains outstanding.

SNES color effects now use a compact scalar state instead of constructing full
capture pixels for every PNG-alpha outcome. The public capture-pixel entry point
delegates to the same color-math implementation. The SNES partial-alpha debug
workload fell from 148.794 to 43.633 ms/frame, retaining checksum
1047099651530752; native compositor tests, the core build, and the full
generated-ROM gameplay regression suite pass. This is composition-only
evidence, not proof of native-speed emulation across systems.

Repeatable `--benchmark-gba-alpha` and `--benchmark-snes-alpha` compositor
workloads now cover eight 16-member groups, source-transparent columns, partial
PNG alpha, and captured color effects at 2x resolution. Initial debug timings
were 37.2224 ms/frame for GBA and 167.394 for SNES. SNES alpha-outcome composition
was copying the pixel's coverage vectors repeatedly; copying only the scalar
color-math inputs reduced that workload to 148.794 ms/frame with unchanged
checksum 1047099651530752. Native regressions, the core build, and focused live
SNES overlap/rotation/fully-transparent gameplay tests pass. These synthetic debug timings
are not emulator throughput; further scalar-state and effect-loop optimization,
release profiling, and native-speed validation remain necessary.

GBA/SNES composition staging buffers now retain capacity between frames instead
of allocating and freeing full HD-sized arrays on every frame. Changed flags,
object override state, and coverage pointers are explicitly reset; main/secondary
colors are overwritten from the current capture. Native regression tests cover
replacement followed by a same-sized nonmatching frame with coverage removed,
including a GBA-to-SNES transition. Native compositor tests, the core build, and
the full generated-ROM gameplay regression suite pass. This removes allocation
churn but retains peak staging capacity and does
not yet establish native-speed emulation.

GBA background PNG activation now accepts hardware layers BG0 through BG3 and
uses the shared layer compositor during gameplay. Background and metasprite PNGs
are composed together using captured priority, window eligibility, and color
effects. An inserted replacement's transparent pixels retain an unrelated native
object underneath. Scrolling 256- and 512-pixel text-map gameplay fixtures verify
the combined output, partial PNG alpha, background blending, and disable
restoration. The UI build, native compositor tests, and full generated-ROM
gameplay regression suite pass. Affine,
bitmap, and mosaic background capture still need dedicated gameplay validation;
SNES background replacement, runtime memory conditions, and native-speed
verification remain unfinished.

Matched metasprites now precompute low/high hardware-ID membership masks once per
instance. Visible/secondary identity and source-hole coverage use constant-time
tests instead of scanning group members at every pixel; GBA/SNES inserted coverage
uses the same lookup. A fallback preserves synthetic IDs outside current hardware
ranges. A debug benchmark of eight 16-member metasprites (128 source sprites,
256x239, 2x artwork) improved from 28.0556 to 15.765 ms/frame with unchanged output
checksum 1047035712263160. Native tests include a group spanning IDs 63/64 with
an unrelated eligible ID excluded. Native build, compositor tests and full gameplay
suite pass. These
timings cover composition only; release/runtime native-speed verification remains
unfinished along with capture/recognition allocation profiling.

Composition now visits the union of each matched group's fetched member bounds and
artwork bounds instead of scanning the entire frame for every match. Member bounds
remain included so a smaller or shifted PNG still removes original pixels outside
its canvas; native tests check this with cropping. A repeatable debug benchmark of
32 matched 16x16 sprites on a 256x239 frame with 2x artwork improved from 541.123 to
6.52919 ms/frame (about 83x), with identical whole-output checksum
1047029318484960. Native build, compositor tests (including smaller/shifted PNGs
and cropping) and the full gameplay suite pass. This measures compositor time in this debug workload, not emulator
throughput or a guarantee of native speed. Release/runtime profiling, allocation
and recognition costs, staged-effects overhead and broader speed validation remain
unfinished.

The shared compositor now has a GBA layer-reselection routine using captured
BG0..BG3, native OBJ and backdrop state. It samples scrolling/placed background
PNGs, preserves native background pixels under transparent background artwork,
supports an independent-alpha object override, retains OBJ/BG tie order, and
reselects the top two layers for alpha outcomes before applying captured effect
targets, brightness and object semitransparency. Native tests cover higher-priority
backgrounds, transparent/partial artwork over a transparent source layer, object
removal revealing a replacement BG, OBJ priority ties, window-disabled effects and
semitransparent OBJ precedence, and masked samples. Native build and compositor tests pass.
The routine is not connected to gameplay yet; GBA background activation and joint
sprite/background rendering through it remain unfinished.

GBA background capture now also records each layer's native color, opacity and
priority, the native object selected before background mixing (including its
priority and semitransparency), the backdrop color, all main/secondary blend-target
masks and window effect enablement. Scrolled 256/512 map fixtures verify these
fields at every native pixel, including object identity/priority under the sprite,
with BG0 and backdrop selected as distinct alpha targets. Native build, compositor
tests and full gameplay suite pass. This supplies layer reselection state for the
next composition step; GBA background PNG activation is still unfinished.

GBA now captures per-layer fetched background coordinates rather than deriving
scrolling from completed-frame register values. Text fetches retain logical map
coordinates for 256/512 dimensions, while affine/bitmap fetches retain transformed
source coordinates. Samples include transparent source texels and are invalidated
for disabled/hidden layers or object-window layer masks; buffers clear each scanline.
Live text fixtures verify every output sample under fine scrolling and wraparound
on 256x256 and 512x512 maps, including beneath an opaque sprite, and confirm other
layers stay invalid. Native build, compositor tests and full gameplay suite pass.
This is capture groundwork: GBA background PNG activation/composition remains
unfinished, and affine, bitmap, mosaic and window coordinates need live validation.

A fully transparent SNES fetched 8x8 object now has a live regression fixture.
Every source texel is zero, every captured main-screen sprite identity is absent,
and every native sample still records eligible fetched-object coverage. The test
matches that transparent graphic by content, checks complete HD replacement output
including PNG-transparent and partial-alpha samples, and restores the native blue
backdrop when disabled. Native build and the focused SNES suite (also covering
normal/rotated overlap) pass. This verifies a single transparent object; multiple
replacement groups and artwork beyond fetched member bounds remain unfinished.

SNES overlap tests exposed a capture identity bug in the native two-cycle fetch
pipeline: the next object's position replaces `_currentSprite` identity between
the previous object's plane fetches. Capture now retains the object identity and
dimensions at attribute fetch, so decoded slices and per-pixel coverage refer to
the object whose pixels are actually being drawn. Live normal-order and rotated
OAM-order fixtures pass: replacement fills a source hole over a later opaque
object, while rotating that object first excludes replacement coverage. PNG alpha
and native restoration are checked. Native build, focused ordering tests and
full gameplay suite pass. Multiple replacement-group ordering and
background artwork integration remain unfinished.

SNES live source-hole fixtures now check object half-add math with a blue fixed
color, the palette-0 exemption from object math, and a replacement drawn only on
the subscreen beneath a red main-screen BG0. The subscreen case checks switching
to fixed-color fallback without halving when PNG alpha removes the object, both
over native opaque pixels and original source holes. Complete HD output, captured
subscreen eligibility and disable restoration pass. Native build and full gameplay
suite pass. Priority rotation, multiple replacement groups, additional color-window
modes and background artwork integration remain unfinished.

SNES live source-hole fixtures now cover object-window masking of the transparent
rightmost column and masking of middle columns while that source hole remains
eligible outside the window. Captured eligibility, complete HD output, PNG alpha
and disable restoration pass, as does the full gameplay suite and native build.
Priority rotation, overlapping groups and live color-math/subscreen cases remain
pending. Sprite Viewer rectangle selection now cancels on pointer capture loss,
restores the previous rectangle, and only commits on the initiating pointer's left
release. This prevents interrupted drags from leaving selection updates suppressed;
interactive UI verification remains pending.

SNES now buffers fetched sprite coverage independently of decoded source alpha.
Fetch order follows the native evaluation/fetch order, including priority rotation;
coverage behind the selected opaque object is excluded before background priority
is checked. Main/subscreen layer enables and object windows are recorded separately,
then background priority is checked after backgrounds render. Inserted replacement
pixels use their object's palette-dependent color-math permission, subscreen
selection and native main/subscreen fallback when PNG alpha removes them. Native
tests exercise insertion on both screens, fixed-color half math, transparent PNGs
and partial alpha. A live transparent-source-column fixture checks complete HD
artwork and restoration, including PNG-transparent and partial-alpha samples over
the source hole. Native build, compositor tests and full gameplay suite pass.
Rotated-order overlap, windows, palette math and subscreen
cases still need broader live tests; high-resolution/interlaced capture, background
replacement and interactions between replacement groups remain unfinished.

GBA live coverage now includes WIN0 masks over the source-transparent column and
over the middle of a sprite, an earlier source-transparent object overlapping a
later opaque native object, and a fully transparent fetched 8x8 object. Each
fixture checks captured source content, eligibility at transparent pixels,
complete HD artwork output with PNG alpha, and restoration after disabling.
These cases pass; multiple replacement groups, background priority, mosaic and
affine cases still need broader coverage. The NES generated-ROM harness now sets
zero-filled RAM explicitly because its empty nametable assumptions conflicted
with the native configuration's random RAM default.
Native build and the full gameplay suite pass with this deterministic setup.

GBA now buffers alpha-independent fetched-object coverage alongside native OAM
scanline output. Composition checks object/background priority and object windows,
and records the inserted object's blend targets and semitransparency independently
of the original main layer. The shared compositor applies those effects against
the native main layer and uses native output as PNG-alpha fallback. Generated live
fixtures exercise a transparent source column with ordinary and darkened output;
native compositor tests cover alpha blend, semitransparency, transparent artwork,
and partial alpha. Both live source-hole fixtures check PNG-transparent and
partial-alpha samples over the transparent source column, complete HD output, and
native restoration. Native build, compositor tests and full gameplay suite pass.
Mosaic/affine coverage, windows and overlapping replacements
still require broader live validation; SNES source-hole integration and GBA/SNES
background replacement remain unfinished.

PCE source-hole coverage now includes 32x16 sprites (two fetched slices) and
16x16 sprites at clock dividers three and two. Each transparent source column
is checked for replacement eligibility and absent original sprite identity;
complete HD artwork output and native restoration pass in all variants.
Native build/full suite pass. Mirrored holes, collision/background-priority cases,
overlapping replacement groups and enlarged artwork remain unfinished.

PCE now records alpha-independent replacement coverage for fetched sprite slices
that pass background priority and preceding opaque-sprite checks. Coverage follows
the same VDC row that feeds cropped output. A live original-source-hole fixture
fills the transparent rightmost column of a 16x16 sprite with HD replacement
artwork and restores native output when disabled. Native build/full suite pass.
Sprite-zero collision interactions, wide/mirrored holes, background-priority holes,
overlapping replacement groups and artwork outside member bounds remain pending.

WonderSwan now buffers alpha-independent sprite coverage with the same scanline
as native output. Sprite windows and earlier opaque sprites constrain eligibility;
opaque BG1 removes low-priority coverage while retaining high-priority coverage.
A color planar-4bpp live fixture fills an original transparent sprite column,
and existing inside/outside BG1 windows and occlusion fixtures pass. Native build
and full suite pass. High-ID live fixtures, sprite-window source holes, monochrome
behavior, overlapping replacement groups and larger artwork remain pending.

Shared replacement coverage now supports transient sprite IDs 0..127 through
bounded low/high bitsets. Native tests verify sprite 96 fills an original alpha
hole, ID 127 is retained, ID 32 does not alias ID 96, and invalid IDs fail closed.
The native build, compositor tests and full gameplay suite pass. This prepares
GBA/SNES/WonderSwan capture adapters; it does not populate their coverage yet.

SMS source-hole tests now include SMS2 zoomed sprites and SMS1's different zoom
behavior. Every transparent source sample is checked for absent native sprite
identity and eligible replacement coverage, including horizontally doubled holes.
The tests verify every HD artwork pixel, background priority and native restoration.
Native build/full suite pass. Multi-group occlusion, fully transparent sprites and
artwork extending past hardware sprite bounds remain unfinished.

SMS Mode 4 now captures alpha-independent replacement coverage alongside sprite
shifter output, excludes candidates behind an already selected opaque sprite,
and suppresses coverage for high-priority opaque background pixels or disabled
sprites. CRAM-dot artifacts clear coverage as well as original sprite identity.
A live source-hole fixture fills the original transparent rightmost sprite column;
existing background-priority and masking checks also pass. Native build/full suite
pass. Fully transparent sources, zoomed holes, and interacting replacement groups
still need coverage.

A fully transparent NES sprite now has live coverage: its fetched graphic contains
only transparent pixels and no original sprite winner, yet opaque replacement
artwork renders its complete HD footprint over the scrolling replacement background.
PNG-transparent and half-alpha samples are checked independently, and clearing
definitions restores the native backdrop. Native build/full suite pass. This
verifies one fetched sprite, not simultaneous replacement-group ordering or artwork
outside the fetched member bounds.

NES now supplies alpha-independent replacement coverage from fetched sprite slots,
checking native sprite enablement, left masking, background priority and competing
slot order. The live scrolled-background source-hole fixture uses a transparent
rightmost sprite column and verifies opaque replacement fills it, along with
existing PNG alpha samples and native restoration. Native build/full suite pass.
Overlapping replacement groups, fully transparent sprites, sprite-limit cases,
background-priority holes and larger artwork still need validation or integration.

Separate DMG and GBC live source-hole fixtures now verify a fully transparent
rightmost native sprite column, absence of an original visible-sprite winner,
and replacement eligibility for every pixel in that column. Both verify opaque
replacement output across the complete HD sprite footprint and native restoration.
The native build and full suite pass. This establishes PNG-controlled filling of
source holes for a single Game Boy sprite; overlapping sprites, behind-background
priority, clipping, and artwork extending beyond members still need coverage.

GB/GBC now supplies alpha-independent coverage from fetched sprite rows at pixel
composition time. It checks sprite enablement, background priority, and competing
sprite ordering before admitting replacement pixels. A live GBC fixture has a
transparent rightmost source column: opaque green replacement artwork fills that
entire column while existing transparent/half-alpha artwork samples still pass.
The native build and complete suite pass. DMG hole/overlap tests, background-priority
hole tests, larger artwork, and the other systems' coverage adapters remain pending.

The shared pixel/compositor path now accepts adapter-approved replacement
coverage independent of source texel alpha (transient hardware IDs 0..63).
A native test uses an originally transparent sprite texel, fills it with opaque
replacement artwork when coverage allows it, and preserves the existing output
when the replacement is transparent. Unapproved competing-sprite pixels remain
unchanged. Adapters do not populate this coverage yet, so PNG-only transparency
is not established in gameplay. Higher hardware IDs, artwork outside original
bounds, staged color effects, and per-system eligibility still require integration.

Rectangle selection in the live Sprite Viewer is now always available on a plain
left-button drag. The mode checkbox and disabled default have been removed.
Ctrl-click retains member toggling; pointer capture keeps an active rectangle
drag consistent if modifiers change mid-drag. Plain presses still update the
ordinary sprite inspector, and the separate artwork-origin preview is unchanged.
Interactive pointer verification remains pending.

Live group recognition is now always enabled in Sprite Viewer; its optional
checkbox and disabled-by-default state have been removed. Replacement PNG alpha
must also be independent of the original sprite's alpha. The current compositor
still restricts writes to pixels won by original members, so it does not yet meet
that requirement. Correct integration requires per-system eligibility/occlusion
metadata for originally transparent pixels and artwork extending beyond member
bounds, preserving background priority, competing sprites, hardware clipping,
windows and color effects. Removing the winner check alone would lose these rules.

Creating a meta group or animation pose now exports its transparent native-size
PNG into the default per-game pack folder, assigns it as replacement artwork with
the captured origin, saves the group enabled, and immediately applies the pack.
Clicking Create authorizes this export without a file dialog. Existing PNG files
are preserved by choosing a unique filename on collision. Definition-save failure
rolls back group insertion, numbering and pose linkage while retaining the PNG.
The Create button explains the behavior in its tooltip. Interactive verification
of creation/export/activation remains needed.

The pack schema now persists masked CPU-byte equality conditions and alternate
metasprite artwork paths with independent scale/origins. Multiple conditions use
AND semantics, matching the requested specific-value checks; absent memory samples
fail closed. Existing packs default to unconditional artwork. Managed checks cover
masked matches, a single mismatch, missing snapshots, and empty conditions.
Runtime capture/packet transfer and authoring controls are unfinished: the loader
explicitly rejects enabled conditioned definitions instead of displaying them
unconditionally. Runtime conditions must eventually use frame-associated CPU
snapshots without triggering memory-mapped I/O reads on the renderer thread.

PCE layer-zero background packs now activate through the native APIs and shared
PNG loader, with Tilemap Viewer authoring and SuperGrafx explicitly excluded.
The live background fixture renders 2x white artwork behind green sprite artwork,
checks transparent/half-alpha sprite pixels and an exposed background probe,
preserves the native outer border, and restores native output when definitions
clear. Native/UI builds and the full suite pass. The fixture supplies decoded
artwork directly; full PNG-loading verification, patterned scrolling, and complete
replacement-output coverage remain pending.

The PCE background-enabled fixture now verifies all valid captured background
coordinates across at least 256x224 pixels, including the sprite-covered region.
Its fixed sprite position anchors expected map coordinates at (8,8), and the
whole-frame check rejects incorrect layer identity or horizontal/vertical mapping.
The native build and full live regression suite pass. This covers the fixture's
unscrolled 32x32 map; scrolling, alternate map dimensions and access modes, and
PNG background activation remain pending.

PCE now captures background map coordinates with each BAT entry fetch and selects
the corresponding sample when drawing pixels, including fine horizontal scroll.
Coordinates follow fetched tile columns and latched map rows, and remain available
under winning sprites. An original background-enabled ROM confirms valid layer-zero
metadata beneath its sprite while existing replacement/restoration checks pass.
The native build and full suite pass. Exact whole-frame coordinate verification,
scrolling, PNG activation, and other VRAM access modes remain pending. SuperGrafx
composition continues to be unsupported by the replacement API.

The NES live cropped-scroll fixture now uses overscan (left 2, right 4, top 3,
bottom 5). It verifies every exposed pixel in the 500x464 HD output against the
wrapped pattern formula, rebases the sprite footprint correctly, checks its
transparent/half-alpha samples, and restores the cropped 250x232 native output.
This establishes that the NES default-filter decoder passes the crop offsets
correctly. The native build and full live suite pass. Other console crop paths
and runtime overscan changes still need coverage.

Shared background composition now accepts cropped output. It selects metadata
from the original frame's cropped region while retaining native screen coordinates
for screen-locked artwork, and builds the sprite alpha fallback at the same
cropped HD dimensions. Native tests verify distinct subpixels for both screen-locked
and scroll-following wrapped artwork with nonzero horizontal/vertical crops.
The native build and full live regression suite pass. Console-specific cropped
background gameplay fixtures remain needed; effect-frame background composition
is still excluded.

The NES masked-scroll fixture disables background drawing in the left eight
native columns while retaining sprites and wrapped scrolling. It checks invalid
background metadata in those columns and verifies every exposed HD pixel retains
the native backdrop there, with patterned replacement artwork starting exactly
at column eight. Transparent/half-alpha sprite samples and native restoration
also pass. The native build and full regression suite pass. Dynamic mask changes
and emulator layer toggles still need coverage.

NES scrolling coverage now starts in logical nametable three with scroll (253,237),
giving fetched origin (509,477). Every native background coordinate is checked
across the 512x480 logical layout wrap. Patterned 2x artwork independently verifies
every exposed output pixel and transparent/half-alpha sprite samples, followed
by native restoration. The native build and complete regression suite pass.
This covers static scrolling with the fixture's mirroring; dynamic register writes,
other mapper layouts, clipping, and masks remain pending.

NES background layer zero now activates through both native replacement APIs and
the shared PNG runtime loader. Tilemap Viewer maps every NES nametable tab to
this single logical background layer; artwork placement uses fetched nametable
coordinates. The default PPU restriction excludes existing HD-pack and VS paths.
The background-enabled live ROM composes white 2x background artwork with green
sprite artwork, verifies every exposed output pixel and the transparent/half-alpha
sprite samples, then restores native rendering when both sets clear. Native/UI
builds and the complete regression suite pass. The live fixture supplies decoded
artwork directly: PNG loader/API end-to-end verification, scrolling, masks,
cropping, and register changes remain pending.

NES background capture now tracks logical nametable coordinates at tile fetch,
loads eight samples alongside the pattern shifters, and shifts them with the
native pixel pipeline. Fine X selects the corresponding captured sample; hidden
or disabled background pixels remain invalid. A background-enabled original NES
ROM verifies every coordinate of its unscrolled 256x240 frame, including samples
under sprites. The native build and complete gameplay suite pass. NES background
PNG activation, scrolling/wrap, register changes, masks, and state restoration
still need additional coverage. Background activation is now implemented as above.

Pack activation now submits sprite and background packets through one native API.
Both packets and console/layer restrictions are checked before mutation; the
compositor publishes both immutable snapshots under one lock. A rejected
background packet can no longer leave newly applied sprite definitions behind.
The UI and native DLL build, and a native composition test verifies simultaneous
sprite/background alpha composition and clearing both sets. A direct interop
fixture now activates a sprite pack, rejects every truncated background packet,
trailing data, a valid but unsupported background layer, and a wrong ROM hash,
checking the previous sprite definitions remain active after each rejection.
It also accepts the complete combined packet and clears both sets. The complete
generated-ROM suite passes with these checks. Concurrent ROM changes and rendered
output retention during failed updates remain outside this fixture's coverage.

WonderSwan BG1 window coverage now enables both backgrounds, clips BG1 to the
inclusive rectangle (40,24)-(95,63), and gives BG1 independent scroll offsets.
The live fixture checks every winning layer/coordinate, renders distinct BG0/BG1
artwork, and verifies every exposed HD pixel at the window edges and outside it.
An additional outside-window fixture inverts this rectangle and places a
high-priority sprite over BG1. Every exposed HD pixel and captured coordinate
passes for both masks; transparent and half-alpha sprite artwork reveal the
correct BG1 replacement beneath that sprite. Native restoration also passes.
A separate outside-window fixture lowers that sprite's priority: its full graphic
is still captured for matching, but BG1 wins every overlapping output pixel.
The replacement stays hidden, BG1 artwork renders across its entire footprint,
and clearing definitions restores the native background. The rebuilt native DLL
and complete generated-ROM regression suite pass. Transparent BG1 tiles still
need live coverage.

The WonderSwan scrolling fixture now uses patterned 2x background artwork rather
than a uniform fill. An independent coordinate formula verifies every exposed HD
output pixel and the transparent/half-alpha sprite samples over that pattern,
covering horizontal/vertical map wrap and artwork repetition. Native restoration
also passes. This verifies visible texture placement for static nonzero scroll;
animated scroll-register changes remain pending.

The WonderSwan BG0 scrolling fixture sets X scroll to 253 and Y scroll to 250,
verifying every captured map coordinate across horizontal and vertical 256-pixel
wrap boundaries. Background artwork remains enabled beneath opaque, transparent,
and partial-alpha sprite samples; native output restores after definitions clear.
The replacement artwork is uniform in this fixture, so coordinate checks prove
mapping while a patterned-artwork scrolling test remains needed to verify visible
texture motion end to end.

WonderSwan background definitions now activate through the native API and PNG
loader for layers zero and one. The live BG0 color-2bpp fixture renders HD white
background artwork and verifies every opaque/transparent/half-alpha sprite
replacement pixel over it, then restores native red OBJ and blue background when
definitions clear. The initial default-filter path excludes frame blending and
LCD icons. BG1/window, scrolling, other video modes, and direct PNG-loader/API
verification remain unfinished.

WonderSwan now captures the top opaque background layer's fetched tilemap
coordinates alongside independent background fallback pixels, including samples
hidden by OBJ. Layer-one window masking and transparent tiles retain the lower
winner; output-time metadata follows the same buffered scanline as native color.
An original color 2bpp fixture enables BG0 and verifies every coordinate while
existing sprite replacement and transparent fallback tests pass. BG1/window,
scrolling, monochrome, and gameplay background activation still need coverage
or integration.

The SMS scroll-lock fixture enables horizontal lock for the top sixteen rows and
vertical lock for the right-side fetched tiles while using five-pixel horizontal
scroll and vertical wrap. It checks every valid captured coordinate across both
lock boundaries, reflecting the current VDP fetch/shifter timing, and verifies
background priority and transparent sprite composition. The full live suite
passes. Dynamic register writes and independent hardware timing comparisons
remain outside this fixture's coverage.

The live SMS scroll fixture now sets horizontal scroll to five pixels and vertical
scroll to 218. It verifies every active background coordinate after the initial
fine-scroll backdrop, including the 224-line tilemap wrap, with background and
sprite replacements enabled. A shifted priority probe verifies the scrolled
background still occludes the second sprite. Transparent/partial-alpha artwork,
native border preservation, and clearing replacements also pass. Horizontal and
vertical scroll-lock live cases remain pending.

SMS Mode 4 background activation now accepts layer-zero PNG definitions through
the pack loader/native API. The basic live SMS fixture replaces exposed background
with HD artwork, verifies every sprite replacement pixel including transparent and
half-alpha fallback, and checks a high-priority background pixel still wins while
the border remains native. Clearing both definitions restores native output.
Default-filter, uncropped output is the initial path. Scrolling/lock cases and
direct PNG-loader/API end-to-end verification remain pending.

SMS Mode 4 now captures fetched background tilemap coordinates alongside the
pixel shifters, preserving fetch-time scroll/lock decisions and masking backdrop
or CRAM artifact samples. The coordinate shifter runs only during capture. Live
SMS/SMS1 fixtures verify all active unmasked pixel coordinates, including samples
behind sprites, while existing sprite zoom/priority/mask checks pass. Background
activation, scrolling/lock live fixtures, older VDP modes, and border handling
still need further integration or coverage.

Tilemap PNG import now derives its target from the selected tab. GB/GBC tabs name
map addresses, so snapshot registers resolve the background/window role. Shared
maps default to background with an explanatory status; unused maps are rejected.
SNES main/sub composite and GBA/SMS access views cannot be imported as hardware
backgrounds. Managed tests cover swapped GB map roles and shared/unused maps.

Background PNG authoring has moved from Sprite Viewer to Tilemap Viewer as requested.
An independent editor loads per-game layers on opening and game changes, handles
PNG import/preview/removal and placement controls, and disposes preview assets with
the viewer. Background saves reload current sprite definitions; sprite saves reload
current background definitions, preserving edits made in the other tool. Interactive
verification and direct PNG/API end-to-end testing are still pending.

Background authoring now previews selected PNG artwork and reports PNG dimensions,
native canvas size, and saved placement. Selection/save refreshes the preview and
disposes its bitmap. A removal control saves the reduced definitions and reapplies
gameplay state while retaining the asset file. Persistent next-background IDs
prevent a subsequent import from reusing a removed layer's filename; managed tests
verify migration and retention after removal. Interactive preview/removal checks
and an on-screen composed placement preview are still pending.

Sprite Viewer background authoring now imports a PNG into the per-game pack as a
disabled layer and exposes its name, hardware layer, native X/Y placement, PNG
scale, scroll following, axis wrapping, and gameplay enable state. Save/apply
validates canvas/scale before writing definitions. Existing saved layers populate
the selector on load. Gameplay activation currently supports GB/GBC layers 0/1;
other system authoring can be persisted but enabled activation is rejected.
Interactive UI verification, background artwork preview, replacement/removal
controls, and complete cross-system integration remain unfinished.

The native background API now validates the ROM hash and complete MBG1 packet
before activation. Enabled definitions currently require standalone GB/GBC and
layer zero or one. The pack loader decodes PNGs and prepares both packets before
calling native activation; automatic per-game loading uses the same path. Capture
stays active while either sprite or background definitions remain enabled.
UI/native builds and rendering regressions verify compilation and existing live
paths; direct end-to-end PNG-loader/API verification and authoring controls remain
pending. Separate sprite/background API calls are not an atomic combined update.

MBG1 background packet serialization and native parsing now transfer enabled
artwork definitions, placement, scale, layer selection, wrapping, and PNG pixels.
Disabled layers do not load assets. Parsing validates identities, flags, scale,
dimensions, pixel lengths, and complete consumption before returning definitions.
Managed-to-native tests verify the transferred values and reject every truncated
packet and trailing bytes. The pack loader/API still needs to call this transfer.

Initial background composition is now connected through the decoder for complete,
uncropped direct-color captures with valid fetched samples. Definitions use an
immutable snapshot under the compositor lock and are cleared when decoder work
stops. Background scale contributes to HD output; artwork composes before sprite
replacement and supplies its transparency fallback. The live GBC window/scroll
fixture sets background and window artwork through the native test API, verifies
replacement background output and every sprite artwork pixel including alpha,
then clears both definitions. PNG pack transfer and authoring are not connected;
other systems' capture adapters, cropped output, and effects remain unfinished.

For direct-color frames, the background compositor now builds a full HD fallback
surface including background samples covered by sprites. The sprite compositor
can consume that surface for transparent and partial-alpha artwork; an invalid
surface size retains native fallback. Combined native tests verify background
artwork appears beneath transparent sprites, blends beneath half-alpha samples,
and leaves opaque artwork and unmatched sprite pixels intact. Pack transfer,
cropped fallback surfaces, and system-specific effect adapters remain unfinished.

The native background compositor can now apply sampled artwork to exposed
background pixels in direct-color captures. It preserves winning OBJ pixels and
invalid captures, blends PNG alpha over the native output, and uses authored order
for overlapping replacements. Native tests verify transparent, half-alpha, and
opaque samples at HD resolution and sprite visibility masks. Main/subscreen and
GBA-effect frames are rejected pending their composition adapters. Decoder
activation, authoring,
and PNG loading are still required; initial GB/GBC gameplay integration is verified.

A shared native background artwork sampler now consumes valid fetched layer
coordinates, with screen locking, native placement, axis wrapping, and HD scale
conversion. It preserves PNG alpha for later composition and rejects mismatched
layers, invalid samples, clipped coordinates, or malformed artwork buffers.
Native tests cover window-layer selection, wrapped seams, unequal scales,
screen locking, clipping, and buffer validation. PNG loading and authoring UI
remain unfinished; full priority/effect composition across systems remains open.

GB/GBC capture now carries fetched background coordinates through the pixel FIFO
to each output pixel, including pixels hidden by OBJ. Layer zero identifies the
background and layer one the window; coordinates describe tilemap placement,
independent of tile mirroring. Disabled backgrounds and inserted glitch pixels
invalidate the sample. These transient fields do not change save-state data.
Live GB/GBC fixtures verify every background sample in an unscrolled frame. A GBC
fixture additionally sets SCX=253 and SCY=250, verifying fine horizontal scrolling
and both 256-pixel map wrap boundaries across every output sample while sprite
replacement remains enabled. A further fixture enables the window at screen
(40, 24) over that scrolled background and verifies the layer/coordinate change
for every output pixel, including its first row and column. Dynamic window
enable/disable, clipped window origins, and mid-scanline register changes still
need live coverage; background artwork composition is not connected yet.

Background replacement groundwork now persists optional PNG layer definitions
alongside per-game sprite groups: hardware layer selection, native placement,
artwork scale, scroll following or screen locking, and independent axis wrapping.
Coordinate sampling clips unwrapped axes, wraps negative coordinates, and uses
wide arithmetic for scroll/placement sums. Managed tests verify serialization,
scaled negative wrapping, clipping, screen locking, and extreme scroll values.
This data model does not yet render backgrounds or expose authoring controls.
Per-system background identity, per-pixel scrolling/window/priority capture,
PNG loading and sprite composition remain required. The shared placement model
now samples HD subpixels using the PNG/output scale ratio. Tests cover unequal
scales, every sample within an output pixel, negative wrapped seams, and invalid
subpixel coordinates. Runtime adapters still need to use these sampling rules.

Live Game Boy coverage now includes an original GBC-only cartridge with explicit
OBJ and background palette RAM programming. The native harness selects GBC mode
and verifies the fetched red OBJ graphic and blue background before replacement.
Both GB and GBC fixtures check every pixel in the 2x replacement rectangle,
including a transparent pixel that restores the backdrop and a half-alpha pixel
with independently calculated channel values, background preservation, and
disabling. A second GBC fixture places the graphic
only in VRAM bank one and selects OBJ palette three, leaving the bank-zero tile
empty. It verifies the same decoded red source and complete replacement rectangle.
Palette changes during rendering, windows, and competing OBJ/background priority
still need more coverage.

Live recognition reports enabled groups that claim the same hardware sprites in
the snapshot. Earlier definitions have runtime matching priority. **Move group
earlier/later** changes the persisted pack order and reapplies gameplay definitions
without changing IDs or pose families. Recognition remains a snapshot diagnostic;
runtime fetched graphics and visibility may differ. Managed packet tests verify
that enabled definitions retain authored priority rather than sorting by ID.

After editing a captured origin, **Align animation pose origins** applies its
offset from bottom center to the other poses in the same family, accounting for
each pose's canvas dimensions. It saves and reapplies the definitions. Other
families and independently positioned replacement artwork retain their origins.
Managed tests cover differing dimensions, family isolation, and artwork anchors.

The Sprite Viewer can remove the selected group or animation pose and undo the
last removal during the current viewer session. Changes are saved before the
viewer list is updated; a save failure restores the in-memory definition.
Removal reapplies gameplay definitions and refreshes live match overlays. Other
poses retain their family IDs, group numbers are not reused, and PNG assets are
retained so undo can restore the same artwork. Loading another pack clears undo.

SNES packs now support Sprite Viewer activation and automatic loading for SFC games.
The optional PPU capture tracks main/subscreen OBJ identities, fetched graphic
slices, native colors, background fallback winners, window/color-math decisions,
and brightness. State loading discards capture. Shared reconstruction assembles
complete sprites and rejects missing or contradictory slices. Standard output
retains the framebuffer's seven-row top offset.

The default decoder composes scaled PNG artwork in low-resolution, noninterlaced
frames with neutral video color settings and no forced-resolution conversion.
Composition supports main/subscreen samples, fixed color, addition/subtraction,
effective half operation, brightness, and transparent/partial-alpha fallback.
Native tests verify effect rules and independence from replacement-group order.
An original generated LoROM fixture verifies live graphic reconstruction, every
pixel of 2x artwork, border/crop placement, transparent and half-alpha backdrop
restoration, background preservation, and disabling replacements.

This is an initial supported path. High-resolution output, OBJ interlace, palette
timing, complex windows and multiple background layers, exact native rounding,
and simultaneous main/subscreen replacements need further integration or stronger
live coverage. Artwork currently follows the original winning sprite footprint;
expanded artwork beyond that footprint is not yet supported.

NES shared integration has begun with optional pixel-priority capture in the PPU.
It records the winning primary OAM identity and native sprite/background
palette colors, including grayscale and emphasis bits, at the composition decision.
Secondary OAM evaluation now retains primary OAM identity, and each fetched shifter
snapshots that identity independently of the next scanline's evaluation. Fetched
rows retain mirrored pattern bytes, position, native palette indices, and height.
Misaligned/overflow-bug evaluations conservatively omit unknown identities.
The shared compositor now reconstructs NES candidates with an explicit native
palette converter. Native tests verify OAM identity, placement, transparency,
palette/emphasis index conversion, and rejection of missing or contradictory rows.
An original generated NROM cartridge now verifies fetched graphic reconstruction,
primary OAM identity, placement, and captured native sprite/background colors in
live emulation. Existing other-system gameplay fixtures remain passing.
The default NES decoder now integrates shared replacement composition. The live
NROM fixture verifies every pixel in 2x artwork, transparent backdrop restoration,
background preservation, and disabling. Other filters and existing HD-pack output
are excluded. Viewer activation and automatic game loading are now connected;
broader NES hardware scenarios remain pending.
Capture starts on a complete frame and is discarded on state loading. Existing HD
frame data and VS dual-console layouts do not attach this metadata. Native activation
requires the standard PPU and excludes VS dual-console layouts. Forced-blank capture,
palette timing, clipping, 8x16/mirrored sprites, and priority scenarios still require
further validation and integration.

WonderSwan has an initial optional capture path. It retains transient sprite IDs
through background priority selection, decodes evaluated 8x8 sprite rows in all
four video modes, and records native sprite/background colors at output time.
Background fallback is selected independently of sprite occlusion. Save-state loads
discard capture; icons, sleeping/disabled LCD, and shortened frames do not attach
capture to output. Capture starts on the next complete frame. An original generated
color WonderSwan ROM verifies live 2bpp graphic reconstruction, sprite identity,
position, native colors, and backdrop. Decoder integration now passes live tests
for every pixel in a 2x replacement, transparent backdrop restoration, and disabling.
Background restoration uses the filter's native palette. NTSC and frame blending
bypass replacement composition. Additional live fixtures verify planar and packed
4bpp capture and replacement. Pack activation is connected to the viewer and
automatic game loading. Monochrome, priority/window scenarios, palette timing,
and output with LCD icons still require further work.

GBA development has begun with optional frame capture at color composition.
Object pixels retain transient OAM IDs through priority selection and mosaic.
Capture records primary/secondary object identity, native colors, window state,
blend coefficients, brightness, and semi-transparent object participation.
Loading a state discards capture and transient pixel identities. Stereoscopic
frames are excluded. An original generated GBA ROM verifies live capture,
complete fetched graphic reconstruction, object identity, native color, and
position for a basic 8x8 object. Additional original-ROM fixtures verify asymmetric
horizontal flipping and 8-bit palette decoding against every expected captured
and displayed pixel. Fetched object pixels accumulate into row fragments using
the renderer's transformed coordinates, native palette, wrapped position, and
object dimensions; affine samples outside texture bounds record transparency.
Only actually fetched pixels are recorded, so incomplete/occluded captures reject
the candidate through shared reconstruction. Broader live verification, first-row capture
across the object pipeline, and effect-aware replacement composition remain
pending. Basic GBA gameplay replacement is now enabled under the filter conditions
described below; complete GBA coverage remains unfinished.

The shared compositor now provides an artwork color-effect operation using captured
GBA coefficients, target flags, window eligibility, and brightness. Native tests
verify alpha blending, semi-transparent object precedence, disabled effects,
coefficient clamping, saturation, and brightness endpoints. This operation preserves
8-bit artwork detail. GBA frames explicitly select this operation during primary
artwork sampling; composition tests verify blended and darkened output and preserve
occluded pixels. Opaque primary and secondary artwork samples are now collected
before effects are applied. Tests verify secondary-only replacement and identical
results with reversed group order. Transparent and partial-alpha GBA samples now
use captured fallback composition. Tests verify removing primary/secondary artwork,
partial alpha, and re-evaluation of blend eligibility when the secondary object is
removed. Original-ROM live tests now verify 2x output dimensions, every replacement
pixel, transparent and partial-alpha samples, backdrop preservation, and restoration
after disabling for basic, horizontally flipped, and 8-bit objects. GBA packs can
be applied from the viewer and reloaded on game load. Composition currently requires
the default filter, no frame blending or LCD color adjustment, and neutral video
color settings. NTSC, stereoscopic output, and corrected-color artwork remain pending.

An additional live brightness fixture darkens both objects and the backdrop. It
verifies every darkened replacement pixel, transparent and partial-alpha fallback,
native source restoration, and the unaffected graphic identity used for matching.
Artwork effects retain 8-bit PNG detail, while untouched native pixels and captured
fallbacks retain the PPU's 5-bit rounding.

GBA capture now also selects the two winning background layers without objects,
respecting active windows, and records their composed native fallback color plus
the leading background color and blend-target flag. Native brightness and alpha
math are used for this fallback. Live fixtures verify a known backdrop beneath
basic, flipped, and 8-bit objects. Multi-background/window fallback scenarios
still require live validation.

PCE development has started with optional per-VDC pixel metadata at the hardware
composition stage. It retains the actual winning SAT sprite ID, sprite color,
background color/index, and grayscale flag, and clears stale capture on state
loads. The capture API enables this for ordinary PC Engine consoles. Fetched
sprite rows now retain evaluated dimensions and slice positions and decode the
actual fetched 2/4-bitplane data, mirroring, and palette. Shared reconstruction
assembles wide sprite slices, rejecting missing or contradictory fragments;
native tests cover those cases. Ordinary PCE capture now assembles full frames
of priority pixels and decoded fragments in VDC dot coordinates, retaining the
picture's horizontal offset. Frame capture begins at the next complete picture
and is invalidated when loading a state. PCE packs can now be enabled and loaded
automatically; the default filter maps stable-dot-clock capture through the same
source crop used for display and converts native background colors through its
palette. Original generated PCE ROMs verify 16- and 32-pixel-wide source graphics,
crop alignment, every pixel in the 2x replacement area, background preservation,
and restoration after disabling. Additional fixtures verify stable divider-3 and
divider-2 output and an asymmetric horizontally mirrored red/blue sprite. Each
fixture configures horizontal display timing for its VCE clock; retaining the
low-clock timing at higher clocks creates extra display periods and incomplete
sprites. Forced
fixed-resolution and mixed-dot-clock mapping, NTSC-filter support, and SuperGrafx
VPC priority composition remain outstanding.

SMS now has optional frame capture for Mode 4 sprites: decoded fetched
rows, per-pixel winning sprite IDs, and background colors after priority decisions.
Capture is invalidated when loading a save state. Shared reconstruction rejects
missing rows, changes in position/dimensions, and contradictory repeated rows.
Native tests cover those reconstruction cases and decoded-row replacement with
background-priority preservation. SMS packs can now be activated from the viewer
and automatically reloaded on game load. The default SMS video filter integrates
replacement composition using its native color conversion; capture coordinates
account for the vertical border offset, and CRAM write artifacts retain their
original pixels. UI and native builds pass. An original generated SMS ROM now
verifies live capture, 2x replacement pixels, 192-line vertical border placement,
background-priority occlusion, and disabling restoration. This test caught and
fixed capture using evaluation rows from the following scanline: the shifters now
retain row numbers and hardware IDs from fetch time. Capture also retains the
unscaled display-row number, enabling doubled vertical size without inventing
missing rows. Additional live fixtures verify full SMS2 zoom and SMS1 vertical
zoom with horizontal zoom disabled. Horizontal capture follows the renderer's
per-row SMS1 zoom eligibility; inconsistent widths across rows deliberately do
not match. The Sprite Viewer now displays vertical SMS1 zoom correctly. A full
SMS1 mixed-width authoring workflow and older VDP modes still need more work.

Fetched Mode 4 bitplanes are retained before the renderer shifts away offscreen
bits. A live fixture verifies that an X=-4 sprite retains its complete expected
graphic for matching and receives replacement pixels in its visible portion.
Another fixture verifies preservation of the VDP's first-column mask. The live
tests also check the captured source against the ROM's known full graphic, rather
than merely using any captured data as its own matching reference.

The shared viewer supports Ctrl-click on the screen layout or sprite grid, or a
rectangle selection mode. Create a group while paused on an animation pose.
Groups receive persistent monotonically allocated IDs, an optional name, and an
origin defaulting to bottom center. Set the origin numerically or click the group
preview with origin editing enabled. Save group edits explicitly.

Export captures transparent previews independently of the viewer background.
Import accepts a canvas whose dimensions are divisible by its chosen scale (1–10),
up to 8192 pixels per dimension, and copies the replacement into the pack folder.
Artwork can be larger than the captured group. Its independent origin aligns to
the captured group's world origin. A uniform enlargement of an exported canvas
is detected automatically on first import. Imported artwork previews in the editor.
Choose an existing group and use “Add selection as animation pose” to capture a
related pose. Poses persist a family ID and editable pose name, retain independent
graphic snapshots and PNGs, and inherit scale and the source origin's offset from
bottom center. For asymmetrical poses, adjust each captured origin to the same
character anchor; bounding boxes alone cannot establish that anchor automatically.
Recognition chooses the matching pose by its graphics and arrangement.
Selection records the displayed copy of wrapped sprites, so selecting across the
left or top screen edge preserves nearby relative positions in the exported canvas.

Definitions are saved under the configured HD pack folder:
`MetaSprites/<ROM SHA1>/<CPU type>/groups.json`.
This separates games with the same filename and separates multiple processors in
a console. Snapshots contain sprite pixels and relative coordinates, rather than
OAM indices or VRAM addresses. The first matcher uses exact RGBA snapshots and
requires every group member; palette changes and partial groups do not match.
The matcher is connected to the optional live Sprite Viewer preview. Enable
"Recognize groups in live preview" to show matching group bounds, origin crosses,
and numbered matches with origin coordinates. Sprite tooltips list matched group
numbers. The preview obtains transparent sprite graphics independently of the
viewer backdrop. Wrapped copies cannot count as multiple members of one group.
This does not yet replace graphics in gameplay renderers.

The first native capture path is available for standalone Game Boy/GBC frames via
`SetMetaSpriteFrameCapture`. It is opt-in and activates on a complete frame.
The PPU records the winning hardware sprite reference, original sprite color,
background color beneath it, color indices, background/sprite attributes, and
window state at draw time. Fetched sprite rows record their actual pattern bytes,
addresses, positions, and attributes for subsequent content recognition. Captures
use shared ownership across asynchronous decoding and are
discarded on save-state loading. Super Game Boy and linked layouts are not yet
supported by this capture path. The first native compositor reconstructs complete
sprite graphics from captured rows and matches supplied replacement definitions.
Video decoding calls it before rotation and display scaling for native-resolution
Game Boy filter output. It uses the filter's native-color conversion when restoring
the background beneath transparent replacement pixels.

The editor-to-native definition/artwork transfer is connected for standalone
Game Boy/GBC. Load a replacement PNG, check "Enable group in gameplay", then
"Apply gameplay replacements" (or save group edits). Enabled state is saved;
saved groups are loaded automatically when a Game Boy/GBC ROM starts or power
cycles. Opening the Sprite Viewer also reloads and applies enabled groups. Both
paths share PNG decoding and packet transfer. A queued game-load request checks
its expected ROM hash before loading, preventing application to a newer game.
Native application
checks the ROM hash and rejects malformed packets before changing definitions.
Definitions are cleared when video decoding stops. Invalid packs and missing PNGs
report a load failure without overwriting saved definitions.

The compositor uses the largest enabled artwork scale for HD output and samples
each PNG within native-pixel blocks, retaining its extra detail. Original graphics
are enlarged with nearest-neighbor sampling. Decoder dimensions, rotation input,
and HUD scale follow the enlarged output. It replaces only pixels where a matched
sprite originally won hardware composition. Drawing artwork outside that footprint,
clipped source-group recognition, and non-native filter
mapping remain required work; this initial behavior is not the completed design.

## Remaining integration

- Exercise selection, origins, export/import, persistence, and game switching on
  real sprite viewer data for every system. The editor now compiles with .NET 10;
  compilation is not a substitute for these runtime checks.
- Exercise wrapped sprite selection and export against real SNES, SMS, and GBA
  viewer data; the editor now stores the chosen wrapped copy's coordinate offset.
- Capture native graphic identity separately from palette, transforms, and effects
  for robust runtime matching. Keep each frame's captured state synchronized with
  asynchronous video decoding; end-of-frame VRAM alone is insufficient.
- Connect recognition and replacement composition to each renderer, suppressing
  original member pixels and preserving priority, masks, windows, hardware color
  math, affine transforms, mosaic, and occlusion. A flattened frame overlay does
  not meet these requirements.
- Add enable/disable, group deletion, pose organization,
  and authoring feedback for ambiguous matches.
- Add PNG background layers with per-layer origin, scrolling, priority, and masks.

## Verification

`dotnet run --project Tests/MetaSprites/MetaSprites.csproj` exercises the production
pack and matcher source on .NET 8 without external dependencies. It covers multiple
instances, reordered candidate indices, translated origins, missing members,
incorrect arrangements, graphic mismatches, JSON round trips with reflection
disabled, persistent ID allocation, ROM isolation, and malformed pixel rejection.
The checks also cover placement of independent replacement origins at HD output
scale, canvas divisibility, persistence of artwork origins, and compatibility with
definitions written before independent artwork origins were added.
Wrapped-copy deduplication and exclusion of transient hardware indices from saved
group definitions are also covered.

The application UI targets .NET 10; SDK 10.0.401 was installed locally under
`.tools/dotnet` for validation. A passing standalone test does not establish
that any system supports gameplay replacements yet. A full UI build succeeded
using that SDK and an explicit workspace `SolutionDir`, with zero warnings and
zero errors after correcting obsolete PNG export and placeholder APIs.
The native Core and InteropDLL projects also built successfully with the Game Boy
capture and exported API. The sandbox initially prevented the MSVC program-database
helper from working; building outside it resolved that failure. A generated original
Game Boy ROM now passes end-to-end capture and rendering checks. Save states and
real-game priority scenarios still need further validation.

`Tests/MetaSprites/NativeCompositor.vcxproj` builds a native test executable using
the production compositor. It verifies captured-row reconstruction, exact graphic
matching, preservation of background and competing-sprite masks, transparent
replacement suppression of original sprites, alpha blending, incomplete-capture
fallback, disabling replacement definitions, distinct HD subpixels, alpha blending
at HD resolution, cropping, and scaled priority masks. These tests passed, and the
native core/interop DLL rebuilt successfully with the compositor integration.
