using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;

namespace Mesen.Debugger.Utilities;

public enum MetaTextMatchKind { TileIndex, GraphicsAddress }

public sealed class MetaTextMapping
{
	public uint Tile { get; set; }
	// Full row-major grid, including Tile at index zero. Empty preserves legacy single-tile mappings.
	public uint[] TilePattern { get; set; } = Array.Empty<uint>();
	public uint[] GetTiles() => TilePattern.Length==0 ? new[] {Tile} : TilePattern;
	public string PatternKey() => string.Join(",",GetTiles().Select(t => t.ToString("X")));
	public int Palette { get; set; } = -1;
	public string Character { get; set; } = "A";
}

// Native-pixel coordinates. Font assets stay relative to the owning ROM pack.
// Runtime glyphs are rasterized at activation, never on the emulation thread.
public sealed class MetaTextProfile : INotifyPropertyChanged, IMetaMemoryConditionOwner
{
	public event PropertyChangedEventHandler? PropertyChanged;
	private string _name = "Text";
	public string Name { get => _name; set { _name=value; PropertyChanged?.Invoke(this,new(nameof(Name))); } }
	public override string ToString() => Name;
	public int Id { get; set; }
	public bool Enabled { get; set; }
	public int NativeLayer { get; set; }
	public MetaTextMatchKind MatchKind { get; set; }
	public string FontPath { get; set; } = "";
	public int FontSize { get; set; } = 8;
	public int RasterScale { get; set; } = 4;
	private int _sourceTileWidth=8;
	public int SourceTileWidth { get => _sourceTileWidth; set { _sourceTileWidth=value; PropertyChanged?.Invoke(this,new(nameof(SourceTileWidth))); } }
	public int CellWidth { get; set; } = 8;
	public int CellHeight { get; set; } = 8;
	public int CellOriginX { get; set; }
	public int CellOriginY { get; set; }
	public int OffsetX { get; set; }
	public int OffsetY { get; set; }
	public bool UsePaletteColor { get; set; } = true;
	public int PaletteInkIndex { get; set; } = 1;
	public string Color { get; set; } = "FFFFFFFF";
	private bool _fillEnabled;
	public bool FillEnabled { get => _fillEnabled; set { _fillEnabled=value; PropertyChanged?.Invoke(this,new(nameof(FillEnabled))); } }
	public string FillColor { get; set; } = "000000FF";
	private bool _proportional,_useDisplayRegion,_outlineEnabled,_shadowEnabled;
	public bool Proportional { get => _proportional; set { _proportional=value; PropertyChanged?.Invoke(this,new(nameof(Proportional))); } }
	public bool UseDisplayRegion { get => _useDisplayRegion; set { _useDisplayRegion=value; PropertyChanged?.Invoke(this,new(nameof(UseDisplayRegion))); } }
	private int _displayX;
	public int DisplayX { get => _displayX; set { if(_displayX==value) { return; } _displayX=value; PropertyChanged?.Invoke(this,new(nameof(DisplayX))); } }
	private int _displayY;
	public int DisplayY { get => _displayY; set { if(_displayY==value) { return; } _displayY=value; PropertyChanged?.Invoke(this,new(nameof(DisplayY))); } }
	private int _displayWidth = 256;
	public int DisplayWidth { get => _displayWidth; set { if(_displayWidth==value) { return; } _displayWidth=value; PropertyChanged?.Invoke(this,new(nameof(DisplayWidth))); } }
	private int _displayHeight = 240;
	public int DisplayHeight { get => _displayHeight; set { if(_displayHeight==value) { return; } _displayHeight=value; PropertyChanged?.Invoke(this,new(nameof(DisplayHeight))); } }
	private bool _wrapText=true,_wrapWords;
	public bool WrapText { get => _wrapText; set { _wrapText=value; PropertyChanged?.Invoke(this,new(nameof(WrapText))); } }
	public bool WrapWords { get => _wrapWords; set { _wrapWords=value; PropertyChanged?.Invoke(this,new(nameof(WrapWords))); } } // Existing packs retain character wrapping.
	public int LineHeight { get; set; } // Zero uses the font's natural line height.
	public double LetterSpacing { get; set; }
	public bool OutlineEnabled { get => _outlineEnabled; set { _outlineEnabled=value; PropertyChanged?.Invoke(this,new(nameof(OutlineEnabled))); } }
	public double OutlineWidth { get; set; } = 1;
	public string OutlineColor { get; set; } = "000000FF";
	public bool ShadowEnabled { get => _shadowEnabled; set { _shadowEnabled=value; PropertyChanged?.Invoke(this,new(nameof(ShadowEnabled))); } }
	public double ShadowX { get; set; } = 1;
	public double ShadowY { get; set; } = 1;
	public string ShadowColor { get; set; } = "000000A0";
	private bool _useRegion;
	public bool UseRegion { get => _useRegion; set { _useRegion=value; PropertyChanged?.Invoke(this,new(nameof(UseRegion))); } }
	private int _x;
	public int X { get => _x; set { if(_x==value) { return; } _x=value; PropertyChanged?.Invoke(this,new(nameof(X))); } }
	private int _y;
	public int Y { get => _y; set { if(_y==value) { return; } _y=value; PropertyChanged?.Invoke(this,new(nameof(Y))); } }
	private int _width = 256;
	public int Width { get => _width; set { if(_width==value) { return; } _width=value; PropertyChanged?.Invoke(this,new(nameof(Width))); } }
	private int _height = 240;
	public int Height { get => _height; set { if(_height==value) { return; } _height=value; PropertyChanged?.Invoke(this,new(nameof(Height))); } }
	public List<MetaTextMapping> Mappings { get; set; } = new();
	public List<MetaMemoryCondition> MemoryConditions { get; set; } = new();
	private string? _memoryConditionExpression;
	[System.Text.Json.Serialization.JsonIgnore(Condition=System.Text.Json.Serialization.JsonIgnoreCondition.WhenWritingNull)]
	public string? MemoryConditionExpression { get=>_memoryConditionExpression; set { _memoryConditionExpression=value; NamedConditionsResolved=false; } }
	[System.Text.Json.Serialization.JsonIgnore] public bool NamedConditionsResolved { get; set; }
	public void Validate()
	{
		if(Id<1 || Id==int.MaxValue || string.IsNullOrWhiteSpace(Name) || Name.Length>128 || NativeLayer<0 || NativeLayer>3 || !Enum.IsDefined(MatchKind)
			|| FontSize<1 || FontSize>128 || RasterScale<1 || RasterScale>8 || CellWidth<1 || CellWidth>128 || CellHeight<1 || CellHeight>128
			|| OffsetX is < -256 or > 256 || OffsetY is < -256 or > 256 || X<0 || Y<0 || X>4096 || Y>4096 || Width<1 || Height<1 || Width>4096 || Height>4096
			|| PaletteInkIndex<1 || PaletteInkIndex>3 || Color==null || Color.Length!=8 || !uint.TryParse(Color,NumberStyles.AllowHexSpecifier,CultureInfo.InvariantCulture,out _)
			|| FillColor==null || FillColor.Length!=8 || !uint.TryParse(FillColor,NumberStyles.AllowHexSpecifier,CultureInfo.InvariantCulture,out _)) {
			throw new InvalidDataException("Invalid text profile: check name, layer, dimensions, offsets and RRGGBBAA color.");
		}
		if(FontPath==null || (Enabled && FontPath.Length==0) || (FontPath.Length>0 && !IsFontPath(FontPath))) {
			throw new InvalidDataException("Select a pack-relative .ttf or .otf font.");
		}
		if(Mappings==null || Mappings.Count>4096 || (Enabled && Mappings.Count==0) || Mappings.Any(m => m==null || m.Palette is < -1 or > 255 || !IsCharacter(m.Character))
			|| Mappings.Any(m => m.TilePattern==null || m.TilePattern.Length>336 || (m.TilePattern.Length>0 && m.TilePattern[0]!=m.Tile))
			|| Mappings.Select(m => (m.PatternKey(),m.Palette)).Distinct().Count()!=Mappings.Count) {
			throw new InvalidDataException("Text mappings need unique tile/palette pairs and one Unicode character per entry (maximum 4096).");
		}
		if(SourceTileWidth is not (6 or 8) || CellWidth%SourceTileWidth!=0 || CellHeight%8!=0 || CellOriginX<0 || CellOriginX>=CellWidth || CellOriginY<0 || CellOriginY>=CellHeight
			|| CellOriginX%SourceTileWidth!=0 || CellOriginY%8!=0 || Mappings.Any(m => m.GetTiles().Length!=(CellWidth/SourceTileWidth)*(CellHeight/8))) {
			throw new InvalidDataException("Source cells must contain complete 6x8 or 8x8 source tiles. Each mapping needs the full tile grid in reading order, with a tile-aligned origin within one cell.");
		}
		MetaMemoryCondition.ValidateAll(MemoryConditions);
		static bool Rgba(string color) => color!=null && color.Length==8 && uint.TryParse(color,NumberStyles.AllowHexSpecifier,CultureInfo.InvariantCulture,out _);
		if(DisplayX is < -4096 or > 4096 || DisplayY is < -4096 or > 4096 || DisplayWidth is <1 or >4096 || DisplayHeight is <1 or >4096 || LineHeight is <0 or >512
			|| !double.IsFinite(LetterSpacing) || LetterSpacing is < -8 or >32 || !double.IsFinite(OutlineWidth) || OutlineWidth is <0 or >8
			|| !double.IsFinite(ShadowX) || ShadowX is < -32 or >32 || !double.IsFinite(ShadowY) || ShadowY is < -32 or >32 || !Rgba(OutlineColor) || !Rgba(ShadowColor)) {
			throw new InvalidDataException("Invalid text layout/effects: check display region, line height, spacing, outline and shadow colors/offsets.");
		}
		if(Proportional && Mappings.Select(m => m.Character).Distinct().Count()>256) { throw new InvalidDataException("Proportional kerning supports up to 256 distinct characters per profile; repeated tile mappings share characters."); }
	}
	public static bool IsFontPath(string path)
	{
		string normalized=path.Replace('\\','/');
		return !normalized.StartsWith('/') && !normalized.Contains(':') && !normalized.Split('/').Any(p => p is ".." or "." or "")
			&& (normalized.EndsWith(".ttf",StringComparison.OrdinalIgnoreCase) || normalized.EndsWith(".otf",StringComparison.OrdinalIgnoreCase));
	}
	public static bool IsCharacter(string? value) => !string.IsNullOrEmpty(value) && Rune.TryGetRuneAt(value,0,out Rune rune) && rune.Utf16SequenceLength==value.Length && !Rune.IsControl(rune);
	public static bool IsWordSeparator(string value) => Rune.TryGetRuneAt(value,0,out Rune rune) && Rune.IsWhiteSpace(rune) && rune.Value is not (0xA0 or 0x202F);
	public MetaTextMapping? Find(uint tile,int palette) => Find(new[] {tile},palette);
	public MetaTextMapping? Find(IReadOnlyList<uint> tiles,int palette) => Mappings.FirstOrDefault(m => m.Palette==palette && m.GetTiles().SequenceEqual(tiles))
		?? Mappings.FirstOrDefault(m => m.Palette==-1 && m.GetTiles().SequenceEqual(tiles));
}

public static class MetaTextMappingText
{
	// HEX_TILE [HEX_PALETTE or *] = character or U+codepoint.
	public static List<MetaTextMapping> Parse(string text)
	{
		var result=new List<MetaTextMapping>();
		foreach(string line in text.Split('\n')) {
			if(string.IsNullOrWhiteSpace(line)) { continue; }
			int separator=line.IndexOf('=');
			if(separator<0) { throw new InvalidDataException("Use TILE [PALETTE or *] = character or U+0041, one mapping per line."); }
			string[] key=line[..separator].Split((char[]?)null,StringSplitOptions.RemoveEmptyEntries);
			string character=line[(separator+1)..].Trim();
			if(character.StartsWith("U+",StringComparison.OrdinalIgnoreCase)) {
				if(!int.TryParse(character[2..],NumberStyles.AllowHexSpecifier,CultureInfo.InvariantCulture,out int scalar) || !Rune.IsValid(scalar)) { throw new InvalidDataException("Invalid Unicode codepoint."); }
				character=new Rune(scalar).ToString();
			}
			string[] parts=key.Length>0 ? key[0].Split(',') : Array.Empty<string>();
			var tiles=new List<uint>();
			foreach(string part in parts) { if(!uint.TryParse(part.TrimStart('$'),NumberStyles.AllowHexSpecifier,CultureInfo.InvariantCulture,out uint id)) { throw new InvalidDataException("Use comma-separated hexadecimal tiles without spaces inside the grid."); } tiles.Add(id); }
			if(key.Length is <1 or >2 || tiles.Count is <1 or >336 || !MetaTextProfile.IsCharacter(character)) {
				throw new InvalidDataException("Use hexadecimal tiles and exactly one printable Unicode character; spaces use U+0020.");
			}
			int palette=-1;
			if(key.Length==2 && key[1]!="*" && (!int.TryParse(key[1].TrimStart('$'),NumberStyles.AllowHexSpecifier,CultureInfo.InvariantCulture,out palette) || palette<0 || palette>255)) { throw new InvalidDataException("Palette must be hexadecimal 00–FF or *."); }
			result.Add(new() {Tile=tiles[0],TilePattern=tiles.Count>1 ? tiles.ToArray() : Array.Empty<uint>(),Palette=palette,Character=character});
			if(result.Count>4096) { throw new InvalidDataException("A text profile supports up to 4096 mappings."); }
		}
		if(result.Select(m => (m.PatternKey(),m.Palette)).Distinct().Count()!=result.Count) { throw new InvalidDataException("Duplicate tile-grid/palette mapping."); }
		return result;
	}
	public static string Format(IEnumerable<MetaTextMapping> mappings) => string.Join(Environment.NewLine,mappings.Select(m => $"{string.Join(",",m.GetTiles().Select(t => t.ToString("X4")))} {(m.Palette<0 ? "*" : m.Palette.ToString("X2"))} = U+{Rune.GetRuneAt(m.Character,0).Value:X4}"));
}
