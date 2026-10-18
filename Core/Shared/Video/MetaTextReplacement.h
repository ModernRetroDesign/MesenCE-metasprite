#pragma once
#include "MetaSpriteFrame.h"
#include <unordered_map>
#include <memory>

struct MetaTextGlyph
{
	std::vector<uint32_t> Pixels; // Straight-alpha white coverage, tinted at output time.
	int Width=0,Height=0,OffsetX=0,OffsetY=0,Advance64=0;
	std::vector<uint32_t> Decorations;
};

struct MetaTextReplacement
{
	struct TilePattern { std::vector<uint32_t> Tiles; int Palette=-1; uint32_t Glyph=0; };
	std::vector<TilePattern> Patterns;
	std::unordered_map<uint64_t,std::vector<size_t>> PatternStarts;
	int CellOriginX=0,CellOriginY=0;
	int Id=0, Layer=0, MatchKind=0, Scale=1, CellWidth=8, CellHeight=8, InkIndex=1;
	bool PaletteColor=true, UseRegion=false;
	uint32_t Color=0xFFFFFFFF;
	bool FillEnabled=false;
	uint32_t FillColor=0xFF000000;
	bool Proportional=false,UseDisplayRegion=false,WrapText=true,Unclipped=false;
	int DisplayX=0,DisplayY=0,DisplayWidth=256,DisplayHeight=240,LineHeight64=512,LetterSpacing64=0;
	std::unordered_map<uint64_t,int> Kerning;
	int X=0,Y=0,Width=256,Height=240;
	std::vector<MetaMemoryCondition> Conditions;
	std::vector<MetaTextGlyph> Glyphs;
	std::unordered_map<uint64_t,uint32_t> Mappings;
	static uint64_t Key(uint32_t tile,int palette) { return (uint64_t(tile)<<9)|uint64_t(palette+1); }
	const MetaTextGlyph* Find(const MetaTextPixel& tile,const MetaBackgroundSample& sample) const
	{
		if(!tile.Valid || !sample.Valid || sample.Layer!=Layer || tile.LocalX>=CellWidth || tile.LocalY>=CellHeight
			|| (UseRegion && (sample.X<X || sample.Y<Y || int64_t(sample.X)>=int64_t(X)+Width || int64_t(sample.Y)>=int64_t(Y)+Height))) { return nullptr; }
		uint32_t identity=MatchKind ? tile.GraphicsAddress : tile.TileIndex;
		auto found=Mappings.find(Key(identity,tile.Palette));
		if(found==Mappings.end()) { found=Mappings.find(Key(identity,-1)); }
		return found!=Mappings.end() && found->second<Glyphs.size() ? &Glyphs[found->second] : nullptr;
	}
	uint32_t Sample(const MetaTextGlyph& glyph,const MetaTextPixel& tile,int sx,int sy,int outputScale,uint32_t ink) const
	{
		int x=(tile.LocalX*outputScale+sx)*Scale/outputScale, y=(tile.LocalY*outputScale+sy)*Scale/outputScale;
		size_t offset=size_t(y)*CellWidth*Scale+x;
		if(offset>=glyph.Pixels.size()) { return 0; }
		uint32_t tint=PaletteColor ? ink|0xFF000000 : Color;
		uint32_t alpha=((glyph.Pixels[offset]>>24)*(tint>>24)+127)/255;
		return (tint&0xFFFFFF)|(alpha<<24);
	}
};
