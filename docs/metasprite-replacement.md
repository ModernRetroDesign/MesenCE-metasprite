# Metasprite replacement development

## Goal

The Sprite Viewer is the authoring tool for numbered groups of hardware sprites,
an editable origin in native pixels, transparent group PNG export, and replacement
PNG import. Runtime replacement must support NES, Game Boy/GBC, GBA, PCE, SMS,
SNES, and WonderSwan while retaining each system's clipping, sprite order,
background priority, and effects. Background PNG layers belong in the Tilemap Viewer.
CPU-memory conditions will select alternate metasprite PNGs and gate background
replacements. 3D renderer investigation is deferred.

Development priorities:

1. Main goal: complete the Sprite Viewer metasprite workflow across all seven
   systems. Select recurring sprite arrangements on screen, assign persistent
   group numbers and optional names, set their origins, export PNG artwork, and
   load replacement PNGs aligned to those origins during gameplay.
2. Secondary goal: complete PNG background replacement layers in the Tilemap
   Viewer across all seven systems, including placement, scrolling, and native
   composition with sprites.
3. CPU-memory conditions: select alternate metasprite PNGs when all configured
   byte/mask conditions match, and gate background replacements the same way.
   Capture and check only user-defined addresses; never save full memory dumps.
4. Performance: investigate and fix slowdowns with PNG replacement active;
   measure and optimize recognition, hashing, capture, drawing and allocation costs.
5. Later investigation: assess whether the renderer can load and draw 3D models
   anchored to these groups. Determine renderer changes, depth/occlusion handling,
   and performance requirements before committing to implementation. This remains
   an optional extension, not a dependency of PNG replacement support.

These are completion targets; the implementation notes below describe partial
progress and outstanding system-specific work.

## Current implementation

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
- Examine native and software renderer paths before designing a 3D asset loader.

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

## Initial 3D renderer assessment

The managed-to-native packet fixture also passed the native parser and compositor.
Tests reject every truncation of that fixture and reject trailing data. Both the
UI and native interop DLL compile with the enabled-group transfer. This establishes
packet compatibility and synthetic composition behavior. The optional `--gameplay`
managed test argument accepts the built MesenCore.dll path and runs an original
generated Game Boy ROM. It verifies live sprite capture, 2x output dimensions,
replacement pixels, background preservation, and restoration when disabled.
The harness uses its generated ROM directory as the emulator home folder.

`Windows/Renderer.h` owns a Direct3D 11 device, texture resources, vertex and pixel
shaders, and a vertex buffer. `Linux/LinuxOglRenderer.cpp` creates a textured quad
with two-dimensional positions and UVs. `Core/Shared/Video/SoftwareRenderer.h`
accepts completed frame buffers. These paths currently present two-dimensional
frames; inspecting them does not establish existing support for model assets.

A future implementation could render a model to a transparent offscreen texture
and feed that result through the same origin and visibility-aware replacement
composition as PNG artwork. This requires model loading, camera configuration,
animation, materials, depth buffering, and a defined software-renderer fallback.
The GPU backends offer a possible foundation, but loading a model is not currently
a supported option. The replacement interface should permit a future image source
without tying group recognition to a particular model format.

