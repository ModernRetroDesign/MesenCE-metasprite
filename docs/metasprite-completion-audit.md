## Named RAM conditions completed in optimized Release - 2026-10-18

The named-condition feature is complete for the seven supported main CPU domains, including GB/GBC. A clean native Release build, optimized UI and Release workflow harness build pass with zero warnings/errors. Exact optimized-assembly identity and embedded-core extraction checks pass. All eight primary-system workflow runs pass authoring, memory mapping, picker/reference safeguards, save/reload/apply and named group/background activation during GameLoaded without advancing emulation. Optimized audio, standalone text/no-ROM safety and widescreen-profile tests pass. The executable is bin/win-x64/Release/Mesen.exe. Packaging now fails clearly when its built native core is missing; the negative check passes. The harness can select actual Release inputs; its default Debug selection is also retained and verified. See docs/named-ram-conditions.md for artifact hashes, test commands and logs. The named-condition feature is complete; the overall metasprite/background goal remains active with its other system/rendering work.

## Named RAM conditions: shared pickers and previews - 2026-10-18

Shared editor controls now list saved names, insert bracket references and preview resolved numeric checks beside group, alternate, background, camera boundary, text and audio fields. Alias insertion requires a user-supplied value; there is no automatic zero. Refresh/reopen re-reads the library, stale/removed selections fail safely, field limits remain 1/8/64, and SPC/I/O fields disable named insertion. Boundary fields notify both their text box and picker. No RAM is read by these controls. Cross-system picker/authoring/activation workflows pass; audio/native/layout and standalone text regressions pass. A no-ROM text-editor crash was caught and fixed by checking loaded CPU support before requesting a hash; the no-ROM and loaded-NES cases pass afterward. Narrow-panel layout was checked and the audio rules view inspected. Debug builds have zero warnings/errors. Evidence: docs/named-ram-conditions.md and Tests/ViewerLifecycle/named-picker-*.log. Remaining named-condition work: optimized Release build/packaging/workflow verification. Overall goal remains active; current Release unchanged.

## Named RAM conditions: Memory Viewer mapping verified - 2026-10-18

Actual Memory Viewer window/control checks pass for NES, GB/GBC, GBA, PCE, SMS, SNES and WonderSwan. Direct CPU selections preserve addresses; physical RAM selections translate explicitly and round-trip to the same physical byte. Multi-byte, PPU/secondary-domain and negative/out-of-range selections are rejected. Added a cursor-domain bounds guard before translation/capture. Synthetic unsupported selections are detached from ordinary label bindings during testing. Full authoring/background/activation workflows pass in the same runs. Debug UI/harness builds have zero warnings/errors. Evidence: Tests/ViewerLifecycle/named-memory-<system>.log and docs/named-ram-conditions.md. Remaining named-condition work: shared picker/resolved preview controls and final Release verification. The overall goal remains active and the current Release is unchanged.

## Named RAM conditions: camera boundary integration - 2026-10-18

PNG left/right camera boundary fields now retain and resolve bracket references with local overrides. Their numeric caches are transient and invalidated by source edits. Save/load/apply reject missing names and more than eight conditions per edge. Library management includes boundary-only references, including disabled settings, and the boundary section has a Manage conditions button. Numeric packet bytes are unchanged. Managed persistence/cache/limit/packet checks and actual background save/apply plus rename/delete protection pass for NES, GB/GBC, GBA, PCE, SMS, SNES and WonderSwan. Debug UI and harness builds pass without warnings. See docs/named-ram-conditions.md and Tests/ViewerLifecycle/named-boundaries-<system>.log. Remaining named-condition work: shared pickers/previews, targeted Memory Viewer mapping coverage and final Release verification. Goal remains active; current Release unchanged.

## Named RAM conditions: audio integration - 2026-10-18

Music and sound main-CPU write triggers and RAM gates now resolve bracket references from the shared per-game conditions.json library. Source strings persist in music.json; packet formats and event-trigger semantics are unchanged. Main CPU aliases are rejected in SPC/I/O source fields. Library rename/delete/default validation includes saved audio references, and audio definitions using names refresh after library edits. Both audio tabs have Manage conditions buttons. Native/editor audio regressions and seven-system managed packet/domain checks pass; Debug builds have zero warnings/errors. See docs/named-ram-conditions.md and Tests/ViewerLifecycle/named-audio-workflow.log. Remaining named-condition work includes camera boundary fields, shared condition pickers/previews, targeted Memory Viewer mapping coverage and final Release verification. The goal remains active and the current Release is unchanged.

## Named RAM conditions: authoring progress - 2026-10-18

The active feature now includes a Memory Viewer context action and editor for main CPU presets/address aliases, stored in conditions.json beside groups.json. Mapped physical selections translate explicitly to CPU addresses; PPU, secondary-CPU and unmapped selections are disabled. Group, alternate, background and text definitions preserve bracket expressions and resolve on save/load/apply. Missing names fail closed instead of using cached values. Management protects referenced names, stale editors and changed-ROM sessions.

Verified in the Debug build: zero warnings/errors; library/pack/numeric-packet regression suite; actual editor window/model, shared-address aliases, saved references, updated values, rename/delete guards and stale/game-change checks in NES, GB/GBC, GBA, PCE, SMS, SNES and WonderSwan workflow fixtures. Logs: Tests/ViewerLifecycle/named-authoring-<system>.log and Tests/MetaSprites/obj/named-condition-bindings.log. The Memory Viewer address mapping itself still needs targeted workflow coverage. Remaining: music gates, camera boundary fields, shared name pickers/resolved previews and final Release verification. The feature and overall goal remain incomplete; the previous Release is unchanged. This update supersedes the older foundation-only status below.

## Active goal: named RAM condition library - 2026-10-18

User promoted this feature from deferred plans into active work. Implement Memory Viewer right-click creation of per-game named CPU conditions, reusable bracket references such as [DialogBoxOpen], multiple names/values sharing one address, address-only aliases and local operator/value/mask overrides. Integrate the library with metasprite, alternate, background, text and audio condition editors; audio conditions remain gates on their existing write-event triggers. Add selection/resolved previews, safe editing/rename/deletion and persistence/reference validation. Preserve all six operators and the configured-address-only runtime path across the seven systems.

Implementation has started with a source-generated JSON library in conditions.json, guarded ROM/system/CPU-domain identity, bounded definitions, atomic optimistic saves and a shared parser overload. Numeric conditions remain compatible. Named references compile to existing numeric conditions; they introduce no RAM reads or native packet schema changes. Primary mapped CPU domains are the initial supported domain; physical/PPU domains are rejected explicitly until translation is implemented.

Verified: the library/parser, override/persistence/error cases, existing managed regression suite and Debug application build pass. See docs/named-ram-conditions.md for implementation details and the remaining workflow. Still required before the feature is user-ready: retain reference expressions in replacement definitions, resolve them on load/apply, connect editor fields and runtime reload, implement the Memory Viewer dialog/context action and library management, then run cross-system authoring and activation checks. This active goal supersedes the older deferred entry below; the overall replacement goal remains active and incomplete.

## Release pack loading regression fixed - 2026-10-18

Confirmed a shared packaging regression behind cross-system startup/save rejection: the October 18 14:25 Release UI embedded the October 17 22:02:04 native core (8452608 bytes), whose SHA-256 differed from the current adjacent DLL. Startup extraction could install that older core. Direct-core tests did not exercise the embedded dependency resource.

Dependency archives are now isolated by configuration, framework, runtime and optimization mode and generated before resource preparation. Both unbundled and bundled extraction paths are verified against stale or missing home cores; the final optimized Release UI embeds the exact current built core. Pack creation/save/import/reload/startup activation suites pass for NES, GB/GBC, GBA, PCE, SMS, SNES and WonderSwan. Native rejection reasons and JSON identity/version errors now identify their actual cause. Failed activation preserves active definitions; no-ROM rejection and clearing old error messages after success are tested. Existing packs need no migration.

See docs/release-pack-loading.md and Tests/MetaSprites/obj/nes-release-stale-core-evidence.log for the cause, fixes and verification. The Debug/Release native cores, Debug UI, optimized Release UI and viewer harness build successfully; managed builds have zero warnings/errors. The overall goal remains active.

Additional previous-turn verification is complete: both new SNES BG mode 5/6 Bob Blend OBJ-interlace fixtures also pass in Debug, each recording 100 forward frames and matching 40 reverse whole frames with zero errors. Their Release checks already passed. The matrix contains 54 fixtures; the earlier 52-case run and the two added cases remain separate evidence rather than a single 54-case invocation.

## SNES Bob Blend hires rewind coverage - 2026-10-18

Added generated SNES LoROM / BG mode 5 and BG mode 6 Bob Blend OBJ-interlace fixtures to the shared hires matrix. Both include animated conditional alternate artwork and an animated native-layer background replacement; the mode 6 case also exercises color-math operands and windows. Each records 100 forward frames and requires 40 exact reverse whole-frame matches across rewind checkpoints, changing animation frames, three configured RAM bytes, native output restoration on disable, and independently checked compressed-history memory accounting.

Both new Release cases pass with zero reverse-frame errors. Their four compressed display-history chunks measure 225211 bytes (mode 5) and 225792 bytes (mode 6) in these fixtures. Debug checks are running and remain unverified at this point. The matrix now contains 54 fixtures: the earlier 52-case Release run is prior evidence, and these two added cases have separate Release evidence. The complete 54-case matrix has not been rerun as one invocation. The managed harness builds with zero warnings/errors.

Repeat an individual case using Tests/MetaSprites with --snes-bob-blend-rewind-mode5 or --snes-bob-blend-rewind-mode6 and the core DLL path. The existing --snes-bob-blend-rewind selection now includes mode 5, mode 6 and pseudo-hires. Concurrent processes require distinct packet output paths.

Remaining rendering investigation: SuperGrafx capture is explicitly disabled in PceVpc::ProcessScanlineStart when a second VDC exists. PceVpc::ProcessScanline merges two native outputs with window-specific enables and three priority modes. Its eventual replacement integration needs capture and composition at that merge point, both VDC identities, and per-pixel priority/window verification. This path remains guarded. Broader mode/raster, real-game and lifecycle coverage and other outstanding replacement work remain open; the overall goal remains active.

## SNES Weave rewind integration and verification - 2026-10-18

Weave replacement capture is connected to in-memory rewind checkpoints. The sidecar stores both native PPU output buffers, their active buffer identity, completed opposite-field captures, the current partial capture, transient sprite/background capture metadata, and frame-latched rendering flags omitted by the native serializer. Field animation bindings use stable IDs and rebind against the current pack revision without reading RAM or advancing live animation clocks. Stale revisions and malformed bindings are rejected before committing restored state.

Capture data is compressed in bounded 1 MiB chunks and its compressed size is included in the rewind memory budget. Normal save-state file format remains unchanged. Native conditions still capture only configured addresses. Successful restoration suppresses the legacy missing-parity repair so it cannot overwrite the restored woven picture.

Verification completed:
- Native Debug and Release compositor suites pass, including distinct field RAM/animation snapshots, no additional RAM reads or live-clock changes, stale-definition rejection and transactional binding restoration.
- The Release SNES LoROM fixtures for BG mode 5, BG mode 6 with windows/color-math operands, and pseudo-hires BG mode 0 each record 100 forward frames and match 40 reverse whole frames exactly across checkpoints with animated conditional artwork and an animated native-layer background replacement. Debug pseudo-hires also passes 100/40 with zero mismatches. Disabling replacements restores native output.
- Extended Release checks in all three modes, plus Debug pseudo-hires, save inside visible scanline 80 after cycle 64. They explicitly leave interlace mode, load the partial checkpoint, compare all captured bytes, and reproduce the same complete output frame. Truncated/trailing snapshots leave capture state unchanged. Live PPU restoration rejects a checkpoint after a pack revision change. All three extended Release cases also pass the full 100/40 reverse-playback check after the partial-field restoration.
- The complete 52-case Release hires/interlace matrix passes, including Current Field, Bob and Bob Blend reverse playback. The SnesObj Sprite Viewer authoring/PNG/camera/animation suite passes. Debug/Release UI and managed harness builds have zero warnings/errors. Editor capability text includes Weave rewind.
- Packaged Debug and Release Dependencies/MesenCore.dll copies match their built cores by SHA-256. Release Mesen.exe is present. No verification processes remain running.

The targeted Release pseudo-hires checkpoint measured 126 compressed chunks totaling 4858138 bytes; this is a fixture measurement, not a general size guarantee. The internal sidecar is not a portable save-state format. Wider raster/mode transitions, real-game coverage, run-ahead/presentation combinations and other system-specific replacement work remain open. Earlier preparation-only notes below describe historical state. The overall cross-system replacement goal remains active and incomplete; the named RAM condition library remains deferred.

Repeat targeted checks with Tests/MetaSprites using --snes-weave-rewind (three full reverse-playback cases), --snes-weave-rewind-pseudo (one full case), --snes-weave-checkpoint (three partial/mode/revision cases), or --snes-weave-checkpoint-pseudo (one partial case), followed by the core DLL path. Use a distinct packet output path for concurrent managed processes.

## SNES Weave rewind preparation: captured-field snapshots - 2026-10-18

Added a bounded, process-local snapshot codec for logical SNES captured fields in Core/Shared/Video/MetaSnesFieldCheckpoint.h. Unlike Bob Blend's previous image, Weave needs fetched sprite identities, relative rows, coverage/native objects, BG main/sub samples, priority/windows, source coordinates, configured RAM observations and field-specific animation timing. The codec retains those capture payloads, optional text/underlay planes and numeric definition-ID/elapsed-time pairs. It never copies container pointers, PNG resource pointers or animation-owner addresses into the payload. Animation owners must be reattached through the matching pack definitions by the future checkpoint integration.

The representation is internal to the current process/native ABI, not a portable pack or ordinary save-state file format. It checks its layout stamp, raw 256x239 shape, collection lengths, row sampling, field identities and animation binding uniqueness; total payload is capped at 256 MiB. It rejects expanded/already woven or mixed-system frames. Encoding/decoding only replace their output after success. RAM data is the frame's existing watched-address snapshot, not a full memory dump. Future storage must wrap these trusted internal POD payloads in verified compressed checkpoint chunks before restoration.

Debug and Release native compositor suites pass with strict warning-as-error builds. New tests cover pixel-state preservation, fetched rows including OBJ-interlace sampling, native coverage, background/subscreen data, optional text/underlays, watched RAM, numeric timing bindings, truncated/trailing/layout-invalid input, duplicate RAM addresses/bindings, incompatible frame types and transactional failures. Recognition/composition before and after a round trip produces identical complete output. Explicit opaque PNG checks cover both an expanded margin and an originally transparent source sprite pixel.

This prepares Weave rewind; it does not enable it. Remaining integration: capture the completed opposite field and native woven buffer at checkpoints, compress/store/account for their data, validate the pack/presentation identity, rebind field animation clocks, restore PPU field continuity and verify whole-frame reverse output with conditional animated PNGs and backgrounds. Initial/partial checkpoints and mode transitions also need explicit handling. Weave remains guarded; existing Bob Blend support is unchanged. The overall replacement goal remains active.

Evidence: Tests/MetaSprites/obj/weave-field-checkpoint-native-{debug,release}.log and build-weave-field-checkpoint-native-{debug,release}.log. Only native unit harnesses are rebuilt for this dormant codec; no application runtime behavior, UI or packaged executable is changed in this step.
## Future goal: named RAM condition library - deferred, 2026-10-18

User-approved future work; not implemented or enabled yet. Add a Memory Viewer right-click action on an address value to save a named, reusable condition or address alias. Names appear in square brackets in condition boxes and are stored with the game's pack for the relevant system/CPU and memory domain.

- [ ] Save complete condition presets with a name, address, comparison operator, value and mask. Example: [DialogBoxOpen] resolves to $DE == $02 / $FF.
- [ ] Allow multiple named presets for the same address. For example, [Town1] and [Town3] can both watch $DE while comparing against $01 and $03 respectively. Names must be unique within their library; addresses need not be unique.
- [ ] Support address-only aliases and partially specified presets with blank/default operator, value or mask fields. Example: [LocationName] == $04 / $FF uses the alias's saved address and supplies the comparison locally. A bare alias with missing required comparison data must show a validation error rather than guess a value.
- [ ] Permit local overrides of saved comparison fields without modifying the shared preset. Explicit fields in the condition box take precedence over saved fields; omitted fields inherit saved defaults. Continue supporting ==, !=, >, <, >= and <=, hexadecimal values and masks.
- [ ] Add selection/autocomplete and a resolved numeric preview in shared condition editors, including metasprite alternates, PNG backgrounds, text profiles and audio condition gates where applicable. Audio aliases remain gates on the existing trigger/write event; using an alias must not change trigger semantics.
- [ ] Provide management for names, addresses, defaults, renaming and deletion, with clear handling of existing references. Preserve old numeric-only conditions and pack compatibility. Unresolved/deleted names, invalid values and unsupported memory domains must fail closed with a useful editor error.
- [ ] Preserve CPU/memory-domain identity from the selected Memory Viewer context. Validate or explicitly translate addresses for condition evaluation; never silently reinterpret a physical-memory address as a CPU-bus address. Offer this workflow across NES, GB/GBC, GBA, PCE, SMS, SNES and WonderSwan.
- [ ] Resolve presets into the existing bounded condition evaluation: capture only referenced, user-defined addresses, deduplicate repeated addresses, and retain distinct names/values without full memory dumps. Test persistence, shared-address presets, inheritance/overrides, missing references and cross-system identity.

This extends the existing condition workflow in the style of named HDnes conditions. It remains deferred future work and does not replace or complete the active replacement goal.
## SNES Bob Blend replacement rewind - 2026-10-18

Enabled Bob Blend replacement playback during rewind with the default, compatible SNES filter. Rewind checkpoints now retain the previous unblended composed image as compressed display-history chunks. Each decoded chunk is bounded to 1 MiB; its compressed bytes are included in RewindData::GetStateSize and therefore the existing rewind memory budget. History restoration waits for decoder synchronization, filter selection and presentation dimensions. Filter identity, replacement-definition revision, image size and consecutive frame numbering prevent incompatible history reuse. Pack updates reset temporal history. Other systems keep their existing rewind blending behavior. The ordinary save-state file format and configured-address-only RAM condition capture are unchanged.

Fixed native interlaced checkpoint presentation as well: only Weave uses the missing-parity repair. Bob/Bob Blend already copy to the following row; repairing by parity shifted odd Bob rows. Current Field explicitly clears inactive rows after checkpoint restoration. Its regular clear now runs on the new drawing buffer after the scanline-zero swap, avoiding modification of the published buffer while the asynchronous decoder reads it in vblank.

Verification: the generated SNES LoROM / pseudo-hires BG mode 0 / OBJ-interlace Bob Blend fixture passes in Debug and Release with an animated conditional alternate metasprite, animated native-layer BG0 replacement, transparent/partial-alpha PNGs and exactly three watched RAM bytes. Both record 100 forward frames and match at least 40 complete reverse frames across checkpoints, with zero errors; recorded frames must actually vary. The test verifies background visibility on a main-screen column and active animation clocks. Four compressed display chunks total 141405 bytes in this fixture, and the test independently recomputes their exact contribution to checkpoint memory accounting. This is a fixture measurement, not a general compression-size promise.

The six targeted Release cases pass, including Current Field and Bob exact reverse output and disable/native restoration. The complete 52-case Release SNES hires/interlace matrix passes after the native buffer fix. The hidden SnesObj Sprite Viewer PNG/camera/animation authoring suite passes. Managed harness, Debug/Release native cores and UI builds succeed; UI/harness builds have zero warnings/errors. UI capability text now leaves only Weave rewind pending. Packaged native core copies match built cores and Release Mesen.exe is present.

Evidence: Tests/MetaSprites/obj/bob-blend-rewind-target-debug.log, bob-blend-rewind-release.log, bob-blend-rewind-matrix-release.log, bob-blend-rewind-editor.log and build-bob-blend-rewind-*.log. Debug Bob Blend verification preceded the final Current-Field-only buffer clear move; its tested Bob Blend path is unchanged by that move, and the final Debug core is rebuilt. Full Release presentation verification includes that move.

Remaining: Weave interlaced rewind capture continuity; broader game/mode, raster, camera, RAM-transition, save-state and run-ahead validation. Dedicated Bob Blend reverse fixtures for modes 5/6 remain broader validation work; their forward presentation is covered by the matrix. The remaining text and cross-system screen-edge continuity work retains its prior status. The overall replacement goal remains active.
## SNES Bob Blend rewind preparation: owned frame history - 2026-10-18

Added BlendFilter::History with an owned copy of the unblended previous frame and its frame number. CaptureHistory returns no data for disabled/invalid history; RestoreHistory clears stale history before accepting a valid enabled snapshot. Restoring does not retain a pointer to caller or decoder storage. Frame-size and frame-number continuity checks still apply on playback. Temporary SetEnabled flag changes retain their existing semantics because BaseVideoFilter reselects that flag every frame.

Verified in Debug and Release native compositor suites: exact next-frame average after repeated restoration, unblended versus displayed-average input, alpha channels, independent snapshot/restored storage, empty/disabled history, size changes, discontinuities, frame-counter wrap and transient per-frame flag selection. The six live SNES hires OBJ-interlace Release cases pass, including forward Bob Blend and 40+ exact reverse Current Field/Bob matches. Native cores and Debug/Release UI builds pass; packaged core copies match and Release Mesen.exe is present.

This is checkpoint infrastructure, not enabled Bob Blend rewind. Remaining: connect history to rewind checkpoint ownership/storage, account for retained data in the rewind memory budget, restore it after decoder synchronization and filter selection, and verify complete Bob Blend reverse output across multiple checkpoints with PNG backgrounds, conditional artwork and animation. Weave additionally needs opposite-field capture continuity. Both pending modes remain guarded. No CPU memory dumps are added, and normal save-state format is unchanged. Overall goal remains active.

Evidence: Tests/MetaSprites/obj/blend-checkpoint-native-{debug,release}.log, blend-checkpoint-live-release.log and build-blend-checkpoint-*.log. Implementation: Core/Shared/Video/BlendFilter.h; tests: Tests/MetaSprites/NativeCompositor.cpp.
## SNES Current Field and Bob replacement rewind - 2026-10-18

Enabled captured interlaced PNG replacement output during rewind for Current Field and Bob. These modes map the field just rendered and do not require the missing opposite-field history. Weave and Bob Blend rewind remain guarded and unfinished.

Fixed a checkpoint presentation error: the native woven rewind repair copied the current parity into rows that Current Field intentionally leaves black. Current Field now skips that repair. This preserves its normal presentation at checkpoint reload as well as replacement PNG alpha and full source-row sampling.

Verified actual rewind playback against recorded forward output in generated SNES LoROM fixtures: BG mode 5 Current Field and BG mode 6 Bob, with hires OBJ interlace, main/subscreen color math, oversized 2x PNG artwork and transparent/partial-alpha pixels. Each records 100 forward frames and compares at least 40 reverse whole frames across rewind checkpoints. Debug: 40 matches per mode, zero errors. Release: 40 Current Field and 41 Bob matches, zero errors. Recording uses lossless run-length encoding to limit test memory use; comparison retains every pixel. These tests do not establish all-game or background-camera/animation rewind coverage.

All six targeted hires OBJ-interlace cases pass in each configuration, including forward Weave/Bob Blend checks and native restoration. Both native compositor suites and the hidden SnesObj Sprite Viewer PNG/camera/animation authoring suite pass. Debug/Release UI and viewer harness builds have zero warnings/errors. UI capability text distinguishes supported Current Field/Bob rewind from pending Weave/Bob Blend rewind. Packaged core DLLs match built cores and Release Mesen.exe is present.

Evidence: Tests/MetaSprites/obj/snes-field-rewind-{debug,release,native-debug,native-release,editor}.log and build-snes-field-rewind-*.log. The earlier 52-case forward matrix remains prior evidence; this change reruns six targeted cases per configuration. Broader lifecycle/hardware/game validation, remaining text work and cross-system screen-edge continuity keep their prior status. The overall replacement goal remains active.
## SNES hires OBJ-interlace PNG color math - 2026-10-18

Implemented full source-row sampling for replacement PNGs in hires OBJ-interlace color-math output. Both field parities use their captured source geometry; nearest-member priority/palette selection follows full object rows. Missing geometry retains native output. Oversized PNG areas and their own alpha remain independent of original sprite transparency. High-resolution artwork retains vertical detail consistently with color math enabled or disabled.

Verified both parities, woven/current/bob/bob-blend output, mixed ordinary/hires fields, field-specific artwork and RAM conditions, native windows, main/subscreen math and transparent/partial-alpha PNGs. The full generated SNES LoROM hires/interlace matrix passes 52 cases in each of Debug and Release. Native compositor suites pass in both configurations, including multi-member priority and malformed-row fallback checks. The hidden SnesObj Sprite Viewer suite passes PNG, camera, animation and authoring regressions. Debug/Release UI builds succeed with zero warnings/errors.

Evidence: Tests/MetaSprites/obj/snes-hires-obj-matrix-{debug,release}.log, snes-hires-obj-native-{debug,release}.log, snes-hires-obj-editor.log and build-snes-hires-obj-*.log. Release Mesen.exe is present; packaged native core DLLs match the built cores.

Interlaced rewind remains guarded and unfinished. SuperGrafx and broader hardware/game validation remain open. This renderer work does not implement the remaining SNES text-recognition modes listed in the text goal checklist. The overall replacement goal remains active.
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
## Optional word-aware font wrapping - 2026-10-18

Implemented the per-profile Prefer word boundaries when wrapping option for cached text overlays and the editor preview. It applies when a display region and wrapping are enabled. New editor profiles enable it; existing JSON profiles and older runtime packets retain character wrapping until the option is selected. Mapped breaking spaces delimit words. Standard and narrow nonbreaking spaces remain part of a word; a word wider than the region falls back to character wrapping. Trailing spaces at an automatic break are suppressed without adding an empty line before an explicit source row break. Source rows remain hard line breaks.

Word widths use cached glyph advances, internal kerning and letter spacing. Reverse suffix accumulation makes lookahead linear in the matched text length, including long unbroken strings. Glyphs/effects still clip to the configured display region and screen; font rasterization stays at activation. Both 6x8 and 8x8 source grids use the shared native layout. The preview follows the same rules using its cached font metrics. The wrapping control updates when the main wrapping toggle changes.

MXT6 carries the option flag and breakable-space glyph IDs. Native readers retain MXT1-MXT5 compatibility, reject invalid separator IDs and preserve older source-grid semantics. Word-wrap settings round-trip through pack saving and independent editor merges.

Verified: the full native compositor suite covers whole-word movement, character fallback, internal kerning, letter spacing, nonbreaking spaces, trailing-space/source-row handling and legacy character wrapping on both source grids. Actual-font preview and checkbox/persistence tests pass. Two new NES mapper-0 cases compare complete real-font gameplay frames against independently specified positions, including effects, alpha/fill, fine source scrolling, RAM/state restoration and paused snapshots. All 26 NES plus 100 cross-system gameplay cases pass in both Debug and Release. Managed packet/persistence tests and legacy MXT4/MXT5 transport checks pass. Final Debug/Release UI and harness builds have zero warnings/errors. Release Mesen.exe is present, and packaged native copies match the built DLLs.

- [x] Optional word-aware wrapping for current cached text-overlay adapters and preview.
- [ ] Ligatures, RTL/complex-script shaping, broader language-specific line breaking/hyphenation and the other outstanding text recognition/system tasks remain future work. This does not complete the overall replacement goal.

Evidence: Tests/MetaSprites/obj/text-words-{native,managed,editor,nes-debug,nes-release,systems-debug,systems-release}.log and build-text-words-*.log. Current workflow and support matrix: docs/text-replacement.md.

## SMS 40-column text and native source grids - 2026-10-18

Implemented SMS 40-column text-mode gameplay capture with genuine 6x8 source tiles. Fetched identities, graphics addresses and foreground/background ink follow the native shifters; the eight-pixel side borders carry no text identity. Profiles persist SourceTileWidth (6 or 8), with old packs defaulting to 8. The native matcher checks the captured tile width, supports complete 6x8/12x8/6x16/12x16 patterns and larger grids, and preserves per-character origins and proportional adjacency. Source cells still use eight-pixel rows; the maximum six-pixel-grid cell is 126x128 with 336 component tiles.

The Tilemap Viewer detects the native width, captures each source tile at its actual column spacing and seeds new profiles correctly. The source selector supports separate X/Y snapping and keyboard steps. Selection reuse, numeric cell widths/origins and full-pattern capture follow the selected grid. Font size, display-region coordinates and outline/shadow bounds remain independent. Existing profiles are not silently converted when a game changes mode; use separate 6x8 and 8x8 profiles. Mixed raster-mode transitions remain broader validation work.

MXT5 transports the source tile width. Native readers retain MXT1-MXT4 compatibility; managed/native tests cover old JSON defaults, MXT4 patterns, malformed widths, maximum grids and incompatible source-width rejection. No new per-frame glyph rasterization or enlarged text-pixel allocation is introduced.

Verified: 100 cross-system actual-font gameplay cases in Debug and Release, including 16 new SMS text-mode cases for 6x8/12x8/6x16/12x16 source cells, index/address matching, inherited/custom ink, effects/fill/display regions, RAM gating and first-frame RAM/tilemap-state restoration. All 24 NES gameplay regressions also pass in both configurations. Actual 40-column Tilemap Viewer opening/detection, 12x16 capture, Save and apply, failed-asset retention/retry and disabling profiles pass against Debug and Release cores. Editor selector/capture tests, maximum 336-tile capture, the managed suite and full native compositor suite pass. Debug/Release UI and harness builds have zero warnings/errors.

- [x] SMS 40-column text-mode capture, source authoring, matching grids and pack transport in the verified fixture scopes.
- [ ] Broader raster transitions, same-index bank/content guards, dynamic palettes, rewind/run-ahead, advanced layout and real-game validation remain unfinished. Other guarded system modes and cross-system screen-edge continuity retain their prior status. The overall replacement goal remains active.

Evidence: Tests/MetaSprites/obj/text-six-{debug,release,nes-debug,nes-release,editor,apply,managed,native}.log and build-text-six-*.log. Release Mesen.exe is present, with packaged native DLL copies matching the built cores. See docs/text-replacement.md for the current workflow and support matrix.

## SMS legacy graphics text overlays - 2026-10-18

Implemented gameplay text capture for legacy SMS Graphics I (mode 0), Graphics II (mode 2) and Multicolor (mode 3). Capture follows the native fetch/shifter pipeline, including transparent/background-colored texels and fetched row colors. Graphics II exposes the expanded banked tile index and masked graphics address, matching the Tilemap Viewer. Multicolor exposes its row-dependent graphics address; its conventional tile index is unavailable. The editor now retains address-only source cells and rejects unavailable index mappings with a clear message. Native single/multi-tile matching also rejects unavailable identity sentinels.

Legacy ink 1 inherits the fetched foreground/left-block color, ink 2 the background/right-block color, and ink 3 aliases ink 1. Color zero resolves through the native backdrop at output time. Scanline resets clear optional text samples. No glyph rasterization is added to gameplay frames.

Verified: 84 cross-system actual-font gameplay cases pass in Debug and Release, including 36 new legacy SMS cases. These cover 8x8/8x16 source cells, Graphics I/II index/address matching, all three Graphics II bank regions, Multicolor address matching, inherited/custom ink, effects/fill/display regions, RAM gating and first-frame RAM/tilemap-state restoration. Each legacy graphics mode also passes actual editor Save and apply, failed-asset retention/retry and disabling profiles. Address-only authoring, the full native compositor suite and 24 NES gameplay regressions pass. Debug/Release UI builds and the harness build have zero warnings/errors. Packaged native core copies match the built DLLs; Release Mesen.exe is present.

- [x] SMS legacy Graphics I, Graphics II and Multicolor text overlays in the verified fixture scopes.
- [ ] SMS 40-column text mode requires proper 6x8 source-cell authoring, selection and matching-grid support; it remains guarded. Broader raster mode transitions and real-game coverage remain open. This does not complete the overall replacement goal.

Evidence: Tests/MetaSprites/obj/text-sms-legacy-{debug,release,editor,apply,native,nes-regression}.log and build-text-sms-legacy-*.log. Current support and workflow: docs/text-replacement.md.

## Text source-identity save-state checks - 2026-10-18

Extended the live text suites beyond RAM toggling: mutate actual native tilemap indices while keeping the configured RAM trigger enabled, verify the overlay disappears, save the changed tilemap, and restore original/changed/original states without reapplying the pack. Every restore compares the first captured gameplay frame against the complete expected overlay/native image and output scale. Tilemap palette/flip/priority bits are preserved; the WonderSwan fixture's shared-RAM trigger byte is excluded from tilemap mutation. Different tile indices may contain identical blank graphics, so native artwork need not visibly change for the identity test to be meaningful.

All 24 NES mapper-0 and 48 other-system cases pass in Debug and Release, using the supported regular tile modes described in the current matrix. This verifies tilemap source-identity restoration, including index/address mappings and multi-tile cells. No production runtime change was needed.

- [x] Verify source identity and first captured frame after loading different native tilemap states across all seven systems in the generated supported-mode fixtures.
- [ ] Verify graphics/bank changes that reuse the same tile IDs, dynamic palettes/raster changes, states saved before profile activation, rewind, run-ahead and real-game sessions. Content/bank safeguards and unsupported-mode adapters remain unfinished.

Evidence: Tests/MetaSprites/obj/text-content-state-{debug,release}.log (48 cases/configuration), text-content-state-nes-{debug,release}.log (24 cases/configuration), build-text-content-state-{debug,release}.log and build-text-content-state-harness.log. Final native builds and the managed harness succeed; the final harness build has zero warnings/errors. See RunMetaTextSystemTest and RunMetaTextNesTest in InteropDLL/TestApiWrapper.cpp. This extends the preceding RAM-trigger-only state evidence; it does not complete the broader state/rewind goal.

## Text overlay save-state restoration checks - 2026-10-18

Verified first-captured-gameplay-frame restoration of active and inactive RAM-triggered text in all seven systems: 24 NES mapper-0 cases plus 48 GB/GBC, SMS Mode 4, single-VDC PCE, GBA regular tile BG, normal-resolution SNES BG1-BG4 and WonderSwan BG/FG cases. Both Debug and Release suites pass. Each case saves both RAM-trigger states, changes the live state, then loads the saved states without reapplying profiles or rewriting RAM. The first captured frame must equal the complete expected active overlay or inactive native image, including the correct output scale. This adds lifecycle evidence; no runtime fix was needed for these fixtures.

- [x] Validate first-frame RAM-trigger restoration from save states in the generated supported-mode text fixtures, in Debug and Release.
- [ ] Validate restoration across different scenes, tile banks/content, palettes and raster configurations, including states saved before profile activation.
- [ ] Validate text during rewind, run-ahead and broader presentation changes and real-game sessions. These remain part of the unfinished text goal.

Evidence: Tests/MetaSprites/obj/text-state-live-{debug,release}.log (48 gameplay cases per configuration), text-state-nes-{debug,release}.log (24 gameplay cases per configuration), build-text-state-core-{debug,release}.log and build-text-state-harness.log. The harness builds with zero warnings/errors. Tests also retain the existing activation, RAM toggling, snapshots and editor checks. See InteropDLL/TestApiWrapper.cpp, RunMetaTextSystemTest and RunMetaTextNesTest.

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

## Cross-system TrueType gameplay adapters - 2026-10-17

Initial cached-font gameplay adapters now cover GB/GBC background/window, GBA regular tile BG0-BG3, PCE VDC1 background, SMS Mode 4, normal-resolution SNES tile BG1-BG4/main and subscreen, and WonderSwan background/foreground with native windows. NES support remains available. Text capture is optional per selected layer and records fetched tile identities, graphics addresses, palette ink and source coordinates, including transparent source texels. Cached glyph atlases are shared rather than copied per frame. The overlay supports multiple text planes and different raster scales. Save and apply is enabled across all seven systems. SNES source capture handles 16x16 hardware tile quadrants/mirroring and mode-0 palette numbering. Native PCE preview dimensions/crop are corrected.

Verified: 48 generated actual-font full-frame gameplay cases in both Debug and Release, covering DMG/CGB/window, SMS, PCE, GBA, all four SNES layers and WS BG/FG windows; 8x8/8x16 cells; index/address patterns; fill/effects/display regions; RAM off/on/clear; and requested native-coordinate snapshots. Each system's actual editor Save and apply/failed-asset retention/retry/disabled clearing is checked in a separate process. Managed transport, authoring, NES gameplay regressions and the full native compositor suite pass, including mixed-scale independent text planes and inherited ink. Debug/Release UI builds pass without warnings/errors.

Guarded/future text work: GBA affine/bitmap and mosaic; SMS legacy graphics/text modes; SNES Mode 7, hires/interlace and mosaic; SuperGrafx, SGB/link variants and incompatible filters; broader bank/content/raster guards and real-game/state/rewind validation; palette ink entries beyond 1-3; complex-script shaping/ligatures/word-aware wrapping and sprite text. Cross-system screen-edge continuity remains deferred. See docs/text-replacement.md for the current support matrix and workflow. This supersedes older authoring-only statuses for these systems; the overall replacement goal remains unfinished.
## Multi-tile TrueType source cells - 2026-10-17

Implemented source-character cells in multiples of 8 pixels, from 8x8 through 128x128, with tile-aligned X/Y grid origins. Mappings store the complete row-major tile grid and emit one character only when every component matches. Shared top tiles are disambiguated by the remaining tiles. Exact palettes must match every component; wildcard patterns permit mixed palettes and inherit anchor ink. Index and graphics-address matching are supported. Existing single-tile pack mappings remain compatible; MXT4 transports patterns/origins and the native reader retains MXT1/MXT2/MXT3 support.

The text editor can take a character cell's size/origin from the cyan selection and capture all tile identities from its frozen tilemap snapshot, or capture at the original Tilemap Viewer tile. Incomplete captures fail without changing mappings. Source cells and font display size remain independent. Use separate profiles for different character sizes; select the full dialogue region or disable the source-region limit after capturing individual letters. Gameplay remains NES-only; other system text adapters remain future work. Missing/offscreen component tiles are not inferred; cross-system screen-edge continuity is still deferred.

Verified: managed pattern parsing/persistence/transport, native complete-pattern matching and malformed input handling, selected-cell capture, shared-top disambiguation, grid phase and palette checks, graphics-address patterns, arbitrary 24x16 cells and region completeness. All 24 generated NES mapper-0 actual-font gameplay cases pass in Debug and Release, including 8x16/16x8/16x16 cells, fine scrolling, fill, RAM restoration and paused/widened/rotated snapshot comparison. Full native compositor regression suite passes. Debug/Release UI builds have no warnings or errors.
## Visual text capture and display selectors - 2026-10-17

Implemented in the TrueType text editor: cyan tile-snapped source selection from a captured hardware tilemap, reuse of Tilemap Viewer's selected rectangle, and an orange gameplay display rectangle with a cached sample-text overlay. Both support drawing, moving, edge/corner resizing, synchronized numeric fields, arrow-key nudging, Ctrl+arrow resizing and Shift larger steps. Display selection uses native pixels; source selection uses 8x8 tiles. Match source size / position copies the source rectangle. Preview layout refresh is debounced during display-region edits.

Gameplay snapshots are requested on demand, work while paused, preserve native gameplay coordinates through decorative widescreen and rotation, and represent cropped overscan with a dark border. No screenshot files or continuous frame-copy loop are required. Source preview captures the layer selected on editor opening; refresh/reopen after changing that layer. Editors opened before a tilemap refresh remain usable with an explicit unavailable-preview message. The sample text preview does not decode dialogue from the snapshot.

Verified: visual-selector pointer and keyboard tests, bidirectional numeric bindings, reversed/zoomed drags and tile snapping; managed pack tests; all 18 live NES mapper-0 TrueType cases with requested paused/widescreen/rotation snapshot comparison in Debug and Release. Other systems retain text authoring/preview support; their gameplay text adapters remain unfinished.
## Cached font overlays, proportional layout and effects - 2026-10-17

At the user's request, text is now a cached overlay over the completed native/
PNG image. It leaves original tiles intact. Optional alpha-capable fill covers
the configured display region, or matched source cells without a display region.
Replacement PNGs can cover old native text with fill left disabled. Font activation
no longer forces full-screen native BG/OBJ reconstruction, and unmatched font
profiles do not raise the output scale. Finished pixels are simply expanded when
matched glyph detail requires it; blending is restricted to glyphs/fill.

The editor adds proportional left-to-right text with real font advances and
HarfBuzz-derived pair kerning; new profiles enable this, existing profiles retain
their saved setting. Font size can exceed the 8x8 NES source-cell dimensions.
Cached glyph bounds include overhang and effects, clipping at the gameplay/display
region rather than the original tile. Display-region X/Y/width/height are separate
from source tilemap matching regions. Source rows become lines, font metrics or
a custom line height determine spacing, and optional character wrapping stays
inside the display region. Letter spacing can be adjusted independently.

Outline width/color and drop-shadow X/Y/color are independent controls, accepting
RGBA colors and quarter-pixel adjustments. Coverage and effect layers are cached
at activation; no font rasterization or shaping occurs on a gameplay frame.
The preview uses the same glyphs/metrics/kerning/effect resources. Actual Arial
A/V kerning, larger glyph bounds and separate red/blue effect colors are checked;
a generated preview was visually inspected. MXT3 adds bounds, advances, layout,
kerning and effect resources while retaining MXT1/MXT2 parsing. Each rendered
frame retains one immutable sprite/background/text definition snapshot.

Eighteen actual-font NES mapper-0 cases pass in Debug and Release, covering
static/fine scroll, tile/address and palette selection, fill off/on, native-image
retention, RAM reactivation/restoration, and larger proportional fonts moved to
a display region with colored outline/shadow. Native tests verify clipping,
kerning placement, glyph/effect overhang, wrapping, region fill, PNG layering,
crops, missing RAM, no-match scaling bypass and legacy packets. The complete
Release native suite and existing NES/SNES PNG authoring regressions pass.
Debug/Release UI builds have zero warnings/errors. Release Mesen.exe is present
with its matching packaged native core.

A matching synthetic Release CPU benchmark (32 letters, eight native sprites)
measures 4x fixed-cell overlay at 0.59 ms/frame and proportional overlay at
0.58 ms/frame, compared with the prior tile-reconstruction path's 18.66 ms.
Unmatched profiles cost 0.14-0.15 ms and leave output at 1x. These are compositor
measurements, not a complete game FPS guarantee; background PNG composition
and capture retain their own costs.

Evidence: Tests/MetaSprites/obj/text-layout-managed.log,
text-layout-native-{debug,release}.log, text-layout-live-{debug,release}.log,
text-layout-viewer-debug.log, text-layout-{nes,snes}-authoring.log,
text-layout-profile[-proportional]-release.log and build-text-layout-*.log.
The earlier text tile-replacement behavior/performance proposal is superseded.

NES gameplay remains initial coverage. Other system adapters, bank/content
guards, broader dynamic palette/raster coverage, state/rewind/presentation,
word-aware wrapping, complex-script/RTL/ligature shaping, multi-tile recognition
and sprite text remain unfinished. The overall replacement goal stays active.
Current workflow and scope: docs/text-replacement.md.
## Initial NES TrueType gameplay replacement - 2026-10-17

NES now captures optional fetched tile identities, logical graphics addresses,
attribute palettes, local coordinates and output-time ink colors when text
profiles are installed. Text definitions join the existing configured-address
RAM snapshot; no full RAM or decoder-thread VRAM reads are introduced.

TTF/OTF glyph coverage is rasterized once at activation, shared by repeated
characters within each profile, budgeted to 64 MiB and published immutably.
MXT1 packets support bounded file/array parsing. The new three-file activation
parses sprites, backgrounds and text before committing them together under the
emulator lock, rechecking console/ROM compatibility. Invalid files or glyphs
leave previously installed definitions intact.

NES composition removes matched native text across 8x8 cells, preserves native
sprite order and background clipping, and handles inherited palette colors or
custom alpha. Glyph transparency reveals native backdrop/behind sprites.
Native-slot background PNGs draw beneath text, even with native-layer hiding;
band-6/final overlays remain above it. The editor now offers Save and apply for
NES. Other systems remain authoring/preview only and reject active gameplay
text definitions rather than silently dropping them.

Eight generated mapper-0 actual-font cases pass in Debug and Release: tile
indices/graphics addresses, exact palette overrides, inherited/custom partial
alpha colors, static/fine X/Y scrolling, two configured RAM bytes, reactivation
and whole-frame disable restoration. Native Debug/Release suites and focused
PNG/hide-layer interaction checks pass. Actual editor activation verifies
three-file handoff, malformed/wrong-ROM retention, missing-glyph retry and
all-disabled clearing. Existing NES/SNES authoring regressions pass. Managed
transport tests, editor glyph-sharing/budget tests and zero-warning Debug/
Release UI builds pass. Release Mesen.exe and the matching core are present.

Evidence: Tests/MetaSprites/obj/text-runtime-managed.log,
text-runtime-native-{debug,release}.log, text-runtime-png-{debug,release}.log,
text-runtime-live-{debug,release}.log, text-runtime-viewer-debug.log,
text-runtime-{nes,snes}-authoring.log and build-text-runtime-*.log.

Remaining: other system adapters, bank/content disambiguation, dynamic palette/
raster combinations, broader mapper coverage, text state/rewind/run-ahead and
presentation checks, multi-tile characters and proportional/sprite text. The
overall goal remains incomplete. Workflow and scope: docs/text-replacement.md.
## TrueType text authoring started - 2026-10-17

TrueType text replacement is now active work, superseding its earlier deferred
status. The Tilemap Viewer opens a dedicated TrueType text replacement editor
for hardware background maps. It supports named profiles, imported TTF/OTF
fonts, tile-index or graphics-address mappings, Unicode characters, exact or
wildcard palettes, native cell/font sizes, raster scale, offsets, inherited or
custom RRGGBBAA colors, tilemap regions and existing RAM comparison conditions.
A captured viewer tile can seed a character mapping. Actual font rasterization
provides a tile-aligned preview. Native palette preview currently uses white.

Profiles persist in the existing ROM pack as TextProfiles. Legacy packs still
load. Saves merge unrelated pack edits, preserve text profiles through sprite
and background saves, reject stale game/editor contexts, and retain prior files
on save failure. Font files already at the pack root are reused; external font
name collisions use short numeric suffixes. Unsaved drafts remain isolated.

Managed mapping/Unicode/path/condition/persistence checks and actual hidden
Avalonia window/font/import/save tests pass. The existing SnesObj authoring
regression passes with independently saved text profiles present, including
sprite/background save preservation. It also verifies new animation frames'
eight-frame default. Debug/Release UI builds pass with zero warnings/errors.
Evidence: Tests/MetaSprites/obj/text-authoring-managed.log,
text-authoring-viewer.log, text-authoring-sprite-background-regression.log and
build-text-authoring-ui-{debug,release}.log.

This increment does not replace text in gameplay on any system. Next: reusable
glyph resources, fetched tile/bank/content capture, RAM-gated native-layer
composition and a generated NES mapper-0 dialog fixture, then the remaining
system adapters. Priority/clipping, native text removal, palette inheritance,
state/rewind/presentation and memory budgets require runtime verification.
See docs/text-replacement.md for the workflow, format and implementation stages.
The overall replacement goal remains incomplete.
## SNES hires interlaced field color math - 2026-10-17

Hires math now retains independent logical sources for woven even/odd fields.
The compositor selects the matching field, preserves its OAM identity space,
artwork/RAM snapshot and background gates, and maps current-field/bob rows
through their real source Y (including bob's first-picture-row adjustment).
An ordinary field can share a weave with a hires field. Retired output frames
release both source references while published captures keep them alive.

Native Debug/Release suites pass independent per-field alternate artwork/RAM,
background RAM activation, mixed field resolution, current/bob placement and
dual-source retirement checks. Six new live LoROM fixtures exercise mode 5/6
and pseudo-hires math through weave/current-field/bob/bob-blend, including
whole-frame PNG alpha/window expectations, both field phases and native
restoration. All 46 live SNES cases pass in Debug and Release. The hidden
SnesObj PNG/camera/animation authoring regression suite passes as well.

UI help now identifies hires OBJ-interlace math (full-source row-step 2) and
interlaced rewind as the remaining guarded paths. This change handles ordinary
OBJ sampling in hires interlaced output; it does not claim row-step-2 support.
Debug/Release UI builds pass with zero warnings/errors.
Evidence: Tests/MetaSprites/obj/snes-hires-fields-native-{debug,release}.log,
snes-hires-fields-live-{debug,release}.log, snes-hires-fields-viewer-release.log
and build-snes-hires-fields-*.log.

## SNES mixed-raster PNG column fix - 2026-10-17

Fixed an observed hires-math compositor bug: ordinary samples inside a hires
frame used phase zero for both physical columns, repeating the left PNG texel.
Sampling now passes the destination phase for ordinary pixels while retaining
explicit main/sub phases for true/pseudo-hires dependencies. This applies to
sprite canvases and background sample offsets.

A native regression failed before the fix (snes-mixed-math-before.log), then
passed with independent columns at sprite artwork scales 2/4 and background
scale 2. Complete Debug/Release native suites pass. A new real LoROM fixture
switches pseudo-hires off after 80 horizontal blanks. Its 400-pixel-tall PNG
spans both regions, requiring hires blend colors above the switch and distinct
ordinary PNG columns below it, with whole-frame/native-restoration checks.
All 16 Debug and Release hires fixtures pass, including this new case.

Release UI rebuild passes with zero warnings/errors. This fixes mixed-raster
sampling; hires interlaced math and interlaced rewind remain open.
Evidence: Tests/MetaSprites/obj/snes-mixed-math-native-{debug,release}.log,
snes-mixed-math-live-{debug,release}.log and build-snes-mixed-math-*.log.

## SNES noninterlaced hires color-math composition - 2026-10-17

Mode 5/6 and pseudo-hires PNG replacement now support color math without
interlace. Each phase resolves its own main/subscreen sprites and backgrounds.
The sub phase uses the previous main pixel for color/math identity and the
previous subscreen priority for operand/half selection. At the left edge the
color operand is black while control remains pixel zero; identical primary
and priority outcomes share alpha rather than being counted independently.
PNG colors retain 8-bit effects; native-only outcomes retain 5-bit rounding.
The path preserves OBJ windows, OAM ordering, palette exemptions, full canvases
and PNG alpha. Background phase samples and independent priorities use their
own logical source positions. Configured CPU conditions/timing stay on the
published frame; no additional RAM dumps or reads are introduced.

Expanded hires frames retain their immutable logical source. Retired output
frames release that reference before pool reuse, avoiding unnecessary source
allocations while keeping published captures alive. Native tests exercise
30-frame pool reuse, malformed phase references, 32 math combinations, alpha,
palette exemption, previous-pixel sampling and separate background phases.
Both complete native Debug/Release suites pass.

Six new live LoROM fixtures cover mode 5/6 and pseudo-hires, fixed/subscreen
operands, main/sub OBJ planes and windows, with independent whole-frame PNG
expectations and native restoration. All 15 Debug hires cases and all 39
Release hires/interlace cases pass. The hidden SnesObj authoring/camera/PNG
animation suite also passes. UI capability help and Debug/Release builds are
updated; hires interlaced math, interlaced rewind and broader live effect/mode
combinations remain unfinished.

Evidence: Tests/MetaSprites/obj/snes-hires-math-native-{debug,release}.log,
snes-hires-math-live-{debug,release}.log, snes-hires-math-viewer-release.log
and build-snes-hires-math-*.log.

## Explicit SNES hires phase-source capture - 2026-10-17

Extracted the production sub-output metadata construction into the shared
MetaSnesCapture::CaptureSubOutputPhase helper. It keeps current subscreen
primary data, previous main-screen color/sprite/background operands and math
control distinct. SecondarySampleX now explicitly records the previous color
coordinate; at the left edge it is -1 while MathControlX remains zero. This
avoids treating the black edge operand as pixel zero when implementing phase
composition. Current rendered behavior and hires-math guards are unchanged.

Native Debug/Release suites verify left/adjacent/right edges, active/blank
captures, sprite identities, raw colors, priority/math flags, layer/sample
coordinates and preserved clip/subtract/half settings. Both cores build; all
nine Debug hires fixtures and all 33 Release hires/interlace cases pass.
General hires color-math composition still needs independent primary/secondary
object/background sampling and math-control priority resolution.
Evidence: Tests/MetaSprites/obj/snes-phase-source-native-{debug,release}.log,
snes-phase-source-live-{debug,release}.log and build-snes-phase-source-*.log.

## Renderer capability help and hires dependency audit - 2026-10-17

Corrected Sprite Viewer and background-editor/import help that still described
all SNES interlaced math as unsupported. Help now distinguishes supported
normal-width interlaced math, hires sub/main math restrictions and pending
interlaced rewind replacements. SNES import context has explicit line breaks.
SMS import help now includes supported legacy graphics/text modes rather than
claiming Mode 4 alone. These are UI text changes; renderer behavior is unchanged.

The hires audit confirms a specific dependency in SnesPpu::ApplyColorMath:
the sub output phase at x uses the previous main pixel at x-1 and its math
control metadata. Current full-canvas composition resolves both object planes
at one destination coordinate. Hires math therefore needs independent primary
and secondary phase sampling/visibility, including earlier native objects and
backgrounds; removing the current guard would lose that dependency. The
normal-width mapping remains valid because it duplicates completed main output.

Debug UI build passes with zero warnings/errors. Evidence:
Tests/MetaSprites/obj/build-renderer-help-ui-debug.log. Existing renderer test
evidence remains in the preceding sections; no new runtime behavior is claimed.

## SNES normal-width interlace color-math support - 2026-10-17

Normal-width interlaced fields can now carry PNG replacements with color math
through weave, current-field, bob and bob-blend output. The new expansion path
duplicates completed main-screen metadata while preserving both math operands,
OBJ/main/sub eligibility, coverage, native lower objects and background math
identity. PNG background sample coordinates expand with the output geometry.
Captured hires sub/main phases and separate hires BG samples remain excluded
from this path; their math dependency mapping is still unfinished.

Debug/Release native suites verify expanded output against independent lowres
math reference checks across 32 mode combinations and three animation-alpha
phases, plus PNG background math/sample parity. Four new LoROM/BG mode 0 live
OBJ-interlace fixtures exercise half-add math and full-frame PNG alpha/native
restoration in all four deinterlace modes. All 18 OBJ-interlace cases pass in
Debug and Release. Existing hires/ordinary-interlace Release regressions and
the hidden SnesObj viewer-authored pack/camera/animation suite also pass.
Interlaced rewind capture, true-hires math and broader live math modes remain
open; this change does not claim those paths.

Evidence: Tests/MetaSprites/obj/snes-main-math-native-{debug,release}.log,
snes-main-math-live-{debug,release}.log, snes-main-math-hires-regression.log,
snes-main-math-viewer-release.log and build-snes-main-math-*.log.

## Whole-frame reverse rewind animation verification - 2026-10-17

The live suite now records 150 fully composed forward frames, starts actual
RewindManager reverse playback and requires at least 110 cached reverse frames
to match their original full pixel arrays exactly in descending frame order.
This includes looping metasprite PNGs and a conditional play-once background
with a partial-alpha first phase and transparent final phase. The first phase
lasts 60 frames so reverse playback reaches it despite rewind's 30-frame
preparation block. Recording-to-rewind transition is under one emulator lock,
preventing an unrecorded forward frame from entering the expected sequence.

All nine Debug/Release fixtures pass, including native restoration after rewind
and previous pause/2x/run-ahead/held-and-early-save-load checks. Required test
flags are now 0x1FFFF. Fixtures: NES mapper 0, DMG/CGB, GBA BG/OBJ, PCE single
VDC, SMS modes 4/0, SNES mode 1 BG0/OBJ and WS BG/OBJ. This proves exact reverse
replay for these modes, not every mode, filter, input change or pack edit.
Test-only forward pixel storage is bounded to 180 frames and released afterward.

Evidence: Tests/MetaSprites/obj/animation-rewind-{debug,release}.log and
build-animation-rewind-core-{debug,release}.log. Release UI rebuild passed
with zero warnings/errors (build-animation-rewind-ui-release.log).

## Live early background frame save/load replay - 2026-10-17

The live suite now saves during the first partial-alpha background PNG, waits
for its transparent final PNG/hold, restores, and requires the partial-alpha
phase and transition to occur again. Both background and metasprite activation
origins must remain unchanged under two-frame run-ahead. A 30-frame first
background duration gives the test enough time to save its early phase.
Existing final-frame hold restoration remains covered.

All nine representative fixtures pass in Debug and Release with required flags
0xFFFF. This verifies sentinel output and captured clocks in existing modes;
whole-frame rewind playback and wider rendering modes remain outstanding.
Release UI rebuild passes with zero warnings/errors, and packaged/root core
DLL hashes match. Evidence: Tests/MetaSprites/obj/animation-early-bg-{debug,release}.log
and build-animation-early-bg-*.log.

## Live background clock restoration with run-ahead - 2026-10-17

Combined live save/load now leaves both the looping metasprite and conditional
play-once background active during two-frame run-ahead. The test records the
background activation origin independently from its captured clock, advances,
restores, requires a returned presentation frame and checks both origins and
PNG output. Background final-frame hold must retain its elapsed clock even
when its transparent final PNG would visually conceal a clock-state error.

All nine representative fixtures pass in Debug and Release, retaining flags
0x7FFF. Save/load during early background frames and wider renderer modes
remain additional coverage; this test covers an already-held play-once phase.
Evidence: Tests/MetaSprites/obj/animation-bg-state-{debug,release}.log and
build-animation-bg-state-core-{debug,release}.log.

## Bounded batch-import preview work - 2026-10-17

Batch animation import now validates existing definitions once, validates each
new frame independently and refreshes the preview/memory estimate only after
the last successful import. Intermediate frames no longer trigger repeated
whole-sequence asset scans or thumbnail decodes. Single-frame import also
avoids its previous duplicate refresh. Partial failures still select retained
successful frames and report their count.

A 64-frame integration case verifies one non-null preview publication, final
frame selection and the memory estimate. The complete hidden SnesObj suite
and Debug UI build pass. This changes import work, not gameplay performance.
Evidence: Tests/MetaSprites/obj/animation-batch-refresh-live-debug.log and
build-animation-batch-refresh-ui-debug.log.

## Live animation save/load during run-ahead - 2026-10-17

Extended all nine live animation fixtures to save while two-frame run-ahead
is enabled, advance through more PNG phases, restore the saved emulator state,
observe a returned presentation frame and verify the same activation origin
and independently expected animation colors. Complete Debug/Release suites
pass with flags 0x7FFF. Direct native test animations now have distinct nonzero
identities, matching the identity prerequisite already supplied by production
packet parsing; anonymous animations intentionally cannot restore clocks.

Coverage: NES mapper 0, DMG/CGB, GBA BG/OBJ, PCE single VDC, SMS modes 4/0,
SNES mode 1 BG0/OBJ and WS BG/OBJ. This adds live default-metasprite phase
verification across run-ahead/save-load; background state restoration, rewind
video and additional renderer modes still need broader combined coverage.
Evidence: Tests/MetaSprites/obj/animation-runahead-state-{debug,release}.log
and build-animation-runahead-state-core-{debug,release}.log.

## Live cross-system animation with run-ahead - 2026-10-17

The live animation suite now enables one- and two-frame run-ahead at normal
speed and requires observing IsRunAheadFrame during speculative emulation.
Presented capture frame numbers, activation-clock origins and representative
PNG output pixels remain consistent with independent timeline/color math.
The suite checks default metasprite looping at both run-ahead settings and
background play-once/partial alpha at two frames, then clears replacements.
Previously covered pause, 2x speed, alternate ping-pong, RAM gating and native
restoration still pass. Speculative frames never reach presentation.

All nine Debug/Release fixtures pass: NES mapper 0, DMG, CGB, GBA BG/OBJ,
PCE single VDC, SMS modes 4/0, SNES mode 1 BG0/OBJ and WS BG/OBJ. Flags now
require 0x3FFF. These are representative captured artwork/sentinel checks,
not whole-frame coverage of every mode, rewind video or run-ahead/save-load
combinations. Wider renderer and rewind verification remains active.
Evidence: Tests/MetaSprites/obj/animation-runahead-{debug,release}.log and
build-animation-runahead-core-{debug,release}.log.

## Batch PNG animation import - 2026-10-17

The animation editor + picker now allows multiple PNGs. Frames append in picker
order and remain reorderable with Earlier/Later. Existing single-file picker
callers keep single-selection behavior through the shared dialog helper.
Animation import preserves in-pack filenames, reuses files already in the pack
and keeps readable collision suffixes for external imports. Batches exceeding
the 256-frame capacity reject before any changes; an I/O/validation failure
retains successful earlier imports and reports the exact added/selected count.

Debug UI build and the complete hidden SnesObj viewer suite pass, including
order/name reuse, capacity rejection and partial-failure checks. The native
OS picker interaction itself was not driven by this test.
Evidence: Tests/MetaSprites/obj/animation-batch-live-debug.log and
build-animation-batch-ui-debug.log.

## Origin-aligned animation editor playback - 2026-10-17

Playback preview now places each PNG in a shared canvas using its effective
origin and the shared artwork scale. Different dimensions or per-frame origins
no longer move the crosshair between frames. Canvas bounds include all frames
and the origin and fit within 512x512; PNG thumbnail caching remains bounded
to three images. Bounds use image headers once at playback start, without
decoding the complete sequence. Manual browsing retains individual PNG views.
Clicking during aligned playback resolves native coordinates before stopping.

The hidden SnesObj viewer integration suite passes, including a small frame
and a portrait frame with different signed origins at scale 2, stable canvas
and crosshair checks, and click-origin round-trip. Debug UI build passes with
zero warnings/errors. Evidence: Tests/MetaSprites/obj/animation-aligned-live-debug.log
and build-animation-aligned-ui-debug.log. Gameplay rendering is unchanged.

## Animated SNES native color math - 2026-10-17

Added independent per-channel reference checks for animated opaque red,
partial-alpha white and transparent full canvases over two loops. All 32
combinations of fixed-color/subscreen operands, add/subtract, half math,
brightness 7/15 and math-window allowance compare every output pixel; a
destination OBJ window also excludes the oversized margin. Complete native
Debug and Release suites pass. These synthetic captures verify composition,
not live register capture, hires or interlace color-math coverage.
Evidence: Tests/MetaSprites/obj/animation-effects-{debug,release}.log.

## Animated GBA native color effects - 2026-10-17

Added an independent per-channel reference check for three-frame PNG animation
(opaque green, partial-alpha red, transparent), over two loops and all four
native GBA effect modes. Normal and hardware semitransparent OBJ variants
verify alpha blend, brighten/darken, native source removal and destination
OBJ visibility, including oversized artwork. Complete Debug/Release native
suites pass. This uses synthetic captured state; live effect-register capture,
other coefficients and cross-system color effects still need broader coverage.
Evidence: Tests/MetaSprites/obj/animation-effects-{debug,release}.log.

## Animated destination sprite visibility - 2026-10-17

The SNES/GBA animated canvas reference matrix now also tests patterned native
destination sprite visibility: SNES main-screen OBJ windows and GBA OBJ enable
flags. All 144 configurations compare whole-output pixels over four ticks,
including oversized margins, signed origins, transparency and scale 1/2/3/10.
Complete native Debug and Release suites pass. Visibility metadata is synthetic;
live window-register capture and additional color effects remain separate work.
Evidence: Tests/MetaSprites/obj/animation-canvas-{debug,release}.log.

## Animated canvas scale and clipping matrix - 2026-10-17

Extended the full-canvas animation regression to synthetic SNES and GBA staged
composition. The 72 configurations cover scales 1/2/3/10, native sprites at
left/interior/right positions and origins -1/0/1 on both axes. Each compares
every output pixel for four animation ticks against independent PNG-coordinate
reference math, including transparent pixels, canvases not divisible by scale,
canvas growth/shrinkage and screen-edge clipping. Both complete native Debug
and Release suites pass. This verifies shared renderer paths with synthetic
captures, not live hardware capture or all system effects/windows.
Evidence: Tests/MetaSprites/obj/animation-canvas-{debug,release}.log.

## Portrait animation preview bound - 2026-10-17

Animation editor thumbnails now fit within 512 pixels on both axes. Previously
only width was bounded, allowing tall background PNGs to retain their full
height in preview memory. Added a 32x1024 PNG integration case at artwork
scale 2; it verifies a 16x512 thumbnail and native-origin/crosshair round-trip.
The Debug UI build and complete hidden SnesObj viewer suite pass.
Evidence: Tests/MetaSprites/obj/animation-portrait-live-debug.log and
build-animation-portrait-ui-debug.log. Gameplay artwork dimensions are unchanged.

## Animated full-canvas regression - 2026-10-17

Added a compositor regression with alternating 1-pixel and 3-pixel PNG frames
over a 1-pixel native sprite. It verifies origin alignment, opaque overhang on
both sides beyond native bounds, and restoration when the animation shrinks.
Debug and Release native compositor suites pass. This is a synthetic SNES
composition check; it does not establish all-system scaling/effects coverage.
Evidence: Tests/MetaSprites/obj/animation-canvas-{debug,release}.log.

## Active goal scope: PNG animation - 2026-10-17

PNG animation is explicitly part of the active replacement goal for metasprites,
alternate artwork and background PNG layers across all supported systems.
Scope includes a dedicated animation editor, ordered frames and durations,
loop/ping-pong/play-once playback, consistent origins, optional restart on
activation, pack persistence and efficient asset loading. Existing implementation
and verification progress is recorded below; remaining cross-system rendering
and playback verification stays active. This supersedes earlier deferred status.

# Metasprite workflow completion audit

## SNES woven background condition/camera continuity - 2026-10-17

Woven SNES background selection now considers both captured field snapshots.
The view retains an active-field mask per PNG, independent X/Y CPU camera
references and native-layer hide masks per field. Sampling and hiding use the
full source-row parity, including cropped output. A missing condition/camera
byte falls back on that field without suppressing a valid neighboring field.
Animation timing remains field-specific. Native-slot, foreground and independent
SNES priority placement use the same field gating. Nonwoven frames retain their
single-snapshot behavior.

Decorative margins now include backgrounds active in either field and sample
each field's camera/conditions. Camera coverage intersects the active rows'
artwork bounds. The gameplay viewport still has one shared placement; this is
not a separate sliding gameplay window per field.

Debug/Release native suites pass opposite field conditions, per-field native
hiding (including transparent artwork), distinct X/Y camera positions, missing-
byte fallback, odd-source-row cropping and decorative margin sampling. A new
LoROM/BG-mode-0 OBJ-interlace fixture toggles two WRAM bytes each vblank. Actual
whole-frame PNG checks pass for both condition values, proving opposite field
activation/cameras from real PPU capture. The 14-case OBJ-interlace fixture suite
passes, as do the shared cross-system foreground/priority/lower-OBJ regressions.
Debug/Release cores build and the Release UI build has zero warnings/errors.
Release packaging contains Mesen.exe and the matching native core.

Evidence under Tests/MetaSprites/obj/: snes-field-background-{debug,release}.log,
snes-field-background-live-{debug,release}.log, snes-field-background-fixtures.log,
snes-field-background-cross-system.log and build-snes-field-background-*.log.
This closes the known newest-snapshot-only background condition/camera gap.
Wider mode/effect/window/transform and visual rewind/run-ahead coverage, resident
asset budgeting and the broader main-goal completion audit remain unfinished.


## Buffered native packet-file loading - 2026-10-17

Runtime pack application now hands sealed owned packet files to the native core
instead of allocating final managed sprite/background byte arrays. Both writers
share one encoding routine between their compatibility byte-array API and the
direct-file API. File handoff flushes/closes the write handle and retains a
read-only lease through synchronous native parsing. Normal completion and
construction/parse failures dispose owned temporary files. Existing array/spill
building remains available for compatibility/tests.

Native sprite/background parsers accept either bytes or UTF-8 file paths. File
input uses a 64 KiB explicit read buffer and the existing 512 MiB per-packet cap.
The same word validation, resource-reference checks and incremental content
identity apply to both sources. File/array words and identities match across
legacy, shared and multi-megabyte fixtures, preserving pack-matched clock state.
No input file handle or borrowed file buffer is retained by installed artwork.

Both sets are parsed before publication. Under the emulator lock, combined
activation rechecks ROM, console identity and compatibility before publishing
and changing capture flags. Missing/truncated files and a wrong ROM leave the
old definitions installed. Unicode-path activation, legacy byte activation,
combined clear and retention after a malformed second packet pass in hidden
Debug/Release integration tests. Actual PNG animation/editor/state checks pass
through the new runtime path. Debug viewer export/import/apply tests pass for
NES, DMG/CGB, GBA, PCE, SMS, SNES and WonderSwan.

Managed ownership, read access, write exclusion and success/failure cleanup tests
pass. Debug/Release native parser suites pass; both cores and UI configurations
build. Release contains Mesen.exe and its matching native core. Evidence under
Tests/MetaSprites/obj/: animation-file-load-managed.log,
animation-file-load-{debug,release}.log, animation-file-load-live-{debug,release}.log,
animation-file-load-viewer-*.log and build-animation-file-load-*.log.

This incrementally reads packet input; it does not page artwork during gameplay.
Decoded native assets remain resident, and old/new assets may coexist around
publication. Decoder/codec buffers and the bounded managed PNG cache remain
separate. Wider animation origins/scales/effects/windows/widescreen, visual
rewind and run-ahead coverage still need work; the broader main-goal audit
remains incomplete.


## Representative live animation coverage across systems - 2026-10-16

A new live fixture suite now exercises NES mapper 0, DMG, CGB, GBA BG/OBJ, PCE
single VDC, SMS modes 4 and 0, SNES mode 1 BG0/OBJ and WonderSwan BG/OBJ. It
recognizes actual captured sprite graphics and drives two configured writable
CPU RAM bytes for enable/selection. Every accepted sample verifies that only
those two bytes were captured and that capture/presentation frame numbers agree.

Independent timeline/color expectations verify default loop durations, alternate
ping-pong reverse visits, background play-once completion/final-frame holding,
partial/transparent background alpha over animated OBJ, alternate/default
re-entry, RAM-disabled native restoration and group reactivation. Group and
background activation start frames must stay consistent across observations.
Inactive clocks must clear. A real pause keeps output/frame number stable, and
resuming at configured 200% speed preserves the same emulated-frame phase.
Clearing definitions restores native pixels and removes condition/timing data.

Debug and Release pass all nine fixtures (2047/2047 flags each). These tests use
native decoded artwork and live rendering; real PNG file loading, packet
references and SNES whole-frame field playback have separate existing checks.
This suite verifies sentinel pixels and clocks, not every pixel or renderer mode.
The shared foreground fixture refactor also passes its original cross-system
static/priority/lower-OBJ regression suite. No production correction was needed
for the exercised paths. Release packaging contains Mesen.exe and the matching
core; the UI build has zero warnings/errors.

Evidence: Tests/MetaSprites/obj/animation-gameplay-{debug,release}.log,
animation-gameplay-static-regression-debug.log and
build-animation-gameplay-ui-release.log. Run with --animation-gameplay.

Incremental native asset loading, wider animation origins/scales/transforms,
native BG insertion modes/effects/windows/widescreen, visual rewind and run-ahead
coverage remain unfinished. The configured 2x-speed check is not a test of every
turbo control path or timing setting. Broader main-goal renderer compatibility
and the deferred future proposals retain their existing scope.


## Shared animation pixel transport - 2026-10-16

Runtime loading now emits MSP6/MBGI animation packets. An inline pixel buffer
gets a packet-local ID; later frames can reference an earlier buffer while
retaining their own dimensions, origins and durations. Resources are shared
across default/alternate/group sequences and background layer sequences within
each packet. Legacy MSP1..5 and MBG1..H parsing remains supported; static packet
formats are unchanged. Native combined loading still interns matching pixels
across the separate sprite/background packets.

A content hash indexes a bounded 4096-entry metadata LRU. Full existing packet
bytes decide equality, including on hash collisions. Lookup retains offsets and
counts, not decoded images; comparisons use the RAM builder or a bounded spill
read buffer. Evicted lookup entries may safely be re-inlined. Native references
must point backward and have exactly the frame's pixel count; invalid, forward,
missing and truncated references reject before installation.

Five repeated 1 MiB frames shrink the sprite fixture from 5,243,068 to 1,048,784
bytes and the background fixture from 5,243,192 to 1,048,908 bytes. Managed tests
also verify forced hash collisions, bounded metadata, multi-block spill-backed
comparisons, append position and safe re-inline after eviction. Debug/Release
native suites verify cross-sequence sharing, timing, dimensions, origins, exact
alpha and malformed reference rejection, alongside legacy regressions.

The editor estimate counts unique paths as an upper estimate; identical content
in different files can share further. It no longer rejects a sequence solely
because its unshared frame sum exceeds 512 MiB. The actual encoded packet limit
is still enforced by the writer. Hidden tests verify real repeated PNG references
with independent origins in whole-frame SNES field playback, sprite/background
editor behavior and a reusable sequence with an unshared sum above that limit.
Shared-format save/load and full/XOR rewind checkpoint tests pass on NES, GB/GBC,
GBA, PCE, SMS, SNES and WonderSwan with Debug and Release cores.

Both cores build; Debug/Release UI builds have zero warnings/errors. Release
contains Mesen.exe and the matching core. Evidence under Tests/MetaSprites/obj/:
animation-shared-transport-managed.log, animation-shared-transport-{debug,release}.log,
animation-shared-transport-live-{debug,release}.log and
animation-shared-transport-state-{debug,release}.log.

Incremental native asset loading and broader visible all-system animation,
reverse playback, turbo/run-ahead coverage remain unfinished. Final interop
arrays and old/new native assets can still coexist during application; this is
content sharing and bounded lookup, not a global process-memory cap.


## Bounded packet builders with temporary spill - 2026-10-16

Both sprite and background packet writers now use a forward-only builder. Small
packets stay in RAM, with capacity growth capped at 4 MiB. Larger builders spill
to uniquely created DeleteOnClose temporary files and allocate the final interop
byte array once. All writes, including static artwork and metadata, enforce the
existing 512 MiB per-packet cap before extending storage. Success and failure
dispose the owned spill file. Pixel arrays are written in little-endian blocks
without changing straight-alpha values or existing native packet formats.

Managed tests pass exact/over limits, spill boundaries, cleanup on success and
failure, bounded capacity after non-power-of-two writes, readback/append behavior
and byte equality with BinaryWriter. Actual MSP5/MBGH animation fixtures exceed
the production threshold. Debug/Release native suites parse them, check frame
dimensions/timings/alpha and shared storage, and reject truncated large packets.
Evidence: Tests/MetaSprites/obj/animation-packet-stream-managed.log and
animation-packet-stream-{debug,release}.log.

Debug/Release UI builds pass with zero warnings/errors. Hidden actual-PNG,
animation editor, playback and save-state integration checks pass against both
cores. Release packaging contains Mesen.exe and the matching native core. Logs:
animation-packet-stream-live-*.log and build-animation-packet-stream-ui-*.log.

This removes a growing contiguous RAM builder beside large final packets; it is
not incremental native asset streaming. Final sprite/background interop arrays,
decoded cache/codec buffers and native old/new assets can still coexist during
application. Repeated frame transport and broader live animation coverage remain
unfinished.


## Bounded managed PNG decode cache - 2026-10-16

Pack application now uses a load-scoped least-recently-used decode cache capped
at 64 MiB of retained pixel arrays and 128 images, shared by primary metasprite,
alternate, background and sequence-frame loading. Canonical paths reuse decoded
straight-alpha pixels. The codec validates headers and reserves space before
allocating bitmap/pixel buffers, evicting older entries first. A single image
larger than the budget clears retained entries and is decoded without caching.
Invalid scale is rejected before decoding. Failed decodes publish no cache entry.
Each Apply starts a fresh cache so subsequent pack applications see edited files.

Managed checks cover recency eviction, canonical reuse, byte/entry bounds,
oversized bypass, preallocation eviction, partial alpha, external ownership and
missing/inconsistent reservations. Hidden saved-PNG/editor/playback and state
integration checks pass against Debug and Release cores. Debug/Release UI builds
pass; the Release package contains Mesen.exe and its matching native core. Logs:
Tests/MetaSprites/obj/animation-cache-managed.log, animation-cache-live-*.log and
build-animation-cache-ui-*.log.

This bounds cache retention, not total resident memory: packet arrays/builders,
codec scratch buffers and caller-owned frames remain separate. Eviction removes
references; normal GC controls reclamation. Packet transport still repeats frame
pixels and retains the existing 512 MiB packet limit. Bounded transport/loading
and wider live animation coverage remain unfinished.


## Animation clock save states and rewind checkpoints - 2026-10-16

Optional named save-state data now stores activation clocks, keyed by stable
definition IDs/alternate slots and a content identity computed from the runtime
packets. Newly parsed matching packs restore their phase. Legacy states, changed
packets and invalid auxiliary clock records start fresh activation. PNG pixels
and additional CPU memory are not stored in this section. Reset clears clocks.

Emulator serialization covers explicit saves and full/XOR-compressed rewind
checkpoints, with and without settings. Reconstructed rewind frames now capture
configured replacement conditions/timing as they are emulated forward; cached
reverse pictures bypass the decoder. Internal run-ahead save/restore excludes
these presentation clocks, and speculative SNES field capture does not advance
them. Full run-ahead behavior remains a separate live-verification task.

Debug and Release native suites pass state round trips, re-parsed definitions,
earlier/same-frame loads, legacy/changed/malformed fallback, retained field
snapshots and speculative isolation. Actual emulator tests pass save/load and
full/XOR rewind checkpoint restoration for NES, GB/GBC, GBA, PCE, SMS, SNES and
WonderSwan. The headless suite waits for startup frames before acquiring its
state-test lock, avoiding a test shutdown/startup race. Hidden editor/saved-PNG
integration tests pass against both cores. Evidence: Tests/MetaSprites/obj/
animation-state-{debug,release}.log, animation-state-cross-system-{debug,release}.log
and animation-state-live-{debug,release}.log.

These checks establish clock persistence and checkpoint handling, not visual
reverse playback on every system. Broader live animation/rewind/turbo/run-ahead
coverage, per-instance/visibility activation and bounded animation loading/
transport remain unfinished.


## Condition-based animation restart - 2026-10-16

The animation editor now exposes restart when RAM conditions activate or this
artwork becomes selected. MSP5/MBGH carry the flag. Emulation-thread clocks
observe configured-address snapshots, reset on deselection/reactivation and pack
publication, and publish immutable timing with captured frames. Default and
alternate artwork share one phase per definition across matching instances.
Unconditional sequences start when applied; moving offscreen does not restart
them. Background camera validity participates in activation.

Repeated frames do not advance clocks; skipped frame numbers advance by their
emulated delta. Counter wrap advances normally. An earlier frame number resets
activation on rewind/load. SNES retains separate field timing snapshots through
weave/expand; source frame numbers are assigned before condition capture. Static
packs avoid allocating timing snapshots. PNG-only alpha and existing priority
composition remain unchanged.

Managed packet/settings checks and Debug/Release native suites pass, including
activation/re-entry, failed reads, repeated frames, rewind, counter wrap, origins,
alpha and woven rendering. Hidden editor tests against both cores verify restart
checkbox binding, draft isolation and default/alternate/background persistence,
plus existing saved-PNG playback. Debug/Release UI builds pass with zero warnings
and errors. Release contains Mesen.exe and the matching core. Evidence is in
Tests/MetaSprites/obj/animation-activation-*.log.

The live static foreground/priority regression suite also passes across NES,
GB/GBC, GBA, PCE, SMS, SNES and WonderSwan; this verifies regression coverage,
not live restart behavior on every system. See animation-activation-static-cross-system.log.

This does not serialize activation clocks into save states. Forward/same-frame
state loads, run-ahead semantics, visibility/per-instance activation and broader
all-system live restart coverage remain unverified or unfinished. Bounded
animation loading/transport also remains unfinished.


## Animation memory sharing - 2026-10-16

Identical decoded animation frames now share immutable native pixel storage
across default metasprites, conditional alternates and backgrounds loaded together.
Origins, dimensions and durations remain per frame. Full pixel comparisons
protect against hash collisions. Definitions retain ownership after parsing and
share buffers when copied. Static primary artwork is not pooled.

Debug/Release native suites pass, including buffer identity, distinct content,
retained lifetime, parser sharing, timing, alpha, origins and invalid payloads.
Both cores build. Hidden saved-PNG/editor integration checks pass against both
cores. Release UI builds with zero warnings/errors; Mesen.exe and its matching
native core are present. Evidence: Tests/MetaSprites/obj/animation-sharing-*.log.

Transport still sends each frame and allocates a temporary buffer before sharing.
The editor estimate remains a conservative sum including repeated frames and a
packet-size guard. Bounded loading/transport, activation clocks, state/pause/turbo
semantics and broader system animation verification remain unfinished.


## Immutable animation pixel sharing � 2026-10-16

Native animation frames now retain shared immutable decoded pixel buffers.
A load-scoped content pool compares complete buffers after hashing, so different
content cannot be shared merely because its hash matches. Default metasprite,
conditional alternate and background sequences use the pool; the combined pack
loader shares one pool across sprites and backgrounds. Frame dimensions, origins
and timings remain independent. Parsed definitions keep buffers alive after the
pool is destroyed, and definition copies retain shared ownership.

Debug/Release native suites verify equal/different buffers, retained lifetime,
parser reuse and existing timing, alpha, origin, field rendering and malformed
payload checks. Both cores build successfully. Hidden saved-PNG/editor tests pass
against both cores; the Release UI package builds and contains Mesen.exe plus the
matching native core. Logs: `Tests/MetaSprites/obj/animation-sharing-{debug,release}.log`
and `animation-sharing-live-{debug,release}.log`.

This reduces retained animation memory when decoded content repeats. Static
primary artwork is not pooled. Packet transport still includes repeated frames,
and one temporary decoded frame is allocated before interning. The editor's
per-frame memory sum remains a conservative upper bound and packet-size guard.
Bounded loading/transport, activation clocks, state/pause/turbo semantics and
broader system animation verification remain unfinished.

## Shared PNG Animation Editor — 2026-10-16

Sprite Viewer now opens a dedicated editor for the displayed default/conditional
alternate artwork; Tilemap Viewer opens it for the selected background. Controls
include enable, loop/ping-pong/play-once, add/remove, previous/next, ordering,
per-frame durations, optional numeric/clicked origin overrides and playback
preview. Origins map through shared artwork scale and downsampled previews.
Synchronized play-once timing is described as starting with the game. Restart-on-
activation remains unavailable and is not exposed as a working control.

The editor keeps a detached draft until save, retains PNG files on removal or
cancel, preserves filenames on import and reuses files already in the pack.
Thumbnail previews are downsampled to at most 512 pixels wide, with three cached
images. Header-based sequence memory estimates count repeated frames because
native transport currently stores those frames separately. Enabled sequences
above the initial artwork limit are rejected before the save callback.

Debug and Release UI builds pass with zero warnings/errors; Release packaging
contains Mesen.exe and the matching native core. Hidden tests against both native
configurations instantiate the actual
window and route navigation/play/stop controls; verify duration boundaries,
default/alternate/background save/load, file reuse, ordering, origin mapping,
failed-save rollback/retry and stale-context rejection. Reordering explicitly
restores the selected frame after observable-collection binding updates. Actual
PNG/native playback checks also pass in the same run. Visible desktop layout and
file-picker interaction are not established by these hidden checks. Runtime
restart/state/pause/turbo behavior, bounded/native-shared assets and wider
all-system live animation coverage remain required.

## Initial background PNG animation playback — 2026-10-16

MBGH extends the existing background packet with validated PNG sequences while
retaining placement, conditions, camera, native hiding and priority fields.
Managed/native loading checks frame durations, origins, dimensions, scale
divisibility and payload lengths. Static background packet layouts are unchanged.
The shared view caches selected frame references before sampling, including
separate SNES field clocks. Sampling uses active frame dimensions/origins for
scrolling and wrap; decorative sliding coverage uses the same active canvas.
Artwork is referenced rather than copied during composition.

Managed and Debug/Release standalone native suites pass transport, changing
canvas sizes/origins, PNG alpha, hide-native behavior, wrapping, CPU cameras,
condition restoration, GBA/SNES staged output, separate field timing and invalid/
truncated payload rejection. A background-enabled LoROM/BG-mode-0 fixture retains
blue native BG0 and distinct interlaced OBJ rows. Actual saved green/white PNG
files and settings pass loader activation and whole-frame live foreground-layer
animation against Debug and Release cores, with disable restoration. Existing
cross-system static foreground regressions pass in Debug. Both UI builds pass;
Release packaging contains the matching native core and Mesen.exe. This covers the explicit tested
scope, not every system/mode/priority combination.

Restart-on-activation, pause/turbo/state semantics, bounded/native-shared asset
caching, editor controls and broader all-system live animation verification
remain required. Per-field background condition/camera semantics remain open.

## Initial metasprite animation transport and playback — 2026-10-16

MSP5 transports default and CPU-conditioned alternate sequences: frame durations,
resolved origins, dimensions and straight-alpha pixels. Existing static packs
retain MSP1–4 layouts. Native parsing validates counts, modes, durations, origins,
dimensions and remaining payload bytes; unsupported restart-on-activation flags
are rejected. Managed loading reuses decoded PNGs by canonical path during one
pack application. Selection returns references to stored frame artwork, avoiding
per-tick pixel copies. The initial packet limit is 512 MiB; bounded large-sequence
asset handling and native sharing remain unfinished.

Shared metasprite composition selects sequences using captured emulated frame
numbers. Woven SNES captures retain each field's own frame number alongside its
configured-byte snapshot, so timing and conditional artwork agree with the field.
Managed tests and Debug/Release standalone native suites pass MSP5 default and
alternate transfer, changing frame origins, PNG alpha, frame-clock rendering,
separate woven clocks, truncation and invalid mode/restart/duration rejection.

A hidden authoring/file test saves two actual PNG frames and animated pack
settings, applies them through the real loader, then verifies whole-frame live
SNES interlace playback with green/white images and different frame origins.
Debug and Release native cores pass; both UI builds pass and Release packaging
includes the matching core. Existing cross-system static foreground regressions
pass in Debug. This is initial synchronized metasprite
playback only. Background transport/playback, restart clocks, pause/turbo/state
semantics, native asset sharing/bounded caching, editors and all-system live
verification remain required. No animation controls are available yet.

## Native PNG animation timing parity — 2026-10-16

Added a native immutable timeline matching the managed preview timing engine.
Selection uses emulated ticks and binary search over compiled frame boundaries;
it allocates no pixels or timing buffers on frame selection. It validates mode,
frame-count and duration limits before compilation.

The managed suite generates `.animation` timing vectors beside the runtime test
packet. Debug and Release native compositor suites pass all twelve combinations
of loop/ping-pong/play-once and one/two/three/256-frame sequences. Checks cover
transition boundaries, cycle boundaries, maximum supported durations, large
64-bit tick values, empty/invalid timelines and existing compositor regressions.
Explicit managed boundary expectations also pass, avoiding parity alone as the
definition of correct timing.

Native packet transport, activation-clock ownership, asset caching, editor
controls and live playback across all systems are still pending. This timing
engine is not yet connected to gameplay.

## PNG animation model and timing foundation — 2026-10-16

Shared sequence settings now persist on default metasprite artwork, conditional
alternates and background layers. Frames carry pack-relative PNG paths, positive
durations in emulated frames and optional origin overrides; sequences retain
playback mode and restart-on-activation preference. Validation rejects invalid
modes, excessive frame counts, unsafe paths, invalid origins and zero durations.

An immutable compiled timeline implements variable-duration loop, ping-pong
(without duplicate endpoint visits) and play-once final-frame holding. Tests
cover exact boundaries, one/two-frame ping-pong, large tick values, editor-draft
isolation, disabled sequences, invalid inputs and JSON round trips for all three
owners. The managed suite and Debug UI build pass. Existing static packets remain
unchanged. Enabled animation is explicitly rejected by runtime packet creation
until transport/playback is integrated, rather than silently ignoring sequences.

This is partial implementation. Editors, native timeline/packet parity,
activation clocks, decoded-asset caching/memory estimates and all-system gameplay
verification remain required. No animation playback UI is available yet.

## Active goal addition: PNG animation — 2026-10-16

The user promoted PNG animation from deferred future work into the active goal.
Support background PNG layers, default metasprite artwork and CPU-conditioned
alternate artwork across NES, GB/GBC, GBA, PCE, SMS, SNES and WonderSwan.
Plan frame add/remove/previous/next and preview controls, durations in emulated
frames, loop/ping-pong/play-once modes, shared scale/origin with optional per-frame
origins, synchronized timing and optional restart on activation. Preserve existing
conditions, camera scrolling, priority, native hiding, clipping and PNG-only alpha.
Pause freezes animation; turbo follows emulation time. Persist sequences in packs
and share/cache decoded assets with memory estimates and bounded handling of large
background sequences. Verify authoring, persistence, timing, activation and all
renderer integrations before claiming completion. Sprite-sheet import remains an
optional later extension. Shared animation groundwork is recorded above; runtime
playback and authoring remain incomplete and block overall
goal completion. See the active PNG-animation section in the development document.

## Future text replacement feature — deferred, 2026-10-16

The user approved recording TrueType text replacement as future work. Profiles
will map native tile indices/graphics addresses to characters, gated by configured
RAM conditions and scoped to layers/regions. Planned controls include font,
size, spacing, offsets and inherited/custom per-palette colors. Rendering must
hide matched native text and preserve priority, scrolling and clipping, with
bank/content context and multi-tile mappings where needed. Start tile-aligned;
proportional text runs and sprite-based text are later extensions. See the
matching future-feature section in `metasprite-replacement.md` for scope.
Implementation is deferred and is not an active-goal completion requirement.

## Woven pose RAM-condition correction — 2026-10-16

A new LoROM/BG-mode-0 fixture changes both sprite palette colors every vblank
without changing OAM identity. Distinct green/yellow PNG poses use the same
origins. Adjacent woven fields must retain different captured poses. The plain
pose case passed, but enabling a configured byte condition on each group failed
with flags 5: output used the newest field's byte value for both fields.

The PPU now samples configured replacement addresses before retaining a woven
field. Woven captures preserve each field's small address/value snapshot. Shared
metasprite selection uses the candidate's field snapshot for group conditions
and alternate artwork, and considers both snapshots when choosing output scale.
Pool reset clears both maps. No memory dump or new user address is introduced.

The twelve-case live OBJ-interlace suite passes in Debug and Release, including the formerly
failing conditional-pose case. Native Debug and Release checks verify independent alternate
selection, per-field group conditions, configured snapshot sizes and reset.
The existing fifteen hires/field probes and cross-system foreground suite pass
in Debug. Release UI packaging passes and includes the matching native core.
Background selection still uses the frame snapshot; field-specific background
condition/camera semantics remain an open audit item.

## Rectangular OBJ-interlace and window verification — 2026-10-16

The focused suite now contains ten live cases. New LoROM/BG-mode-0 fixtures use
OAM size mode 6 (16x32 sprites), both unmirrored and vertically mirrored. The
source oracle models independent mirroring of the two 16-row halves, including
transparent unused tile rows/columns. Full fetched rows must match on both field
parities. The PNG remains smaller than the rectangular source, checking removal
outside its canvas as well as shifted origins and alpha.

An OBJ-window fixture masks native X=12..15. Its whole-frame oracle verifies that
the window clips replacement pixels, including artwork above/below the native
sprite rectangle, while the remaining canvas and disable restoration work.
All ten cases pass in Debug and Release. Changing poses, screen wrapping and broader effects
remain open; these checks do not establish complete SNES compatibility.

## Viewer-authored OBJ-interlace pack round trip — 2026-10-16

The hidden Sprite Viewer test now has a `SnesObj` fixture using the two-member
LoROM/BG-mode-0 interlace ROM. It verifies the actual 512-row viewer snapshot,
eight-source-row vertical separation, selection, enabled creation, default PNG
and JSON save, 20x16 group canvas, transparent spacing and source holes. It then
imports a 22x18 opaque PNG through the real editor, reloads the saved definitions,
decodes the saved artwork into the runtime packet, and verifies the whole live
frame against the authored origins and canvas. Reload preserves member offsets.

This passes against Debug and Release native cores, including clean debugger/emulator
shutdown. It exercises real view models and files without creating a visible
window; visible desktop interaction remains a separate verification scope. The
seven existing OBJ-interlace gameplay probes also pass after the shared native
test-helper refactor.

## OBJ-interlace output-mode and group verification — 2026-10-16

The focused live suite now covers weave, current field, bob and bob blend,
requiring successful replacement output for both field parities. Whole-frame
oracles use physical output geometry, including temporal blend and inactive
current-field rows, rather than deriving expected pixels from captured source Y.
Distinct red/blue native rows verify capture phase. Additional cases cover a
two-member group with a vertical source-coordinate offset, and horizontal and
vertical mirrors. The second member extends beyond the replacement canvas, so
the whole-frame check also verifies removal outside PNG bounds.

All seven cases pass in Debug and Release. Capture expansion now explicitly retains source
Y for separate hires subscreen phases as well; the standalone Debug regression
checks both output phases. Viewer-created group round trips, rectangular sprite
mirroring, field-changing poses, wrapping, windows and effects remain open.

## Initial live OBJ-interlace integration — 2026-10-16

SNES sprite fetch now emits explicit alternate source rows, full-source Y origins
and native coverage for OBJ-interlace. Rendered pixels retain their full-source
screen row. Hires/field submission can carry these captures through the existing
no-math mapping. Sprite Viewer uses a 512-row source-coordinate preview in this
mode, retaining full sprite artwork and doubled vertical placement; raw OAM Y
remains available separately.

The dedicated `--snes-obj-interlace` probe verifies both captured parities and
whole-frame replacement output with shifted origins, oversized margins, PNG
holes/partial alpha and disable restoration. Alternating red/blue-row Debug and
Release probes pass. The 15 existing hires/field cases and cross-system
foreground regression also pass in Debug. This is initial integration, not full
compatibility: viewer-created multi-member groups, mirrored/rectangular sprites,
field-changing poses, wrapping, windows, priorities, color math and all
deinterlace modes still require broader live coverage. Historical unsupported
notes below describe the state before this integration.

## SNES explicit field-source artwork mapping — 2026-10-16

SNES pixel state can retain an absolute full-source screen row independently
of framebuffer Y. Sparse field canvases sample PNG Y and choose member attributes
using that source row; ordinary canvases keep the existing coordinate path.
Origin offsets and oversized margins remain in source-artwork units. Captured
source rows copy with their field sample through expansion, weave and bob.
Missing source geometry neither draws a sparse replacement nor suppresses its
native sprite. No persistent packet or memory-condition format changes.

Debug native tests pass both parities, partial/transparent PNG alpha, shifted
origins, oversized margins and expansion/weave/bob retention. Release passes the
same checks plus missing-source native fallback. PPU capture and Sprite Viewer
source-coordinate integration are still required; OBJ-interlace gameplay remains
disabled and has not been claimed to work.

## Explicit SNES source-row matching — 2026-10-16

Decoded rows and runtime graphics now carry explicit source-row step/phase
metadata. SNES step-2 captures require all texels in every fetched row, reject
conflicting fetches, and compare only those source rows with the full saved
member. Missing rows are unknown, not transparent. Ordinary captures retain
their all-rows completeness requirement; non-SNES captures cannot opt into the
step-2 path. Group members must share row phase/step and field identity.

Debug and Release standalone compositor suites pass both-parity matching,
observed-row mismatch, unknown-row isolation, incomplete/conflicting capture
rejection and ordinary capture compatibility. This is matching groundwork only:
the PPU capture guards remain until vertical output mapping, origin alignment
and field-aware gameplay tests are implemented. OBJ-interlace is not enabled.

## OBJ-interlace capture investigation — 2026-10-16

The remaining SNES OBJ-interlace gap is confirmed in the production capture
path, not just the test harness: `FetchSpriteTile` excludes decoded rows and
coverage when `ObjInterlace` is set, and frame submission excludes that mode
from weave/current-field/bob mapping. Native fetch chooses source row
`2 * (scanline - spriteY) + oddField` before vertical mirroring. A hardware
sprite therefore occupies half its source artwork height in each field.

Removing the guards alone is insufficient: `BuildDecodedCandidates` requires
every source texel to have been observed, while a field only fetches alternate
rows. Combining fields must preserve each field's OAM identity, content, origin,
clipping and palette/priority rather than manufacture unobserved rows or mix
different poses. Output mapping also needs the differing horizontal and vertical
sprite scales. Required coverage includes alternating row colors, vertical and
rectangular mirroring, field-changing poses, screen wrapping, PNG-only alpha and
all supported deinterlace modes before enabling this path.

Editor status text now acknowledges the existing hires/interlaced support and
states the remaining color-math and OBJ-interlace limitations instead of claiming
that every SNES replacement requires noninterlaced low-resolution output.

## SNES mixed-member PNG attributes — 2026-10-16

Full-canvas replacement pixels select priority and palette attributes from the
nearest logical member rectangle, with overlaps and equal distances resolved by
current OAM rotation. Selection does not consult original sprite transparency.
Logical coordinates preserve attribute placement through hires expansion and
shifted artwork origins, including oversized margins.

The Release live foreground regression passes mixed priorities against native
BG, partial PNG alpha, shifted origin, 2x artwork, member palette color math and
rotated overlapping members. The Release standalone compositor suite also passes.
This scoped verification does not close the known SNES OBJ-interlace capture gap
or establish complete cross-system compatibility.

## SNES lower native OBJ reveal — 2026-10-16

SNES fetched coverage now retains each texel's color index. At render time,
nonzero lower-object texels receive current CGRAM colors, OAM identity, priority
code and palette metadata independently of the winning native object. Single
unoverlapped winners continue using their existing capture fields; extra sample
storage is limited to additional opaque objects on an enabled OBJ screen.
Matched originals are excluded and the existing SNES outcome resolver includes
remaining native objects beneath PNG alpha, preserving OAM arbitration before BG
priority, main/subscreen windows and palette-dependent color math. Pack matching
and configured CPU-address reads are unchanged; no packet-format change is needed.

Native samples survive hires expansion and bob/weave copying. Odd woven-field
IDs receive the same transient +128 tag as other sprite identities, and already
tagged input is rejected. Hidden math-enabled objects keep hires on the existing
native fallback when math is unsupported. Frame reuse clears lower-object lists.

Both native compositor suites pass yellow/opaque-black reveal, partial alpha,
OAM-before-BG blocking, fixed/subscreen math, hires/field propagation and retained
frame/reset checks. Both live foreground suites verify lower yellow OBJ through
PNG holes/partial alpha, OBJ-window suppression and palette-5 fixed-blue math.
The broader 49-case SNES ordering suite passes in Debug and Release. Fifteen
Release hires/interlace fixtures pass; the full 16-probe runner exits nonzero on
the known unsupported OBJ-interlace fixture (flags 0). This is an open gap, not a
passing full-suite result. All 15 supported Debug hires/field fixtures pass with
OBJ-interlace explicitly excluded by the `--snes-hires-weave` runner.
Both cores and UI builds pass; Release Mesen.exe is present and its packaged DLL
hash matches. Native Release retains existing debug-symbol warnings.

Representative ordinary lower-OBJ reveal is implemented. Broader live hires/
field overlap, OBJ-interlace capture, mixed-priority metasprite members and other
documented SNES compatibility gaps remain required; the overall goal stays open.

## WonderSwan independent insertion bands and native overlap — 2026-10-16

WonderSwan now enables shared MBGG bands 4–6, preserving its native ordering:
backdrop/BG0, band 4, low OBJ, band 5, BG1, band 6, high OBJ, final foreground
overlay. Assigned native BG still supplies scroll/camera coordinates and windows.
BG0 and BG1 native-priority PNGs remain at their respective native stages. Both
activation APIs, managed serialization/loading and the editor accept the bands.

WS capture marks its frames and retains both BG stages for sprite-only packs as
well as background packs. Frames captured before those stages were requested
retain their original image until complete capture is available. The ordered
resolver supports all 128 transient OAM IDs and separate inside/outside sprite
window classes. An opaque earlier low OBJ blocks later high OBJ before BG1 hides
it; PNG holes and partial alpha resolve the lower native object independently.
High-priority native sprites are now classified correctly in captured metadata
(native priority 2 is high). Native PPU drawing/arbitration is unchanged.

Managed serialization and Debug/Release native suites pass, including two-layer
ordering, PNG alpha, OAM IDs above 63, window classes and incomplete-stage fallback.
Both live foreground suites pass BG1 on/off, low/high native and replacement OBJ,
all bands, foreground overlay and partial artwork. A live overlapping low/high
OBJ test without any BG PNG pack verifies opaque/transparent/partial replacement
outcomes against BG1. All 21 Debug/Release WS fixtures pass (mono/color, planar/packed,
scrolling, windows and native BG reveal). Both editor save/apply/reload runs pass.
Both UI builds pass with zero warnings/errors; Mesen.exe is present and its
packaged native DLL hash matches.
Native Release retains the existing debug-symbol link warnings.

Initial independent-priority integration now exists for all requested systems,
using native priority controls on GBA/SNES and shared insertion bands elsewhere.
Broader compatibility verification, dynamic palette/mode effects and remaining
documented native reveal gaps still prevent completion of the overall goal.

## SMS independent PNG insertion bands — 2026-10-15

SMS now enables shared MBGG positions 4–6 plus the existing foreground overlay.
Managed creation/loading, both native activation APIs and Tilemap Viewer controls
accept the bands. Hardware layer selection still offers only the one SMS BG.
Draw slots, CPU conditions, placement, scrolling and camera behavior are retained.
SMS has no independent behind-BG OBJ flag; its two lower PNG bands can coincide
relative to native content, while authored PNG order still distinguishes them.

SMS capture identifies its frames and uses the decoded-row ordered resolver.
Its background-priority masks select each sprite outcome's BG/OBJ placement,
including high-priority tiles, transparent color-zero tiles and native-priority
PNG alpha. PNG transparency can reveal lower native OBJ without consulting the
matched source's alpha. CRAM-dot overrides clear reconstructable-backdrop capture
so ordered composition retains the original dot pixel. Source VDP fetching,
collision and native rendering logic remain in place.

Managed serialization and both native suites pass. Native assertions cover tile
priority on/off, insertion order, lower native reveal, priority PNG alpha and
unreconstructable-dot fallback. Both live foreground suites pass SMS Mode 4 and
legacy Graphics I band checks with native/replacement sprites and transparent/
partial artwork. Mode 4 requires and checks a native OBJ hidden by an opaque
priority tile. All thirteen Debug/Release SMS/legacy fixtures pass, including graphics
modes, text BG, large/zoomed sprites, source holes, early-clock/left clipping,
column masks and scroll locks. Both hidden-editor save/apply/reload runs pass.
Both UI builds pass with zero warnings/errors. Release Mesen.exe is present and
the packaged native DLL hash matches. Native Release retains existing debug-symbol
link warnings.

Live CRAM-write timing, midframe mode/palette changes and broader lower-OBJ overlap
still require coverage. WonderSwan independent insertion bands remain required;
this does not complete overall SMS compatibility or the cross-system goal.

## PCE independent PNG insertion bands — 2026-10-15

The supported single-VDC PCE path now enables MBGG insertion bands 4–6 alongside
native priority and foreground overlay. Existing draw slots, CPU conditions,
placement, wrapping and camera settings are retained. Pack creation, runtime
validation, both native activation APIs and the editor accept these positions for
PCE while keeping unsupported-system and SuperGrafx guards. No packet layout
change is needed.

PCE frames identify their adapter explicitly. The ordered resolver consumes raw
decoded sprite slices, including slice offsets, rather than requiring every
unreplaced sprite to reconstruct a complete candidate. PNG alpha resolves native
sprite outcomes before the BG insertion stages, preserving opaque hidden-winning
OBJ blocking and allowing holes/partial alpha to reveal lower objects. Visible
native winners retain output-time color; other decoded texels use fetched colors.
PCE capture now forces hardware nonzero colors opaque even for RGB-only palette
entries; hardware color zero alone remains transparent. This fixes native reveal
and authoring capture with default/alpha-less palettes. Hardware VDC rendering,
fetching and collision logic are unchanged.

Managed serialization and Debug/Release native compositor suites pass. Both live
foreground suites pass with native/replacement PCE front OBJ across all bands and
opaque/transparent/partial artwork. Debug and Release pass nine HuCard/VDC1 fixtures:
front/behind opaque BG, hide-original, native PNG priority, wide and hole-wide
sprites, divider-3/divider-2 holes and horizontal mirroring. Both hidden editor
runs save/apply/reload the positions and reject GBA/SNES-only packets. UI builds
pass without warnings/errors; Release Mesen.exe is present and its native dependency
hash matches. Native Release retains
existing debug-symbol warnings.

Live lower-OBJ overlap with the new bands, wider mixed-priority member layouts,
dynamic lower-object palette/grayscale behavior and SuperGrafx remain unverified
or unsupported. SMS/WonderSwan insertion bands remain required. This is further
integration progress, not completion of PCE compatibility or the overall goal.

## GB/GBC independent PNG insertion bands — 2026-10-15

GB/GBC now supports the three MBGG insertion bands already used by NES: below
behind-BG sprites, between those sprites and native tiles, and above native tiles
but below front sprites. Foreground overlay remains the final stage. Each position
retains ten authored draw slots, CPU gating, scale, placement and hardware/CPU
scroll sources. UI enables the shared choices for GB/GBC; managed and native APIs
continue rejecting them for unsupported systems. The binary field layout is
unchanged. Earlier NES-only guard descriptions below are superseded by this entry.

The ordered resolver retains native sprite arbitration, PNG alpha and CGB tile
priority. A priority BG PNG's partial-alpha outcomes pass under the above-tiles
band while uncovered front OBJ stays above it. CGB master priority disabled still
allows behind-flagged OBJ above BG. Captured underlays now retain BG color index
to insert bands correctly when a window is hidden. Bands assigned to BG can use
the retained BG scroll sample under a visible window. A per-frame GB capture flag
also handles background-only scenes without fetched sprites.

Managed serialization and Debug/Release native suites pass, including transparent
and opaque tiles, opaque/transparent/partial replacement outcomes, priority BG PNG
alpha, CGB master priority, window underlays and empty-sprite frames. Debug/Release
foreground suites pass with live CGB insertion-band checks. All twelve Debug/Release
GB/GBC gameplay fixtures pass. Both hidden-editor runs save/apply/reload all
three positions and rejects GBA/SNES-only packets. UI builds pass with zero
warnings/errors. Mesen.exe is present and the Release dependency DLL hash matches
the rebuilt core. Native Release retains the existing debug-symbol link warnings.
PCE, SMS and WonderSwan independent insertion integration remains required.

## GB native reveal palette timing — 2026-10-15

GB fetched rows now retain an output-color mask and eight output-time RGB555
colors. Capture populates these while processing each visible pixel, before BG
priority/OBJ winner checks, so both the winning and lower fetched objects retain
current palette colors. Recognition still uses the unchanged fetch-time palette
and graphic. The ordered compositor uses recorded colors when present and falls
back to the fetched palette for older/synthetic rows without them. This metadata
is transient native capture, not a pack-format change or CPU memory snapshot.

The Debug/Release native suites pass a differential palette test: fetched blue remains
the matching graphic while revealed pixels use output red/green and unrecorded
pixels fall back to blue. A copied retained frame preserves the recorded colors.
Live CGB assertions verify output samples for both the hidden winning object and
the lower object. Debug/Release cross-system foreground suites pass, as do all
twelve Release GB/GBC gameplay fixtures after this change. Both cores build; the
Release UI package builds with zero warnings/errors and its native DLL hash
matches. No new pack serialization or additional CPU-memory reads are introduced.
A live DMG regression now pauses the emulation thread, steps the real PPU and
sweeps an OBP1 memory-manager write across a hidden lower object's fetched row.
It requires both old and new output colors in that row while the fetched palette
remains unchanged. The captured frame is then passed through a transparent upper
metasprite; all eight revealed pixels must equal their recorded output colors.
Debug and Release pass this stronger check and the full cross-system foreground suite.
This establishes the PPU-write/capture/composition path, not a ROM program's CPU
instruction timing. Broader palette effects and game-specific raster behavior
remain open. The added regression is test-only; packaged production behavior is
the palette-capture implementation already built above.

## GB/GBC ordered sprite alpha outcomes — 2026-10-15

Game Boy capture now uses the shared ordered-sprite scene resolver when captured
backdrop data and sprite or native-mode background definitions are available.
Opaque OBJ wins sprite arbitration before BG priority is applied, so a winning
behind-BG object continues to block lower front objects. PNG holes and partial
alpha resolve lower native objects as separate outcomes. Native fetched rows
retain horizontal flips, palette data and captured DMG X/OAM or CGB OAM order,
including incomplete sprites that cannot form recognized groups. Mixed-member
replacement regions inherit the nearest member rectangle's class/order without
consulting source alpha. Hidden-window composition uses the retained BG map and
its own object eligibility and PNG priority. Missing backdrop capture retains the
existing fallback; unrelated systems retain their previous paths.

Debug/Release native suites pass hidden-OBJ blocking, opaque/transparent/partial
PNG outcomes, changed sprite order, horizontal flips, PNG priority/hiding and
hidden-window BG reveal. All twelve existing Debug/Release GB/GBC ROM-only gameplay
fixtures pass, including banked VRAM, scrolling, windows, source holes, retained
window underlays and native sprite overlap. A new live CGB test places a winning
behind-BG object over a later front object against opaque green tiles: opaque PNG
keeps green, holes reveal blue, and partial alpha produces the expected green/blue
mix. Debug/Release cross-system foreground checks pass including this test. The
Release UI rebuild has zero warnings/errors, Mesen.exe is present and the packaged
native DLL hash matches. The native Release link retains existing debug-symbol
warnings. Broader DMG/CGB sprite limits,
midframe attribute/palette changes and broader insertion-band verification
remain open; this does not complete GB/GBC compatibility or the overall goal.

## NES mixed-member canvas attributes — 2026-10-15

NES replacement canvases now select front/behind-background priority and OAM
order from the nearest matched member rectangle per destination pixel. Overlapping
rectangles and equal-distance margins resolve by NES OAM order. Source sprite
alpha is not used for attribute selection. This replaces the previous whole-canvas
front-member assignment. A dedicated two-member regression checks opposite BG
priorities, expanded margins and transparent/partial-alpha PNG pixels. Debug and
Release native suites pass; an additional Debug assertion verifies an unrelated
native OBJ ordered between the two group members wins over the later member.
The Debug/Release core rebuilds and seven existing Release NES live gameplay fixtures pass.
The Release core link retains the existing LNK4020 debug-symbol warnings.
A live NROM test now captures two adjacent members with opposite BG priorities,
recognizes their group by graphics/relative placement, and verifies opaque artwork,
oversized margins and partial PNG alpha against an opaque native background.
The live check additionally overlaps the members by four pixels and shifts the
PNG origin left by two pixels. It verifies the overlap inherits earlier OAM
priority, the left margin stays behind native BG, and the right member/margin
remains in front, with opaque and partial-alpha artwork. Debug and Release pass.
The full foreground suite passes in Debug and Release with these checks. Its OAM
setup disables rendering before editing the second sprite; writing OAM during
active rendering did not reliably establish the intended scene. Broader live OAM
ordering and mapper coverage remain required; the overlap check is representative,
not an exhaustive test of member layouts.
The Release UI package rebuild passes with zero warnings/errors; Mesen.exe is
present and the packaged native core matched that rebuilt DLL. Debug now includes
the mixed-member production change; a later test-only core rebuild adds the live
check above and has not yet been repackaged into the Release UI dependency archive.

## Background authoring controls and artwork filenames — 2026-10-15

The hardware-layer selector now lists system-specific names: SNES BG1–BG4,
GBA BG0–BG3, Game Boy background/window, WonderSwan BG0/background and
BG1/foreground, and one background for NES, SMS and the supported PCE path.
These choices describe supported native roles rather than the current video mode.
Background import and artwork replacement preserve the source filename, reuse
artwork already in the pack directory, and use short numbered suffixes on external
filename collisions. Rollback only removes a newly created copy, never reused
artwork. The Debug UI build passes with no warnings or errors.

The latest Release native compositor suite passes, including NES insertion-band
alpha/OAM cases. The NES hidden-editor workflow passes with the updated controls
and filename handling, including targeted assertions for original filenames,
short collision suffixes and in-pack artwork reuse.

The final Debug and Release NES raster suites pass with the latest scene resolver:
sprite-zero splits, hardware/reference/CPU cameras, seventeen decorative canvas
variants, signed player-speed sliding, finite 1024px coverage at camera boundary
766, full-canvas alpha/origin checks and disable restoration. Smooth-slide tests
allow up to 128 emulated frames with a bounded host-time watchdog; this is a test
convergence allowance, not a runtime motion change. Release cross-system foreground
checks pass, including all NES bands, GBA priorities 0–3 and SNES priorities 1–15.
The Release UI rebuild passes with zero warnings/errors and Mesen.exe is present.

NES scene reconstruction also applies to ordinary metasprite packs and native-mode
background PNGs when fetched rows and backdrop capture are available, preserving
behind-BG OAM blocking and replacement alpha without requiring an independent
band. Sprite-only reconstruction redraws affected source/canvas row bounds rather
than the entire viewport. Legacy captures without backdrop data retain the existing
fallback. Broader mixed-priority NES group and mapper verification remains open.


## NES independent PNG insertion bands — 2026-10-15

NES now exposes three insertion positions alongside the final foreground overlay:
below behind-background sprites; above those sprites but below native tiles; and
above native tiles but below foreground sprites. Every position retains ten PNG
draw slots, placement/scrolling/wrapping, CPU camera/raster overrides and CPU
conditions. Native layer priority remains the default. Native-mode PNGs compose
before the explicit above-tiles band; the final foreground overlay stays last.

The NES scene resolver evaluates sprite PNG alpha in hardware order, then applies
the selected band sequence for each outcome. An opaque behind-BG sprite still
blocks later foreground sprites even when tiles hide its artwork. Transparent or
partial replacement alpha can reveal lower native sprites. Only replacement PNG
alpha governs replacement coverage. Captured fetched rows supply native sprites,
including partial rows that cannot form a recognized group. Winning sprite palette
and priority class are now retained even when native BG hides the sprite; hardware
sprite-zero hit/rendering logic is unchanged. Offscreen group tracking stays deferred.

MBGG persists the positions and remains compatible with earlier packets. Managed
and native APIs reject NES-only bands on other systems. Debug pixel tests cover
OAM blocking, opaque/partial/transparent artwork, native/replacement overlap,
hiding tiles, native sprite reveal, clipping, source-alpha holes and 2x cropped
origin alignment. Packet tests cover all positions, saved order, every truncated
packet and invalid position rejection. Live NROM checks cover all bands with
front/behind native and replacement OBJ, opaque tiles and lower OBJ alpha outcomes.
Hidden NES editor checks save/apply/reload each new band and restore native mode.
A GBA editor test rejects NES-only packets through standalone and combined native
APIs. The seven existing NES gameplay/crop/alpha fixtures pass in Debug. Additional
pixel assertions verify condition mismatch, missing/present CPU camera, and reads
of exactly the two configured condition/camera addresses. Mixed-priority member
groups and unusual OAM evaluation ordering still require broader verification.
Other systems' remaining insertion positions and wider NES mode/mapper coverage
remain required; this does not complete the overall goal.


## Independent SNES PNG hardware priority and native OBJ reveal — 2026-10-15

SNES now offers independent priority 1..15 (higher draws nearer the front), separate
from the selected BG layer. The source BG supplies captured coordinates, windows,
main/subscreen eligibility and color-math identity. Native tiles retain their own
priority. PNG wins a BG priority tie; OBJ wins an equal priority. Equal-priority
PNGs retain authored draw order. The post-effects foreground option remains separate.
Composition resolves each eligible screen before native color math/brightness.
A reusable thread-local entry buffer avoids per-texel allocation/sort scratch.

The live test exposed native OBJ missing after hiding an opaque BG. The staged
resolver now reselects the captured fetched OBJ color, priority and math identity,
respecting main/sub OBJ window masks, when it was originally blocked by a BG.
This fixes that reveal case without treating original sprite alpha as PNG alpha.
It does not establish complete lower-OBJ overlap reveal or all interlace modes.

MBGF persists the new position/priority; MBGD/MBGE defaults remain compatible.
UI and native APIs reject SNES-only priority packets on other systems. Managed
and Debug/Release native tests cover all 15 priorities, OBJ/BG ties, partial/transparent
alpha, authored order, layer hiding, screen/window eligibility, math identity,
packet truncation/invalid values and native-object reveal in both native and
independent priority positions. End-to-end pixel assertions also cover fixed-color
add/subtract, half, brightness, math windows, subscreen addition and main/sub OBJ
masks. Hidden SNES authoring
save/apply/reload/restore and NES rejection tests pass. All 49 broader SNES ordering
regressions pass in Debug. The Debug/Release live ROM suites
pass all nine cross-system foreground regressions plus GBA/SNES priority checks.
Other system insertion bands and wider SNES hardware modes/effects remain required.
All 49 broader SNES ordering cases pass in Debug and Release. Both live suites
also pass nine foreground regressions and GBA/SNES independent-priority checks.
Hidden SNES editor checks pass with both cores; NES rejects SNES-only packets.
Both UI builds complete with zero warnings/errors. Release Mesen.exe is present
and its packaged native DLL matches the rebuilt core. The Release native build
retains the previously documented LNK4020 debug-symbol warnings.


## Independent GBA PNG hardware priority — 2026-10-15

GBA backgrounds now offer an independent hardware-priority composition position,
with priority 0 (highest) through 3 (lowest), separate from source native BG0..BG3.
The assigned BG still supplies coordinates, window visibility and color-effect
target identity; native tile artwork keeps its own priority. PNGs become separate
entries in the GBA layer resolver rather than recoloring/reprioritizing that BG.
OBJ wins equal priorities; BG ties follow native BG number, with PNG ahead of its
own native BG and later authored PNGs ahead of earlier PNGs at the same priority.
PNG alpha participates in selecting the visible main/secondary effect layers.
Hide original layer remains independent. Foreground overlay remains a separate
post-effects option. Widescreen scenery orders independent PNGs below foreground.

JSON and MBGE preserve the position/priority; legacy MBGD remains compatible.
Unsupported-system activation and out-of-range priorities are rejected by managed
validation. The GBA editor exposes the option; other systems cannot select it.

Managed and Debug/Release native tests cover transport/truncation/invalid values, all four
priorities, OBJ/BG ties, authored PNG overlap, partial alpha, layer suppression,
clipped samples and blend-target/window-effect behavior. Hidden GBA editor checks
save/apply/reload/restore with Debug and Release cores. Native APIs reject a GBA-only packet on NES through both standalone and combined activation. Live Debug/Release ROM tests verify every priority against native
and replacement OBJ2 plus partial alpha. Other system insertion positions and
broader native mode/window/effect combinations remain required for completion.
Both application cores and the Debug/optimized Release UI builds are updated.
The UI builds have zero warnings/errors; the Release native build retains the
known LNK4020 debug-symbol warnings. Release Mesen.exe is present, and its packaged
native DLL matches the rebuilt core. Both cores pass all nine live foreground
regressions and the added GBA native/replacement priority-switching checks.


## Live foreground PNG integration verification — 2026-10-15

Added --foreground ROM-driven coverage for NES NROM horizontal-scroll BG, DMG,
GBC, GBA mode-0 scrolling BG0, PCE single-VDC enabled BG, SMS Mode 4, SMS legacy
Graphics I, SNES mode-1 subscreen BG0 and WonderSwan color scrolling backgrounds.
The test selects a complete hardware sprite that is actually visible with a valid
assigned BG sample. It verifies opaque foreground artwork above the native sprite,
partial-alpha artwork above an independently verified magenta metasprite PNG,
transparent foreground reveal, condition mismatch restoration and disable restore.
Each setting transition waits for new emulated frames, so unchanged-color checks
cannot pass against a stale frame. Snapshots contain only the configured address
and clear after all definitions are removed. PCE assertions account for overscan.

All nine cases pass with both Debug and Release cores (18 live runs total). The Release build retains the previously documented LNK4020 debug-symbol warnings. Test startup requests BG capture explicitly and skips
the GBA boot screen, matching the synthetic fixture requirements. This verifies
basic live foreground integration across all seven systems; it does not establish
all rendering modes/windows/interlace/color math or the remaining priority bands.


## Independent foreground PNG position — 2026-10-15

The Tilemap Viewer now offers Native layer priority (default) and Foreground
overlay as independent composition positions. Foreground artwork is drawn after
native and replacement sprites, native backgrounds, color effects and brightness.
It retains the assigned layer's captured coordinates/visibility samples, PNG
placement, scaling, scrolling, CPU camera, raster override, wrapping and CPU
conditions. It does not inherit SNES main/subscreen color math or native blending;
this is an explicit final display overlay. Hide original layer remains independent.

CompositionPosition persists in JSON and MBGD packets, with legacy packets
remaining native-priority. Packet emission groups native PNGs before foreground
PNGs and preserves the ten draw slots/stable ties within each position. Decorative
widescreen margins also sample the foreground artwork when opted in.

Managed persistence and validation plus Debug/Release native pixel tests cover MBGD
transport, every truncated packet, invalid position rejection, native OBJ overlap,
shared/GBA/SNES/underlay sample selection, missing sample clipping, CPU gating,
cropped/scaled CPU-camera artwork and composition above replacement metasprites.
The other independent priority bands and broad per-system hardware-window,
interlace/color-math and gameplay verification remain required; this does not
complete independent priority layers or the overall goal.
Debug and optimized Release application builds are updated; both UI builds report
zero warnings/errors. Hidden SmsLegacy editor tests save/apply foreground mode,
reload its persisted position, then restore native priority with both rebuilt
cores.
Hidden editor foreground save/apply/reload/native-position restoration checks also
pass on NES, GB, GBC, GBA, PCE, SMS, SNES and WonderSwan with the rebuilt Debug core,
with clean shutdown on every system. These are authoring/native-activation checks,
not exhaustive on-screen hardware composition tests.
Debug/Release tests additionally cover independent native-layer suppression under
transparent/partial foreground PNGs. Wider per-system gameplay coverage is pending.


## PNG draw-order controls — 2026-10-15

Each background now saves a DrawOrder slot from 0 through 9. The Tilemap Viewer
provides a numeric control with a higher-number-draws-above explanation. Native
layer assignment remains independent; hardware background/sprite priorities still
apply. Equal slots retain saved PNG order, and existing packs default to slot 0.
The runtime packet writer sorts references stably without changing editor order
or requiring a new native packet version. Out-of-range slots are rejected.

Managed persistence/validation tests and Debug/Release native tests verify saved
slots reach composition, equal-slot stability, partial/transparent overlap,
condition mismatch restoration and matching GBA/SNES layer adapter output.
Hidden SmsLegacy background-editor authoring also verifies DrawOrder survives
Save/apply and pack reload, with clean shutdown using the rebuilt Debug core.
Independent insertion bands relative to sprites/native backgrounds remain
unfinished; these within-native-layer ordering controls do not complete that goal.
Both application native cores were rebuilt. All 13 legacy/Mode 4 SMS renderer
regressions pass against each rebuilt core. The optimized Release core build
reports LNK4020 debug-symbol/PDB warnings; runtime checks pass.
The Debug and optimized Release UI builds complete with zero warnings/errors.
Release Mesen.exe is present and its packaged native DLL matches the rebuilt core.
Hidden editor save/reload/shutdown also passes with the rebuilt Release core. This does not
establish complete cross-system gameplay coverage of independent priority bands.


## Overlapping background PNG composition — 2026-10-15

The shared direct-color compositor now blends every matching PNG in saved order,
with later entries above earlier entries, matching the GBA/SNES adapters. Opaque
or transparent entries no longer stop evaluation of later artwork. The retained
background-underlay and behind-background replacement-sprite paths use the same
order, preserving PNG alpha independently of original sprite texels.

Debug and Release native compositor suites pass, including new pixel assertions
for overlapping opaque/partial/transparent PNGs, reversed order, and two partial
PNGs above replacement sprites on both ordinary and revealed background layers.
This is prerequisite composition work; independent insertion bands, user ordering
controls and cross-system gameplay verification remain required. Application binaries were subsequently rebuilt with the draw-order controls.


## SMS legacy graphics/text replacement integration — 2026-10-15

Extended native replacement capture beyond Mode 4. Legacy Graphics I, Graphics II
and multicolor now expose fetched sprite graphics, native sprite identities,
per-pixel replacement eligibility and background samples through the shared
compositor. Legacy 16x16 sprites combine their fetched left/right halves into one
hardware sprite; zoom, per-sprite early-clock X shift and source transparency are
retained. Capture preserves raw fetched pixels before native shifters consume or
clip them, while native rendering and collision handling continue normally.

Text mode provides 240x192 background sample coordinates inside its eight-pixel
side borders, with no sprite groups. PNG background replacement, transparent
hide-original-layer fallback, condition mismatch restoration and exact sparse
memory capture are checked using a ROM-defined C000 RAM gate. Graphics modes use
256x192 samples without invented hardware scrolling. Fixed legacy palette colors
and native backdrop values are captured independently of Mode 4 CRAM colors.

The `--sms-legacy` suite contains eight legacy fixtures (modes 0/1/2/3, 16x16,
zoomed 16x16, transparent source texels and early-clock left clipping) plus five
Mode 4 regressions (ordinary, zoom, left clipping, column mask and locked scroll).
Debug and Release pass all 13 cases. Native/UI builds succeed, and the Release
package contains the matching native core.
The hidden `--selection-suite SmsLegacy` passes against Debug and Release cores
for actual editor model authoring,
automatic PNG creation/import/export, origin/alternate conditions, background
settings, stale-save protection and pack loading before CPU execution.

These fixtures use SMS2 hardware. SG-1000/ColecoVision palette/model variants,
limit-removal extra sprites, mid-frame mode changes and broader overlap/timing
combinations still need dedicated checks. Hidden authoring checks do not establish
visible desktop interactions. The cross-system main goal remains active.

## GBA ordinary 4bpp mosaic coverage — 2026-10-15

Added original live fixtures for an ordinary 4bpp OBJ with vertical mosaic and
for the same asymmetric two-dimensional graphic with both hardware flips and
combined horizontal/vertical mosaic. Both use OBJ palette bank 3 while palette
zero contains a different color, verifying palette selection in the capture path.
The oracle independently checks unsampled recognition pixels and native displayed
pixels. It also checks full-canvas PNG alpha, origin alignment, oversized margins
and original-pixel restoration after disabling replacements.

The existing vertical-mosaic capture correction handles these cases without
another renderer change. Updated native first-pixel assertions to account for
the flipped graphic's blue corner instead of assuming every unmirrored fixture
starts red. All 37 GBA suite cases pass in Debug and Release; native and UI builds succeed,
and the Release package contains the matching native core.
Other mosaic sizes, independent single-axis flip combinations and broader
window/effect interactions remain outside this specific coverage. Replacement
PNGs continue to use their authored pixels rather than hardware mosaic filtering.

## GBA affine vertical mosaic capture — 2026-10-15

Fixed recognition capture for vertically mosaicked OBJ graphics. The native
renderer keeps using its hardware-sampled row; the capture path independently
reads the underlying graphic row using the affine matrix (or ordinary flip
coordinates). Captured group identities no longer contain repeated display rows.
The extra reads only run with capture active and do not alter native arbitration,
retained lower-OBJ samples, mosaic counters or display output.

Two original live fixtures use a quarter-turn enlarged asymmetric 8bpp OBJ:
vertical groups of four and combined horizontal pairs/vertical groups of four.
They verify all captured/native pixels, shifted full-canvas replacement, opaque,
transparent and partial-alpha PNG samples, and disable restoration. All 35 GBA
affine-suite cases pass in Debug and Release. Replacement artwork remains at its
authored resolution; this does not add mosaic filtering of PNGs. Ordinary 4bpp and combined flips now have the live coverage recorded above;
other mosaic sizes and broader effect combinations still need dedicated coverage.
The main cross-system goal remains active.

## Current audio progress — 2026-10-15

Initial music and sound replacement integration is implemented for **NES,
GB/GBC, GBA, PCE, SMS/Game Gear, SNES and WS/WSC**. The shared Audio Replacement
Editor supports imported assets, event-triggered music/effects, optional RAM
conditions, music looping/fades, per-track volume and whole-native-output mute.
SMS/GG and WS/WSC have separate memory/I/O triggers; SNES has independent main
CPU/SPC trigger and condition-source selection. Packs load before CPU execution.

WonderSwan completion adds 20-bit linear memory, 16-bit I/O ports, segmented and
wrapped address handling, native aligned/split word events and operation-type
preservation for split DMA writes. Generated monochrome/Color V30 fixtures and
editor lifecycle checks pass in Debug and Release. The complete audio regression
suite passes for all seven systems. Debug/Release native and UI builds succeed,
and the updated Release executable is present with the matching native DLL.

Real-game tests and valid OGG/audio-device listening remain **deferred by user
request**. Broader special hardware/mapper/audio-driver coverage, global SFX volume,
same-effect overlap, fades/crossfades, disk streaming, precise sample scheduling,
state/rewind playback positions and advanced synchronization remain unfinished.
See [music-replacement.md](music-replacement.md) for exact current capabilities
and fixture scope. This completes the initial audio system rollout, **not the
main metasprite/background goal**. Existing renderer gaps and deferred PNG work
remain as recorded later in this audit. The slowdown issue stays resolved; 3D
model replacements stay cancelled.

## Historical progress

2026-10-14 music first stage: dedicated Debug → Audio Replacement Editor, NES
post-commit CPU-write triggers plus optional RAM conditions, OGG Vorbis/PCM 16-bit
WAV playback, looping, volumes, fades and whole-native-output mute. Packs are
separate at HdPacks/Audio/Nes/<SHA1>/music.json and load before CPU execution.
Native/editor checks pass for generated PCM and NROM/mapper-0 fixtures, including
event/context selection, repeated writes, playback controls, rollback, stale saves,
and pre-execution loading. Sound effects, other console/audio-CPU trigger sources,
exact saved playback positions and fully streamed assets remain future work. Valid
OGG files and real audio-device audition are not yet verified by these fixtures.
Full implementation notes are in docs/music-replacement.md. This does not mark the
main metasprite/background goal complete.
Final Debug/Release music checks passed, including automatic loading before CPU
execution and independent native/music volume/disable restoration. The write hook
uses a compiled address mask so unrelated NES writes avoid rule processing. An
early Release test-harness stall was avoided by loading its direct memory-write
fixture without a CPU thread; the separate editor/running-ROM checks still run.

2026-10-14 pack-only boundary settings: removed Video Options camera background
ID, camera limits, RAM boundary conditions/toggle, slide speed and release delay,
along with global/game-profile persistence, native interop fields and renderer
fallback. Removed the copy-video-positioning-to-PNG button/action. Gameplay
position remains in Video Options, along with enable, ratio, margins, brightness
and fade. Existing active PNG positioning continues to take priority; Automatic
without a controlling pack definition stays centred. Pack positioning metadata
and editor controls remain unchanged. Old profiles ignore obsolete fields and
omit them on subsequent saves. Live NES NROM and GBA/SNES fixtures passed with
pack-owned limits/flags, including Automatic-without-pack centred fallback.
This supersedes older compatibility-fallback/copy-button instructions below.
Profile/editor checks also passed: the four-choice Gameplay position selector
remains bound, obsolete profile fields are dropped safely, native presentation
interop matches, and PNG authoring retains pack-owned boundary/motion metadata.
The rendered Video Options layout was inspected. Debug/Release UI and core builds
completed successfully; the Release executable and matching core dependency were
verified.

2026-10-14 finite-PNG coverage: Automatic placement now constrains the stored
window position to the active controller PNG's horizontal coverage, before the
configured camera/RAM boundary triggers. This applies to non-wrapping CPU-camera
backgrounds and accounts for artwork scale, origin, output scale, crop, scroll
multiplier and captured horizontal pixel scale. Coverage overrides speed/delay
when needed; signed reversal remains available within the covered range. Wrapping
PNGs and manual positioning retain their behavior. An image too narrow or badly
placed to cover the output is not stretched or wrapped automatically.
Native tests pass for a 1024px PNG, camera stop 766, approach from both directions,
zero/reverse player speed, 1x/2x/3x output, crop and origin offsets. The live NES
NROM fixture also verifies finite PNG coverage before the boundary trigger while
preserving every gameplay pixel. The user's pack is on another system and has
not been tested locally.

2026-10-14 direction correction: signed player X RAM speed now retains direction
while a boundary remains active. Positive moves right, negative moves left, with
the window constrained between its normal position and the active edge. Turning
around no longer requires a camera/flag transition. Unsigned sources keep their
target-directed behavior. On boundary release, speed magnitude returns the window
toward normal. Missing/zero speed still stops movement. This supersedes the
absolute-only direction description below; existing pack data requires no change.
Native regression tests pass for forty-pixel walk-away/return trips at both edges,
normal/edge clamps, stopped/missing speed, unsigned fallback and redraw stability.
The live NES NROM CPU-limit fixture passes with negative speed at the left edge,
positive at the right edge, and zero speed stopped, including 2x output.

2026-10-14 player X RAM slide speed: optional per-PNG CPU address, signed/unsigned
8-bit or 16-bit LE/BE format, conversion multiplier and cap. Automatic camera
limits/RAM flags still determine the target; absolute X speed determines motion
toward it, including returning home. Zero/unavailable speed freezes motion.
Only the selected active controller's one/two bytes are captured on the emulation
thread, deduplicated against existing samples. Native output scaling preserves
the native-pixel speed. MBGC adds source metadata; MBGA/MBGB remain compatible
and default to fixed speed. Native tests cover six formats, signed extrema,
acceleration, cap, zero/missing values, redraw stability, packet rejection and
legacy defaults. The NES NROM test ROM verifies actual $18/$E8 motion at 2x and
zero-speed stopping with CPU edge limits. This is horizontal sliding only.

2026-10-14 fractional sliding and release delay: slide speed now supports fractional
native pixels per emulated frame, accumulated before integer output placement.
Per-PNG settings and the legacy Video/profile fallback expose a release delay
(0–600 consecutive inactive frames, default 0/off). Brief inactive readings keep
the last edge target; a renewed match resets the timer. An opposite edge takes
effect immediately. Missing/conflicting checks, inactive controllers and scene
changes do not retain an old edge. Numeric camera limits also support the delay.
MBGB transports milli-pixel speeds and delay; MBGA integer speeds still load.
Native tests cover accumulation, flicker, timer/redraw stability, missing/conflict
fallback and legacy packet compatibility. Release NES NROM and eight GBA/SNES
live widescreen fixtures passed, as did profile persistence/native interop checks.

2026-10-14 sliding-window verification: reran Release live GBA/SNES widescreen
fixtures and extended the NES NROM sprite-0 raster fixture with per-PNG RAM
left/right/both/neither cases. The right-edge case uses nonzero slide speed;
each case compares every native viewport pixel at its expected output offset.
CPU-limit and RAM-boundary cases passed. This verifies the native renderer,
not a full visible UI setup or the user's currently failing pack. PNG positioning
overrides Video settings; new PNG imports default to Centre. Automatic mode,
margin extension and saved/applied edits are required for boundary tracking.

2026-10-14 per-PNG positioning: moved the primary gameplay position, slide speed,
camera limits and RAM boundary rules into background definitions. New imports
have centred per-PNG defaults; existing packs retain global/profile fallback.
The Tilemap Viewer edits and saves these fields and can copy saved video values.
Scene activation conditions choose the first active margin PNG with an override;
changing the controller resets slide history. Only its extra flag bytes are read.
MBGA adds bounded positioning metadata; older packet versions remain compatible.
Managed/native transfer and truncation checks, actual NES editor persistence/copy
workflow, and Debug GBA/SNES per-scene cases passed. This supersedes older notes
that place all positioning settings only in Video/game profiles.
Release per-scene cases also passed across eight GBA/SNES fixtures, alongside
the existing RAM-flag and large-canvas checks. Debug/Release native suites and
UI/native builds passed; the Release executable/native dependency copy were
verified. Full visible desktop and remaining filter/mode coverage are still pending.

2026-10-14 RAM boundary flags: implemented eight optional CPU-memory conditions
per edge, all six comparison operators and masks, profile/global persistence and
Automatic positioning in place of camera limits. Both/neither/missing checks use
normal placement; empty edges are inactive. The selected active margin PNG gates
positioning, including hardware-scroll PNGs. Only configured bytes are captured
on the emulation thread and deduplicated against existing camera/group bytes.
Native tests, profile/editor and managed/native configuration-layout checks passed.
Rendered UI was inspected, with horizontal overflow prevented. Older pending
entries for optional boundary flags are superseded. Wider mode/filter coverage
and full visible desktop verification remain pending; the main goal is incomplete.
Debug/Release native suites and eight GBA/SNES live fixtures passed, including
five RAM-flag variants each (80 flag checks across both configurations), alongside
the existing presentation/large-canvas checks. Profile validation and native
interop checks passed; Debug/Release UI/native builds passed and the Release
executable/current native dependency copy were verified.

2026-10-14 fade correction: replaced the outer-edge black fade with the requested
brightness transition outward from gameplay: 100% at the adjacent margin pixel,
linear transition over the fade width to the selected side brightness, then a
constant side level. No blur; native gameplay is unchanged. Zero width is uniform
dimming, 100% side brightness stays full throughout, and the transition follows
the moving viewport. Existing profile fields remain compatible. Older outer-edge
fade descriptions below are superseded by this correction.
Validation passed: Debug/Release native tests (0%, 37%, 100%, unchanged native
pixels), 112 dedicated GBA/SNES live presentation/canvas checks across both
configurations, and the Release NES NROM widescreen/sprite-0 fixture. Debug and
Release UI/native builds passed; the Release executable is present.

2026-10-14 large-canvas continuation: patterned 256/512/1024-pixel replacement
backgrounds passed live GBA tiled/affine/bitmap and SNES LoROM mode 0/1/7 checks,
including CPU X=1014 wrapping and signed FF:F6=-10. Eight fixtures run seven
presentation/canvas checks each; 56 passed in Debug and 56 in Release. Native
viewport pixels stay identical across widening and restore on disable. Only the
three configured memory bytes are captured. Automatic viewport history now resets
for zero/invalid margin canvases and decoder stop, with native regression coverage.
Debug/Release native tests and UI/native builds passed. This supersedes the older
48-check count below. Optional boundary flags, remaining filters/hires/interlace
modes and full visible desktop verification remain pending.

2026-10-14 profile/validation continuation: per-game widescreen profiles now save
separately by CPU/system and ROM SHA1 under the configured game-settings folder.
Video settings offer preview, Save for this game, Remove game profile and Use as
global defaults. Loading/stopping/switching games applies/clears profiles; unrelated
video fields and global defaults remain independent. Actual editor tests passed
for save/load/remove, stale-game rejection, invalid-write retention and isolation.
Dedicated GBA tiled/affine/bitmap and SNES LoROM modes 0/1/7 live fixtures passed
all 24 widescreen variants in both Debug and Release (48 checks). Actual settings
UI construction/render and brightness-spinbox binding tests passed; the rendered
layout was inspected and clipped fade/slide labels received line breaks.
Older pending entries below are superseded for profiles and these live fixtures.
Optional CPU boundary flags, broader filters/modes and full visible desktop
verification remain pending. The overall metasprite goal is not complete.

2026-10-14 continuation: added custom asymmetric margin widths, outer-edge fade,
and automatic gameplay positioning at explicit signed CPU-camera limits for a
selected active PNG background ID. Zero slide speed is instant; positive speeds
advance per emulated frame, with stable redraws and reset/rewind fallback.
Native tests and the live NROM fixture cover the new controls. Per-game
presentation profiles, optional CPU boundary flags and visible desktop checks
remain pending. The older pending lists below are superseded for these controls.
Verification: Debug/Release native suites passed; nine live NROM widescreen
variants passed in both configurations. The full Debug gameplay regression suite
passed, including 59 live asymmetric-canvas checks across NES, GB/GBC, SMS, PCE
and WS. GBA/SNES dedicated live widescreen checks remain pending (their existing
gameplay regressions passed; margin sampling has native adapter tests).
The actual video-config serializer round-tripped the new settings. Debug/Release
UI and native builds passed, and the Release executable/native dependency copy
were verified. No visible desktop verification is claimed.

2026-10-14: Decorative widescreen resumed. Initial shared canvas and settings are
implemented: 16:9/21:9/custom ratio, centre/left/right viewport placement,
0–100% side-brightness numeric input with 1% arrows, and per-background PNG
margin opt-in (MBG9; old packets default off). Native gameplay pixels remain
unchanged; margins use matching PNGs with existing scroll/camera/alpha rules.
Managed/native checks and a live NES NROM sprite-0 split fixture cover ratios,
placement, arbitrary brightness, opt-in, signed CPU camera and disabling.
Cross-system live/visible verification, custom side widths, edge fade, automatic
camera-boundary sliding and per-game presentation settings remain pending.
Debug/Release native compositor suites and live NROM widescreen checks passed;
the hidden NES editor save/reload suite passed and exited cleanly. Debug UI and
Release UI/native builds succeeded; the Release executable is present. Hidden
checks do not replace visible desktop verification.
This does not complete the overall metasprite goal.

The previously planned write-event plus RAM-context selection is now implemented
across the requested systems. Context-only changes do not trigger playback. See
the current audio progress above; real-game listening remains deferred.

The approved shared music/SFX editor, write-event rules, whole-native-output mute,
music looping and overlap/retrigger policy are now implemented. Native/music and
per-track volumes are present; a separate global effect volume remains planned.
Command addresses still require pack-author knowledge of the game/driver. Audio
packs remain separate from PNG packs.

Decorative widescreen was originally deferred; its initial implementation is
recorded above. The remaining proposal
includes aspect-ratio/custom-width controls, per-PNG margin extension, side
brightness through a 0–100% numeric box with up/down arrows and 1% steps,
optional edge fade, and optional gameplay-viewport sliding at configured CPU
camera boundaries. It adds scenery outside the native viewport without expanding
game logic. Remaining implementation and verification are still pending.

Future goal addition: per-PNG background Normal alpha/Add/Subtract blend modes
across all supported systems, optional opacity/strength, PNG-alpha preservation,
priority-aware composition and explicit ordering with native SNES/GBA effects.
Recorded at the user's request; implementation and verification remain pending.

The user cancelled the 3D model replacement proposal on 2026-10-14. It is removed
from future plans and is not part of the goal or completion requirements.

CPU-camera integration is applied across NES, GB/GBC, GBA, PCE, SMS, SNES and
WonderSwan. GBA background change detection/composition and SNES captured-layer
and fallback composition now use the shared camera-aware sampler instead of
direct hardware-coordinate PNG sampling. Native Debug/Release regressions prove
camera-coordinate selection in all three staged paths. Hidden editor assertions
verify CPU-camera readout, save/reload and activation for all eight configurations;
seven finish shutdown cleanly, while GBC assertions pass but that run stalls at
Stop and is terminated. The optimized Release application is rebuilt. Exhaustive
per-game/mode camera gameplay coverage remains outside these checks.

Signed page is available independently for each pixel-offset/page camera axis,
defaults off and persists through saved profiles/link/unlink. Managed and native
Debug/Release checks pass for FF:F6=-10, FF:FF=-1 and page-zero transitions,
MBG8 transfer/truncation and older unsigned packet compatibility. The live NES
NROM raster fixture verifies negative camera sampling alongside unchanged
sprite-0-hit splits in Debug/Release. Hidden NES save/link/unlink assertions and
clean shutdown pass. Both UI builds pass and the optimized Release executable
is updated. Per-system gameplay coverage beyond NES remains pending.

Saved CPU-camera reuse is implemented per game pack. Backgrounds can link to the
saved camera, follow subsequent saved updates, or unlink while retaining its
settings. Managed tests verify multi-background resolution/update, persistence,
conflict snapshots and dangling-link rejection. The latest hidden NES editor
suite passes saving/linking/unlinking, failed-save rollback, shared-pack merge
and clean shutdown. Debug/Release UI builds pass. The earlier isolated editor
Stop stall remains unexplained; this run does not establish a lifecycle fix.

CPU-camera scrolling is implemented in shared background sampling, editor,
JSON and MBG7 transport. Per-axis byte/endian/pixel-plus-page formats, configurable
page size and live editor readout use only configured CPU addresses. Managed and
native Debug/Release tests pass; the live NES sprite-0 raster fixture verifies a
781-pixel CPU camera independently of nametable wrapping and the native split.
Hidden NES authoring assertions pass, but that run stalled at emulator Stop and
was terminated. The optimized Release application was rebuilt successfully.
Broader per-system CPU-camera gameplay verification remains open.

Shared direct-colour composition now restores captured unreplaced native
sprites beneath removed replacement members, not only beneath hidden BG layers.
A Debug native fixture verifies transparent PNG revealing blue lower OBJ,
half-green PNG blending over it, and transparent native lower texels retaining
the backdrop. The complete Debug native suite passes. A twelfth ROM-only GBC
fixture overlaps red OAM 0 with yellow OAM 1. Transparent front PNG reveals
yellow and half-green blends over yellow; opaque front pixels remain green.
All twelve live cases pass in Debug and Release, including full-canvas and
CPU-condition checks. Native Debug/Release suites pass too; this does not establish
lower-object capture completeness for every system.

Game Boy/GBC capture now retains the background map beneath active window
pixels, including signed tile addressing, CGB banks/flips/palettes and scroll
coordinates. Shared composition uses the retained native colour when hiding the
window, and retains the lower BG's sprite eligibility. A Debug native fixture
verifies occluded and revealed replacement sprites. Retained lower-layer PNG
priority now uses its own captured mask; native tests verify partial-alpha PNGs
above behind-BG sprites and transparent PNGs revealing them. The focused
`--gameplay-gb` runner now passes all seven existing live Debug cases, including
a GBC window-hide check for retained BG colour, wraparound scroll coordinates
and partial-alpha PNG blending. An eighth live Debug ROM-only GBC fixture gives
the BG yellow palette 1 and the window blue palette 0; hiding the window reveals
yellow, and a half-green PNG produces the expected blend over yellow. All eight
cases pass in both Debug and Release. The Release native compositor suite also
passes retained BG priority and lower PNG alpha checks. The optimized Release
application was rebuilt with these changes. Live overlapping sprites still
need dedicated coverage.

The focused suite now has a ninth ROM-only monochrome DMG window/scroll fixture.
All nine live cases pass in Debug and Release, including retained lower BG
samples and PNG alpha when hiding either DMG or GBC windows. Live overlapping
sprites beneath a hidden window remain pending.
Counterfactual BG sampling reads registers/VRAM at pixel output; unusual midline
VRAM/register effects still require dedicated coverage.

A tenth ROM-only GBC fixture places the sprite inside the active window region
over a distinct yellow BG. All ten live cases pass in Debug and Release. With the window hidden,
the front-priority replacement remains green above transparent and half-green
window artwork while uncovered pixels reveal/blend over yellow. Disabling the
sprite replacement restores the front-priority native red object above both
transparent and partial-alpha window PNGs. The test waits for restored group
memory capture before condition checks. Behind-BG objects and overlapping native
objects still require coverage.

An eleventh ROM-only GBC fixture marks the window-region OBJ behind the BG.
All eleven live cases pass in Debug and Release. Opaque PNG backgrounds cover its entire
replacement canvas; hiding the native window with transparent PNG reveals the
replacement and native OBJ, and half-green artwork blends over the native red
OBJ. GB capture now records native sprite class so direct background composition
does not skip behind-BG native objects. The Release native regression suite also
passes. Native OBJ/OBJ overlap coverage remains pending.

WonderSwan foreground sampling now retains BG1 coordinates at transparent native
texels. The Debug native suite verifies PNG alpha over BG0 and native sprites on
both sides of BG1 priority, including fully transparent replacement artwork.
All 21 WonderSwan live cases pass in Debug and Release, including a dedicated
mode 0x80 transparent-BG1/window fixture. It checks PNG coverage, partial alpha,
lower-layer visibility and CPU-condition restoration. The optimized Release UI
was rebuilt with the updated native DLL and the Tilemap Viewer text-width fix.
These checks do not establish full completion of the overall objective.

The active objective includes sprite authoring, background replacements and CPU
conditions. The slowdown issue is resolved per user confirmation on 2026-10-13;
further investigation is no longer pending. This audit does
not establish completion of the full objective.

| Requirement | Current evidence | Status / remaining work |
| --- | --- | --- |
| Ctrl/rectangle selection, persistent groups and names | Actual editor model and routed XAML-control suites for eight configurations (NES, GB, GBC, GBA, PCE, SMS, SNES, WS) | Verified in these suites; user verified visible artwork navigation and origin clicks |
| Create automatically saves/assigns transparent PNG and enables gameplay | SaveNewMetaGroup, UseExportedArtwork, actual Create/save/native activation/reload tests | Verified, including rollback and retry |
| Click/numeric origins, scale and pose alignment | Default/alternate canvas events, numeric controls, persisted origin and pose-family tests | Verified; clicks now always enabled |
| Content/relative-layout recognition rather than hardware indices | Shared managed matcher and native decoded candidates; reordered-index and incomplete-group checks | Verified in fixture coverage |
| Replacement PNG alpha governs artwork | Exact 256-alpha-value PNG round trip; shared/GBA/SNES crossing-canvas regression with opaque/transparent/partial-alpha front PNGs in both pack orders | Full-canvas paths verified by native fixtures; unreplaced lower sprite reveal remains to verify |
| Full PNG canvas replaces HDnes-style additions | Shared complete-canvas compositor, native 1x/2x/3x and alternate checks; Debug/Release whole-frame NES NROM, GB/GBC ROM-only, single-VDC PCE, SMS Mode 4 and legacy graphics/text, two GBA cases, three SNES lowres plus nine no-math hires cases and 17 WS color/monochrome gameplay checks; synthetic ordering/background gating | Implemented for these fixture scopes; PCE SuperGrafx/PNG priority transitions, broader WS combinations, broader GBA effects/transforms and SNES hires color math/interlace remain unfinished |
| PNG dimensions need not divide evenly by scale | All eight hidden viewer configurations import/persist 17x19 default/alternate PNGs at scale 2; native final-corner and packet checks; complete PNG alpha round trip | Verified for authoring/loading and tested renderer scope; background wrap canvas validation remains separate |
| System-specific clipping, visibility and priority | Native fixtures listed in metasprite-replacement.md | Verified for tested modes; broader scope incomplete below |
| Background PNG export/import/placement/scrolling/composition | All eight authoring suites and native gameplay fixtures | Verified for tested hardware modes; resize repair and failed replacement save covered |
| User-defined background scroll speed per axis | Persisted X/Y multipliers, MBG4 transport, native offset/wrap sampling; managed/native tests, live WS half/reverse rate fixtures and hidden SNES save/reload | Implemented; broader transformed/raster/rollover gameplay coverage pending |
| NES background PNG ignores raster scroll changes | Per-layer toggle/reference scanline, MBG5 transport, uncropped-frame reference preparation; native split/crop/fallback tests, live NROM sprite-0-hit fixture and hidden NES authoring | Implemented; broader game-specific split coverage remains unverified |
| Independent PNG background priority layers across all seven systems | Added to goal by user on 2026-10-13; four HDnes-style bands with ten ordered slots each, with system-specific layer/priority mapping | Initial integration implemented: NES/GB/GBC/PCE/SMS/WS insertion bands, native ordering, final foreground overlay and independent GBA/SNES hardware priorities; broader cross-system verification remains pending |
| Per-replacement Hide original layer toggle | UI/JSON/MBG6; shared/GBA/SNES suppression; native alpha/reveal/gating checks, live PCE, 21 WS and 9 GB/GBC Debug/Release cases, hidden GBA authoring; ordinary SNES lower-OBJ reveal now has live window/math checks | Implemented for tested scopes; live overlapping sprites under hidden GB windows, broader SNES field/lower-OBJ effects and other native capture coverage remain incomplete |
| Alternate artwork and required background CPU checks | Packet filtering, masked condition authoring, live SNES WRAM transitions | Verified; draft alternates remain inactive and backgrounds require configured checks |
| CPU operators ==, !=, >, <, >=, <= | Shared managed/native unsigned masked comparisons, JSON/editor round trips, MSP4/MBG3 transport and invalid-operator rejection | Implemented for groups, alternates and backgrounds; native Debug/Release tests pass, with live WonderSwan activation coverage |
| PNG animation for metasprites, alternates and backgrounds | Shared sequence model/timing, MSP5/MBGH transport, synchronized native playback, actual PNG/SNES field checks and shared animation editor | Authoring/playback, condition clocks/state and representative all-system live pause/2x-speed checks implemented; incremental native assets and advanced renderer/rewind/run-ahead coverage remain incomplete |
| Read only configured CPU addresses | CompileMemoryAddresses and CaptureMemorySnapshot iterate only compiled condition addresses; native snapshots tested with one/two configured bytes | Verified; no full-memory snapshot path is used for replacement conditions |
| Current build artifacts | Debug/Release UI and native builds, Release GBA affine/bitmap suite | Verified; full Release desktop interaction remains unverified |

## Renderer gaps that prevent broad completion

WonderSwan BG0 is now retained beneath the visible foreground background in a
separate frame vector, allocated only when background definitions are present.
Hiding BG1 reveals native BG0 or its active PNG; hiding both reveals the backdrop.
The retained sample includes BG0's own scroll and wrapping. Native Debug/Release
tests cover alpha, both layers hidden, cropped output, condition restoration and
frame-pool retention/reset. A distinct-color live fixture (yellow BG0, blue BG1
and blue backdrop) verifies native lower-layer reveal, white BG0 PNG reveal and
partial-alpha artwork in Debug and Release; both complete 20-case WS suites exit 0.
Transparent foreground texel capture and broader sprite/scroll
combinations remain pending; this is representative lower-layer support, not
completion of every WonderSwan mode.

The rebuilt Release core also passes all four PCE priority cases, including live
hide-layer native/replacement sprite reveal, partial PNG alpha, condition mismatch
restoration and toggle-off overlay behavior. Optimized Release UI compilation
passes with no warnings/errors and Mesen.exe is present alongside the updated
core. This artifact includes the hide-layer work within its verified scopes;
Broader GB window overlap, SNES field/lower-OBJ effects and other listed rendering
gaps remain open; ordinary SNES lower-OBJ reveal is covered by the entry above.

Hide-original-layer preparation now captures the native backdrop separately from
the visible tile color in NES, GB/GBC, PCE, SMS and WonderSwan frames. This is
needed for transparent replacement pixels when the assigned tile artwork is
suppressed. GBA/SNES already retain backdrop values in their staged metadata.
Frame-pool tests check retention and reset, and live Debug PCE opaque-background
fixtures distinguish the blue tile layer from the black backdrop. This preparation
is now used by the hide-layer toggle and compositor suppression. Lower native
background/OBJ capture remains necessary for complete reveal.

Direct-color adapters now also retain sprite destination classes before native
background occlusion, separately from their existing visible classes. NES,
GB/GBC, PCE, SMS and WS preserve sprite enable, clipping and OBJ-window rules in
this metadata. The compositor now consumes it when hiding
a native background would unblock a behind-background sprite. Frame-pool retention
and reset tests pass in Debug. Live PCE opaque-BG fixtures verify normal classes
remain foreground-only while background-independent classes include both priority
classes. Current hide-layer verification is listed above; broad completion is
still unproven.

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

### Cross-system screen-edge continuity — deferred

Scope expanded by the user on 2026-10-16 from the earlier NES proposal to NES,
GB/GBC, GBA, PCE, SMS, SNES and WonderSwan. The feature remains deferred; this
update authorizes planning scope, not immediate implementation.

Extend each system's existing capture while retaining native clipping, windows,
background priority and sprite order. Preserve available offscreen sprite
graphics/positions, establish partial-group origins from visible anchors, and
validate missing members against decoded candidates where available. Keep group
coordinates outside the viewport and clip the full replacement PNG during
composition. Test all four screen edges, coordinate wrapping, entry/exit,
partially visible groups, oversized artwork and native restoration per system.

Consider brief evidence-based tracking only when games remove offscreen members
from sprite memory. Previous position alone must not establish identity or leave
stale replacements after disappearance, scene changes or state loads. Persistent
recognition remains based on graphic content and relative layout, never fixed
hardware sprite indices. Replacement PNG alpha remains authoritative.

NES-specific requirements remain: mapper-aware OAM/CHR candidates, the native
left-column mask, unsigned X-wrap disambiguation using saved arrangements and
visible anchors, and preservation of mapper bank/fetch timing and sprite flips.
A generic end-of-frame CHR read may not be sufficient. Other adapters need their
own coordinate, fetch, affine/interlace and visibility handling as applicable.

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
establish hardware mosaic processing of replacement artwork. Vertical affine mosaic now has the representative live coverage recorded above;
other mosaic sizes and broader mosaic/effect combinations still need dedicated
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
