using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;

namespace Mesen.Debugger.Utilities;

public sealed record MetaTextGlyph(string Character,uint[] Pixels,int Width=0,int Height=0,int OffsetX=0,int OffsetY=0,int Advance64=-1,uint[]? Decorations=null);

public sealed class MetaTextGlyphSet : List<MetaTextGlyph>
{
	public bool Unclipped { get; set; }
	public int LineHeight64 { get; set; }
	public Dictionary<ulong,int> Kerning { get; }=new();
}

public static class MetaTextRuntimePacket
{
	public const uint Magic=0x3454584D; // MXT4: source-cell grids and full tile-pattern mappings.
	public const int MaximumBytes=64*1024*1024;
	public static byte[] Create(MetaSpritePack pack,Func<MetaTextProfile,IReadOnlyList<MetaTextGlyph>> rasterize)
	{
		using var stream=new MetaReplacementPacketStream(MaximumBytes);
		Write(stream,pack,rasterize); return stream.ToArray();
	}
	internal static MetaReplacementPacketFile CreateFile(MetaSpritePack pack,Func<MetaTextProfile,IReadOnlyList<MetaTextGlyph>> rasterize)
	{
		using var stream=new MetaReplacementPacketStream(MaximumBytes,fileLease:true);
		Write(stream,pack,rasterize); return stream.DetachFile();
	}
	private static void Write(Stream stream,MetaSpritePack pack,Func<MetaTextProfile,IReadOnlyList<MetaTextGlyph>> rasterize)
	{
		var profiles=pack.TextProfiles.Where(p => p.Enabled).ToArray();
		if(profiles.Length>64 || profiles.Select(p => p.Id).Distinct().Count()!=profiles.Length) { throw new InvalidDataException("Invalid text profile list."); }
		int maxLayer=pack.System switch {"Nes" or "Sms" or "Pce" => 0,"Gameboy" or "Ws" => 1,"Gba" or "Snes" => 3,_ => -1};
		if(profiles.Length>0 && maxLayer<0) { throw new InvalidDataException("This system does not support text overlays."); }
		using var writer=new BinaryWriter(stream,System.Text.Encoding.UTF8,true);
		writer.Write(Magic); writer.Write(profiles.Length);
		foreach(var p in profiles) {
			p.Validate();
			if(p.NativeLayer>maxLayer) { throw new InvalidDataException("Select a hardware background layer available on this system."); }
			var glyphs=rasterize(p);
			var expected=p.Mappings.Select(m => m.Character).Distinct().ToArray();
			if(glyphs.Count!=expected.Length || glyphs.Select(g => g.Character).Distinct().Count()!=glyphs.Count || expected.Any(c => !glyphs.Any(g => g.Character==c))) { throw new InvalidDataException("Glyph atlas does not match the profile's characters."); }
			int pixelCount=checked(p.CellWidth*p.CellHeight*p.RasterScale*p.RasterScale);
			writer.Write(p.Id); writer.Write(p.NativeLayer); writer.Write((int)p.MatchKind); writer.Write(p.RasterScale); writer.Write(p.CellWidth); writer.Write(p.CellHeight);
			writer.Write(p.UsePaletteColor ? 1 : 0); writer.Write(p.PaletteInkIndex);
			uint rgba=uint.Parse(p.Color,NumberStyles.AllowHexSpecifier,CultureInfo.InvariantCulture);
			writer.Write((rgba>>8)|((rgba&255)<<24)); writer.Write(p.UseRegion ? 1 : 0);
			writer.Write(p.X); writer.Write(p.Y); writer.Write(p.Width); writer.Write(p.Height);
			uint fill=uint.Parse(p.FillColor,NumberStyles.AllowHexSpecifier,CultureInfo.InvariantCulture);
			writer.Write(p.FillEnabled ? 1 : 0); writer.Write((fill>>8)|((fill&255)<<24));
			var atlas=glyphs as MetaTextGlyphSet;
			bool unclipped=atlas?.Unclipped==true;
			if(p.Proportional && !unclipped) { throw new InvalidDataException("Proportional text requires an atlas with glyph bounds and advances."); }
			if((p.CellWidth!=8 || p.CellHeight!=8) && !unclipped) { throw new InvalidDataException("Multi-tile source cells require a text-run atlas."); }
			writer.Write((p.Proportional ? 1 : 0)|(p.UseDisplayRegion ? 2 : 0)|(p.WrapText ? 4 : 0)|(unclipped ? 8 : 0));
			writer.Write(atlas?.LineHeight64>0 ? atlas.LineHeight64 : p.CellHeight*64); writer.Write((int)Math.Round(p.LetterSpacing*64));
			writer.Write(p.DisplayX); writer.Write(p.DisplayY); writer.Write(p.DisplayWidth); writer.Write(p.DisplayHeight);
			writer.Write(p.CellOriginX); writer.Write(p.CellOriginY);
			writer.Write(p.MemoryConditions.Count);
			foreach(var condition in p.MemoryConditions) { writer.Write(condition.Address); writer.Write(condition.Value); writer.Write(condition.Mask); writer.Write((int)condition.Comparison); }
			writer.Write(glyphs.Count);
			var indices=new Dictionary<string,int>();
			for(int i=0;i<glyphs.Count;i++) {
				var g=glyphs[i]; int width=g.Width>0 ? g.Width : p.CellWidth*p.RasterScale,height=g.Height>0 ? g.Height : p.CellHeight*p.RasterScale;
				if(width<1 || width>2048 || height<1 || height>2048 || g.OffsetX is < -4096 or >4096 || g.OffsetY is < -4096 or >4096
					|| g.Pixels.Length!=checked(width*height) || (!unclipped && g.Pixels.Length!=pixelCount) || (g.Decorations!=null && g.Decorations.Length!=g.Pixels.Length)) { throw new InvalidDataException("Invalid glyph pixel dimensions/effects."); }
				writer.Write(width); writer.Write(height); writer.Write(g.OffsetX); writer.Write(g.OffsetY); writer.Write(g.Advance64>=0 ? g.Advance64 : p.CellWidth*64);
				indices.Add(glyphs[i].Character,i); MetaReplacementPacketStream.WritePixels(writer,glyphs[i].Pixels);
				writer.Write(g.Decorations!=null ? 1 : 0); if(g.Decorations!=null) { MetaReplacementPacketStream.WritePixels(writer,g.Decorations); }
			}
			writer.Write(p.Mappings.Count);
			foreach(var mapping in p.Mappings) { var tiles=mapping.GetTiles(); writer.Write(tiles.Length); foreach(uint tile in tiles) { writer.Write(tile); } writer.Write(mapping.Palette); writer.Write(indices[mapping.Character]); }
			writer.Write(atlas?.Kerning.Count ?? 0);
			if(atlas!=null) { foreach(var pair in atlas.Kerning) { writer.Write((uint)(pair.Key>>32)); writer.Write((uint)pair.Key); writer.Write(pair.Value); } }
		}
	}
}
