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
	public List<MetaSpriteGroup> Groups { get; set; } = new();
	public List<MetaBackgroundLayer> BackgroundLayers { get; set; } = new();

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
public sealed class MetaBackgroundLayer
{
	public int Id { get; set; }
	public string Name { get; set; } = "";
	public int NativeLayer { get; set; }
	public string ArtworkPath { get; set; } = "";
	public int ArtworkWidth { get; set; }
	public int ArtworkHeight { get; set; }
	public int ArtworkScale { get; set; } = 1;
	public int X { get; set; }
	public int Y { get; set; }
	public bool FollowScroll { get; set; } = true;
	public bool WrapX { get; set; }
	public bool WrapY { get; set; }
	public bool Enabled { get; set; }

	public void Validate()
	{
		if(Id < 1 || Name == null || ArtworkPath == null || NativeLayer < 0 || NativeLayer > 7) {
			throw new InvalidDataException("Invalid background layer identity.");
		}
		MetaSpritePlacement.ValidateCanvas(ArtworkWidth, ArtworkHeight, ArtworkScale);
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

public sealed class MetaSpriteGroup : INotifyPropertyChanged
{
	public event PropertyChangedEventHandler? PropertyChanged;
	private string _name = "";
	private string _poseName = "";
	public int PoseFamilyId { get; set; }
	public string PoseName { get => _poseName; set { _poseName = value; PropertyChanged?.Invoke(this, new(nameof(PoseName))); } }
	private int _originX, _originY;
	private int _replacementScale = 1, _replacementOriginX, _replacementOriginY;
	private bool _enabled;
	public bool Enabled { get => _enabled; set { _enabled = value; PropertyChanged?.Invoke(this, new(nameof(Enabled))); } }
	public int Id { get; set; }
	public string Name { get => _name; set { _name = value; PropertyChanged?.Invoke(this, new(nameof(Name))); } }
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
	public override string ToString() => $"Meta {Id:D3}" + (Name.Length > 0 ? $" — {Name}" : "") + (PoseName.Length > 0 ? $" ({PoseName})" : "");

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
			foreach(var member in group.Members.Skip(1)) {
				int found = -1;
				for(int j = 0; j < candidates.Count; j++) {
					bool sameHardwareSprite = candidates[j].RuntimeSpriteIndex >= 0 && used.Any(index => candidates[index].RuntimeSpriteIndex == candidates[j].RuntimeSpriteIndex);
					if(!used.Contains(j) && !sameHardwareSprite && candidates[j].X == left + member.X && candidates[j].Y == top + member.Y && SameGraphic(member, candidates[j])) {
						found = j;
						break;
					}
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
	public static byte[] Create(MetaSpritePack pack, Func<MetaSpriteGroup, (int Width, int Height, uint[] Pixels)> loadArtwork)
	{
		using MemoryStream stream = new();
		using BinaryWriter writer = new(stream);
		var groups = pack.Groups.Where(g => g.Enabled).ToList();
		writer.Write(Magic);
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
		}
		return stream.ToArray();
	}
}
