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

# Metasprite replacement development

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


## PNG Animation Editor available — 2026-10-16

Use **Edit PNG animation…** in Sprite Viewer for the displayed default/alternate
PNG, or in Tilemap Viewer for the selected background. The shared window supports
frame import/removal/navigation/reordering, duration, playback mode, optional
origin overrides, click-to-set crosshairs, preview and save/apply. Drafts are
isolated until save; filenames are preserved, in-pack PNGs reused and files
retained on removal/cancel. Preview uses a three-image downsampled cache and
shows approximate decoded sequence memory, including repeated frames.

Debug/Release UI builds and hidden window/control tests against both native cores
pass, including all three owners,
scaled thumbnail origin clicks, stale-context rejection and failed-save recovery.
These tests also exercise actual PNG/native playback. Synchronized play-once
starts with the game. Restart-on-activation, pause/turbo/state semantics, bounded
native asset handling and broader all-system live coverage are still pending;
visible desktop interaction/layout has not been verified by the hidden tests.

## Initial background sequence integration — 2026-10-16

MBGH now transports background PNG sequences. Shared sampling caches immutable
frame references, uses active dimensions/origins for wrap/placement/camera paths,
and retains each woven field's animation clock. Widescreen sliding coverage
tracks the active frame's horizontal bounds. Managed and Debug/Release native
tests pass, including direct-color and GBA/SNES staged output, PNG alpha, hiding,
RAM gating, camera scrolling and malformed data rejection.

Saved PNG files and settings also pass whole-frame live SNES background-animation
activation and disable restoration against Debug and Release cores. Existing
cross-system static foreground checks and UI builds pass; Release packaging
contains the matching native core. This initial playback uses global
emulated-frame timing. Editors, restart clocks, pause/turbo/state checks, bounded
caches and wider all-system live verification remain pending. Background
conditions/cameras still need a separate per-field semantics audit.

## Initial metasprite PNG sequence playback — 2026-10-16

MSP5 now carries default and conditional-alternate animation frames, inheriting
owner scale and resolving optional origins. Native composition selects immutable
artwork by emulated frame number; woven SNES fields retain independent clocks.
Managed loading decodes repeated paths once per pack application. Static MSP1–4
packets stay unchanged. Debug/Release standalone checks cover rendering, origins,
alpha, alternate selection, field clocks and invalid/truncated packets.

Actual saved green/white PNGs and sequence settings pass loader activation and
whole-frame SNES interlace playback against Debug and Release cores. Existing
cross-system static foreground checks pass in Debug; UI builds and Release
packaging pass. This initial path uses synchronized
timing; restart-on-activation is explicitly rejected until implemented. Background
animation, editors, activation clocks, pause/turbo/state behavior, bounded caches
and all-system gameplay checks remain pending. Initial packets are capped at
512 MiB; native asset sharing and bounded loading still need work.

## Native animation timing engine — 2026-10-16

Managed and native immutable timelines now agree for all three playback modes,
one/two/three/256-frame sequences, exact duration/cycle boundaries and 64-bit
ticks. Managed expectations and twelve exported timing-vector combinations pass
the Debug and Release native compositor suites. Selection performs no artwork
allocation or copying. Native validation rejects invalid modes, counts and
durations. Gameplay transport, activation clocks, caching and editor integration
remain to be implemented.

## PNG animation sequence foundation — 2026-10-16

Animation settings now persist on backgrounds, default metasprite PNGs and
conditional alternates. The shared model validates frame paths, durations,
optional origins and playback settings. An immutable timeline selects frames
using emulated ticks for loop/ping-pong/play-once. Managed timing/persistence tests
and the Debug UI build pass. Runtime packet creation rejects enabled sequences
until native playback exists, preserving existing static behavior.

Next work: editor controls, native transport/timing parity, activation clocks,
shared decoded artwork caching and memory estimates, then all-system live tests.
The feature is not yet available for gameplay or editor preview.

## Cross-system screen-edge continuity — deferred, updated 2026-10-16

The user expanded the deferred NES offscreen-tracking proposal to all supported
systems: NES, GB/GBC, GBA, PCE, SMS, SNES and WonderSwan. Preserve available
offscreen graphics/positions and recognize partial groups from visible anchors
and relative layout. Maintain offscreen origins, clipping PNGs during rendering
with native masks, windows and priority preserved. Add limited evidence-based
tracking where games remove offscreen members, with stale-state safeguards;
previous position or hardware sprite indices alone must not establish identity.

Verify all four edges, wrapping, entry/exit, missing members, oversized PNGs,
state/scene changes and native restoration. Use system-specific capture and
coordinate handling, including NES mapper timing and X wrap, and affine/field
geometry where applicable. PNG alpha remains the replacement mask. This is a
planning-scope update; implementation remains deferred until resumed.

## Woven pose conditions retain field values — 2026-10-16

Live alternating-pose tests exposed newest-field RAM values being applied to
both fields. SNES now captures only configured replacement bytes before storing
each woven field. Metasprite conditions and alternate PNG selection use the
matching field's snapshot, with both field scales considered. Frame reuse clears
these snapshots. The twelve-case Debug/Release live suite and native field-alternate
checks pass. This change covers metasprites; per-field background condition and
camera behavior remains to be audited.

## Rectangular interlace sprites and OBJ windows — 2026-10-16

Focused live Debug and Release coverage now includes ten cases, adding OAM mode-6 16x32
sprites, the hardware's separate-half vertical mirror rule, and native OBJ-window
clipping of full replacement canvases. PNG origins, source removal outside PNG
bounds, alpha and disable restoration remain checked. Changing poses, wrapping
and broader effects are still pending.

## Viewer-authored interlace pack verification — 2026-10-16

`ViewerLifecycle --selection-suite SnesObj` verifies an actual two-member source
snapshot, selection, default enabled creation, transparent 20x16 PNG export,
oversized 22x18 PNG import, saved pack decoding and live whole-frame replacement.
The eight-row source-coordinate offset survives reload. Debug and Release native
cores pass; this is a
hidden view-model/file/runtime test, not a visible desktop interaction test.

## OBJ-interlace mode/group checks — 2026-10-16

Seven focused live Debug and Release cases pass: weave, current-field, bob, bob-blend,
two-member source-coordinate grouping, horizontal mirror and vertical mirror.
Output checks cover whole frames, both parities, PNG-only alpha, oversized and
shifted artwork, originals outside PNG bounds and disable restoration. Separate
hires subscreen phases retain source Y explicitly. Viewer-created round trips,
rectangular mirroring, changing poses, wrapping and effects still need coverage.

## Initial OBJ-interlace capture and viewer integration — 2026-10-16

The PPU now captures alternate source rows and native coverage with full-source
vertical origins. Output field samples retain their source Y through no-math
field mapping. Sprite Viewer retains full artwork in a 512-row source-coordinate
preview with doubled sprite Y placement, so relative group offsets use the same
units as capture. Raw OAM Y is unchanged.

Live tests pass distinct red-even/blue-odd Debug and Release capture,
whole-frame shifted oversized PNG output, PNG-only alpha and disable restoration.
`--snes-obj-interlace` runs this focused probe. Support remains experimental:
broader viewer-created groups, geometry, clipping, effects and deinterlace tests
remain required. Earlier unsupported notes below are historical.

## SNES field-source artwork coordinates — 2026-10-16

Sparse SNES canvases now accept an explicit full-source Y coordinate per captured
pixel, keeping PNG origins, margins and member attributes separate from output
field positioning. Expansion, weave and bob preserve that coordinate. A pixel
without source geometry keeps native output. Native tests cover both fields,
PNG-only alpha and shifted/oversized artwork; Debug and Release pass. Actual
OBJ-interlace capture and viewer coordinates remain to be connected before
gameplay support can be enabled.

## SNES field-row recognition groundwork — 2026-10-16

Runtime source-row step/phase metadata permits explicit SNES alternate-row
captures to match full saved members without inventing missing texels. All
fetched rows must be complete and consistent; ordinary partial captures still
fail recognition. Group matching requires consistent phase, step and field.
Both Debug and Release native suites pass both-parity and rejection checks.
The PPU does not emit these sparse captures yet: OBJ-interlace still needs its
vertical mapping and gameplay verification before enabling replacement output.

## SNES mixed-member priority and palette — 2026-10-16

SNES full-canvas PNG pixels now use the nearest logical member rectangle's
priority and palette attributes. Current OAM rotation resolves overlap/ties;
original sprite transparency is never an attribute mask. Logical coordinates
keep shifted origins, enlarged artwork and hires expansion aligned.

Release live checks pass native BG insertion, mixed priority, partial alpha,
shifted origins, 2x PNG artwork, palette-dependent color math and rotated overlap.
Release standalone compositor checks pass too. SNES OBJ-interlace capture remains
an open compatibility task.

## SNES unmatched lower native sprites — 2026-10-16

Fetched coverage retains color index, and render-time capture records additional
opaque lower OBJ with current CGRAM color, priority code, palette and transient
OAM identity. Existing winner fields handle single-object pixels. The SNES
compositor excludes matched originals and resolves lower native objects under
PNG alpha through its BG priority, window and color-math stages. Matching and CPU
memory sampling do not change. Hires/field mapping retains samples and tags odd
field IDs; frame reuse clears them, and unsupported hidden-object math keeps the
native fallback.

Both native suites and live foreground suites pass reveal, partial alpha, OBJ
windows and palette-dependent fixed-color math. All 49 ordering fixtures pass in
Debug/Release. The full Release hires probe passes 15 cases but fails the known
unsupported OBJ-interlace case; it does not establish full interlace support.
All 15 supported Debug hires/field checks pass with OBJ-interlace excluded.
Release packaging is rebuilt and
verified. Broader live field overlap and mixed-member priorities remain open.

## WonderSwan BG0/BG1 insertion and ordered native reveal — 2026-10-16

WS supports MBGG bands 4–6 in native order BG0 → band 4 → low OBJ → band 5 →
BG1 → band 6 → high OBJ, followed by final foreground overlay. Native-priority
PNGs remain attached to BG0/BG1; existing conditions, draw slots, scrolling,
camera and window sources remain available. Managed/native activation and editor
controls enable the positions. Capture retains both BG stages for sprite-only
packs and defers frames lacking complete stage data. The ordered resolver keeps
128 OAM identities and inside/outside window classes transient. Captured native
high-priority classification is corrected without changing PPU arbitration.

Both native suites, live band/overlap suites and hidden-editor persistence checks
pass. Live overlap without a BG PNG pack verifies that an opaque low replacement
retains BG1 coverage while holes reveal a later high native OBJ and partial alpha
mixes the outcomes. All 21 WS fixtures pass in Debug and Release. Both UI builds
pass and the Release executable/dependency hash is verified.
Initial independent background-priority integration now covers all target systems;
the remaining compatibility matrix and documented native-rendering gaps stay open.

## SMS PNG insertion bands and tile priority — 2026-10-15

SMS supports shared MBGG insertion positions 4–6 with the existing foreground
overlay and per-position draw slots/conditions/camera settings. UI, managed and
native activation paths enable them while retaining one native hardware BG.
The ordered decoded-row compositor uses SMS tile-priority eligibility and native
PNG alpha outcomes. SMS has no separate behind-BG OBJ flag, so its lower two bands
can coincide against native content. CRAM-dot pixels are marked unavailable for
reconstruction to preserve the native override.

Managed and both native suites pass. Debug/Release foreground tests check bands
on Mode 4 and legacy Graphics I, including a required priority-hidden native OBJ,
opaque/transparent/partial artwork and foreground overlay. Thirteen Debug/Release
legacy/Mode 4 fixtures and both hidden-editor persistence checks pass. Both UI
builds pass with zero warnings/errors; Release Mesen.exe is present and its native
dependency hash matches. Live CRAM timing, dynamic mode/palette
changes and broader overlap remain open. WonderSwan insertion bands are next.

## PCE PNG insertion bands and decoded native reveal — 2026-10-15

Standard single-VDC PCE now supports MBGG positions 4–6 with existing PNG draw
slots, conditions, scrolling and placement. UI and managed/native activation
guards accept these positions; SuperGrafx remains unsupported. PCE capture marks
its frames, and the shared ordered resolver reads decoded native slices with
their offsets, preserving hidden winning OBJ blocking before BG insertion while
PNG transparency can reveal lower native sprites. Nonzero hardware colors are
captured with opaque alpha independently of palette-entry alpha. Visible native
winners use output-time colors; other native reveal currently uses fetched colors.

Managed and both native suites pass. Debug/Release live band/foreground checks and
hidden-editor save/apply/reload pass. Nine Debug/Release HuCard/VDC1 cases cover native
BG priority/hiding, wide sprites, holes, horizontal mirroring and clock dividers
4/3/2. Both UI builds pass and the Release
package's DLL hash matches the rebuilt core. Live lower-OBJ band overlap, dynamic
lower palettes/grayscale and broader layouts remain open; SMS/WS insertion bands
still require integration.

## GB/GBC independent background PNG bands — 2026-10-15

GB/GBC now enables MBGG positions 4–6 shared with NES: below behind-BG sprites,
between those sprites and tiles, and above tiles/below front sprites. Foreground
overlay stays last. Ten draw slots, CPU conditions and existing placement/camera
settings remain available per position. Native CGB tile priority, master priority
disabled, PNG alpha and native sprite arbitration remain part of composition.
Hidden-window underlays retain color index and scrolling; BG bands can sample BG
coordinates beneath a visible window. Background-only GB frames have an explicit
capture flag. No new packet layout or CPU-memory snapshot is introduced.

Managed and Debug/Release native checks pass. Live CGB band checks pass in both
foreground suites; twelve GB/GBC fixtures and hidden-editor save/apply/reload
checks pass in Debug and Release. Both UI builds pass with zero warnings/errors;
the Release executable is present and its dependency DLL hash matches the core.
Earlier NES-only MBGG descriptions are superseded;
PCE/SMS/WonderSwan independent insertion integration remains open.

## GB output-time palette capture — 2026-10-15

Native rows retain per-pixel output colors alongside the fetched palette used
for recognition. GB capture records these before background or sprite winner
filtering; native reveal uses them without changing PNG colors or content matching.
Debug/Release native checks distinguish fetched and displayed palette colors,
fallback for absent samples, and retained-frame copying. Live CGB assertions
verify samples for hidden winning and lower objects. Both foreground suites and
all twelve Release GB/GBC gameplay fixtures pass. Both cores and Release UI build;
the packaged DLL hash matches. A further live DMG test steps the PPU, performs an
OBP1 memory-manager write across a hidden lower sprite row, and checks old/new
output colors against the unchanged fetched palette. A transparent replacement
of the upper object must reveal all eight recorded colors from that same frame.
Debug and Release pass this test with the full foreground suite. CPU instruction timing
of a ROM-issued write and broader game-specific raster effects remain open.

## GB/GBC sprite arbitration before background priority — 2026-10-15

The shared ordered-sprite scene resolver now supports GB/GBC fetched rows as well
as NES. It resolves PNG-alpha sprite outcomes in captured hardware order before
applying native background visibility. This preserves hidden winning OBJ blocking
while allowing replacement holes/partial alpha to reveal lower native objects.
GB flips and captured palettes remain intact; hidden windows use retained BG-map
eligibility and PNG composition. Member rectangles supply replacement class/order,
never original sprite transparency. Missing backdrop capture keeps the previous
fallback. Configured-address memory sampling is unchanged.

Debug/Release native regression suites pass. Twelve Debug/Release GB/GBC gameplay
fixtures and the cross-system foreground suite pass, including a new live CGB
hidden-winning-OBJ test with opaque, transparent and partial artwork. Both live
foreground suites pass. Release packaging succeeds with zero UI warnings/errors,
the executable is present and its native dependency matches. Broader mode/limit/raster verification and
broader GB/GBC insertion-band verification remain required.

## NES mixed-priority group regions — 2026-10-15

NES artwork now inherits front/behind-background priority and sprite order from
the member rectangle under each destination pixel. Oversized margins use the
nearest rectangle; ties use OAM order. Attribute selection never samples original
sprite transparency. Native Debug/Release suites cover opposite member priorities,
PNG holes/partial alpha and expanded margins; a further Debug check covers an
unrelated native sprite ordered between the members.

A live NROM check captures adjacent back/front members, recognizes their relative
graphic arrangement and verifies opaque/partial artwork and oversized margins
against opaque native tiles. Debug and Release foreground suites pass, including
the existing cross-system and independent NES/GBA/SNES priority checks. A second
live configuration overlaps the members by four pixels, shifts artwork origin
left by two pixels and verifies overlap OAM ties plus both oversized margins with
opaque/partial PNG alpha. Debug and Release pass. Both
application cores are rebuilt. Broader member-layout/OAM/mapper cases remain
open; these representative tests do not establish complete NES compatibility.

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

## Sliding-window smoothing

Automatic placement also protects finite PNG coverage. For the controlling
non-wrapping CPU-camera PNG, the window begins shifting before a boundary trigger
if a decorative margin would otherwise pass the image edge. Coverage takes
precedence over slide speed and release delay; reversal is constrained to the
remaining covered range. At camera X=766, a 1024px PNG with a 256px gameplay view
has only two native pixels available on its right. Additional movement into that
margin requires artwork padding or camera movement that reveals covered pixels.
Wrapping PNGs and manual placement are unaffected. This does not fill transparent
artwork or invent coverage when the PNG is too narrow for the output.

Each PNG can optionally use **player X RAM speed** for horizontal sliding. This
is separate from the CPU camera and from RAM boundary conditions: those still
select the active edge, while signed player speed controls slide direction there.
In Tilemap Viewer, enable gameplay positioning for the PNG, choose Automatic,
and enable **Use player X RAM speed for sliding**. Enter the CPU address in hex,
select signed/unsigned 8-bit or signed/unsigned 16-bit little/big endian, and set
the multiplier and maximum slide pixels/frame. Signed 8-bit `$18` and `$E8`
yield +24 and -24. With multiplier 0.0625, each produces 1.5 native
pixels/frame, subject to the cap. That multiplier is a starting example and must
be calibrated to the game's speed units. Zero or unavailable speed stops motion;
it never invokes the fixed-speed instant shortcut. At an active edge, positive
signed speed moves right and negative moves left, clamped between normal position
and that edge. Turning around reverses the slide without requiring a camera/flag
change. Unsigned formats retain target-directed movement. When scrolling resumes,
speed magnitude controls the return toward normal. Release delay still controls
when the boundary is released.
X alone drives horizontal sliding; a vertical speed value must not be combined
with it. There is no vertical gameplay sliding or Y-speed option in this feature.
Save and apply edits. Older packs retain fixed-speed behavior by default.

In each PNG's gameplay positioning settings, slide speed accepts fractional native
pixels per emulated frame (for example 0.1, about 6 pixels/second at 60 FPS).
Zero remains instant. Fractional distance accumulates internally; the displayed
window still moves in whole output pixels without filtering the gameplay image.
Boundary release delay is optional, in emulated frames (0 disables it, maximum
600). Start with 6 frames to filter brief inactive boundary readings. This holds
the last edge target until neither boundary has matched for the configured time;
reacquiring the edge restarts the timer. The opposite edge switches immediately.
Conflicting/missing checks or an inactive PNG return toward normal without holding
the old target. The delay also applies when a valid CPU camera leaves a limit.
Save and apply background edits to activate changes. Boundaries, speed and delay
are pack-only settings. Existing pack saves retain integer speed values and delay
defaults to off; obsolete Video/profile boundary fields are ignored.

## Music and sound-effect replacement — initial rollout, 2026-10-15

Implemented across NES, GB/GBC, GBA, PCE, SMS/Game Gear, SNES and WS/WSC in
**Debug > Audio Replacement Editor**. Each system now has CPU-write event hooks,
optional RAM checks, shared OGG/PCM WAV playback, whole-native-output mute, music
looping/fades and independent one-shot effects. SMS/GG and WS/WSC also support
I/O-port write rules. SNES separates main CPU/SPC triggers and RAM-condition
sources. Packs save under `HdPacks/Audio/<system>/<ROM SHA1>/music.json` and load
before CPU execution.

Event/context track selection is implemented: changing a context byte alone does
not trigger playback. The first matching music rule and first matching effect
rule run independently. Effects overlap different assets, with restart/ignore
for repeated instances of the same asset. Up to 32 effect assets are supported.
Native hardware continues running when its combined output is muted.

Generated fixtures cover all seven requested systems, including interpreted SPC
instructions, WS/WSC segmented addressing, aligned/split word writes and I/O
source isolation. Editor checks cover persistence, preview/apply/reload, source
controls and pre-execution loading. Real-game testing and valid OGG/device
listening remain deferred by request; special hardware/driver coverage is still
limited. Initial system integration is complete, not every audio refinement.

Remaining audio work includes separate global SFX volume, same-effect overlap,
crossfades/effect fades, loop-point tools, disk streaming/cache cleanup, precise
sample scheduling, playback-position state restoration and broader synchronization.
See [music-replacement.md](music-replacement.md) for current setup, system address
ranges, verified fixtures and limitations. The main metasprite/background goal,
including its renderer gaps and deferred features, remains open.

## Decorative widescreen — initial implementation, 2026-10-14

Per-PNG positioning: gameplay position, slide speed, signed CPU-camera limits and
RAM boundary condition groups now live in each background definition. In the
Tilemap Viewer, enable "Use gameplay positioning saved with this PNG", edit its
values and save/apply background edits. A newly imported PNG has its own centred
positioning defaults. Camera limits, RAM boundary conditions, slide speed and
release delay are defined only by the pack. Video Options retains Gameplay
position (Centre/Left/Right/Automatic), widescreen enable, aspect ratio, margins,
brightness and fade. An active PNG's saved positioning takes priority over that
selector. Without an active positioning definition, the selector supplies manual
placement; Automatic remains centred. The former copy-to-PNG button and legacy
Video/profile boundary fallback have been removed. Older profiles still load,
but their obsolete boundary fields are ignored and omitted when saved again.

Only an active, matching, margin-enabled PNG can control positioning. Existing
background activation conditions select the level/scene, allowing different PNGs
to use different level limits, flag addresses/values and slide speeds. If multiple
matching PNGs define positioning, the first in pack order controls the viewport;
other layers can disable their positioning override. Numeric limits use that
PNG's CPU camera; RAM flags also support hardware-scroll/screen-locked PNGs.
Changing the controlling PNG resets the slide history, avoiding offsets carried
from another level. Aspect ratio, margins, dimming and fade remain presentation
settings and can still be saved globally or per game.

MBGA transfers per-background positioning with bounded condition groups and strict
scalar/truncation checks. MBG1–9 retain their prior defaults. Additional flag bytes
are captured only for the selected PNG controller; inactive scene rules are not
read. Managed/native packet tests and the actual NES background editor save/reload
workflow passed. GBA/SNES fixtures add five per-PNG scene cases: two different
camera limits, RAM edge selection, conflicting flags, and selection of a later
matching PNG when the earlier scene is inactive. Older descriptions of video-only
positioning are superseded.

RAM boundary flags are now implemented as an alternative to numeric camera
limits. Select Automatic positioning, choose the position background ID, and
enable "Use RAM boundary flags instead of camera limits". Each edge accepts up
to eight hexadecimal `ADDRESS OP VALUE / MASK` lines using the existing PNG
condition parser (`== != > < >= <=`; mask optional, defaults FF). Every condition
for that edge must match. Empty edges are inactive. Left-only/right-only matches
select the respective edge; both, neither, or any unavailable configured byte
select normal centre/custom-left placement, with the existing slide speed.

The selected PNG must be valid, match its activation conditions and have margin
extension enabled. RAM flags also work with hardware-scroll or screen-locked PNGs;
a CPU-camera PNG still requires its camera bytes to be available. Flags replace
numeric camera limits and do not change PNG camera/scroll coordinates. The new
fields save in per-game profiles/global defaults; old profiles default flags off.
Invalid drafts show a message and preview normal positioning. Invalid profile or
global-default saves are rejected, retaining existing saved settings.

The emulation thread captures only the configured flag addresses when widescreen,
Automatic and RAM flags are enabled, using the existing side-effect-free CPU-bus
reader for all systems. Addresses shared by groups/cameras/edges are deduplicated.
The decoder reads the frame snapshot; rewind retains historical sampled bytes.
Native tests cover masks, comparisons, grouped/empty/conflicting/missing rules,
hardware-scroll PNGs and inactive-background fallback. Actual editor tests cover
profile persistence, invalid-save rejection, managed/native configuration layout
and the vertically wrapped settings UI. Live GBA/SNES fixtures add five flag cases
per fixture and assert configured-byte capture/deduplication and viewport pixels.
Older pending references to optional RAM boundary flags are superseded.

Brightness-fade correction: the user clarified that the fade should join gameplay
to the selected side brightness, with no blur. Fade width now measures outward
from the gameplay viewport. The adjacent decorative pixel is full brightness;
brightness decreases linearly to the configured side percentage over that width,
then stays at that percentage. Gameplay pixels are unchanged. A width of zero
applies uniform side brightness immediately; 100% side brightness makes the fade
invisible, and 0% fades from the gameplay edge to black. The transition follows
viewport movement. The previous outer-edge-to-black fade description is superseded.
The existing saved fade-width setting is retained with this corrected behaviour.

Large-canvas validation continuation: live GBA tiled/affine/bitmap and SNES LoROM
mode 0/1/7 fixtures now render patterned 256-, 512- and 1024-pixel replacement
canvases. Patterns distinguish each 256-pixel page, with CPU camera X=1014 and
signed FF:F6=-10, so native-map wrapping or incorrect negative coordinates cannot
pass as a uniform colour. Checks compare every native viewport pixel and the
patterned side margins against independent expected coordinates. Eight fixtures
each run four presentation variants plus three canvas widths: all 56 checks
passed in each of Debug and Release (112 total). Snapshots contain exactly the
condition byte and two configured camera bytes.

Automatic-position history now resets when no valid wider canvas is requested
(including zero custom margins) and when the decoder stops. This prevents a
previous position leaking into a later session. A native regression checks the
zero-margin/re-enable transition. Debug/Release native suites and UI/native builds
passed. Optional CPU boundary flags, remaining filters/hires/interlace modes and
full visible desktop verification remain pending.

Profile/live-validation continuation: Video settings now use a separate widescreen
preview draft. "Save for this game" immediately saves only widescreen fields to
`GameConfig/Widescreen/{Cpu}/{ROM_SHA1}.json` (under the configured game-settings
folder). Profiles automatically apply during the supported GameLoaded path;
switching/stopping games clears the prior profile. A profile overrides only
widescreen fields, preserving unrelated video settings. "Remove game profile"
restores global defaults. "Use as global defaults" updates ordinary Video
settings, committed/cancelled with the Settings dialog. Closing without saving
the preview restores the saved profile or global defaults. Profile save/remove
buttons persist immediately, independently of the dialog's global-settings save.
Saving a game profile does not overwrite global defaults. Stale editor saves
after a ROM switch are rejected. Invalid profile writes preserve the old file;
malformed profile loading reports the error and uses global defaults.

Dedicated live fixtures now cover GBA tiled/affine/bitmap backgrounds and SNES
LoROM modes 0/1/7. Four variants per fixture check PNG-filled margins, native
viewport preservation, custom widths, fade, arbitrary brightness, both CPU-camera
boundaries and disable restoration: all 24 variants passed in Debug and Release.
The actual profile editor/save/load/remove and system/hash isolation tests passed,
including unrelated-video-field preservation and stale/invalid save rejection.
The real settings view was constructed in a hidden window, rendered and visually
inspected; its brightness numeric control was exercised through the binding.
This is not a full visible desktop/file-picker walkthrough across systems.

Continuation: Video settings also support custom left/right widths (0–1024
native display pixels each), overriding the target aspect ratio. The resulting
aspect ratio is derived from the wider canvas while preserving the native
viewport's pixel aspect. An outer-edge fade width (0 disables it) fades PNG
margins toward black; side brightness remains independently adjustable.

Automatic gameplay positioning can follow an explicitly selected background ID's
active CPU camera. Enter decimal signed left/right limits with left < right.
At/below the left limit, move gameplay left; at/above the right limit, move it
right; otherwise use the normal centre/custom-left placement. The selected PNG
must have margin extension, CPU camera and Follow scroll enabled, valid camera
bytes and matching conditions. A missing/inactive camera or invalid limit pair
uses the normal position. A stopped camera alone does not trigger movement.
Slide speed is 0 for instant changes or 1–64 native display pixels per emulated
frame. Duplicate redraws do not advance motion; decreasing frame numbers/large
discontinuities reset motion. Dimming/fade follow the current gameplay viewport.
These controls support both global defaults and the per-game profiles described
above. Optional CPU boundary flags remain pending.

Native tests cover asymmetric placement, fade and signed camera limits, instant/
smooth motion, redraw stability, missing bytes and rewind/reset fallback. The live
NROM fixture adds asymmetric widths/fade and both automatic camera boundaries.
Both native configurations and all nine live NROM variants passed. The full
Debug gameplay regression suite passed, including 59 live asymmetric-canvas
checks across NES, GB/GBC, SMS, PCE and WS. GBA/SNES dedicated live widescreen
checks remain pending; their existing gameplay regressions passed. The actual
video-config serializer round-tripped all new fields. Debug/Release builds passed.

The user resumed this feature. Video settings now offer enable/disable,
16:9/21:9/custom width-to-height ratio, centre/left/right gameplay placement,
and a directly editable 0–100% side-brightness numeric box with up/down arrows
in 1% steps. These are saved with video settings. Native viewport pixels retain
their dimensions and colours; the compositor adds scenery margins rather than
stretching the gameplay across them. Existing native pixel-aspect correction
is retained, with canvas rounding at the replacement scale. A requested ratio
narrower than the native display does not crop or shrink gameplay.

Each background has an opt-in "Extend this PNG into widescreen margins" setting,
saved in the pack and transferred in MBG9. Older MBG1–8 packets still load with
extension disabled. Only active, matching backgrounds contribute scenery, using
their existing scroll source, CPU camera, placement, rates, wrapping and PNG alpha.
Missing camera bytes suppress scenery; transparent/uncovered margins are black.
Native sprites and tiles are not extrapolated. Margin PNGs compose in pack order;
the independent PNG priority-layer feature remains separate pending work.

CPU camera is recommended for continuous scenery beyond hardware map wrapping.
Hardware coordinates are extrapolated from a captured reference on each row;
this does not implement off-screen affine/mosaic/window emulation. Extension
requires the same compatible video-filter/capture path as PNG replacement;
unsupported filters can show black margins. Rotation and subsequent output scaling
operate on the extended canvas. Side dimming is applied after PNG alpha blending
and only outside the native viewport.

Managed pack/packet checks and native tests cover opt-in, arbitrary brightness,
alpha composition, negative signed CPU cameras, missing memory, row references
from shared/GBA/SNES/underlay captures and negative hires coordinates. Live NES
NROM sprite-0 split tests cover 16:9/21:9/custom, centre/left/right, 37%/0%/100%,
opt-in and disabling restoration without modifying native viewport pixels.
Other-system live widescreen and visible desktop checks remain pending.

Remaining widescreen work: optional CPU boundary flags and broader system/filter/
mode and visible desktop checks. Custom widths, per-game presentation profiles,
edge fade and camera-limit-based instant/smooth sliding are now implemented.

The original proposal was to extend
selected replacement PNG backgrounds into a wider output canvas while keeping
the original gameplay viewport and game logic intact. No off-screen native
tiles or sprites are implied; off-screen metasprite capture remains a separate
deferred proposal.

Proposed Video settings: enable Decorative widescreen; aspect ratio (16:9,
21:9 or custom); optional custom left/right widths in native pixels; side
brightness as a user-editable percentage numeric box with up/down arrows,
accepting every whole percentage from 0% to 100% in 1% steps (100% full
brightness, 0% black), rather than a preset-only selector; optional edge fade;
gameplay viewport position (centred, fixed left/right, or automatic at camera
limits); and instant/smooth slide speed. The Tilemap Viewer would provide a
per-background "Extend this PNG into widescreen margins" checkbox so scenery
can extend while HUD/other layers stay inside the native viewport.

CPU-camera coordinates should keep scenery aligned while the gameplay viewport
slides. At a configured left camera boundary, move the viewport left; at a right
boundary, move it right; otherwise centre it. Use game-specific camera limits or
configured CPU-memory boundary flags: a stationary camera alone cannot distinguish
a level boundary from a stationary player or paused game. Dimming/fade must follow
the current viewport and affect only decorative margins. Implementation would
require a wider compositor canvas, native viewport offsets, appropriate priority
and clipping, and persistence of per-game settings. The initial canvas/settings
above are implemented; the remaining proposal is not yet complete.

## Future feature: TrueType text replacement — deferred, 2026-10-16

Approved editor design: a dedicated Text Replacement Editor with character
mapping, font preview/size/spacing/offset controls, native/custom palette-color
rules, RAM activation conditions, target layer/dialog region and saved profiles
for dialogs, menus and other text. A Tilemap Viewer action, "Map selected tiles
to text...", should open the editor with the selection. Tile Viewer selection
can also supply character mappings. Implementation remains deferred.

User-approved future work: replace mapped native tilemap text with TrueType font
glyphs in the game window. Profiles map tile indices or graphics addresses to
characters, with bank/content context where addresses are reused. Multi-tile
characters need grouped mappings. Target profiles by native layer and optional
screen/dialog region, and enable them with user-configured CPU RAM conditions
(for example, an address equals 2 while a dialog is open). Read only configured
addresses, consistent with existing replacement conditions.

Provide font selection, size, spacing and position offsets. Support inheriting
native palette colors, explicit color overrides and per-palette color rules.
Suppress the matched native text pixels and render cached font glyphs with native
layer priority, scrolling and clipping preserved. Larger glyphs may extend past
the source tile bounds, with optional dialog-region clipping.

Start with tile-aligned glyph placement to retain native typing effects and
spacing; proportional text-run layout is a later extension. Target NES, GB/GBC,
GBA, PCE, SMS, SNES and WonderSwan through shared profiles and system-specific
rendering integration. Sprite-based text is a possible later extension.
This feature is planned/deferred; no implementation has started, and it is not
an additional completion gate for the active metasprite goal.

## Future goal: PNG background blend modes

Add per-background Normal alpha, Add and Subtract modes across NES, GB/GBC,
GBA, PCE, SMS, SNES and WonderSwan through the shared compositor. Use a blend-mode
dropdown, with optional opacity/strength control, and respect PNG alpha.
Blend against the image beneath the PNG at its assigned drawing priority;
integrate with the planned independent PNG background priority layers.
Define ordering relative to native SNES/GBA colour effects so that effects are
not unintentionally applied twice. Preserve placement, camera scrolling,
conditions, clipping and saved definitions. HDnes Alpha/Add/Subtract modes are
the reference. The user requested recording this as future goal work; no blend
mode implementation is included yet.

## Active goal: PNG animation — added 2026-10-16

The user moved PNG animation from deferred future work into the active goal on
2026-10-16. Implementation and verification are now required for goal completion.
Background PNGs and metasprite artwork (including CPU-conditioned alternates)
will each support a sequence of separate PNG frames across all supported systems.
Planned controls include
add/remove/previous/next, duration in emulated frames, loop/ping-pong/play-once,
shared scale/origin with optional per-frame origins, and synchronized timing
with an optional restart-on-activation mode. Pausing would freeze animation;
turbo would follow emulation speed. Existing recognition, conditions, camera
scrolling, priority and clipping would continue to apply.

HDnes frameRange is a repeating eligibility condition:
`frame number % cycle length >= threshold`. It could inform timing, while the
sequence UI would avoid requiring users to author a condition for every frame.
Validate positive cycle lengths rather than reproducing zero-divisor behavior.

Memory/loading considerations: decoded RGBA artwork costs approximately four
bytes per pixel per frame, excluding previews, temporary buffers and GPU copies.
Only the selected frame should be drawn. Prefer shared immutable image caching
and no buffer copies on frame switches; show estimated decoded memory usage.
Small metasprite sequences could preload; large background sequences may need
bounded caching and prefetching to prevent stutters. Measure runtime cost before
claiming an FPS budget, particularly for SNES composition. Sprite-sheet import
is another possible later extension and is not required for the initial animation
implementation. Verify editor preview, PNG/JSON round trips, frame timing and
playback modes, pause/turbo, activation/restart behavior, origins/scales,
conditional alternates, system-specific composition and bounded asset handling.
Shared sequence, persistence and timing groundwork is implemented; editor and
native playback integration remain pending.

## CPU-camera background scrolling

CPU-camera controls and runtime integration cover NES, GB/GBC, GBA, PCE, SMS,
SNES and WonderSwan. All background composition paths use the shared camera-aware
sampler, including GBA change detection/layer effects and SNES captured-layer and
fallback paths. Native Debug/Release tests verify those staged paths use CPU
coordinates instead of captured hardware scroll. Camera readout, save/reload and
activation assertions pass in all eight hidden editor configurations; GBC's
shutdown stalls in this run while the others complete. Existing background mode
support limits still apply; these checks do not prove every game-specific effect.

Pixel offset + page has an independent Signed page checkbox for each axis.
It defaults off: page FF is 255. When enabled, pages 80–FF represent −128 to −1;
with page size 256, FF:F6 is −10, FF:FF is −1 and 00:00 is 0. Other camera formats
remain unsigned. The flag is saved with the background and shared camera profile,
and copied when unlinking. MBG8 transports per-axis flags while older MBG7
camera packets retain unsigned pages. Managed/native Debug/Release tests cover
the signed transition, negative sampling and strict packet parsing. The live
NROM raster fixture verifies FF:F6 negative sampling in Debug/Release, and hidden
NES authoring verifies saved-camera unlink retains the signed flag with clean
shutdown. Both UI builds pass and the Release executable is updated.

The CPU-camera section also offers Save camera for this game, Link saved camera
to this background, and Unlink camera (keep settings). One camera configuration
is stored per ROM/system pack and survives removing backgrounds. Linking enables
CPU-camera scrolling while preserving each background's speed, placement, scale
and conditions. Linked backgrounds resolve the saved addresses/formats at runtime;
saving a revised camera updates every linked background. Unlinking copies the
current saved configuration into that background for independent editing.
Profiles participate in background conflict checks, and failed saves leave the
previous saved profile and file intact. Managed tests verify two linked layers,
shared updates, JSON persistence and missing-profile rejection. The hidden NES
editor suite verifies save/link/unlink, failed-save rollback and shared-pack
preservation, and completes shutdown cleanly. Debug/Release UI builds pass.

Each background can select Hardware scrolling or CPU memory camera. CPU mode
uses independent X/Y axes: Fixed, 8-bit unsigned, 16-bit low-byte-first,
16-bit high-byte-first, or Pixel offset + page. Addresses are CPU-bus addresses
entered in hex. Contiguous 16-bit formats read the start address and the next
byte. Paged format reads separate offset/page addresses and computes
`page * page size + offset`; page size defaults to 256 native pixels. Fixed axes
read no bytes. An 8-bit-only camera still wraps at 256; use its page counter or a
full 16-bit coordinate for longer travel.

CPU-camera coordinates drive PNG sampling independently of hardware nametable
wrapping and NES raster splits. Existing speed multipliers, placement, PNG
wrapping, native layer visibility/priority and CPU conditions still apply.
Disabling Follow selected scroll source keeps fixed-screen placement. Missing
camera bytes suppress the background for that frame, including native-layer
hiding. Only configured camera/condition bytes are captured, with deduplication.
The editor shows current decoded camera values and refreshes on edits/frames.
Definitions persist in JSON; MBG7 transfers camera axes alongside existing
settings, and native loading retains MBG1–MBG6 compatibility.

Managed tests cover formats, page size, configured bytes and JSON/packet
persistence. Native Debug/Release tests cover MBG7 parsing/truncation, invalid
formats, missing bytes, selected-address capture and CPU positions 768/1024 past
the NES 512-pixel coordinate wrap. The live NROM sprite-0-hit split fixture passes
in Debug/Release with a 781-pixel CPU camera while native raster scrolling stays
unchanged. The hidden NES editor assertions verify readout, save/reload and
activation; the run stalled during emulator Stop and was terminated after all
assertions passed. Both Debug and optimized Release UI builds succeed, and the
Release executable is present. Broader system-specific CPU
camera gameplay coverage remains pending.

## NES background PNG raster-scroll override

The Tilemap Viewer exposes "Ignore raster scroll changes (NES)" per background
PNG and a reference scanline (0–239, default 32). When enabled with hardware
scroll tracking, the PNG uses the selected scanline's captured scroll offset
throughout that frame. Choose a scanline below a fixed HUD to follow gameplay
scrolling. X/Y multipliers, placement, wrapping, CPU conditions, native priority
and destination clipping still apply. Sprite 0 hit and the game's scroll writes
continue normally; the setting only changes PNG sampling.

Reference selection uses the uncropped NES frame, so overscan does not shift the
configured scanline. If that scanline has no valid native background sample,
sampling falls back to normal raster tracking for that frame. Disabled hardware
scroll tracking keeps fixed-screen placement. The option is NES-only and defaults
off for existing definitions. JSON persists both settings; MBG5 transfers them
alongside rates and comparison conditions, retaining older packet support.

Native tests verify split/uniform sampling, cropped output, speed multipliers,
missing-reference fallback and clipping. A generated NROM sprite-0-hit fixture
checks the game's real raster split, choosing HUD/gameplay references, half-rate
tracking and native restoration in Debug and Release. Both native suites pass;
the seven existing NES Debug gameplay cases also pass. Hidden NES authoring
verifies save/reload and activation, including clean shutdown. The optimized
Release UI build completes without warnings/errors and includes the option.
Representative generated-ROM coverage does not establish every game's
mid-frame register sequence or split configuration.

## Background scroll multipliers

Background PNGs now expose independent X/Y scroll multipliers in the Tilemap
Viewer, enabled while Follow hardware scrolling is selected. Defaults are 1;
0 holds an axis fixed, 0.5 gives half-rate parallax, 2 doubles the rate, and
negative values reverse movement. Values range from -16 to 16 with three-decimal
runtime precision. Placement, wrapping and CPU conditions continue to apply.
Settings persist in JSON; custom rates transfer through MBG4, with older packets
defaulting to 1. Native adapters capture scroll offsets separately from sampled
coordinates so ordinary wrapped tilemaps keep a consistent offset across edges.
Artwork sampling uses native-pixel steps. These controls scale hardware-driven
scrolling; independent timed artwork movement is a separate future extension.

Managed persistence/coordinate tests and native rate/wrap/packet checks pass.
Live WonderSwan monochrome/color fixtures check X=0.5, Y=-1 across wrapped map
edges with patterned PNGs and CPU gating. Hidden SNES authoring verifies saving
and reloading both rates. Broader custom-rate affine/Mode 7, mosaic, raster-scroll
and register-rollover behavior still needs dedicated gameplay verification.

## CPU memory comparison operators

Metasprite, alternate-PNG and background conditions support `==`, `!=`, `>`, `<`,
`>=` and `<=`. Enter one hexadecimal condition per line, for example
`$C000 >= $20 / $FF`. The optional mask defaults to FF and applies to both the
captured unsigned byte and expected value before comparison. Every line must
match; unavailable addresses fail all operators, including `!=`. Only configured
addresses are sampled. Existing saved conditions default to equality and legacy
`=` text remains accepted, but formatted equality now uses `==`.

JSON stores the operator with each condition. Runtime transfers use MSP4/MBG3
when operators other than equality are present, while equality-only packs retain
their older packet versions. Unknown operators and truncated packets are rejected.
Managed/native Debug/Release boundary, mask, persistence, alternate selection and
packet tests pass. Live WonderSwan checks exercise all six comparisons; hidden GBA
authoring assertions verify relational alternate/background conditions, but that
editor run stalled at emulator shutdown after debugger release (session 11336 was
interrupted). This is an unresolved lifecycle concern, not a completed editor run.

## Slowdown issue: resolved

The user confirmed closure of the slowdown issue on 2026-10-13 and requested
that it be marked solved. Further slowdown investigation is no longer pending.
Earlier deferred-investigation notes and benchmark limitations below are
historical records; this status supersedes them. Resolution is user-confirmed,
not a claim that those earlier synthetic checks established a universal fix.

## Goal requirements: independent PNG background priority layers

### Required background control: hide original layer

User added a per-replacement "Hide original layer" toggle to the goal on 2026-10-13.
The toggle, JSON persistence, MBG6 transfer and compositor suppression are now
implemented; full cross-system lower-layer reveal remains under development.
When enabled and the replacement is active (including
matching all configured CPU memory conditions), suppress the assigned native
layer's tile artwork. Transparent PNG areas reveal lower backgrounds, sprites or
the backdrop according to native composition rules, rather than the suppressed
tiles. Keep the layer's scrolling, priority, clipping and hardware-effect rules
for the replacement artwork. When disabled, retain the current PNG-over-native
overlay behavior. Restore native artwork automatically when the replacement is
disabled or its conditions stop matching. Define deterministic behavior when
multiple active replacements share a native layer, and verify alpha, lower-layer
reveal, priority and activation/deactivation across each supported system.

Any active replacement requesting suppression hides that native layer, regardless
of PNG placement or alpha. Other active PNGs assigned to that layer continue to
compose over its remaining layers. GBA/SNES suppress native tile color before
their staged layer composition. The shared direct-color path uses the captured
backdrop and background-independent sprite classes, reconstructing unmatched
decoded native sprites where a hidden background had blocked them. Debug/Release
native fixtures verify lower BG/native OBJ/backdrop reveal, partial PNG alpha and
condition mismatch restoration. A live Debug single-VDC PCE fixture verifies
native and replacement sprite reveal, transparent/partial artwork, CPU gating and
toggle-off overlay restoration. Hidden GBA authoring saves/reloads/activates the
toggle and exits cleanly. WonderSwan lower BG and GB window-underlay capture,
incomplete native sprite captures, SNES lower OBJ reveal and broader effects/modes
still need work; this evidence does not establish complete support on all systems.

User promoted independent priority layers and hiding original background layers
from the future agenda into the goal on 2026-10-13. Both are required for completion
of the expanded goal across all seven target systems. Preserve all existing
metasprite, background export/placement/scrolling and CPU-condition requirements.
Native-layer ordering, a final foreground overlay, NES/GB/GBC/PCE/SMS/WS insertion bands and independent GBA/SNES hardware priorities are implemented. Broader cross-system verification remains pending; hide-original-layer rendering still has documented lower-layer reveal gaps.

Extend PNG backgrounds with independent composition positions inspired by HDnes
`BackgroundsByPriority`. The current HDnes code provides four groups of ten slots
(40 total), including `BehindBgPriority` in addition to the three originally discussed:

- 0–9: below sprites marked behind the native background.
- 10–19: above those sprites, below the native background.
- 20–29: above the native background, below foreground sprites.
- 30–39: above foreground sprites.

Keep native tilemap layer selection, composition position and order within the
position as separate settings. Reuse PNG placement, scrolling, wrapping, alpha
and explicitly configured CPU memory conditions. Preserve existing native-layer
replacement behavior as the default. Replacement PNG alpha must govern artwork
visibility independently of original sprite transparency.

Target NES, GB/GBC, GBA, PCE, SMS, SNES and WonderSwan. NES maps most directly to
the four HDnes positions. Other systems need adapters for their background/sprite
priority rules. GBA and SNES need positions relative to selected native layers or
hardware priorities, rather than forcing all sprites into two categories. Respect
windows, clipping and hardware effects; SNES additionally needs defined main/subscreen
and color-math participation. Capture hidden native layers where necessary to insert
artwork correctly. A final foreground overlay is the simplest common position.

Validate overlapping PNG order, opaque/partial/transparent artwork, native and
replacement sprites at each insertion position, scrolling, clipping, memory gating
and disable restoration across supported modes. Rendering cost depends on active
overlapping artwork, not merely the available slot count. The previously reported
slowdown issue is resolved per the user's confirmation.

## Full PNG canvas implementation (in progress)

Background PNG priority now uses adapter-supplied sprite-class metadata in the
shared direct-color compositor. Artwork on an occluding background layer remains
above behind-background replacement canvases, including their oversized margins;
PNG alpha blends that layer over the replacement. NES, GB/GBC, PCE, SMS and WS
supply their respective priority rules. WonderSwan BG0 stays below sprites while
BG1 covers its low-priority sprite classes. GBA/SNES retain their separate staged
layer/effect compositors. Native alpha regression, live PCE priority cases and
cross-system Debug gameplay checks pass; broader live partial-background-alpha
transitions remain to verify.

The user selected full-canvas replacement as the addition method. A default or
conditional alternate PNG that is larger than the exported metasprite must use
its complete canvas automatically. Its scale and PNG origin align it with the
recognized group's origin; the original member bounds must not crop the artwork.
This requires no separate addition definitions, attached-PNG editor or opt-in
switch. Existing PNG import, load and alternate navigation remain the workflow.

The shared compositor now removes matched source pixels before drawing complete
replacement canvases in hardware order. This fixes crossing replacements where
an original transparent texel previously allowed the lower replacement to win
inside native bounds, despite an opaque front PNG. Native regression cases cover
opaque, transparent and partial-alpha front PNGs in either pack order. GBA and
supported SNES paths also implement full-canvas composition; their remaining
hardware-mode and effect gaps are tracked in the completion audit.
NES, GB/GBC, single-VDC PCE, SMS and WonderSwan adapters capture destination eligibility, including opaque foreground
backgrounds, sprite ordering and screen/left-column clipping. Margins blend PNG
alpha over the composed destination; transparent margins do not erase it. The
frontmost hardware member supplies the group's expansion priority, and overlapping
expanded instances compose back-to-front. Default and conditional alternate
artwork share this path. No new pack schema version or additional editor controls are required.

GBA uses member rectangles to assign priority, OAM order and semitransparency
within a replacement canvas. Each pixel inherits the nearest member rectangle;
overlapping rectangles and equal distances resolve by OBJ priority, then OAM
order. Thus oversized margins have a deterministic priority without using the
original sprite alpha as a mask. Transparent PNG pixels reveal the remaining
native OBJ/background layers. Mixed-priority groups have native regression
coverage and a live two-member fixture covering overlap ordering, PNG alpha,
shifted origin and oversized margins against a background between their priorities.

GB/GBC now captures expanded destination background eligibility and native OBJ
occluders. A frame-local draw order preserves DMG hardware-X ordering with OAM
ties, and GBC OAM ordering. This value is independent of persistent graphic
recognition. Native checks explicitly exercise these order differences and
behind-background gating. Live whole-frame oversized-canvas checks pass for the
seven existing GB/GBC fixtures in Debug and Release, including banked palettes,
source holes, scrolling and window backgrounds. The existing live fixtures use
transparent native backgrounds; opaque-background and overlapping native OBJ
priority are covered by synthetic compositor checks rather than new ROM cases.

Native compositor checks pass for oversized artwork at 1x/2x/3x, shifted origins,
transparent/partial-alpha margins, destination masks/occlusion and oversized
alternates. Debug and Release live gameplay suites pass for all seven systems. Their NES
mapper 0 (NROM) and SMS Mode 4 fixtures now include whole-frame oversized-canvas
checks, including repeated SMS instances, zoom, scroll, native foreground priority,
left-edge masking and NES overscan. These are runtime checks rather than a new
visible-desktop import test.

Single-VDC PCE now uses the shared full-canvas path and passes the existing live
fixtures in Debug and Release; explicit opaque-background front/behind cases and
SuperGrafx dual-VDC capture remain unfinished. SNES full-canvas integration now
uses main/subscreen layer composition before color math. Mixed-priority groups and
unreplaced lower hardware sprites revealed through PNG holes still need explicit coverage.

SNES full-canvas preparation now records independent destination OBJ permissions
for main and subscreens after window masking, the current BG-mode mapping of all
four OBJ priorities, and OBJ color-math permission. Decoded fetched fragments
record priority code, palette and fetch order; reconstruction retains these
without adding them to graphic matching. No-math hires expansion maps the source
subscreen's permission to its output phase and disables the unused secondary
screen. Native tests verify metadata transfer and phase mapping. These fields
now feed expanded composition. Matched source pixels are removed, complete PNGs
are sampled in output coordinates, and each screen resolves PNG-alpha outcomes
in destination OAM order against its destination background priority. Unreplaced
captured OBJ winners retain their color and priority. Color math and brightness
then apply to each main/subscreen outcome. Native checks cover window masking,
background priority, fixed-color math, alpha, source removal and crossings in
both pack orders. Three Debug/Release live lowres whole-frame cases verify 21x21 PNGs at scale
2 with shifted origins and partial edges; the focused `--snes-canvas` suite also
covers native-bound math, windows, subscreen effects and rotated overlap.
The evaluation's first OAM sprite is latched through the fetch/draw pipeline and
captured per destination pixel. Each canvas chooses its frontmost matched member
relative to that latch, independent of the number of fetched tiles elsewhere in
the frame. Native regression cases deliberately reverse fetch counts and change
the destination latch; the live rotated-OAM case checks the captured latch.
Expanded hires/interlace math, mixed-priority groups, live changes to rotation
across scanlines and revealing uncaptured lower native OBJ pixels still need coverage.

Nine no-math hires cases now verify the full PNG canvas in Debug and Release:
BG modes 5/6, pseudo-hires, both screens, OBJ windows and scrolled BG replacement.
The whole-frame oracle starts from the captured destination with the source OBJ
removed, blends a shifted 21x21 PNG at scale 2 only in eligible phases, and
requires visible pixels outside the original bounds. It waits for complete frame
restoration before checking background PNGs. `--snes-hires-canvas` runs these nine
supported cases; `--snes-hires` retains the two additional interlace probes, which
still fail because capture is unavailable. Passing the focused command does not
establish interlace or hires color-math support.

Interlace preparation now includes `MapFieldWithoutMath`, a native helper that
maps one field to its even or odd output rows and clears replacement eligibility
on the inactive field. Native checks cover both parities, repeated use of the
same output buffer, retained BG capture, recognition rows and configured memory
samples, and rejection of unsupported math. This helper is not attached to the
runtime frame yet. Woven output needs both fields' independent recognition data;
OBJ interlace also fetches alternating source rows and changes displayed sprite
height, so its geometry must be handled before interlace activation.

GBA full-canvas preparation now captures per-sprite OBJ priority and hardware
semi-transparency on decoded row fragments, and preserves those fields during
candidate reconstruction. They remain independent of graphic content matching.
Each destination pixel records whether OBJ is enabled by the display and active
window, independent of original source alpha. Native tests cover reconstruction
of all four priority values and both blend modes. The focused
`--gba-capture-metadata` Debug/Release live suite verifies capture and window permission with
ordinary, source-hole, overlap, transparent-source and brightness fixtures.
GBA now queues complete matched canvases and composes their PNG-alpha outcomes
with destination backgrounds before native color effects. Hardware priority and
OAM order decide OBJ ordering; an OBJ blends with a background rather than another
OBJ. Native-only fallback outcomes retain five-bit effect rounding. Native checks
cover window masking, background priority, brightness, transparent source removal
and crossing canvases in both pack orders. Two whole-frame live cases verify
21x21 PNGs at scale 2 with shifted origins and partial native edges. Six capture
and eight affine/bitmap background cases pass in Debug and Release. Expanded
affine OBJ/mosaic, hardware blend and mixed-priority multi-member groups still
need broader live coverage; native sprites revealed below PNG holes remain a gap.

WonderSwan full-canvas capture records four destination classes for front/behind
foreground sprites and inside/outside the OBJ window. Opaque foreground pixels
reject behind-foreground artwork; native sprite order supplies occlusion.
Eleven Debug/Release live cases verify full-frame PNG composition and restoration
in color modes 0x80 (2bpp), 0xC0 (planar 4bpp) and 0xE0 (packed 4bpp), including
background scroll/window composition, fully occluded artwork and both OBJ window
sides. The monochrome renderer uses the same capture integration but still needs
a dedicated live fixture. Run the focused cases with `--gameplay-ws` and the core
DLL path in the managed MetaSprites test runner.
Sprite PNG dimensions no longer have to be divisible by the selected scale.
Default and alternate artwork retain every PNG pixel, including partial native
texels at the right/bottom edges. Alternate creation keeps the selected scale
rather than resetting it to one. The shared loader, save preflight and native
packet parser accept these canvases within the existing 1..8192 dimension and
1..10 scale limits. Background wrap canvases retain their whole-texel validation.

All eight hidden Sprite Viewer configurations pass import, fractional preview
bounds and persistence for 17x19 default/alternate PNGs at scale two. Routed scale
editing and alternate navigation also preserve fractional canvases. Native tests
check the final corner pixel at scales two/three, complete default/alternate
packet payloads and rejection of truncated packets. PNG decoding preserves all
256 alpha values and still rejects corrupt images and out-of-range scales.
Debug and Release live NES/GB/GBC/SMS whole-frame checks now include an extra
right/bottom PNG pixel at scale two. Both full gameplay suites pass. Debug/Release
UI builds pass with zero warnings/errors, and both native cores build successfully.

Verification must cover oversized default and alternate PNGs, shifted origins,
multiple scales, transparent and partial-alpha margins, overlapping instances,
screen edges, windows and background/sprite occlusion for all seven systems.
These checks are required before claiming full-canvas gameplay support.

## Fixture aliases and hardware coverage

Development notes use neutral fixture aliases instead of commercial game titles.
Hardware descriptions refer to the tested configuration, rather than implying
coverage of every mapper or video mode supported by a system.

| Fixture | System / mapper or hardware mode | Tested behavior |
| --- | --- | --- |
| Cedar | NES mapper 1 (MMC1); supplied ROM | Side-scrolling boundaries, player poses, PNG composition |
| Flint | NES mapper 0 (NROM); generated ROM | Sprite/background replacement, fine scroll, clipping, transparency |
| Quartz | SNES LoROM; generated ROM, BG modes 0–7 | Sprite ordering, main/subscreen backgrounds, windows, color math, Mode 7 coordinates; modes 5/6 and pseudo-hires currently cover non-interlaced OBJ without color math |
| Reed | GB/GBC ROM-only cartridge; generated ROM | DMG/CGB palettes, CGB VRAM bank 1, window and scroll/wrap |
| Slate | GBA; generated ROM, modes 0, 2, 3, 4 and 5 | 4/8-bit OBJ artwork, windows, overlap, text background scrolling, wrapped affine BG2, indexed/direct-color bitmap BG2 |
| Ember | PCE; generated ROM | VDC sprites/backgrounds, wide sprites, dot-clock divider variants |
| Moss | SMS; generated ROM, VDP Mode 4 | Zoom, SMS1 variants, scroll locks and left-edge masking |
| Opal | WS; generated ROM, video modes 0x80/0xC0/0xE0 | Planar/packed artwork, backgrounds, windows and occlusion |

## Main workflow resumed; slowdown investigation deferred

The active goal was updated to contain the metasprite workflow, background layers
and explicitly configured CPU conditions. Slowdown investigation is now outside
that objective; earlier performance notes below are historical.

SNES now maps non-interlaced hires OBJ captures to the 512x478 output while
keeping recognition and group origins in logical sprite coordinates. Separate
main/subscreen phase coverage preserves screen eligibility and windows; only
replacement PNG alpha controls the artwork. Six generated LoROM fixtures pass
per-phase native checks for modes 5/6 and pseudo-hires, including both-screen OBJ
and main-screen windows. Native tests also cover shifted 1x PNGs, odd artwork
scales, and lowres mirroring in a hires output grid. Hires background capture now
also retains separate main/subscreen samples and pixel density. Three additional
LoROM fixtures pass whole-frame checks for mode 5/6 and pseudo-hires scrolling
background PNGs at 1x and 2x alongside OBJ artwork in Debug and Release. Hires color math and
interlace remain unfinished and retain native rendering.

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
Same-screen BG2/OBJ priority transitions, out-of-map behavior, hires color math
and backgrounds, and interlace remain incomplete.

Before the hires work described above, the source audit identified these gaps:
`SnesPpu` acquires a fixed 256x239 capture, excludes OBJ-interlace decoded rows
and coverage, and skips detailed tiled BG capture in hires modes. Color-math
state capture was disabled for double-width output. `MetaSpriteCompositor::Apply`
returned the native buffer whenever the filtered output exceeded capture dimensions.
The earlier gate in `SnesPpu::SendFrame` sent high-resolution/interlaced frames
without a metasprite capture attached. `ApplyHiResMode` emits 512x478
output, places subscreen/main pixels in even/odd columns for hires, duplicates
main pixels for low-resolution lines within a high-resolution frame, and maps
rows using current-field parity and the selected deinterlace mode. Capture must
follow those per-line mappings, including mixed-resolution frames and bob/weave
handling; changing the SendFrame gate alone cannot enable correct replacements.
The new output-phase adapter addresses the non-interlaced, no-math OBJ subset.
The remaining gaps require implementation as well as tests. Extending support must
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
Origin clicks are always enabled for both default and alternate PNG previews.

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
layers remain a secondary goal.

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
4. Performance: resolved per user confirmation on 2026-10-13; investigation is
   closed. Historical investigation scope: slowdowns with PNG replacement active;
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
preview; origin clicking is always enabled. Save group edits explicitly.

Export captures transparent previews independently of the viewer background.
Import accepts a full canvas at its chosen scale (1–10), including partial native texels,
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
`MetaSprites/<CPU type>/<ROM SHA1>/groups.json`.
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
