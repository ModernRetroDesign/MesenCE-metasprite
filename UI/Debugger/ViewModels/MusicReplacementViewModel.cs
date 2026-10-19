using CommunityToolkit.Mvvm.ComponentModel;
using Mesen.Debugger.Utilities;
using Mesen.Interop;
using System;
using System.IO;
using System.Linq;

namespace Mesen.Debugger.ViewModels;

public partial class MusicReplacementViewModel : ObservableObject
{
	[ObservableProperty] public partial MusicReplacementPack Pack { get; set; } = new();
	[ObservableProperty] public partial ReplacementMusicTrack? SelectedTrack { get; set; }
	[ObservableProperty] public partial ReplacementMusicRule? SelectedRule { get; set; }
	[ObservableProperty] public partial string Status { get; set; } = "";
	[ObservableProperty] public partial string PlaybackStatus { get; set; } = "No replacement music playing.";
	public bool HasTrack => SelectedTrack != null;
	public bool HasRule => SelectedRule != null;
	public string PackPath { get; }
	private readonly string _hash;
	private readonly ConsoleType _console;
	public bool SupportsSpcTriggers => _console == ConsoleType.Snes;
	public bool SupportsAlternateTriggers => SupportsPortTriggers || SupportsSpcTriggers;
	public string AlternateTriggerName => SupportsSpcTriggers ? "SPC sound CPU write" : "I/O port write";
	public string TriggerSourceHelp => _console == ConsoleType.Ws ? "Memory: 00000-FFFFF linear physical address (segment*10h + offset, masked to 20 bits). Port: 0000-FFFF. RAM checks always use memory." : SupportsSpcTriggers ? "Main CPU: 000000-FFFFFF. SPC: 0000-FFFF. Select the RAM source separately below." : "Memory: 0000-FFFF. Port: 00-FF (for example 7F for PSG). RAM checks always use CPU memory.";
	public bool SupportsPortTriggers => _console == ConsoleType.Sms || _console == ConsoleType.Ws;
	public string SystemDescription => _console == ConsoleType.Ws ? "WonderSwan CPU memory triggers: 00000-FFFFF; I/O ports: 0000-FFFF. Compare individual bytes of byte/word writes. RAM checks use linear memory addresses." : _console == ConsoleType.Snes ? "SNES main CPU triggers: 000000-FFFFFF; SPC triggers: 0000-FFFF. RAM checks can use main CPU or SPC memory independently." : _console == ConsoleType.Gba ? "GBA CPU write triggers; addresses 00000000-0FFFFFFF. Byte comparisons also match individual bytes of halfword/word writes." : _console == ConsoleType.Sms ? "SMS / Game Gear CPU triggers: memory 0000-FFFF or I/O ports 00-FF. RAM checks always use memory addresses. Port aliases match the exact written port." : _console == ConsoleType.PcEngine ? "PCE CPU write triggers; logical addresses 0000-FFFF through current MPR banks. RAM checks cannot read I/O registers." : $"{_console} CPU write triggers; addresses 0000-FFFF.";
	private string? _snapshot;
	private bool _loadError;
	partial void OnSelectedTrackChanged(ReplacementMusicTrack? value) => OnPropertyChanged(nameof(HasTrack));
	partial void OnSelectedRuleChanged(ReplacementMusicRule? value) => OnPropertyChanged(nameof(HasRule));
	public MusicReplacementViewModel() { _console = EmuApi.GetRomInfo().ConsoleType; _hash = EmuApi.GetRomHash(HashType.Sha1); PackPath = MusicReplacementRuntime.GetPath(_hash); Reload(); }
	public void RefreshPlayback() { if(EmuApi.GetRomHash(HashType.Sha1) != _hash) { PlaybackStatus = "Game changed; reopen this editor."; return; } int id = DebugApi.GetReplacementMusicTrack(); PlaybackStatus = id > 0 ? $"Active track: {id} (may be paused)" : "No replacement music playing."; }
	private void CheckGame() { if((_console != ConsoleType.Nes && _console != ConsoleType.Gameboy && _console != ConsoleType.Gba && _console != ConsoleType.PcEngine && _console != ConsoleType.Sms && _console != ConsoleType.Snes && _console != ConsoleType.Ws) || EmuApi.GetRomInfo().ConsoleType != _console || EmuApi.GetRomHash(HashType.Sha1) != _hash) { throw new InvalidOperationException("The game changed. Reopen the Audio Replacement Editor."); } }
	private void CheckEditable() { CheckGame(); if(_loadError) { throw new InvalidDataException("Fix the pack load error and reload before editing."); } }
	public void Reload() {
		try { CheckGame(); var pack = MusicReplacementRuntime.LoadPack(PackPath, _hash, _console.ToString()); _snapshot = File.Exists(PackPath) ? File.ReadAllText(PackPath) : null; Pack = pack; SelectedTrack = Pack.Tracks.FirstOrDefault(); SelectedRule = Pack.Rules.FirstOrDefault(); _loadError = false; Status = "Loaded music pack. Unsaved edits do not affect playback."; }
		catch(Exception ex) { _loadError = true; Status = ex.Message; }
	}
	public void Import(string source) {
		try {
			CheckEditable(); string ext = Path.GetExtension(source).ToLowerInvariant(); if(ext != ".ogg" && ext != ".wav") { throw new InvalidDataException("Import OGG Vorbis or PCM 16-bit WAV."); }
			if(Pack.Tracks.Count >= 256 || new FileInfo(source).Length > 67108864) { throw new InvalidDataException("Maximum 256 tracks; 64 MiB per file, 128 MiB per active pack."); }
			uint id = Pack.Tracks.Select(t => t.Id).DefaultIfEmpty(0u).Max() + 1; if(id > 65535) { throw new InvalidDataException("No track IDs remain."); }
			Directory.CreateDirectory(Path.GetDirectoryName(PackPath)!); string name = $"track-{id:D3}-{Guid.NewGuid():N}{ext}";
			File.Copy(source, MusicReplacementPack.AssetPath(Path.GetDirectoryName(PackPath)!, name));
			var track = new ReplacementMusicTrack { Id = id, Name = $"Track {id:D3}", Path = name }; Pack.Tracks.Add(track); SelectedTrack = track; Status = "Imported. Save and apply before previewing. Audio decoding is validated on apply.";
		} catch(Exception ex) { Status = ex.Message; }
	}
	public void RemoveTrack() { try { CheckEditable(); if(SelectedTrack is not {} track) { return; } if(Pack.Rules.Any(r => (r.Action == 0 || r.Action == 4) && r.TrackId == track.Id)) { throw new InvalidDataException("Remove rules referring to this track first."); } Pack.Tracks.Remove(track); SelectedTrack = Pack.Tracks.FirstOrDefault(); Status = "Track removed from draft. Files are retained on disk."; } catch(Exception ex) { Status = ex.Message; } }
	public void AddRule(int action = 0) { try { CheckEditable(); var r = new ReplacementMusicRule { Id = Pack.Rules.Select(r => r.Id).DefaultIfEmpty(0u).Max() + 1, TrackId = SelectedTrack?.Id ?? 1, Trigger = _console == ConsoleType.Ws ? "00652 == 08" : _console == ConsoleType.Snes ? "002140 == 08" : _console == ConsoleType.Gba ? "02000652 == 08" : _console == ConsoleType.PcEngine ? "2652 == 08" : (_console == ConsoleType.Gameboy || _console == ConsoleType.Sms) ? "C652 == 08" : "0652 == 08", Name = action == 4 ? "New sound effect rule" : "New rule", Action = action, Restart = action == 4 }; Pack.Rules.Add(r); SelectedRule = r; } catch(Exception ex) { Status = ex.Message; } }
	public void AddEffectRule() => AddRule(4);
	public void RemoveRule() { try { CheckEditable(); if(SelectedRule is {} rule) { Pack.Rules.Remove(rule); SelectedRule = Pack.Rules.FirstOrDefault(); } } catch(Exception ex) { Status = ex.Message; } }
	public void MoveRule(int direction) { try { CheckEditable(); if(SelectedRule is not {} rule) { return; } int index = Pack.Rules.IndexOf(rule), next = index + direction; if(next >= 0 && next < Pack.Rules.Count) { Pack.Rules.Move(index, next); } } catch(Exception ex) { Status = ex.Message; } }
	private void Write(string text) { Directory.CreateDirectory(Path.GetDirectoryName(PackPath)!); string temp = PackPath + "." + Guid.NewGuid().ToString("N") + ".tmp"; try { File.WriteAllText(temp, text); File.Move(temp, PackPath, true); } finally { if(File.Exists(temp)) { File.Delete(temp); } } }
	public void SaveApply() {
		try {
			CheckEditable(); string? disk = File.Exists(PackPath) ? File.ReadAllText(PackPath) : null;
			if(disk != _snapshot) { throw new InvalidOperationException("Music pack changed on disk. Reload before saving."); }
			var library=MusicReplacementRuntime.GetLibrary(_hash,Pack.NamedExpressions());
			string json = Pack.ToJson(library); byte[] packet = Pack.CreatePacket(Path.GetDirectoryName(PackPath)!,library);
			Write(json);
			if(!DebugApi.SetMusicReplacements(_hash, packet, (uint)packet.Length)) {
				if(disk != null) { Write(disk); } else { File.Delete(PackPath); }
				throw new InvalidDataException("Apply failed; saved definitions were restored and previous playback retained. Check audio format, loop bounds and memory limits.");
			}
			_snapshot = json; Status = "Music pack saved and applied. Writes trigger playback; context changes alone do not.";
		} catch(Exception ex) { Status = ex.Message; }
	}
	public void Playback(uint action) { try { CheckEditable(); if(!DebugApi.ReplacementMusicCommand(_hash, action, (action == 0 || action == 4) ? SelectedTrack?.Id ?? 0 : 0)) { throw new InvalidOperationException("Enable, save and apply the pack before previewing. An effect also needs an enabled Play sound effect rule for the selected track."); } Status = "Playback command sent. Preview uses the applied track and advances only while emulation runs."; } catch(Exception ex) { Status = ex.Message; } }
}
