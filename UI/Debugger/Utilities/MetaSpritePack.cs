using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace Mesen.Debugger.Utilities;

// Coordinates and origins are in native pixels, independent of replacement scale.
public sealed class MetaSpritePack
{
	public int Version { get; set; } = 1;
	public string RomHash { get; set; } = "";
	public string System { get; set; } = "";
	public int NextGroupId { get; set; } = 1;
	public int NextBackgroundId { get; set; } = 1;
	public List<MetaSpriteGroup> Groups { get; set; } = new();
	public List<MetaBackgroundLayer> BackgroundLayers { get; set; } = new();

	public string GetSpriteDefinitionsSnapshot() => JsonSerializer.Serialize(new MetaSpritePack {
		Groups = Groups, NextGroupId = NextGroupId
	}, MetaSpriteSerializerContext.Default.MetaSpritePack);
	public string GetBackgroundDefinitionsSnapshot() => JsonSerializer.Serialize(new MetaSpritePack {
		BackgroundLayers = BackgroundLayers, NextBackgroundId = NextBackgroundId
	}, MetaSpriteSerializerContext.Default.MetaSpritePack);

	public int AlignPoseOrigins(MetaSpriteGroup source)
	{
		if(source.PoseFamilyId == 0 || !Groups.Contains(source)) { return 0; }
		int offsetX = source.OriginX - source.Width / 2;
		int offsetY = source.OriginY - source.Height;
		int count = 0;
		foreach(var pose in Groups.Where(g => g != source && g.PoseFamilyId == source.PoseFamilyId)) {
			pose.OriginX = pose.Width / 2 + offsetX;
			pose.OriginY = pose.Height + offsetY;
			count++;
		}
		return count;
	}

	public int AlignPoseArtworkOrigins(MetaSpriteGroup source)
	{
		if(source.PoseFamilyId == 0 || !Groups.Contains(source) || source.ReplacementPath.Length == 0) { return 0; }
		MetaSpritePlacement.ValidateCanvas(source.ReplacementWidth, source.ReplacementHeight, source.ReplacementScale);
		int offsetX = source.ReplacementOriginX - source.ReplacementWidth / source.ReplacementScale / 2;
		int offsetY = source.ReplacementOriginY - source.ReplacementHeight / source.ReplacementScale;
		var poses = Groups.Where(g => g != source && g.PoseFamilyId == source.PoseFamilyId && g.ReplacementPath.Length > 0).ToList();
		// Validate the entire family before changing any anchors.
		foreach(var pose in poses) { MetaSpritePlacement.ValidateCanvas(pose.ReplacementWidth, pose.ReplacementHeight, pose.ReplacementScale); }
		foreach(var pose in poses) {
			pose.ReplacementOriginX = pose.ReplacementWidth / pose.ReplacementScale / 2 + offsetX;
			pose.ReplacementOriginY = pose.ReplacementHeight / pose.ReplacementScale + offsetY;
		}
		return poses.Count;
	}

	public void Save(string path)
	{
		Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(path))!);
		string temporary = path + ".tmp";
		File.WriteAllText(temporary, JsonSerializer.Serialize(this, MetaSpriteSerializerContext.Default.MetaSpritePack));
		File.Move(temporary, path, true);
	}

	public static MetaSpritePack Load(string path, string romHash, string system)
	{
		if(!File.Exists(path)) {
			return new() { RomHash = romHash, System = system };
		}
		MetaSpritePack pack = JsonSerializer.Deserialize(File.ReadAllText(path), MetaSpriteSerializerContext.Default.MetaSpritePack)
			?? throw new InvalidDataException("Empty metasprite pack.");
		if(pack.Version != 1 || pack.RomHash != romHash || pack.System != system) {
			throw new InvalidDataException("Metasprite pack version, ROM, or system does not match.");
		}
		if(pack.Groups == null || pack.Groups.Any(g => g == null || g.Id < 1 || g.Name == null || g.PoseName == null || g.PoseFamilyId < 0 || g.ReplacementPath == null
			|| g.Width < 1 || g.Height < 1 || g.Width > 2048 || g.Height > 2048 || g.ReplacementScale < 1 || g.ReplacementScale > 10
			|| g.Members == null || g.Members.Count == 0 || g.Members.Any(m => m == null || m.Width < 1 || m.Height < 1
				|| m.Width > 128 || m.Height > 128 || m.X < 0 || m.Y < 0 || m.X + m.Width > g.Width || m.Y + m.Height > g.Height
				|| m.Pixels == null || m.Pixels.Length != m.Width * m.Height))
			|| pack.Groups.Select(g => g.Id).Distinct().Count() != pack.Groups.Count) {
			throw new InvalidDataException("Invalid metasprite group definitions.");
		}
		pack.NextGroupId = Math.Max(pack.NextGroupId, pack.Groups.Select(g => g.Id).DefaultIfEmpty(0).Max() + 1);
		if(pack.BackgroundLayers == null || pack.BackgroundLayers.Any(layer => layer == null)
			|| pack.BackgroundLayers.Select(layer => layer.Id).Distinct().Count() != pack.BackgroundLayers.Count) {
			throw new InvalidDataException("Invalid background layer definitions.");
		}
		foreach(var layer in pack.BackgroundLayers) { layer.Validate(); }
		foreach(var group in pack.Groups) {
			MetaMemoryCondition.ValidateAll(group.MemoryConditions);
			if(group.AlternateArtwork == null || group.AlternateArtwork.Count > 64) { throw new InvalidDataException("Invalid alternate artwork list."); }
			foreach(var alternate in group.AlternateArtwork) { alternate.Validate(); }
		}
		pack.NextBackgroundId = Math.Max(pack.NextBackgroundId, pack.BackgroundLayers.Select(layer => layer.Id).DefaultIfEmpty(0).Max() + 1);
		foreach(var group in pack.Groups.Where(g => g.ReplacementPath.Length > 0 && g.ReplacementWidth == 0)) {
			// Packs written before independent replacement origins used the captured origin.
			group.ReplacementOriginX = group.OriginX;
			group.ReplacementOriginY = group.OriginY;
		}
		return pack;
	}
}

// Artwork coordinates are native pixels; PNG dimensions include ArtworkScale.
// Runtime adapters supply the selected hardware layer's scroll at each pixel/scanline.
public sealed class MetaBackgroundLayer : INotifyPropertyChanged
{
	public event PropertyChangedEventHandler? PropertyChanged;
	private int _artworkScale = 1, _x, _y;
	public static int? ResolveGameboyRole(int tilemap, bool backgroundMap, bool windowMap)
	{
		if(tilemap < 0 || tilemap > 1) { return null; }
		if(tilemap == (backgroundMap ? 1 : 0)) { return 0; }
		if(tilemap == (windowMap ? 1 : 0)) { return 1; }
		return null;
	}
	public int Id { get; set; }
	public string Name { get; set; } = "";
	public int NativeLayer { get; set; }
	public string ArtworkPath { get; set; } = "";
	public int ArtworkWidth { get; set; }
	public int ArtworkHeight { get; set; }
	public int ArtworkScale { get => _artworkScale; set { if(_artworkScale != value) { _artworkScale = value; PropertyChanged?.Invoke(this, new(nameof(ArtworkScale))); } } }
	public int X { get => _x; set { if(_x != value) { _x = value; PropertyChanged?.Invoke(this, new(nameof(X))); } } }
	public int Y { get => _y; set { if(_y != value) { _y = value; PropertyChanged?.Invoke(this, new(nameof(Y))); } } }
	public bool FollowScroll { get; set; } = true;
	public bool WrapX { get; set; }
	public bool WrapY { get; set; }
	public bool Enabled { get; set; }
	public List<MetaMemoryCondition> MemoryConditions { get; set; } = new();
	public override string ToString() => $"Background {Id:D3}" + (Name.Length > 0 ? $" — {Name}" : "");

	public void Validate()
	{
		if(Id < 1 || Name == null || ArtworkPath == null || NativeLayer < 0 || NativeLayer > 7) {
			throw new InvalidDataException("Invalid background layer identity.");
		}
		MetaSpritePlacement.ValidateCanvas(ArtworkWidth, ArtworkHeight, ArtworkScale);
		MetaMemoryCondition.ValidateAll(MemoryConditions);
	}

	public bool TryGetArtworkPixel(int screenX, int screenY, int scrollX, int scrollY, out int artworkX, out int artworkY)
	{
		Validate();
		long x = (long)screenX + (FollowScroll ? scrollX : 0) - X;
		long y = (long)screenY + (FollowScroll ? scrollY : 0) - Y;
		int width = ArtworkWidth / ArtworkScale, height = ArtworkHeight / ArtworkScale;
		if(WrapX) { x = (x % width + width) % width; }
		if(WrapY) { y = (y % height + height) % height; }
		artworkX = artworkY = 0;
		if(x < 0 || y < 0 || x >= width || y >= height) { return false; }
		artworkX = (int)x * ArtworkScale;
		artworkY = (int)y * ArtworkScale;
		return true;
	}

	// Subpixel coordinates identify a sample inside one native output pixel.
	public bool TryGetArtworkPixel(int screenX, int screenY, int scrollX, int scrollY,
		int subpixelX, int subpixelY, int outputScale, out int artworkX, out int artworkY)
	{
		if(outputScale < 1 || subpixelX < 0 || subpixelY < 0 || subpixelX >= outputScale || subpixelY >= outputScale) {
			throw new ArgumentOutOfRangeException(nameof(outputScale), "Use a positive output scale and subpixel coordinates inside that pixel.");
		}
		if(!TryGetArtworkPixel(screenX, screenY, scrollX, scrollY, out artworkX, out artworkY)) { return false; }
		artworkX += (int)((long)subpixelX * ArtworkScale / outputScale);
		artworkY += (int)((long)subpixelY * ArtworkScale / outputScale);
		return true;
	}
}

public static class MetaBackgroundRuntimePacket
{
	public const uint Magic = 0x3147424D; // MBG1
	public const uint ConditionalMagic = 0x3247424D; // MBG2 adds per-layer byte/mask conditions
	public static byte[] Create(MetaSpritePack pack, Func<MetaBackgroundLayer, (int Width, int Height, uint[] Pixels)> loadArtwork)
	{
		using MemoryStream stream = new();
		using BinaryWriter writer = new(stream);
		// Keep older conditionless definitions editable, but never activate them.
		var layers = pack.BackgroundLayers.Where(layer => layer.Enabled && layer.MemoryConditions.Count > 0).ToList();
		if(layers.Count > 128 || layers.Select(layer => layer.Id).Distinct().Count() != layers.Count) { throw new InvalidDataException("Invalid background layers."); }
		bool conditional = layers.Any(layer => layer.MemoryConditions.Count > 0);
		writer.Write(conditional ? ConditionalMagic : Magic); writer.Write(layers.Count);
		foreach(var layer in layers) {
			layer.Validate();
			var artwork = loadArtwork(layer);
			if(artwork.Width != layer.ArtworkWidth || artwork.Height != layer.ArtworkHeight || artwork.Pixels.Length != checked(artwork.Width * artwork.Height)) {
				throw new InvalidDataException("Background PNG dimensions changed.");
			}
			writer.Write(layer.Id); writer.Write(layer.NativeLayer); writer.Write(layer.X); writer.Write(layer.Y);
			writer.Write((layer.FollowScroll ? 1 : 0) | (layer.WrapX ? 2 : 0) | (layer.WrapY ? 4 : 0));
			writer.Write(layer.ArtworkScale); writer.Write(artwork.Width); writer.Write(artwork.Height);
			foreach(uint pixel in artwork.Pixels) { writer.Write(pixel); }
			if(conditional) {
				writer.Write(layer.MemoryConditions.Count);
				foreach(var condition in layer.MemoryConditions) {
					writer.Write(condition.Address); writer.Write(condition.Value); writer.Write(condition.Mask);
				}
			}
		}
		return stream.ToArray();
	}
}

public sealed class MetaSpriteGroup : INotifyPropertyChanged
{
	public event PropertyChangedEventHandler? PropertyChanged;
	private string _name = "";
	private string _poseName = "";
	public int PoseFamilyId { get; set; }
	public string PoseName { get => _poseName; set { _poseName = value; PropertyChanged?.Invoke(this, new(nameof(PoseName))); PropertyChanged?.Invoke(this, new(nameof(DisplayName))); } }
	private int _originX, _originY;
	private int _replacementScale = 1, _replacementOriginX, _replacementOriginY;
	private bool _enabled;
	public bool Enabled { get => _enabled; set { _enabled = value; PropertyChanged?.Invoke(this, new(nameof(Enabled))); } }
	public int Id { get; set; }
	public string Name { get => _name; set { _name = value; PropertyChanged?.Invoke(this, new(nameof(Name))); PropertyChanged?.Invoke(this, new(nameof(DisplayName))); } }
	public int Width { get; set; }
	public int Height { get; set; }
	public int OriginX { get => _originX; set { _originX = value; PropertyChanged?.Invoke(this, new(nameof(OriginX))); } }
	public int OriginY { get => _originY; set { _originY = value; PropertyChanged?.Invoke(this, new(nameof(OriginY))); } }
	public int ReplacementScale { get => _replacementScale; set { _replacementScale = value; PropertyChanged?.Invoke(this, new(nameof(ReplacementScale))); } }
	// Replacement origin is independent of the captured canvas, allowing larger artwork.
	public int ReplacementOriginX { get => _replacementOriginX; set { _replacementOriginX = value; PropertyChanged?.Invoke(this, new(nameof(ReplacementOriginX))); } }
	public int ReplacementOriginY { get => _replacementOriginY; set { _replacementOriginY = value; PropertyChanged?.Invoke(this, new(nameof(ReplacementOriginY))); } }
	public int ReplacementWidth { get; set; }
	public int ReplacementHeight { get; set; }
	public string ReplacementPath { get; set; } = "";
	public List<MetaSpriteMember> Members { get; set; } = new();
	public List<MetaMemoryCondition> MemoryConditions { get; set; } = new();
	public List<MetaAlternateArtwork> AlternateArtwork { get; set; } = new();
	[JsonIgnore]
	public string DisplayName => $"Meta {Id:D3}" + (Name.Length > 0 ? $" — {Name}" : "") + (PoseName.Length > 0 ? $" ({PoseName})" : "");
	public override string ToString() => DisplayName;

	public void LinkAsPoseOf(MetaSpriteGroup source)
	{
		if(source.PoseFamilyId == 0) { source.PoseFamilyId = source.Id; }
		PoseFamilyId = source.PoseFamilyId;
		Name = source.Name;
		// Preserve the anchor's offset from bottom center across differently sized poses.
		OriginX = Width / 2 + source.OriginX - source.Width / 2;
		OriginY = Height + source.OriginY - source.Height;
		ReplacementScale = source.ReplacementScale;
	}
	public void UseExportedArtwork(string path)
	{
		if(string.IsNullOrWhiteSpace(path)) { throw new InvalidDataException("Exported artwork needs a PNG path."); }
		MetaSpritePlacement.ValidateCanvas(Width, Height, 1);
		ReplacementPath = path;
		ReplacementWidth = Width; ReplacementHeight = Height; ReplacementScale = 1;
		ReplacementOriginX = OriginX; ReplacementOriginY = OriginY;
		Enabled = true;
	}
}

// Conditions refer to the owning console CPU's mapped byte addresses.
// Missing snapshots fail closed; an empty list is unconditional.
public sealed class MetaMemoryCondition
{
	public uint Address { get; set; }
	public int Value { get; set; }
	public int Mask { get; set; } = 255;
	public void Validate()
	{
		if(Value < 0 || Value > 255 || Mask < 0 || Mask > 255) { throw new InvalidDataException("Memory conditions require byte values and masks."); }
	}
	public static void ValidateAll(List<MetaMemoryCondition> conditions)
	{
		if(conditions == null || conditions.Count > 64 || conditions.Any(c => c == null)) { throw new InvalidDataException("Invalid memory condition list."); }
		foreach(var condition in conditions) { condition.Validate(); }
	}
	public static bool Matches(List<MetaMemoryCondition> conditions, Func<uint, byte?> readSnapshot)
	{
		ValidateAll(conditions);
		foreach(var condition in conditions) {
			byte? value = readSnapshot(condition.Address);
			if(!value.HasValue || (value.Value & condition.Mask) != (condition.Value & condition.Mask)) { return false; }
		}
		return true;
	}
}

public static class MetaMemoryConditionText
{
	public static List<MetaMemoryCondition> Parse(string text)
	{
		List<MetaMemoryCondition> result = new();
		foreach(string line in text.Split(new[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries)) {
			if(string.IsNullOrWhiteSpace(line)) { continue; }
			string[] values = line.Split(new[] { ' ', '\t', '=', '/' }, StringSplitOptions.RemoveEmptyEntries);
			if(values.Length < 2 || values.Length > 3) { throw new InvalidDataException("Use one hexadecimal ADDRESS = VALUE / MASK condition per line."); }
			static uint Hex(string value) {
				if(value.StartsWith("$")) { value = value[1..]; }
				else if(value.StartsWith("0x", StringComparison.OrdinalIgnoreCase)) { value = value[2..]; }
				try { return Convert.ToUInt32(value, 16); }
				catch(Exception ex) when(ex is FormatException || ex is OverflowException || ex is ArgumentException) { throw new InvalidDataException("Conditions require hexadecimal addresses, byte values, and masks."); }
			}
			uint value = Hex(values[1]), mask = values.Length == 3 ? Hex(values[2]) : 255;
			if(value > 255 || mask > 255) { throw new InvalidDataException("Condition values and masks must be 00 through FF."); }
			result.Add(new() { Address = Hex(values[0]), Value = (int)value, Mask = (int)mask });
		}
		MetaMemoryCondition.ValidateAll(result);
		return result;
	}
	public static string Format(List<MetaMemoryCondition> conditions) => string.Join(Environment.NewLine,
		conditions.Select(c => $"${c.Address:X} = ${c.Value:X2} / ${c.Mask:X2}"));
}

public sealed class MetaAlternateArtwork : INotifyPropertyChanged
{
	public event PropertyChangedEventHandler? PropertyChanged;
	private int _scale = 1, _originX, _originY;
	private string _name = "";
	public string Name { get => _name; set { if(_name == value) { return; } _name = value; PropertyChanged?.Invoke(this, new(nameof(Name))); } }
	public override string ToString() => Name.Length > 0 ? Name : System.IO.Path.GetFileName(Path);
	public string Path { get; set; } = "";
	public int Scale { get => _scale; set { if(_scale == value) { return; } _scale = value; PropertyChanged?.Invoke(this, new(nameof(Scale))); } }
	public int OriginX { get => _originX; set { if(_originX == value) { return; } _originX = value; PropertyChanged?.Invoke(this, new(nameof(OriginX))); } }
	public int OriginY { get => _originY; set { if(_originY == value) { return; } _originY = value; PropertyChanged?.Invoke(this, new(nameof(OriginY))); } }
	public List<MetaMemoryCondition> MemoryConditions { get; set; } = new();
	public void Validate()
	{
		if(string.IsNullOrWhiteSpace(Path) || Scale < 1 || Scale > 10 || MemoryConditions == null) { throw new InvalidDataException("Alternate artwork requires a PNG, scale, and a memory condition list."); }
		if(OriginX < -4096 || OriginX > 4096 || OriginY < -4096 || OriginY > 4096) { throw new InvalidDataException("Alternate artwork origin is out of range."); }
		MetaMemoryCondition.ValidateAll(MemoryConditions);
	}
}

public static class MetaSpritePlacement
{
	// Output coordinates are expressed in the selected HD output scale.
	public static (int X, int Y) GetTopLeft(MetaSpriteGroup group, int worldOriginX, int worldOriginY, int outputScale)
	{
		if(outputScale < 1) { throw new ArgumentOutOfRangeException(nameof(outputScale)); }
		return (checked((worldOriginX - group.ReplacementOriginX) * outputScale), checked((worldOriginY - group.ReplacementOriginY) * outputScale));
	}

	public static void ValidateCanvas(int width, int height, int artworkScale)
	{
		if(artworkScale < 1 || artworkScale > 10 || width < 1 || height < 1 || width > 8192 || height > 8192 || width % artworkScale != 0 || height % artworkScale != 0) {
			throw new InvalidDataException("Use a PNG canvas up to 8192 pixels with dimensions divisible by the chosen scale (1–10).");
		}
	}
}

// A candidate's coordinates are the current frame position. Members store positions relative to the group canvas.
// Exact RGBA matching is the initial policy; palette-independent pattern matching will be added with runtime capture.
public static class MetaSpriteMatcher
{
	public static List<(int X, int Y, int[] Members)> FindMatches(MetaSpriteGroup group, IReadOnlyList<MetaSpriteMember> candidates)
	{
		List<(int X, int Y, int[] Members)> matches = new();
		if(group.Members.Count == 0) { return matches; }
		MetaSpriteMember anchor = group.Members[0];
		for(int i = 0; i < candidates.Count; i++) {
			if(!SameGraphic(anchor, candidates[i])) { continue; }
			int left = candidates[i].X - anchor.X, top = candidates[i].Y - anchor.Y;
			List<int> used = new() { i };
			for(int memberIndex = 1; memberIndex < group.Members.Count; memberIndex++) {
				MetaSpriteMember member = group.Members[memberIndex];
				int found = -1;
				for(int j = 0; j < candidates.Count; j++) {
					MetaSpriteMember candidate = candidates[j];
					// Reject unrelated positions before walking the matched members.
					if(candidate.X != left + member.X || candidate.Y != top + member.Y || !SameGraphic(member, candidate)) { continue; }
					bool alreadyUsed = false;
					foreach(int index in used) {
						if(index == j || (candidate.RuntimeSpriteIndex >= 0 && candidates[index].RuntimeSpriteIndex == candidate.RuntimeSpriteIndex)) {
							alreadyUsed = true;
							break;
						}
					}
					if(alreadyUsed) { continue; }
					found = j;
					break;
				}
				if(found < 0) { break; }
				used.Add(found);
			}
			if(used.Count == group.Members.Count) { matches.Add((left + group.OriginX, top + group.OriginY, used.ToArray())); }
		}
		return matches;
	}

	private static bool SameGraphic(MetaSpriteMember a, MetaSpriteMember b) => a.Width == b.Width && a.Height == b.Height && a.Pixels.AsSpan().SequenceEqual(b.Pixels);
}

public sealed class MetaSpriteMember
{
	// Used only to avoid counting wrapped copies twice; never persisted or used as graphic identity.
	[JsonIgnore] public int RuntimeSpriteIndex { get; set; } = -1;
	public int X { get; set; }
	public int Y { get; set; }
	public int Width { get; set; }
	public int Height { get; set; }
	// Transparent BGRA snapshot. No OAM index or VRAM address is used as identity.
	public uint[] Pixels { get; set; } = Array.Empty<uint>();
}

[JsonSerializable(typeof(MetaSpritePack))]
[JsonSourceGenerationOptions(WriteIndented = true)]
internal partial class MetaSpriteSerializerContext : JsonSerializerContext { }

public static class MetaSpriteRuntimePacket
{
	public const uint Magic = 0x3150534D; // MSP1, little endian
	public const uint ConditionalMagic = 0x3250534D; // MSP2 adds per-group byte/mask conditions
	public const uint AlternateMagic = 0x3350534D; // MSP3 adds alternate PNGs
	public static byte[] Create(MetaSpritePack pack, Func<MetaSpriteGroup, (int Width, int Height, uint[] Pixels)> loadArtwork,
		Func<MetaAlternateArtwork, (int Width, int Height, uint[] Pixels)>? loadAlternate = null)
	{
		using MemoryStream stream = new();
		using BinaryWriter writer = new(stream);
		var groups = pack.Groups.Where(g => g.Enabled).ToList();
		bool alternates = groups.Any(g => g.AlternateArtwork.Any(a => a.MemoryConditions.Count > 0));
		bool conditional = alternates || groups.Any(g => g.MemoryConditions.Count > 0);
		writer.Write(alternates ? AlternateMagic : conditional ? ConditionalMagic : Magic);
		writer.Write(groups.Count);
		foreach(var group in groups) {
			var artwork = loadArtwork(group);
			MetaSpritePlacement.ValidateCanvas(artwork.Width, artwork.Height, group.ReplacementScale);
			if(artwork.Pixels.Length != checked(artwork.Width * artwork.Height)) { throw new InvalidDataException("Invalid replacement pixel buffer."); }
			writer.Write(group.Id);
			writer.Write(group.OriginX); writer.Write(group.OriginY);
			writer.Write(group.ReplacementOriginX); writer.Write(group.ReplacementOriginY);
			writer.Write(group.ReplacementScale);
			writer.Write(artwork.Width); writer.Write(artwork.Height);
			writer.Write(group.Members.Count);
			foreach(var member in group.Members) {
				writer.Write(member.X); writer.Write(member.Y); writer.Write(member.Width); writer.Write(member.Height);
				foreach(uint pixel in member.Pixels) { writer.Write(pixel); }
			}
			foreach(uint pixel in artwork.Pixels) { writer.Write(pixel); }
			if(conditional) {
				MetaMemoryCondition.ValidateAll(group.MemoryConditions);
				writer.Write(group.MemoryConditions.Count);
				foreach(var condition in group.MemoryConditions) {
					writer.Write(condition.Address); writer.Write(condition.Value); writer.Write(condition.Mask);
				}
			}
			if(alternates) {
				if(group.AlternateArtwork.Count > 64) { throw new InvalidDataException("Too many alternate PNGs."); }
				var activeAlternates = group.AlternateArtwork.Where(a => a.MemoryConditions.Count > 0).ToList();
				writer.Write(activeAlternates.Count);
				foreach(var alternate in activeAlternates) {
					alternate.Validate();
					if(loadAlternate == null) { throw new InvalidDataException("Alternate PNG loader is required."); }
					var image = loadAlternate(alternate);
					MetaSpritePlacement.ValidateCanvas(image.Width, image.Height, alternate.Scale);
					if(image.Pixels.Length != checked(image.Width * image.Height)) { throw new InvalidDataException("Invalid alternate PNG pixels."); }
					writer.Write(alternate.OriginX); writer.Write(alternate.OriginY); writer.Write(alternate.Scale);
					writer.Write(image.Width); writer.Write(image.Height);
					foreach(uint pixel in image.Pixels) { writer.Write(pixel); }
					writer.Write(alternate.MemoryConditions.Count);
					foreach(var condition in alternate.MemoryConditions) {
						writer.Write(condition.Address); writer.Write(condition.Value); writer.Write(condition.Mask);
					}
				}
			}
		}
		return stream.ToArray();
	}
}
