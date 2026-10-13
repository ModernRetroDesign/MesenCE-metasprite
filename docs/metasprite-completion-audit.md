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
| Replacement PNG alpha governs artwork | Exact 256-alpha-value PNG round trip; native source-hole, transparent-source and partial-alpha fixtures | Verified in fixture coverage |
| System-specific clipping, visibility and priority | Native fixtures listed in metasprite-replacement.md | Verified for tested modes; broader scope incomplete below |
| Background PNG export/import/placement/scrolling/composition | All eight authoring suites and native gameplay fixtures | Verified for tested hardware modes; resize repair and failed replacement save covered |
| Alternate artwork and required background CPU checks | Packet filtering, masked condition authoring, live SNES WRAM transitions | Verified; draft alternates remain inactive and backgrounds require configured checks |
| Read only configured CPU addresses | CompileMemoryAddresses and CaptureMemorySnapshot iterate only compiled condition addresses; native snapshots tested with one/two configured bytes | Verified; no full-memory snapshot path is used for replacement conditions |
| Current build artifacts | Debug/Release UI and native builds, Release GBA affine/bitmap suite | Verified; full Release desktop interaction remains unverified |

## Renderer gaps that prevent broad completion

SNES hires/interlace currently cannot activate replacements. SendFrame excludes
capture attachment for these outputs; capture is fixed at 256x239, detailed hires
background/color-state capture is absent, and OBJ-interlace decoded rows are
excluded. Correct support needs per-output-phase main/subscreen state, field
parity, mixed-resolution scanline mapping and native deinterlace behavior. Removing
the gates alone would produce incorrect placement/composition.

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
