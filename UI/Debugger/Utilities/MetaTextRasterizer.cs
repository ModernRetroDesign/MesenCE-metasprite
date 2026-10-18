using SkiaSharp;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Globalization;

namespace Mesen.Debugger.Utilities;

public static class MetaTextRasterizer
{
	// One activation-scoped instance budgets all profiles, including duplicates.
	public sealed class Budget
	{
		private long _bytes;
		private void Reserve(long bytes)
		{
			if(bytes>MetaTextRuntimePacket.MaximumBytes-_bytes) { throw new InvalidDataException("Text glyph atlases exceed the 64 MiB activation budget."); } _bytes+=bytes;
		}
		public IReadOnlyList<MetaTextGlyph> Rasterize(MetaTextProfile profile,string directory,IEnumerable<string>? previewCharacters=null)
		{
			profile.Validate();
			if(!MetaTextProfile.IsFontPath(profile.FontPath)) { throw new InvalidDataException("Select a pack-relative font."); }
			string path=Path.Combine(directory,profile.FontPath);
			if(!File.Exists(path) || new FileInfo(path).Length>32*1024*1024) { throw new InvalidDataException("Font is missing or exceeds 32 MiB."); }
			string[] characters=(previewCharacters ?? profile.Mappings.Select(m => m.Character)).Distinct().ToArray();
			bool free=profile.Proportional || profile.UseDisplayRegion || profile.OutlineEnabled || profile.ShadowEnabled || profile.FontSize>profile.CellHeight;
			if(profile.Proportional && characters.Length>256) { throw new InvalidDataException("Proportional kerning supports up to 256 distinct characters per profile."); }
			int scale=profile.RasterScale,cellWidth=profile.CellWidth*scale,cellHeight=profile.CellHeight*scale;
			if(!free) { Reserve((long)cellWidth*cellHeight*4*characters.Length); }
			using var typeface=SKTypeface.FromFile(path) ?? throw new InvalidDataException("Could not read font: "+profile.FontPath);
			using var font=new SKFont(typeface,profile.FontSize*scale) {Edging=SKFontEdging.Antialias,LinearMetrics=free};
			using var paint=new SKPaint {Color=SKColors.White};
			using var outline=new SKPaint {Color=Color(profile.OutlineColor),Style=SKPaintStyle.StrokeAndFill,StrokeWidth=(float)(profile.OutlineWidth*scale*2),IsAntialias=true};
			using var shadow=new SKPaint {Color=Color(profile.ShadowColor),IsAntialias=true};
			var result=new MetaTextGlyphSet {Unclipped=free || profile.CellWidth!=8 || profile.CellHeight!=8,LineHeight64=profile.LineHeight>0 ? profile.LineHeight*64 : (int)Math.Ceiling((font.Metrics.Descent-font.Metrics.Ascent+font.Metrics.Leading)/scale*64)};
			foreach(string character in characters) {
				using var builder=new SKTextBlobBuilder(); var run=builder.AllocateRun(font,font.CountGlyphs(character),0,0); font.GetGlyphs(character,run.Glyphs);
				if(run.Glyphs.Contains((ushort)0)) { throw new InvalidDataException($"Font '{profile.FontPath}' has no glyph for '{character}'."); }
				using var blob=builder.Build();
				float advance=font.MeasureText(character,out SKRect bounds);
				float baseline=(free ? -font.Metrics.Ascent : cellHeight-font.Metrics.Descent)+profile.OffsetY*scale,baseX=profile.OffsetX*scale;
				int left=0,top=0,width=cellWidth,height=cellHeight;
				if(free) {
					bounds.Offset(baseX,baseline); var extent=bounds;
					if(profile.OutlineEnabled) { extent.Inflate((float)(profile.OutlineWidth*scale),(float)(profile.OutlineWidth*scale)); }
					if(profile.ShadowEnabled) { var moved=bounds; moved.Offset((float)(profile.ShadowX*scale),(float)(profile.ShadowY*scale)); extent=SKRect.Union(extent,moved); }
					left=(int)Math.Floor(extent.Left)-1; top=(int)Math.Floor(extent.Top)-1;
					width=Math.Max(1,(int)Math.Ceiling(extent.Right)+1-left); height=Math.Max(1,(int)Math.Ceiling(extent.Bottom)+1-top);
					if(width>2048 || height>2048) { throw new InvalidDataException("Glyph bounds exceed 2048 pixels; reduce font size/effects or raster scale."); }
					Reserve((long)width*height*4*(profile.OutlineEnabled || profile.ShadowEnabled ? 2 : 1));
				}
				using var bitmap=new SKBitmap(width,height,SKColorType.Bgra8888,SKAlphaType.Premul); using var canvas=new SKCanvas(bitmap); canvas.Clear(SKColors.Transparent);
				uint[]? decorations=null;
				if(profile.OutlineEnabled || profile.ShadowEnabled) {
					if(blob!=null && profile.ShadowEnabled) { canvas.DrawText(blob,baseX-left+(float)(profile.ShadowX*scale),baseline-top+(float)(profile.ShadowY*scale),shadow); }
					if(blob!=null && profile.OutlineEnabled) { canvas.DrawText(blob,baseX-left,baseline-top,outline); }
					canvas.Flush(); decorations=Read(bitmap,false); canvas.Clear(SKColors.Transparent);
				}
				if(blob!=null) { canvas.DrawText(blob,baseX-left,baseline-top,paint); } canvas.Flush();
				result.Add(new(character,Read(bitmap,true),width,height,left,top,(int)Math.Round(advance/scale*64),decorations));
			}
			if(profile.Proportional) { AddKerning(result,path,profile.FontSize); }
			return result;
		}
	}
	public static SKColor Color(string rgba)
	{
		uint value=uint.Parse(rgba,NumberStyles.AllowHexSpecifier,CultureInfo.InvariantCulture);
		return new SKColor((byte)(value>>24),(byte)(value>>16),(byte)(value>>8),(byte)value);
	}
	private static uint[] Read(SKBitmap bitmap,bool coverage)
	{
		var pixels=new uint[bitmap.Width*bitmap.Height]; var row=new int[bitmap.Width];
		for(int y=0;y<bitmap.Height;y++) {
			Marshal.Copy(bitmap.GetPixels()+y*bitmap.RowBytes,row,0,row.Length);
			for(int x=0;x<row.Length;x++) {
				uint raw=unchecked((uint)row[x]),alpha=raw>>24,color=alpha<<24;
				if(coverage) { color|=0xFFFFFF; }
				else if(alpha>0) { for(int c=0;c<3;c++) { color|=Math.Min(255u,(((raw>>(c*8))&255)*255+alpha/2)/alpha)<<(c*8); } }
				pixels[y*row.Length+x]=color;
			}
		}
		return pixels;
	}
	private static void AddKerning(MetaTextGlyphSet set,string path,int size)
	{
		using var blob=HarfBuzzSharp.Blob.FromFile(path); using var face=new HarfBuzzSharp.Face(blob,0); using var font=new HarfBuzzSharp.Font(face);
		font.SetFunctionsOpenType(); font.SetScale(size*64,size*64);
		using var buffer=new HarfBuzzSharp.Buffer();
		var features=new[] {new HarfBuzzSharp.Feature(new HarfBuzzSharp.Tag('l','i','g','a'),0),new HarfBuzzSharp.Feature(new HarfBuzzSharp.Tag('c','l','i','g'),0),new HarfBuzzSharp.Feature(new HarfBuzzSharp.Tag('k','e','r','n'),1)};
		HarfBuzzSharp.GlyphPosition[] Shape(string text) { buffer.ClearContents(); buffer.AddUtf16(text); buffer.GuessSegmentProperties(); buffer.Direction=HarfBuzzSharp.Direction.LeftToRight; font.Shape(buffer,features); return buffer.GlyphPositions; }
		for(int i=0;i<set.Count;i++) { var positions=Shape(set[i].Character); if(positions.Length==1) { set[i]=set[i] with {Advance64=Math.Max(0,positions[0].XAdvance)}; } }
		for(int a=0;a<set.Count;a++) { for(int b=0;b<set.Count;b++) {
			var positions=Shape(set[a].Character+set[b].Character); if(positions.Length!=2) { continue; }
			int adjustment=positions[0].XAdvance+positions[1].XOffset-set[a].Advance64;
			if(adjustment!=0 && Math.Abs(adjustment)<=128*64) { set.Kerning.Add(((ulong)a<<32)|(uint)b,adjustment); }
		} }
	}
}
