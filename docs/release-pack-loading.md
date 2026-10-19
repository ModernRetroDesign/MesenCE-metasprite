# Release replacement-pack loading regression - 2026-10-18

The Release UI built on October 18 at 14:25 embedded a `MesenCore.dll` dated October 17 at 22:02:04. Its embedded core was 8,452,608 bytes and differed from the adjacent current core by SHA-256. Startup extracted this stale core from the UI dependency resource. Direct-core tests bypassed that extraction, so their successful pack activation did not verify the actual packaged application's core identity.

Evidence is recorded in `Tests/MetaSprites/obj/nes-release-stale-core-evidence.log`. This explains the shared startup/save rejection across systems while the ordinary NES mapper-0 activation fixture passed.

The fix isolates dependency archives by configuration, target framework, runtime identifier and `OptimizeUi`, and generates them before resources are prepared. Builds no longer share the old `UI/Dependencies.zip` resource. Unbundled startup refreshes the home-folder core from the application assembly directory; bundled startup extracts its own embedded core. Application-directory lookup works independently of the launch working directory, with an explicit single-file fallback.

Native pack rejection reasons now reach the editors and startup loader. ROM mismatch, unsupported renderers, invalid hardware layers and packet parser errors have distinct messages. JSON version, ROM and system validation also report their individual failure. Failed activation preserves existing definitions, and successful activation clears the previous error. A no-ROM call fails safely before hash lookup.

Verification:

- Actual unbundled Release extraction replaces a stale same-size/same-timestamp core, installs a missing core and preserves the current portable core.
- Actual optimized Release extraction verifies that its embedded core equals the built adjacent DLL, then installs that exact core over stale/missing home-folder copies and a stale portable adjacent copy.
- Actual unbundled Debug extraction passes the same refresh checks.
- NES, GB, GBC, GBA, PCE, SMS, SNES and WonderSwan viewer suites pass pack creation, default PNG/JSON saving, reload, native activation, background authoring and saved-pack activation during GameLoaded.
- NES diagnostics verify safe no-ROM failure, ROM mismatch and malformed-packet failure without changing active definitions, followed by successful saving/activation with no stale error.
- Debug/Release native cores, Debug UI, optimized Release UI and the viewer harness build successfully. Managed builds have zero warnings/errors.

Repeat extraction checks with `Tests/ViewerLifecycle` using `--dependency-copy` followed by the Debug or Release `Mesen.dll` path. The check runs extraction in an isolated child process and cleans up its temporary folders after process exit. Bundled UI/core mismatches fail the check. Pack workflow logs are `Tests/MetaSprites/obj/pack-compatibility-<system>-release.log`; extraction logs are `nes-*-dependency-copy.log` in the same directory.

The final Release build includes the current core in its embedded dependency resource. Existing packs and their PNGs need no migration. Close the application and restart the updated Release executable so startup loads the corrected core. The overall replacement goal and its other outstanding work remain active.

The named-condition Release verification also adds `ValidateOptimizedNativeCore` before resource preparation. Optimized builds require a built adjacent native core even when an older archive exists. The explicit missing-core negative check passes (`Tests/MetaSprites/obj/named-release-missing-core.log`). A clean native rebuild cleared stale PDB type-record warnings; the final native, optimized UI and Release harness builds have zero warnings/errors. Actual optimized-assembly identity, stale/missing/portable extraction and all eight primary system workflows pass. The workflow harness selects Release inputs with `-p:MesenBuildConfiguration=Release`; `--verify-optimized-ui` checks both file hash and optimization metadata. See `docs/named-ram-conditions.md` for final artifact identities and evidence.
