# Music and sound-effect replacement

Progress updated 2026-10-15. Initial audio replacement is implemented for NES,
GB/GBC, GBA, PCE, SMS/Game Gear, SNES and WonderSwan/WonderSwan Color. This is a
shared event-based editor and playback engine, with system-specific write hooks.
Real-game testing and valid OGG/device listening remain deferred by user request.
It does not complete the separate metasprite/background goal.

## Open and save

Open **Debug > Audio Replacement Editor** while a supported game is loaded.
The Music and Sound effects tabs share the asset library and write-event rules.
Enable the audio pack, import files, configure rules, then **Save and apply**.
Saved packs load during GameLoaded, before CPU execution.

Packs are separate from PNG packs:
`HdPacks/Audio/<system>/<ROM SHA1>/music.json`, with copied audio files alongside.
System folder/JSON names are `Nes`, `Gameboy`, `Gba`, `PcEngine`, `Sms`, `Snes`
and `Ws`. GB/GBC share Gameboy; SMS/GG share Sms; WS/WSC share Ws. ROM hashes
isolate individual games. Removing a track retains its file; remove referring
rules first. Reload discards draft edits. Stale-game, wrong-system and stale-file
saves are rejected. A failed decoder/application restores prior saved definitions
and retains prior runtime playback. Orphaned imports are not automatically deleted.

## Assets and playback

Import OGG Vorbis or PCM 16-bit mono/stereo WAV, 8-192 kHz. Each track has a stable
ID, name and volume. Music additionally supports looping and fade-in. Loop points
are source sample frames: start at frame zero for the intro, then loop from the
configured start to the exclusive end; end zero uses file length. Multiply seconds
by the file's sample rate to calculate a sample-frame position.

Limits: 256 tracks, 512 rules, 64 RAM checks per rule, 64 MiB per file and 128 MiB
of cached file data per pack. OGG stays compressed in memory and decodes in blocks;
PCM WAV retains its file data. There is no disk streaming yet.

Music actions are Play, Stop, Pause and Resume. Switching tracks replaces the
current music. A repeated Play continues the same track unless Restart is checked.
Fade-in applies to a newly started track; the pack's stop fade applies to Stop
(paused music stops immediately). The status shows the active music ID, including
paused tracks. Preview uses the applied definitions and advances only while
emulation runs; it does not unpause the emulator.

Sound effects use Play sound effect and Stop all effects. They play the whole file
once, ignoring music loop points/fades and global music volume. Each effect uses
its track volume. Up to 32 distinct effect assets can overlap music and each other.
A repeated effect restarts when Restart is checked; otherwise the event is ignored
until that instance finishes. Each effect uses an independent preloaded decoder,
whose additional cached file data counts against the pack budget. Multiple
instances of the same effect are not yet supported. Effect preview requires an
enabled Play sound effect rule for that track in the applied pack. Music
Stop/Pause/Resume do not stop effects; Stop all effects leaves music alone.

**Mute all native audio** suppresses the combined original output while hardware
continues to run. Unmapped original music and effects will be silent. Native,
music and per-track volumes remain under the application's master volume/effects.
There is no independent global effect-volume control or selective native-channel
muting in this stage. Shared assets use the same track volume for both purposes.

## Write events and RAM conditions

Rules compare a written byte, with optional AND conditions read at that event.
Numbers are hexadecimal; values/masks are 00-FF and the default mask is FF.
Operators are `==`, `!=`, `>`, `<`, `>=`, `<=`. The first matching enabled music
rule and first matching effect rule run independently in list order; use
Earlier/Later to change precedence. Context changes alone never trigger playback.
If context is updated after a command, that command sees the old context.

For example, two NES rules may share trigger `0652 == 08` but distinguish tracks
using `0760 == 06` and `0760 == 04`. Addresses are author-defined; these examples
do not identify a built-in driver or a real game's memory layout.

| System | Trigger sources | RAM-condition addresses |
| --- | --- | --- |
| NES | CPU memory 0000-FFFF | CPU memory 0000-FFFF |
| GB/GBC | CPU memory 0000-FFFF | Current mapped CPU memory |
| GBA | CPU memory 00000000-0FFFFFFF | CPU memory 00000000-0FFFFFFF |
| PCE | Logical CPU memory 0000-FFFF | Current MPR mapping; I/O unavailable |
| SMS/GG | CPU memory 0000-FFFF or I/O port 00-FF | CPU memory 0000-FFFF |
| SNES | Main CPU 000000-FFFFFF or SPC 0000-FFFF | Independently select main CPU or SPC memory |
| WS/WSC | Linear CPU memory 00000-FFFFF or I/O port 0000-FFFF | Linear CPU memory 00000-FFFFF |

All conditions on one SNES rule use its selected RAM source; a rule cannot mix
main CPU and SPC condition lines. Unavailable context fails all comparisons,
including !=, rather than comparing a placeholder byte. Debugger edits, DMA
writes and DSP sample writes do not create replacement events. Speculative
run-ahead and rewind writes are excluded.

## System details

### NES and GB/GBC

NES uses ordinary single-console CPU writes, without debugger attachment or ROM
patches. VS dual-console mode is excluded. GB/GBC conditions follow current bank
mappings, not physical bank numbers. Writes blocked by GB OAM DMA conflict do not
trigger. New GB rules default to example WRAM trigger `C652 == 08`. SGB inside a
SNES console is not separately validated.

### GBA

New rules default to example EWRAM trigger `02000652 == 08`. Halfword/word writes
produce little-endian byte events at hardware-aligned destinations. All bytes of
the transaction commit before conditions run. A transaction may therefore trigger
several configured byte addresses. Values/masks remain bytes.

### PCE

New rules default to example RAM trigger `2652 == 08`. Logical addresses follow
the MPR mapping at the event. There is no physical bank selector. I/O-mapped
context is unavailable because the debug reader cannot safely supply register
values; I/O writes can still be triggers with readable RAM conditions. CD-specific
playback and SuperGrafx have not been separately validated.

### SMS / Game Gear

The Trigger source selector separates memory from ports. For example, port-source
`7F == 90` observes that exact PSG command write. Hardware aliases are not grouped:
7E and 7F remain different rule addresses. A memory write to 007F cannot match a
port rule for 7F. Native port handlers still receive every write. Other SMS-family
models and mapper-specific cases are not separately validated.

### SNES

New rules default to example main CPU command `002140 == 08`. Main CPU APU-port
writes and SPC memory/register writes are separate sources. RAM condition source
is independent, so an SPC event can use a main WRAM room value. Main CPU register
handlers with placeholder Peek values cannot supply context; use readable RAM.
SPC conditions follow read semantics: F4-F7 read incoming CPU ports, while SPC
writes update outgoing ports. Main CPU 16-bit stores are successive byte bus
events. Special coprocessors and SPC-file playback are not separately validated.

### WonderSwan / WonderSwan Color

New rules default to example RAM trigger `00652 == 08`. Enter the linear physical
address `(segment * 10h + offset) & FFFFFh`, not segment:offset notation. Segment
aliases match the same linear trigger; offset wrapping follows the native CPU.
Port rules observe the exact 16-bit port written, including ignored/unmapped bus
writes. Hardware aliases such as 0080 and 0280 are not automatically grouped.

Byte and word writes are supported. An aligned word on a word bus commits both
bytes before emitting its two byte events. Odd addresses and byte buses split a
word into native byte transactions, each with its own event/context timing.
Port word writes follow the same native aligned/split behavior. Debugger edits,
DMA writes and direct internal device writes do not trigger. Split memory writes
now preserve their supplied operation type, preventing DMA writes from being
misclassified as CPU writes. Monochrome and Color have generated test coverage;
SwanCrystal, Pocket Challenge and special cartridge hardware are not separately
validated.

## Lifecycle and timing

Disabling the pack restores native output. Stopping emulation or changing ROMs clears replacement
state and native mute. Reset and user save-state load stop playback but retain
rules; a subsequent event is needed. Emulator pause freezes music and effects.
Rewind clears live replacement playback; historical audio uses the existing
rewind pipeline. Track positions are not serialized into save states.

Mixing occurs after native/add-on audio providers and before master audio
processing/recording. An event starts at the next mixed buffer, without precise
sample offsets inside an accumulated buffer. Advanced run-ahead behavior,
movie/netplay synchronization and exact rewind/state restoration remain future
work. Native audio hardware is never disabled by replacement mute.

Native MUR1 packets remain supported for original 16-bit memory rules. MUR2
encodes wider addresses and separate port/SPC address ranges. These markers are
internal; authors always enter normal addresses and select the source in the UI.
A low-16-bit mask cheaply filters writes before the engine checks full addresses.

## Verification and remaining work

`Tests/ViewerLifecycle --music-replacement` uses original PCM and generated ROMs:
NES NROM/mapper-0; GB/GBC ROM-only; GBA ARM ROM-only; PCE 8 KiB HuCard; SMS/GG Sega
mapper; SNES LoROM plus interpreted SPC700 code; WS/WSC 64 KiB V30 ROM.
Debug and Release automated checks pass for these fixtures.
Checks cover memory/context timing, source isolation, native mute/volume,
continue/restart, loops, resampling, fades, effect overlap/completion, pause/reset,
malformed packets, failed-apply rollback, stale saves, asset/system isolation,
source controls, minimum-width layout and loading before CPU execution.

The GBA generated fixture skips the boot screen, requiring no proprietary BIOS.
Final running fixtures wait for the first frame before shutdown to avoid the
existing immediate-load/Stop startup race. Direct memory-write native fixtures
load without CPU threads; the separate editor/lifecycle fixtures run normally.
Hidden UI tests do not replace visible file-picker and real-game listening tests.

Remaining work:

- Real-game and valid OGG/device listening tests, explicitly deferred by the user.
- Broader mapper, special hardware and audio-driver validation across systems.
- Separate global SFX volume, multiple instances of the same effect and effect fades.
- Music crossfades and loop-point preview/authoring tools.
- Disk streaming, caching refinements and orphaned-import cleanup.
- Exact sample-offset scheduling and saved playback positions through states/rewind.
- Advanced run-ahead and movie/netplay synchronization.
