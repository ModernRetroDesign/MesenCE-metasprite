using SkiaSharp;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;

namespace Mesen.Debugger.Utilities;

public readonly record struct MetaTextPlacement(int Glyph,long X,long Y);

public static class MetaTextPreview
{
	public static IReadOnlyList<MetaTextPlacement> Layout(MetaTextProfile profile,MetaTextGlyphSet atlas,string text)
	{
		var runes=text.EnumerateRunes().Take(128).ToArray();
		int scale=profile.RasterScale;
		var indices=atlas.Select((g,i) => (g.Character,i)).ToDictionary(g => g.Character,g => g.i);
		var ids=runes.Select(r => Rune.IsControl(r) ? -1 : indices[r.ToString()]).ToArray();
		var separators=atlas.Select(g => MetaTextProfile.IsWordSeparator(g.Character)).ToArray();
		int spacing=(int)Math.Round(profile.LetterSpacing*64);
		bool words=profile.WrapWords && profile.WrapText && profile.UseDisplayRegion;
		var wordWidths=words ? new long[ids.Length] : Array.Empty<long>();
		if(words) { for(int i=ids.Length-1;i>=0;i--) { int id=ids[i]; if(id<0 || separators[id]) { continue; }
			wordWidths[i]=profile.Proportional ? atlas[id].Advance64 : profile.CellWidth*64;
			if(i+1<ids.Length && ids[i+1]>=0 && !separators[ids[i+1]]) {
				wordWidths[i]+=spacing+wordWidths[i+1];
				if(profile.Proportional && atlas.Kerning.TryGetValue(((ulong)id<<32)|(uint)ids[i+1],out int k)) { wordWidths[i]+=k; }
			}
		} }
		int marginX=profile.UseDisplayRegion ? 0 : Math.Max(0,-atlas.Select(g => g.OffsetX).DefaultIfEmpty(0).Min());
		int marginY=profile.UseDisplayRegion ? 0 : Math.Max(0,-atlas.Select(g => g.OffsetY).DefaultIfEmpty(0).Min());
		long start=(long)marginX*64/scale,cursor=start,line=(long)marginY*64/scale;
		int previous=-1; bool pendingWrap=false;
		var positions=new List<MetaTextPlacement>();
		for(int i=0;i<runes.Length;i++) { var rune=runes[i];
			if(rune.Value=='\n') { cursor=start; line+=atlas.LineHeight64; previous=-1; pendingWrap=false; continue; }
			if(Rune.IsControl(rune)) { continue; }
			int id=ids[i]; var g=atlas[id];
			int kern=0; if(profile.Proportional && previous>=0) { atlas.Kerning.TryGetValue(((ulong)previous<<32)|(uint)id,out kern); }
			int advance=profile.Proportional ? g.Advance64 : profile.CellWidth*64;
			if(words) {
				bool separator=separators[id];
				if(separator && (pendingWrap || (cursor+kern+advance>profile.DisplayWidth*64L && cursor>0))) { pendingWrap=true; previous=-1; continue; }
				if(pendingWrap) { cursor=0; line+=atlas.LineHeight64; kern=0; pendingWrap=false; }
				bool wordStart=!separator && (previous<0 || (i>0 && ids[i-1]>=0 && separators[ids[i-1]]));
				if(wordStart && cursor>0 && cursor+kern+wordWidths[i]>profile.DisplayWidth*64L) { cursor=0; line+=atlas.LineHeight64; kern=0; }
			}
			cursor+=kern;
			if(profile.UseDisplayRegion && profile.WrapText && cursor+advance>profile.DisplayWidth*64L && cursor>0) { cursor=0; line+=atlas.LineHeight64; }
			positions.Add(new(id,cursor,line)); cursor+=advance+spacing; previous=id;
		}
		return positions;
	}
	public static byte[] Render(MetaTextProfile profile,string directory,string text)
	{
		var characters=text.EnumerateRunes().Take(128).Where(r => !Rune.IsControl(r)).Select(r => r.ToString()).ToArray();
		var atlas=(MetaTextGlyphSet)new MetaTextRasterizer.Budget().Rasterize(profile,directory,characters);
		int scale=profile.RasterScale;
		var positions=Layout(profile,atlas,text);
		int width=profile.UseDisplayRegion ? profile.DisplayWidth*scale : Math.Min(2048,Math.Max(1,(int)Math.Ceiling(positions.Select(p => p.X*scale/64.0+atlas[p.Glyph].OffsetX+atlas[p.Glyph].Width).DefaultIfEmpty(1).Max())));
		int height=profile.UseDisplayRegion ? profile.DisplayHeight*scale : Math.Min(2048,Math.Max(1,(int)Math.Ceiling(positions.Select(p => p.Y*scale/64.0+atlas[p.Glyph].OffsetY+atlas[p.Glyph].Height).DefaultIfEmpty(1).Max())));
		if((long)width*height>8*1024*1024) { throw new InvalidDataException("Preview region exceeds 8 million pixels; reduce its dimensions or raster scale."); }
		using var bitmap=new SKBitmap(width,height,SKColorType.Bgra8888,SKAlphaType.Premul); using var canvas=new SKCanvas(bitmap); canvas.Clear(SKColors.Transparent);
		using var fill=new SKPaint {Color=MetaTextRasterizer.Color(profile.FillColor)};
		if(profile.FillEnabled) {
			if(profile.UseDisplayRegion) { canvas.DrawRect(0,0,width,height,fill); }
			else { foreach(var p in positions) { canvas.DrawRect(p.X*scale/64f,p.Y*scale/64f,profile.CellWidth*scale,profile.CellHeight*scale,fill); } }
		}
		uint tint=profile.UsePaletteColor ? 0xFFFFFFFF : Rgba(profile.Color);
		var cached=new Dictionary<int,(SKBitmap Glyph,SKBitmap? Decorations)>();
		try {
			foreach(var p in positions) {
				var g=atlas[p.Glyph];
				if(!cached.TryGetValue(p.Glyph,out var images)) {
					uint[] colored=g.Pixels.Select(pixel => (tint&0xFFFFFF)|((((pixel>>24)*(tint>>24)+127)/255)<<24)).ToArray();
					images=(Image(colored,g.Width,g.Height),g.Decorations==null ? null : Image(g.Decorations,g.Width,g.Height)); cached.Add(p.Glyph,images);
				}
				float x=p.X*scale/64f+g.OffsetX,y=p.Y*scale/64f+g.OffsetY;
				if(images.Decorations!=null) { canvas.DrawBitmap(images.Decorations,x,y); } canvas.DrawBitmap(images.Glyph,x,y);
			}
			using var image=SKImage.FromBitmap(bitmap); using var png=image.Encode(SKEncodedImageFormat.Png,100); return png.ToArray();
		} finally { foreach(var images in cached.Values) { images.Glyph.Dispose(); images.Decorations?.Dispose(); } }
	}
	private static uint Rgba(string text) { uint value=Convert.ToUInt32(text,16); return (value>>8)|((value&255)<<24); }
	private static SKBitmap Image(uint[] pixels,int width,int height)
	{
		var image=new SKBitmap(width,height,SKColorType.Bgra8888,SKAlphaType.Unpremul);
		var row=new int[width]; for(int y=0;y<height;y++) { for(int x=0;x<width;x++) { row[x]=unchecked((int)pixels[y*width+x]); } Marshal.Copy(row,0,image.GetPixels()+y*image.RowBytes,width); }
		return image;
	}
}
