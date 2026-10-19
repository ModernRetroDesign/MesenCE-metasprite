using System;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace Mesen.Debugger.Utilities;

public sealed class ReplacementMusicTrack : INotifyPropertyChanged
{
	public event PropertyChangedEventHandler? PropertyChanged;
	private uint _id;
	public uint Id { get => _id; set { _id = value; PropertyChanged?.Invoke(this, new(nameof(Id))); } }
	private string _name = "Track";
	public string Name { get => _name; set { _name = value; PropertyChanged?.Invoke(this, new(nameof(Name))); } }
	private string _path = "";
	public string Path { get => _path; set { _path = value; PropertyChanged?.Invoke(this, new(nameof(Path))); } }
	private bool _loop = true;
	public bool Loop { get => _loop; set { _loop = value; PropertyChanged?.Invoke(this, new(nameof(Loop))); } }
	private uint _loopStart;
	public uint LoopStart { get => _loopStart; set { _loopStart = value; PropertyChanged?.Invoke(this, new(nameof(LoopStart))); } }
	private uint _loopEnd;
	public uint LoopEnd { get => _loopEnd; set { _loopEnd = value; PropertyChanged?.Invoke(this, new(nameof(LoopEnd))); } }
	private uint _volume = 100;
	public uint Volume { get => _volume; set { _volume = value; PropertyChanged?.Invoke(this, new(nameof(Volume))); } }
	private uint _fadeInMs;
	public uint FadeInMs { get => _fadeInMs; set { _fadeInMs = value; PropertyChanged?.Invoke(this, new(nameof(FadeInMs))); } }
}
public sealed class ReplacementMusicRule : INotifyPropertyChanged
{
	public event PropertyChangedEventHandler? PropertyChanged;
	private uint _id;
	public uint Id { get => _id; set { _id = value; PropertyChanged?.Invoke(this, new(nameof(Id))); } }
	private string _name = "Rule";
	public string Name { get => _name; set { _name = value; PropertyChanged?.Invoke(this, new(nameof(Name))); } }
	private bool _enabled = true;
	public bool Enabled { get => _enabled; set { _enabled = value; PropertyChanged?.Invoke(this, new(nameof(Enabled))); } }
	private int _contextSource;
	public int ContextSource { get => _contextSource; set { _contextSource = value; PropertyChanged?.Invoke(this, new(nameof(ContextSource))); } }
	private int _triggerSource;
	public int TriggerSource { get => _triggerSource; set { _triggerSource = value; PropertyChanged?.Invoke(this, new(nameof(TriggerSource))); } }
	private int _action;
	public int Action { get => _action; set { _action = value; PropertyChanged?.Invoke(this, new(nameof(Action))); } }
	private uint _trackId = 1;
	public uint TrackId { get => _trackId; set { _trackId = value; PropertyChanged?.Invoke(this, new(nameof(TrackId))); } }
	private bool _restart;
	public bool Restart { get => _restart; set { _restart = value; PropertyChanged?.Invoke(this, new(nameof(Restart))); } }
	private string _trigger = "0652 == 08";
	public string Trigger { get => _trigger; set { _trigger = value; PropertyChanged?.Invoke(this, new(nameof(Trigger))); } }
	private string _conditions = "";
	public string Conditions { get => _conditions; set { _conditions = value; PropertyChanged?.Invoke(this, new(nameof(Conditions))); } }
}
public sealed class MusicReplacementPack
{
	public int Version { get; set; } = 1;
	public string RomHash { get; set; } = "";
	public string System { get; set; } = "Nes";
	public bool Enabled { get; set; }
	public bool MuteNative { get; set; }
	public uint NativeVolume { get; set; } = 100;
	public uint MusicVolume { get; set; } = 100;
	public uint FadeOutMs { get; set; }
	public ObservableCollection<ReplacementMusicTrack> Tracks { get; set; } = new();
	public ObservableCollection<ReplacementMusicRule> Rules { get; set; } = new();
	public static string AssetPath(string directory, string relative) {
		if(string.IsNullOrWhiteSpace(relative) || Path.IsPathRooted(relative)) { throw new InvalidDataException("Music files must be inside the pack folder."); }
		string root = Path.TrimEndingDirectorySeparator(Path.GetFullPath(directory)) + Path.DirectorySeparatorChar;
		string path = Path.GetFullPath(Path.Combine(directory, relative));
		if(!path.StartsWith(root, OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal)) { throw new InvalidDataException("Music path escapes the pack folder."); }
		return path;
	}
	public System.Collections.Generic.IEnumerable<string> NamedExpressions() => Rules.SelectMany(r=>new[]{r.Trigger,r.Conditions});
	public System.Collections.Generic.List<MetaMemoryCondition> ParseCondition(string text,int source,MetaNamedConditionLibrary? library=null)
	{
		if(MetaNamedConditionBindings.UsesNames(text)) {
			if(source!=0) { throw new InvalidDataException("Named RAM conditions use main CPU addresses. Select the main CPU source for this field."); }
			if(library==null || library.RomHash!=RomHash || library.System!=(System=="PcEngine" ? "Pce" : System)) { throw new InvalidDataException("The named condition library is missing or belongs to another game/system."); }
		}
		return MetaMemoryConditionText.Parse(text,library);
	}
	public void Validate(MetaNamedConditionLibrary? library=null) {
		if(Version != 1 || (System != "Nes" && System != "Gameboy" && System != "Gba" && System != "PcEngine" && System != "Sms" && System != "Snes" && System != "Ws") || RomHash.Length != 40 || !RomHash.All(Uri.IsHexDigit) || NativeVolume > 100 || MusicVolume > 100 || FadeOutMs > 10000
			|| Tracks == null || Rules == null || Tracks.Count > 256 || Rules.Count > 512) { throw new InvalidDataException("Invalid music pack settings."); }
		if(Tracks.Any(t => t == null || t.Id == 0 || t.Id > 65535 || string.IsNullOrWhiteSpace(t.Name) || t.Path == null || t.Volume > 100 || t.FadeInMs > 10000 || (t.LoopEnd != 0 && t.LoopStart >= t.LoopEnd))
			|| Tracks.Select(t => t.Id).Distinct().Count() != Tracks.Count) { throw new InvalidDataException("Invalid track identity, volume or loop points."); }
		if(Rules.Any(r => r == null || r.Id == 0 || r.Action < 0 || r.Action > 5 || r.TriggerSource < 0 || r.TriggerSource > 1 || (r.TriggerSource == 1 && System != "Sms" && System != "Snes" && System != "Ws") || r.ContextSource < 0 || r.ContextSource > 1 || (r.ContextSource == 1 && System != "Snes") || r.Name == null || r.Trigger == null || r.Conditions == null) || Rules.Select(r => r.Id).Distinct().Count() != Rules.Count) { throw new InvalidDataException("Invalid music rules."); }
		if(Rules.Where(r => r.Enabled && r.Action == 4).Select(r => r.TrackId).Distinct().Count() > 32) { throw new InvalidDataException("Maximum 32 distinct sound effects per active pack."); }
		foreach(var r in Rules.Where(r => r.Enabled)) {
			var trigger = ParseCondition(r.Trigger,r.TriggerSource,library); var conditions = ParseCondition(r.Conditions,r.ContextSource,library);
			if(trigger.Count != 1 || conditions.Count > 64 || trigger.Any(c => c.Address > (r.TriggerSource == 1 ? (System == "Sms" ? 255u : 65535u) : System == "Gba" ? 0x0FFFFFFFu : System == "Snes" ? 0xFFFFFFu : System == "Ws" ? 0xFFFFFu : 65535u)) || conditions.Any(c => c.Address > (r.ContextSource == 1 ? 65535u : System == "Gba" ? 0x0FFFFFFFu : System == "Snes" ? 0xFFFFFFu : System == "Ws" ? 0xFFFFFu : 65535u))) { throw new InvalidDataException("Use one CPU write trigger and at most 64 RAM checks within the current system CPU address space."); }
			if((r.Action == 0 || r.Action == 4) && !Tracks.Any(t => t.Id == r.TrackId)) { throw new InvalidDataException($"Rule '{r.Name}' refers to a missing track."); }
		}
	}
	public byte[] CreatePacket(string directory,MetaNamedConditionLibrary? library=null) {
		Validate(library); using MemoryStream data = new(); using BinaryWriter w = new(data);
		w.Write(System == "Gba" || System == "Sms" || System == "Snes" || System == "Ws" ? 0x3252554Du : 0x3152554Du); w.Write(Enabled ? 1 : 0); w.Write(MuteNative ? 1 : 0); w.Write(NativeVolume); w.Write(MusicVolume); w.Write(FadeOutMs);
		var tracks = Enabled ? Tracks.ToArray() : Array.Empty<ReplacementMusicTrack>();
		w.Write(tracks.Length);
		foreach(var t in tracks) {
			string path = AssetPath(directory, t.Path);
			if(!File.Exists(path)) { throw new FileNotFoundException("Music file is missing.", t.Path); }
			byte[] name = Encoding.UTF8.GetBytes(path); if(name.Length > 32768) { throw new InvalidDataException("Music file path is too long."); }
			w.Write(t.Id); w.Write(name.Length); w.Write(name); w.Write(t.Loop ? 1 : 0); w.Write(t.LoopStart); w.Write(t.LoopEnd); w.Write(t.Volume); w.Write(t.FadeInMs);
		}
		var rules = Enabled ? Rules.Where(r => r.Enabled).ToArray() : Array.Empty<ReplacementMusicRule>(); w.Write(rules.Length);
		void Condition(MetaMemoryCondition c) { w.Write(c.Address); w.Write(c.Value); w.Write(c.Mask); w.Write((int)c.Comparison); }
		foreach(var r in rules) {
			w.Write(r.Id); w.Write(r.Action); w.Write(r.TrackId); w.Write(r.Restart ? 1 : 0); var trigger = ParseCondition(r.Trigger,r.TriggerSource,library).Single(); if(r.TriggerSource == 1) { trigger.Address |= System == "Snes" ? 0x1000000u : System == "Ws" ? 0x100000u : 0x10000u; } Condition(trigger);
			var conditions = ParseCondition(r.Conditions,r.ContextSource,library); w.Write(conditions.Count); foreach(var c in conditions) { if(r.ContextSource == 1) { c.Address |= 0x1000000u; } Condition(c); }
		}
		if(data.Length > 1048576) { throw new InvalidDataException("Music definitions exceed the supported size."); } return data.ToArray();
	}
	public static MusicReplacementPack Load(string path, string hash, string system = "Nes",MetaNamedConditionLibrary? library=null) {
		var pack = File.Exists(path) ? JsonSerializer.Deserialize(File.ReadAllText(path), MusicReplacementJson.Default.MusicReplacementPack) ?? throw new InvalidDataException("Empty music pack.") : new() { RomHash = hash, System = system };
		pack.Validate(library); if(pack.RomHash != hash || pack.System != system) { throw new InvalidDataException("Music pack belongs to another game."); } return pack;
	}
	public string ToJson(MetaNamedConditionLibrary? library=null) { Validate(library); return JsonSerializer.Serialize(this, MusicReplacementJson.Default.MusicReplacementPack); }
}
[JsonSourceGenerationOptions(WriteIndented = true)]
[JsonSerializable(typeof(MusicReplacementPack))]
public partial class MusicReplacementJson : JsonSerializerContext { }
