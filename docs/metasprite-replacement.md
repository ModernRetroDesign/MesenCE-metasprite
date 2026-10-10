# Metasprite replacement development

## Goal

The Sprite Viewer is the authoring tool for numbered groups of hardware sprites,
an editable origin in native pixels, transparent group PNG export, and replacement
PNG import. Runtime replacement must support NES, Game Boy/GBC, GBA, PCE, SMS,
SNES, and WonderSwan while retaining each system's clipping, sprite order,
background priority, and effects. Background PNG layers are the next goal.
3D models are a later renderer feasibility investigation.

## Current implementation

Background replacement groundwork now persists optional PNG layer definitions
alongside per-game sprite groups: hardware layer selection, native placement,
artwork scale, scroll following or screen locking, and independent axis wrapping.
Coordinate sampling clips unwrapped axes, wraps negative coordinates, and uses
wide arithmetic for scroll/placement sums. Managed tests verify serialization,
scaled negative wrapping, clipping, screen locking, and extreme scroll values.
This data model does not yet render backgrounds or expose authoring controls.
Per-system background identity, per-pixel scrolling/window/priority capture,
PNG loading, HD subpixel sampling, and sprite composition remain required.

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

