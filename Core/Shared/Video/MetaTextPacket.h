#pragma once
#include "MetaReplacementPacketReader.h"
#include "MetaTextReplacement.h"
#include <set>

class MetaTextPacket
{
	MetaReplacementPacketReader _reader;
	uint32_t Read() { return _reader.Read(); }
public:
	MetaTextPacket(const uint8_t* data,size_t length) : _reader(data,length) {}
	explicit MetaTextPacket(const std::string& path) : _reader(path) {}
	std::vector<MetaTextReplacement> Parse()
	{
		if(!_reader.IsValid() || _reader.Length()>64*1024*1024) { throw std::runtime_error("Invalid text packet"); }
		uint32_t magic=Read(); if(magic!=0x3154584D && magic!=0x3254584D && magic!=0x3354584D && magic!=0x3454584D) { throw std::runtime_error("Invalid text packet"); }
		uint32_t count=Read(); if(count>64) { throw std::runtime_error("Too many text profiles"); }
		std::set<int> ids; std::vector<MetaTextReplacement> result;
		for(uint32_t i=0;i<count;i++) {
			MetaTextReplacement p;
			p.Id=int32_t(Read()); p.Layer=int32_t(Read()); p.MatchKind=int32_t(Read()); p.Scale=int32_t(Read());
			p.CellWidth=int32_t(Read()); p.CellHeight=int32_t(Read());
			uint32_t palette=Read(); p.InkIndex=int32_t(Read()); p.Color=Read(); uint32_t region=Read();
			p.PaletteColor=palette!=0; p.UseRegion=region!=0;
			p.X=int32_t(Read()); p.Y=int32_t(Read()); p.Width=int32_t(Read()); p.Height=int32_t(Read());
			if(magic>=0x3254584D) { uint32_t fill=Read(); if(fill>1) { throw std::runtime_error("Invalid text fill flag"); } p.FillEnabled=fill!=0; p.FillColor=Read(); }
			if(magic>=0x3354584D) {
				uint32_t flags=Read(); p.Proportional=flags&1; p.UseDisplayRegion=flags&2; p.WrapText=flags&4; p.Unclipped=flags&8;
				p.LineHeight64=int32_t(Read()); p.LetterSpacing64=int32_t(Read()); p.DisplayX=int32_t(Read()); p.DisplayY=int32_t(Read()); p.DisplayWidth=int32_t(Read()); p.DisplayHeight=int32_t(Read());
				if(flags>15 || (p.Proportional && !p.Unclipped) || (p.UseDisplayRegion && !p.Unclipped) || p.LineHeight64<1 || p.LineHeight64>512*64 || p.LetterSpacing64< -8*64 || p.LetterSpacing64>32*64
					|| p.DisplayX< -4096 || p.DisplayX>4096 || p.DisplayY< -4096 || p.DisplayY>4096 || p.DisplayWidth<1 || p.DisplayWidth>4096 || p.DisplayHeight<1 || p.DisplayHeight>4096) { throw std::runtime_error("Invalid text run layout"); }
			}
			if(p.Id<1 || !ids.insert(p.Id).second || p.Layer<0 || p.Layer>3 || p.MatchKind<0 || p.MatchKind>1 || p.Scale<1 || p.Scale>8
				|| p.CellWidth<1 || p.CellWidth>128 || p.CellHeight<1 || p.CellHeight>128 || palette>1 || region>1 || p.InkIndex<1 || p.InkIndex>3
				|| p.X<0 || p.X>4096 || p.Y<0 || p.Y>4096 || p.Width<1 || p.Width>4096 || p.Height<1 || p.Height>4096) { throw std::runtime_error("Invalid text profile"); }
			if(magic==0x3454584D) {
				p.CellOriginX=int32_t(Read()); p.CellOriginY=int32_t(Read());
				if(p.CellWidth%8 || p.CellHeight%8 || p.CellOriginX<0 || p.CellOriginX>=p.CellWidth || p.CellOriginY<0 || p.CellOriginY>=p.CellHeight || p.CellOriginX%8 || p.CellOriginY%8
					|| ((p.CellWidth!=8 || p.CellHeight!=8) && !p.Unclipped)) { throw std::runtime_error("Invalid source cell grid"); }
			}
			uint32_t conditions=Read(); if(conditions>64) { throw std::runtime_error("Too many text conditions"); }
			for(uint32_t c=0;c<conditions;c++) {
				uint32_t address=Read(),value=Read(),mask=Read(),op=Read();
				if(value>255 || mask>255 || op>5) { throw std::runtime_error("Invalid text condition"); }
				p.Conditions.push_back({address,uint8_t(value),uint8_t(mask),uint8_t(op)});
			}
			uint32_t glyphs=Read(); if(!glyphs || glyphs>4096) { throw std::runtime_error("Invalid text glyph count"); }
			size_t pixels=size_t(p.CellWidth)*p.CellHeight*p.Scale*p.Scale;
			if(magic<0x3354584D && glyphs>_reader.Remaining()/4/pixels) { throw std::runtime_error("Truncated glyph atlas"); }
			if(p.Proportional && glyphs>256) { throw std::runtime_error("Too many proportional glyphs"); }
			p.Glyphs.resize(glyphs);
			for(auto& glyph:p.Glyphs) {
				glyph.Width=p.CellWidth*p.Scale; glyph.Height=p.CellHeight*p.Scale; glyph.Advance64=p.CellWidth*64;
				if(magic>=0x3354584D) {
					glyph.Width=int32_t(Read()); glyph.Height=int32_t(Read()); glyph.OffsetX=int32_t(Read()); glyph.OffsetY=int32_t(Read()); glyph.Advance64=int32_t(Read());
					if(glyph.Width<1 || glyph.Width>2048 || glyph.Height<1 || glyph.Height>2048 || glyph.OffsetX< -4096 || glyph.OffsetX>4096 || glyph.OffsetY< -4096 || glyph.OffsetY>4096 || glyph.Advance64<0 || glyph.Advance64>512*64
						|| (!p.Unclipped && (glyph.Width!=p.CellWidth*p.Scale || glyph.Height!=p.CellHeight*p.Scale))) { throw std::runtime_error("Invalid glyph bounds/advance"); }
				}
				size_t glyphPixels=size_t(glyph.Width)*glyph.Height; if(glyphPixels>_reader.Remaining()/4) { throw std::runtime_error("Truncated text coverage"); }
				glyph.Pixels.reserve(glyphPixels); for(size_t j=0;j<glyphPixels;j++) { glyph.Pixels.push_back(Read()); }
				if(magic>=0x3354584D) {
					uint32_t effects=Read(); if(effects>1 || (effects && glyphPixels>_reader.Remaining()/4)) { throw std::runtime_error("Invalid text decoration coverage"); }
					if(effects) { glyph.Decorations.reserve(glyphPixels); for(size_t j=0;j<glyphPixels;j++) { glyph.Decorations.push_back(Read()); } }
				}
			}
			uint32_t mappings=Read(); if(!mappings || mappings>4096 || mappings>_reader.Remaining()/12) { throw std::runtime_error("Invalid text mappings"); }
			std::set<std::pair<std::vector<uint32_t>,int>> patterns;
			for(uint32_t m=0;m<mappings;m++) {
				if(magic==0x3454584D) {
					uint32_t cells=Read(); if(cells!=uint32_t(p.CellWidth/8*p.CellHeight/8) || cells>256 || cells>_reader.Remaining()/4) { throw std::runtime_error("Incomplete source tile pattern"); }
					MetaTextReplacement::TilePattern pattern; for(uint32_t t=0;t<cells;t++) { pattern.Tiles.push_back(Read()); }
					pattern.Palette=int32_t(Read()); pattern.Glyph=Read();
					if(pattern.Palette< -1 || pattern.Palette>255 || pattern.Glyph>=glyphs || !patterns.emplace(pattern.Tiles,pattern.Palette).second) { throw std::runtime_error("Invalid source tile pattern"); }
					if(cells==1) { p.Mappings.emplace(MetaTextReplacement::Key(pattern.Tiles[0],pattern.Palette),pattern.Glyph); }
					else { p.PatternStarts[MetaTextReplacement::Key(pattern.Tiles[0],pattern.Palette)].push_back(p.Patterns.size()); p.Patterns.push_back(std::move(pattern)); }
					continue;
				}
				uint32_t tile=Read(); int paletteIndex=int32_t(Read()); uint32_t glyph=Read();
				if(paletteIndex< -1 || paletteIndex>255 || glyph>=glyphs || !p.Mappings.emplace(MetaTextReplacement::Key(tile,paletteIndex),glyph).second) { throw std::runtime_error("Invalid text mapping"); }
			}
			if(magic>=0x3354584D) {
				uint32_t pairs=Read(); if(pairs>65536 || pairs>_reader.Remaining()/12) { throw std::runtime_error("Invalid kerning table"); }
				for(uint32_t k=0;k<pairs;k++) { uint32_t a=Read(),b=Read(); int value=int32_t(Read()); if(a>=glyphs || b>=glyphs || value< -128*64 || value>128*64 || !p.Kerning.emplace((uint64_t(a)<<32)|b,value).second) { throw std::runtime_error("Invalid kerning pair"); } }
			}
			result.push_back(std::move(p));
		}
		_reader.Identity(); return result;
	}
};
