# Metasprite workflow completion audit

The active objective includes sprite authoring, background replacements and CPU
conditions. Performance investigation was removed by the user. This audit does
not establish completion of the full objective.

| Requirement | Current evidence | Status / remaining work |
| --- | --- | --- |
| Ctrl/rectangle selection, persistent groups and names | Actual editor model and routed XAML-control suites for eight configurations (NES, GB, GBC, GBA, PCE, SMS, SNES, WS) | Verified in these suites; user verified visible artwork navigation and origin clicks |
| Create automatically saves/assigns transparent PNG and enables gameplay | SaveNewMetaGroup, UseExportedArtwork, actual Create/save/native activation/reload tests | Verified, including rollback and retry |
| Click/numeric origins, scale and pose alignment | Default/alternate canvas events, numeric controls, persisted origin and pose-family tests | Verified; clicks now always enabled |
| Content/relative-layout recognition rather than hardware indices | Shared managed matcher and native decoded candidates; reordered-index and incomplete-group checks | Verified in fixture coverage |
| Replacement PNG alpha governs artwork | Exact 256-alpha-value PNG round trip; shared/GBA/SNES crossing-canvas regression with opaque/transparent/partial-alpha front PNGs in both pack orders | Full-canvas paths verified by native fixtures; unreplaced lower sprite reveal remains to verify |
| Full PNG canvas replaces HDnes-style additions | Shared complete-canvas compositor, native 1x/2x/3x and alternate checks; Debug/Release whole-frame NES NROM, GB/GBC ROM-only, single-VDC PCE, SMS Mode 4, two GBA cases, three SNES lowres plus nine no-math hires cases and 17 WS color/monochrome gameplay checks; synthetic ordering/background gating | Implemented for these fixture scopes; PCE SuperGrafx/PNG priority transitions, broader WS combinations, broader GBA effects/transforms and SNES hires color math/interlace remain unfinished |
| PNG dimensions need not divide evenly by scale | All eight hidden viewer configurations import/persist 17x19 default/alternate PNGs at scale 2; native final-corner and packet checks; complete PNG alpha round trip | Verified for authoring/loading and tested renderer scope; background wrap canvas validation remains separate |
| System-specific clipping, visibility and priority | Native fixtures listed in metasprite-replacement.md | Verified for tested modes; broader scope incomplete below |
| Background PNG export/import/placement/scrolling/composition | All eight authoring suites and native gameplay fixtures | Verified for tested hardware modes; resize repair and failed replacement save covered |
| Alternate artwork and required background CPU checks | Packet filtering, masked condition authoring, live SNES WRAM transitions | Verified; draft alternates remain inactive and backgrounds require configured checks |
| Read only configured CPU addresses | CompileMemoryAddresses and CaptureMemorySnapshot iterate only compiled condition addresses; native snapshots tested with one/two configured bytes | Verified; no full-memory snapshot path is used for replacement conditions |
| Current build artifacts | Debug/Release UI and native builds, Release GBA affine/bitmap suite | Verified; full Release desktop interaction remains unverified |

## Renderer gaps that prevent broad completion

PCE single-VDC opaque-background priority has two new live generated-ROM cases
(`--pce-priority`) passing in Debug and Release. Both use an opaque blue tilemap
and a red sprite, with the foreground flag enabled or disabled respectively.
Foreground replacement artwork draws above the tilemap; behind-background artwork
is suppressed inside source bounds and throughout the expanded canvas (5,329
blocked output samples in the margin oracle). Checks include crop/origin alignment,
full-canvas PNG alpha, memory-gated activation and native restoration. Both cases
are included in the cross-system gameplay runner. This closes representative
native opaque-BG priority coverage; SuperGrafx, lower-native-OBJ reveal and priority
transitions with changing background PNG opacity remain incomplete.

A subsequent live PCE case exposed behind-background metasprites incorrectly
drawing over opaque background PNGs (flags 0x1B before the fix). The shared
direct-color full-canvas compositor now resolves the active background PNG over
the opaque sprite outcome before applying sprite PNG alpha. This keeps the
background above behind-background artwork without masking artwork by original
sprite alpha, and preserves foreground artwork priority. A native regression
checks transparent, half-alpha and opaque background samples with opaque and
half-alpha replacement pixels inside member bounds and margins. System adapters
now explicitly capture which sprite classes a background PNG covers: NES/PCE
behind-background sprites, GB/GBC hardware background priority, SMS priority
tiles, and WonderSwan foreground BG1 (including both sprite-window classes).
WonderSwan BG0 stays below sprites. Debug/Release native regressions pass;
the three-case PCE priority runner passes in Debug and Release with the explicit
adapter metadata. The complete cross-system Debug gameplay runner exits 0 with
the updated SMS priority oracle, covering all seven systems, and all 17 WonderSwan
Release cases pass. Live partially transparent background PNG priority transitions
still need dedicated fixtures; the native regression establishes the blending
formula but is not full hardware capture coverage.

WonderSwan monochrome now has six live Debug/Release cases in addition to the
11 existing color-mode cases. Generated ROMs select mode 0 and monochrome hardware,
configure shade/palette registers for black OBJ and white background, and verify
decoded sprite colors and native RGB444 capture. Metasprite full-canvas alpha,
origin/margins, scrolling background PNGs, inside-window BG1 selection, explicit
CPU condition checks and disable restoration pass; both 17-case runners exit 0.
This closes the missing representative monochrome gameplay coverage. Changing
shade registers during gameplay and unmatched lower-native-OBJ reveal remain
outside this verification scope. Monochrome outside-background-window occlusion
blocks the original bounds and all expanded artwork; inside/outside OBJ windows
clip expanded canvases correctly. All three additional cases pass in both builds.

WonderSwan background fixtures now explicitly configure two distinct CPU addresses
(0 and 1). Masked conditions use bits 0 and 7 respectively: either mismatch
suppresses both authored BG layers, while both matches restore them. Pixel checks
cover BG0 and the window region, including scrolling and occluded sprites in
color and monochrome. Capture contains exactly these two addresses, then shrinks
to only the sprite's address 0 when backgrounds are removed. The complete 17-case
Debug/Release runners pass after these checks. No full memory dump is used.

Rebuilt Debug UI SNES `--selection-suite Snes` runs report successful
selection, Create/PNG/JSON/native activation/reload, named pose/origin persistence,
default and conditional alternate import, background export/import/placement/scroll
and CPU conditions, rollback and stale-editor protection. One run stalled after
its final GameLoaded activation check and was interrupted (session 83920 is now
terminal). Shutdown markers were added to the test harness. Three subsequent
runs completed with exit code 0, including debugger release, emulator stop and
emulator release. These establish completed hidden-window authoring checks;
the intermittent earlier stall has no identified cause or production fix and
remains a reliability concern. Visible Release interaction is still unverified.

Live BobBlend state-restore verification now passes in Debug and Release: the harness saves
and reloads an in-memory state, drains pending decode work, and records the first
subsequent PNG-composed frame. Its field-edge pixel matches the unblended active
field instead of the previous temporal average. All fifteen supported Debug
and Release cases pass with this check. The latest Release run also verifies
mode 5/6 and pseudo-hires PNG canvases, main/subscreen windows, background scroll,
Weave/Current Field/Bob/BobBlend output, first-picture-row sampling, memory-gated
suppression and native restoration. This suite excludes OBJ-interlace and active
hires color math; those remain incomplete.

Fixed Bob first-picture-row PNG sampling: the native renderer copies an odd
field's first scanline upward, but the PNG sampler previously applied the global
one-row shift to that copied row too, omitting the artwork's first row. Capture
now marks the copied destination row with its sample offset. A reproducing Debug
native sprite/background test failed before the fix and passes afterward. Full
Debug/Release native suites pass; the rebuilt Debug core passes all thirteen
supported live hires/interlace cases. Two new Debug live LoROM fixtures place
oversized artwork at the first picture row in Bob and BobBlend. Whole-frame
output, both field parities, copied-row alpha and temporal averaging pass in Debug
and Release. Both supported runners now have fifteen passing cases after rebuilding
the native cores with the copied-row fix.

Bob background PNG composition now has Debug/Release native regression coverage for
both parities, distinct 2x artwork texels, complete output-pixel checks and a
single configured CPU condition that suppresses artwork when unmatched. Transparent
and partial-alpha PNG pixels fall back to the captured native layer correctly;
the composed even/odd images are then checked through the real blend filter with
exact per-channel averages. This
uses generated capture metadata; live background capture and temporal BobBlend
composition are still not established by this test.

The Debug Bob background fixture also composes a foreground metasprite PNG over
the background in both parities, verifying native OBJ priority, the odd-field
vertical alignment and retention of transparent/partial-alpha background texels
outside the sprite canvas. This is compositor coverage rather than live PPU proof.

Latest BobBlend status: runtime now attaches Bob-mapped ordinary-OBJ captures
without color math in BobBlend mode. SNES default filtering defers frame blending
until after PNG composition. The Debug and Release thirteen-case hires/interlace suite passes;
its BobBlend oracle checks every output pixel against the average of even/odd
21x21 PNG canvases, including transparent/partial-alpha edges and native borders,
and requires both capture parities. CPU suppression and disable restoration pass.
Live state-load resets, background PNG temporal composition,
and moving poses remain to verify. Earlier entries below record foundation stages.

Bob mapping foundation: `MapBobWithoutMath` repeats the current field's rows,
shifts odd-field capture down one physical row, and copies the first picture row
up as the native renderer does. `SnesOutputOffsetY` aligns SNES PNG sampling to
that shift and resets with pooled frames. Debug native tests cover both parities
and reuse. Runtime now attaches Bob captures for ordinary OBJ without color math.
The Debug and Release twelve-case hires/interlace suite passes; Bob's whole-frame enlarged
PNG oracle must observe both parities with correct one-row offset, alpha, borders,
CPU suppression and restoration. Bob background-layer coverage remains
outstanding. BobBlend remains unsupported.

BobBlend integration inspection: SNES default filtering enables frame blending
in `OnBeforeApplyFilter`, while `VideoDecoder::DecodeFrame` composes replacement
PNGs after `SendFrame`. Attaching Bob capture alone would therefore paint fresh
unblended PNG pixels over an already blended native frame. Implement final-frame
blending after sprite/background composition, with history reset on dimensions,
game/state changes and mode changes, before enabling BobBlend. Debug native Bob
mapping assertions now also verify background sample coordinates, captured layer
colors and physical subpixel offsets through the odd-field row shift; live
background PNG composition remains to verify.

Post-composition blend hook is implemented: `BaseVideoFilter` can defer its
frame blend, and `VideoDecoder` finishes it after PNG composition before rotation
and scaling. SNES default filtering opts into deferral for supported metasprite
captures. Debug core build and all twelve supported hires/interlace cases pass.
BobBlend capture is still disabled; history reset on mode/shape/game/state changes
and a live temporal pixel oracle are required before enabling it.

Blend history now has explicit validity/reset handling. Actual frame publication
resets history when blending is disabled, paused/rewinding, dimensions change or
the pipeline switches between native and post-composition blending. Debug native
tests verify exact consecutive-color averaging and no stale averaging after
explicit reset, disabled filtering and frame-number discontinuity. Debug core
build passes. Game/state generation resets and live BobBlend tests remain pending.

Successful console deserialization now queues blend-history reset atomically;
the decode thread consumes it before filtering the next frame. Game startup
already recreates its video filter. Debug core build and twelve supported
hires/interlace cases pass after the state-reset hook. A live state-load temporal
oracle and BobBlend activation verification remain outstanding.

SNES Current Field deinterlacing now attaches ordinary-OBJ captures without color
math, restricting replacements to the active parity. The Debug and Release eleven-case
`--snes-hires-weave` suite passes, including whole-frame enlarged PNG/alpha,
inactive black rows, condition suppression and disable restoration. OBJ-interlace and BobBlend
are still unsupported by this integration.

### Deferred NES screen-edge tracking proposal

User deferred this proposal; do not implement it until resumed. Extend existing
NES capture rather than replace it: retain visible pixel capture for clipping,
background priority and sprite order, and add mapper-aware OAM/CHR recognition
candidates for off-screen members. Establish partial-group origins using visible
anchors and validate missing members against decoded candidates. Keep group
coordinates outside the viewport, clipping the full replacement PNG only during
composition, including the NES left-column sprite mask. Resolve unsigned OAM X
wraparound using saved relative arrangements and visible anchors rather than
treating every high X value as negative. Consider brief evidence-based tracking
only for games that remove off-screen members from OAM; previous position alone
must not establish identity. Preserve mapper bank/fetch timing and sprite flips;
a generic end-of-frame CHR memory read may not be sufficient. Replacement PNG
alpha remains authoritative. No capture or matching implementation changes were
authorized by this consultation.

The woven-field foundation now has Debug and Release native regression coverage.
It preserves separate even/odd graphic poses, tags OAM IDs by field, prevents
recognition from joining members across fields, and copies only the newest
field's configured CPU samples. Already tagged or out-of-range input IDs are
rejected before changing output. SNES runtime now attaches woven captures for
consecutive screen-interlace fields with ordinary OBJ, no color math and Weave
deinterlacing. Cached fields are cleared on state loading, rewind and unsupported
modes. The Debug screen-interlace probe now passes graphic/capture validation,
whole-frame oversized 21x21 PNG rendering at 512x478, transparent/partial-alpha
pixels, native black border preservation, configured CPU-condition suppression,
and disable restoration. Release now passes the same ten supported hires/weave
cases through `--snes-hires-weave`; this excludes OBJ-interlace explicitly, while
the full `--snes-hires` runner retains that failing probe. Debug/Release native
regressions additionally move one field's pose while retaining the other field's
position and verify newest configured CPU samples in either parity. Live moving
field-pose verification remains unfinished. OBJ-interlace still
has no supported capture. All nine existing no-math hires probes continue to pass.

SNES non-interlaced hires OBJ replacement now activates for frames without color
math. Logical decoded sprite coordinates remain unchanged; a separate 512x478
capture maps output phases and mirrored lowres lines to those coordinates. The
compositor uses a coordinate scale of two for PNG origins and sampling. Detailed
hires background capture now supplies main/subscreen samples and pixel density.
Three scrolling-background fixtures pass whole-frame 1x/2x PNG checks alongside
OBJ artwork in Debug and Release. Color-math dependencies and interlace field mapping
remain incomplete. Frames that need unsupported color math or field weaving
continue native rendering.

The opt-in `--snes-hires` runner collects failures so every case runs. Six neutral
LoROM hires fixtures now pass a dedicated native per-phase oracle: BG modes 5/6,
pseudo-hires, OBJ on both screens, both-screen Mode 5 with a main-screen window,
and main-only pseudo-hires with a window. The oracle checks the complete native
frame, independent screen eligibility, original sprite holes, replacement PNG
alpha, one-address CPU snapshots, condition gating and clearing. Screen interlace
and screen-plus-OBJ interlace still fail their initial capture probes. They need
field-aware assertions as well as implementation.
The current Debug and Release eleven-case runners pass all nine non-interlaced
cases and exit nonzero because their two interlace probes fail. Both native cores
build successfully, and the full gameplay suites pass in Debug and Release after
the background and NES/SMS full-canvas changes.

The current native output path places subscreen pixels in even columns and main
pixels in odd columns for double-width scanlines. Subscreen color math uses the
previous main pixel, while main color math uses the current subscreen pixel.
Interlaced rows also depend on field parity and the configured deinterlace mode.
These dependencies must be represented independently of the logical OBJ grid;
duplicating a lowres capture cannot preserve them.

The first renderer step now records main-phase color math on double-width lines
and stores independent subscreen output-phase scalar state. The math helper takes
an explicit destination pixel, so using the previous main pixel's math-control X
does not overwrite that main pixel's capture. Subscreen phase records retain the
current subscreen and previous main colors/identities, background samples and
fallback colors, plus the math-control X. Frame-pool reset clears this optional
storage. Debug core build and the full lowres SNES ordering/background/memory
suite passed after this change. The native compositor suite also passed,
including retained-frame immutability and clearing phase records on pool reuse.
Its build required running outside the sandbox for the Visual C++ PDB service.
Native unit tests now cover phase coverage mapping, lowres mirroring inside a
hires output grid, unchanged logical matching coordinates, shifted 1x artwork,
odd artwork scale and rejection of unsupported color math. The full lowres SNES
ordering/background/memory suite passed again after the output mapping change.
No broader hires color-math/background/interlace completion is claimed.
After the shared coordinate-sampling change, the full Debug `--gameplay` suite
also passed for NES, GB/GBC, GBA, PCE, SMS, SNES and WonderSwan, including its
existing background, alpha, window/priority and live CPU-condition fixtures.

After the GBA hidden-OBJ capture/composition changes, the complete Debug
`--gameplay` suite passes across NES NROM, GB/GBC ROM-only, GBA, single-VDC PCE,
SMS Mode 4, SNES LoROM lowres modes and WonderSwan color modes. This includes
ROM-driven SNES alternate/background memory transitions sampling only two
configured addresses and combined activation/invalid-packet retention. The GBA
hidden-window editor selection suite also passes creation/default PNG+JSON save,
reload, origin, alternate conditions, tilemap export/import and rollback checks.
Visible Release desktop interaction and the remaining system rendering gaps
are not established by these regressions.

GBA affine OBJ identity now has live Debug/Release checks for an oversized full
PNG canvas and for source holes combined with WIN0 clipping. The targeted GBA
affine/bitmap suite now passes all thirty-two cases in both configurations, including
an enlarged OBJ using a 0.5 diagonal inverse matrix with its normal bounding box.
A second enlarged 8bpp OBJ uses an asymmetric source with columns 0..4 red and
5..7 blue. The oracle checks every captured/native pixel against the inverse
mapping floor(2 + x/2), placing the displayed color boundary at x=6 rather than
the untransformed x=5. Both enlargement cases also verify PNG alpha, shifted
origins, expanded canvas and restoration. An off-diagonal transpose matrix also
passes: source vertical red/blue bands become horizontal bands, checked over all
64 captured/native pixels along with full-canvas replacement and restoration.
The axis swap is distinct from the added quarter-turn enlargement: its inverse
matrix [0, 0.5; -0.5, 0] produces horizontal bands with the asymmetric boundary at
y=6, verified in Debug/Release with replacement alpha, full canvas and restoration.
An additional two-dimensional source makes rows 0..3 blue as well. The oracle
checks floor(6 - x/2) for source Y: that adds blue at output x>=5, independently
of the source-X boundary at output y>=6. All 64 native/captured pixels and the
replacement canvas pass in Debug/Release. The same two-dimensional quarter-turn
fixture also passes with half-strength hardware darkening. Its whole-frame
21x21 shifted replacement oracle checks darkened opaque green, transparent blue
fallback and partial-alpha green/blue, including expanded margins, in Debug and
Release. A double-size affine OBJ also passes: an 8x8 source enlarged into a
16x16 bounding box is checked across all 256 captured/native pixels. The shifted
21x21 PNG whole-frame oracle proves the original 16x16 footprint is removed,
including areas beyond the replacement, with restoration after disabling the
pack. An asymmetric 8bpp double-size source also passes all 256 captured/native
pixel comparisons, with its source column-5 boundary doubled to output column
10, followed by the same replacement and restoration checks. A rotated double-size
case uses inverse matrix [0, 0.25; -0.25, 0] and a two-dimensional asymmetric
source. All 256 capture/native pixels match source X=floor(2+y/4),
Y=floor(6-x/4), with blue boundaries at y=12 and x=9; full-footprint removal,
PNG alpha, shifted canvas and restoration pass in Debug/Release. A horizontal
two-pixel OBJ mosaic case also passes: the captured transformed pattern retains
its x=5 boundary while native displayed pairs move it to x=6, checked separately.
Recognition, PNG alpha, shifted full canvas and native mosaic restoration pass.
The replacement PNG is rendered at its authored resolution; this test does not
establish hardware mosaic processing of replacement artwork. Vertical mosaic and
broader mosaic/effect combinations still need dedicated
live checks.

GBA ordinary unreplaced lower-OBJ reveal is now implemented. While capture is
active, hidden OBJ texels are read into separate per-pixel native records without
changing native OAM writes or arbitration. The full-canvas compositor excludes
matched originals and includes remaining native objects beneath PNG alpha. A
live fixture distinguishes a yellow lower OBJ from the blue backdrop: PNG holes
reveal yellow and partial-alpha green blends with yellow. All thirty-two targeted
cases pass Debug/Release; the six existing Debug capture cases and native
compositor suite also pass. Original overlap fixtures with blue OBJ/backdrop
could not distinguish this behavior. Added live cases verify WIN0 suppresses both
the replacement and lower native object, and hardware darkening applies to native
yellow reveal and partial-alpha green/yellow composition. Native pool tests verify
the hidden records survive retained-frame publication and clear on reuse. A
semitransparent lower OBJ also passes: native yellow blends with the blue backdrop
to hardware-rounded gray; PNG holes reveal gray and partial-alpha green composes
with it. Additional priority-1 ordinary and priority-3 semitransparent lower OBJ
cases pass beneath the priority-0 replacement in Debug/Release. Existing layer
composition explicitly skips other OBJ entries as hardware blend secondary
targets while retaining them as PNG-alpha fallback outcomes. Lower native OBJ
horizontal mosaic now uses each hidden object's pixel from the screen-aligned
mosaic block start, retaining transparent mosaic samples to avoid revealing
unsampled texels. A lower OBJ with alternating yellow/red texels and two-pixel
mosaic passes PNG-hole/partial-alpha reveal and native restoration in Debug and
Release. The native original hole starts a red sample when its preceding winning
OBJ is nonmosaic; the isolated revealed lower OBJ repeats yellow instead.
Vertical two-row mosaic also passes with alternating yellow/red source rows;
transparent and partial-alpha PNG columns reveal the repeated yellow samples
over all sixteen replacement rows, with native restoration in Debug/Release.
An OBJ beginning inside a horizontal block now falls back to its current texel
when its block-start sample is absent, rather than dropping the pixel. A live
patterned lower OBJ beginning at x=9 verifies blue backdrop before its left edge,
yellow at its first pixel and red repeated in a later block through PNG holes
and partial alpha, with native restoration in Debug/Release. Screen-edge clipping
and changing mosaic registers, backgrounds
between differing OBJ priorities and broader alpha-blend combinations
remain to verify; broader cross-system lower-OBJ reveal is unfinished.

Native compositor regression now checks a priority-1 magenta background between
a priority-0 replacement and a yellow hidden OBJ at each priority 0..3. PNG holes
and partial alpha reveal the correct native layer, including OBJ/BG priority ties,
inside the matched source and in expanded canvas margins. Debug/Release native
suites pass. A live GBA fixture also passes in Debug/Release with opaque magenta
BG0 at priority 1 between upper OBJ priority 0 and lower yellow OBJ priority 3.
PNG holes reveal the background, partial alpha mixes green/magenta, and disabling
the pack restores the original sprite/background composition. Additional live
Debug/Release cases verify the lower priority-1 OBJ wins its tie with BG0, and a
white background PNG appears behind the replacement but ahead of priority-3 OBJ.
The background PNG checks exactly address 0: a nonmatching full-byte condition
restores native magenta, a matching condition restores white, and capture contains
exactly that one address. PNG-hole/partial-alpha composition and final disable
restoration pass in both configurations.

GBA mixed-priority metasprites now choose priority, semitransparency and OAM
order per canvas pixel from the nearest matched member rectangle. Inside a
member rectangle its attributes apply; overlapping rectangles and equal-distance
gaps resolve by OBJ priority and OAM order. Original sprite alpha never determines
this choice. A native regression places priority-0 and priority-3 members around
a priority-1 background and checks transparent artwork, partial-alpha artwork,
the gap tie and oversized margins. Debug/Release native suites pass. The existing
32-case live GBA suite also passes in Debug and Release after this change, but
those cases did not establish live mixed-priority group capture. An additional
live two-member fixture now checks captured priority-0/priority-3 members with
overlapping rectangles, priority-1 BG0, a shifted origin, transparent/partial PNG
pixels and oversized margins. Its full-frame oracle checks that the back member
and its margin remain behind BG0 while the overlap inherits priority 0. Debug
and Release pass all 33 cases, including native restoration after disabling the pack.

Mode 7 identity, scaling, quarter-turn rotation and extended BG2 low/high priorities
have native coverage. Same-screen extended BG2/OBJ transitions and out-of-map modes
still need coverage. GBA modes 0/2/3/4/5 have representative tests, including live
Mode 4 page changes; this does not prove every transform/window/mosaic combination.

Desktop file-picker paths and complete visible Tilemap Viewer interaction have
not been verified. Hidden-window routed events and model tests cannot substitute
for those interactions. The user's visible Sprite Viewer feedback covers the
artwork navigation/removal controls and preview-origin click.

Detailed fixtures, implementation changes and historical checks are recorded in
[metasprite-replacement.md](metasprite-replacement.md).
