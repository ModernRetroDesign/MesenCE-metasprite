using SkiaSharp;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;

namespace Mesen.Debugger.Utilities;

public static class MetaTextPreview
{
	public static byte[] Render(MetaTextProfile profile,string directory,string text)
	{
		var runes=text.EnumerateRunes().Take(128).ToArray();
		var characters=runes.Where(r => !Rune.IsControl(r)).Select(r => r.ToString()).ToArray();
		var atlas=(MetaTextGlyphSet)new MetaTextRasterizer.Budget().Rasterize(profile,directory,characters);
		int scale=profile.RasterScale;
		var indices=atlas.Select((g,i) => (g.Character,i)).ToDictionary(g => g.Character,g => g.i);
		int marginX=profile.UseDisplayRegion ? 0 : Math.Max(0,-atlas.Select(g => g.OffsetX).DefaultIfEmpty(0).Min());
		int marginY=profile.UseDisplayRegion ? 0 : Math.Max(0,-atlas.Select(g => g.OffsetY).DefaultIfEmpty(0).Min());
		long start=(long)marginX*64/scale,cursor=start,line=(long)marginY*64/scale;
		int previous=-1;
		var positions=new List<(int Glyph,long X,long Y)>();
		foreach(var rune in runes) {
			if(rune.Value=='\n') { cursor=start; line+=atlas.LineHeight64; previous=-1; continue; }
			if(Rune.IsControl(rune)) { continue; }
			int id=indices[rune.ToString()]; var g=atlas[id];
			if(profile.Proportional && previous>=0 && atlas.Kerning.TryGetValue(((ulong)previous<<32)|(uint)id,out int kern)) { cursor+=kern; }
			int advance=profile.Proportional ? g.Advance64 : profile.CellWidth*64;
			if(profile.UseDisplayRegion && profile.WrapText && cursor+advance>profile.DisplayWidth*64L && cursor>0) { cursor=0; line+=atlas.LineHeight64; }
			positions.Add((id,cursor,line)); cursor+=advance+(int)Math.Round(profile.LetterSpacing*64); previous=id;
		}
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
