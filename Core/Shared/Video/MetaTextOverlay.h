#pragma once
#include "MetaTextReplacement.h"
#include <functional>
#include <unordered_set>
#include <tuple>

// Glyphs are cached at pack activation. This pass only locates visible text,
// expands finished pixels if necessary, then blends matched glyph coverage.
// It never reconstructs the native background/OBJ scene or erases native tiles.
class MetaTextOverlay
{
	struct Match { size_t Index; const MetaTextReplacement* Profile; const MetaTextGlyph* Glyph; };
	std::vector<Match> _matches;
	std::vector<uint32_t> _output;
	std::vector<uint32_t> _previousOutput;
	std::vector<uint32_t> _columns;
	struct Token { int X,Y,SourceX,SourceY; uint32_t GlyphIndex; const MetaTextReplacement* Profile; const MetaTextGlyph* Glyph; const MetaTextPixel* Tile; };
	std::vector<Token> _tokens;
	static uint32_t Blend(uint32_t front,uint32_t back)
	{
		uint32_t alpha=front>>24;
		if(!alpha) { return back; } if(alpha==255) { return front; }
		uint32_t result=0xFF000000;
		for(int c=0;c<3;c++) { result|=((((front>>(c*8))&255)*alpha+((back>>(c*8))&255)*(255-alpha)+127)/255)<<(c*8); }
		return result;
	}
	static int Floor(int64_t n,int64_t d) { return int(n>=0 ? n/d : -((-n+d-1)/d)); }
	static uint64_t Position(int x,int y) { return (uint64_t(uint32_t(x))<<32)|uint32_t(y); }
	void DrawRuns(const std::vector<const MetaTextReplacement*>& profiles,uint32_t width,uint32_t height,uint32_t scale,int cropLeft,int cropTop,
		const std::function<uint32_t(uint16_t)>& nativeColor)
	{
		auto* output=_output.data();
		for(const auto* profile:profiles) {
			if(!profile->Unclipped) { continue; } const auto& p=*profile;
			std::vector<const Token*> tokens; for(const auto& token:_tokens) { if(token.Profile==profile) { tokens.push_back(&token); } }
			if(tokens.empty()) { continue; }
			std::stable_sort(tokens.begin(),tokens.end(),[&](const auto* a,const auto* b) { return p.UseDisplayRegion ? std::tie(a->SourceY,a->SourceX)<std::tie(b->SourceY,b->SourceX) : std::tie(a->Y,a->X)<std::tie(b->Y,b->X); });
			int left=p.UseDisplayRegion ? std::max(0,(p.DisplayX-cropLeft)*int(scale)) : 0,top=p.UseDisplayRegion ? std::max(0,(p.DisplayY-cropTop)*int(scale)) : 0;
			int right=p.UseDisplayRegion ? std::min(int(width),(p.DisplayX+p.DisplayWidth-cropLeft)*int(scale)) : int(width);
			int bottom=p.UseDisplayRegion ? std::min(int(height),(p.DisplayY+p.DisplayHeight-cropTop)*int(scale)) : int(height);
			if(left>=right || top>=bottom) { continue; }
			auto fill=[&](int x,int y,int w,int h) {
				int x1=std::max(left,x),y1=std::max(top,y),x2=std::min(right,x+w),y2=std::min(bottom,y+h);
				for(int dy=y1;dy<y2;dy++) { auto* row=output+size_t(dy)*width; for(int dx=x1;dx<x2;dx++) { row[dx]=Blend(p.FillColor,row[dx]); } }
			};
			if(p.FillEnabled) {
				if(p.UseDisplayRegion) { fill(left,top,right-left,bottom-top); }
				else { for(const auto* t:tokens) { fill((t->X-cropLeft)*int(scale),(t->Y-cropTop)*int(scale),p.CellWidth*int(scale),p.CellHeight*int(scale)); } }
			}
			int64_t cursor=0,line=0; int previousRow=INT32_MIN,previousX=INT32_MIN; uint32_t previousGlyph=UINT32_MAX;
			for(const auto* token:tokens) {
				int row=p.UseDisplayRegion ? token->SourceY : token->Y,x=p.UseDisplayRegion ? token->SourceX : token->X;
				bool newRow=row!=previousRow,adjacent=previousGlyph!=UINT32_MAX && x==previousX+p.CellWidth;
				if(newRow) {
					if(p.UseDisplayRegion) { cursor=int64_t(p.DisplayX)*64; line=previousRow==INT32_MIN ? int64_t(p.DisplayY)*64 : line+p.LineHeight64; }
					else { cursor=int64_t(token->X)*64; line=int64_t(token->Y)*64; }
					previousGlyph=UINT32_MAX;
				} else if(!p.UseDisplayRegion && (!p.Proportional || !adjacent)) { cursor=int64_t(token->X)*64; }
				if(p.Proportional && adjacent && !newRow) { auto kern=p.Kerning.find((uint64_t(previousGlyph)<<32)|token->GlyphIndex); if(kern!=p.Kerning.end()) { cursor+=kern->second; } }
				int advance=p.Proportional ? token->Glyph->Advance64 : p.CellWidth*64;
				if(p.UseDisplayRegion && p.WrapText && cursor+advance>int64_t(p.DisplayX+p.DisplayWidth)*64 && cursor>int64_t(p.DisplayX)*64) { cursor=int64_t(p.DisplayX)*64; line+=p.LineHeight64; }
				const auto& g=*token->Glyph;
				int64_t startX=cursor-int64_t(cropLeft)*64+int64_t(g.OffsetX)*64/p.Scale,startY=line-int64_t(cropTop)*64+int64_t(g.OffsetY)*64/p.Scale;
				int x1=std::max(left,Floor(startX*scale,64)),y1=std::max(top,Floor(startY*scale,64));
				int x2=std::min(right,Floor((startX+int64_t(g.Width)*64/p.Scale)*scale+63,64)),y2=std::min(bottom,Floor((startY+int64_t(g.Height)*64/p.Scale)*scale+63,64));
				uint32_t tint=p.PaletteColor ? nativeColor(token->Tile->InkColors[p.InkIndex-1])|0xFF000000 : p.Color;
				const auto* coverage=g.Pixels.data(); const auto* decorations=g.Decorations.data();
				for(int dy=y1;dy<y2;dy++) {
					int sy=Floor((int64_t(dy)*64+32-startY*scale)*p.Scale,64*scale); if(sy<0 || sy>=g.Height) { continue; }
					auto* destination=output+size_t(dy)*width;
					for(int dx=x1;dx<x2;dx++) {
						int sx=Floor((int64_t(dx)*64+32-startX*scale)*p.Scale,64*scale); if(sx<0 || sx>=g.Width) { continue; }
						size_t index=size_t(sy)*g.Width+sx; if(index>=g.Pixels.size()) { continue; }
						uint32_t color=destination[dx]; if(!g.Decorations.empty()) { color=Blend(decorations[index],color); }
						uint32_t alpha=((coverage[index]>>24)*(tint>>24)+127)/255; destination[dx]=Blend((tint&0xFFFFFF)|(alpha<<24),color);
					}
				}
				cursor+=advance+p.LetterSpacing64; previousRow=row; previousX=x; previousGlyph=token->GlyphIndex;
			}
		}
	}
public:
	uint32_t* ApplyPlane(uint32_t* source,uint32_t width,uint32_t height,const MetaSpriteFrame& frame,int cropLeft,int cropTop,
		uint32_t& outputScale,const std::vector<MetaTextReplacement>& profiles,const std::function<uint32_t(uint16_t)>& nativeColor,int plane)
	{
		_matches.clear();
		_tokens.clear();
		if(!source || profiles.empty() || !nativeColor || !width || !height || cropLeft<0 || cropTop<0 || outputScale<1 || outputScale>10
			|| uint64_t(width)+cropLeft>frame.Width || uint64_t(height)+cropTop>frame.Height || frame.Pixels.size()!=size_t(frame.Width)*frame.Height
			|| (plane<0 ? frame.TextPixels.size()!=frame.Pixels.size() : frame.TextLayers[plane].size()!=frame.Pixels.size())) { return source; }
		std::vector<const MetaTextReplacement*> active;
		bool hasRegion=false;
		int minWidth=128,minHeight=128;
		for(const auto& profile:profiles) { if((plane<0 || profile.Layer==plane) && MetaMemoryCondition::Matches(profile.Conditions,frame.MemorySnapshot)) { active.push_back(&profile); hasRegion|=profile.UseRegion; minWidth=std::min(minWidth,profile.CellWidth); minHeight=std::min(minHeight,profile.CellHeight); } }
		if(active.empty()) { return source; }
		const auto* pixels=frame.Pixels.data();
		auto tileAt=[&](size_t index) -> const MetaTextPixel& { return plane<0 ? frame.TextPixels[index] : frame.TextLayers[plane][index].Tile; };
		auto sampleAt=[&](size_t index) { if(plane<0) { return pixels[index].BackgroundSample; } const auto& t=frame.TextLayers[plane][index]; return MetaBackgroundSample {t.X,t.Y,(uint8_t)plane,t.Tile.Valid}; };
		const MetaTextReplacement* lastProfile=nullptr; const MetaTextGlyph* lastGlyph=nullptr;
		uint32_t lastTile=0,lastAddress=0; uint8_t lastPalette=0,lastLayer=0; bool cached=false;
		uint32_t targetScale=outputScale;
		// Build one visible-tile index only when a multi-tile profile is active.
		std::unordered_map<uint64_t,size_t> cells;
		std::unordered_map<uint64_t,size_t> owners;
		bool multi=std::any_of(active.begin(),active.end(),[](const auto* p) { return !p->Patterns.empty(); });
		if(multi) {
			for(uint32_t y=0;y<height;y++) { for(uint32_t x=0;x<width;x++) {
				size_t i=(size_t(y)+cropTop)*frame.Width+x+cropLeft; const auto& t=tileAt(i); const auto& s=sampleAt(i);
				if(t.Valid && s.Valid && t.LocalX<8 && t.LocalY<8) { cells.emplace(Position(int(x)+cropLeft-t.LocalX,int(y)+cropTop-t.LocalY),i); }
			} }
			for(size_t priority=0;priority<active.size();priority++) {
				const auto& p=*active[priority]; if(p.Patterns.empty()) { continue; }
				for(const auto& cell:cells) {
					size_t index=cell.second; const auto& tile=tileAt(index); const auto& sample=sampleAt(index);
					int sourceX=sample.X-tile.LocalX,sourceY=sample.Y-tile.LocalY;
					if(sample.Layer!=p.Layer || (sourceX-p.CellOriginX)%p.CellWidth || (sourceY-p.CellOriginY)%p.CellHeight
						|| (p.UseRegion && (sourceX<p.X || sourceY<p.Y || int64_t(sourceX)+p.CellWidth>int64_t(p.X)+p.Width || int64_t(sourceY)+p.CellHeight>int64_t(p.Y)+p.Height))) { continue; }
					int tx=int(index%frame.Width)-tile.LocalX,ty=int(index/frame.Width)-tile.LocalY;
					const MetaTextReplacement::TilePattern* matched=nullptr;
					uint32_t first=p.MatchKind ? tile.GraphicsAddress : tile.TileIndex;
					for(int palette:{int(tile.Palette),-1}) {
						auto starts=p.PatternStarts.find(MetaTextReplacement::Key(first,palette)); if(starts==p.PatternStarts.end()) { continue; }
						for(size_t id:starts->second) {
							const auto& pattern=p.Patterns[id]; bool valid=pattern.Glyph<p.Glyphs.size() && pattern.Tiles.size()==size_t(p.CellWidth/8*p.CellHeight/8);
							for(size_t t=0;valid && t<pattern.Tiles.size();t++) {
								int ox=int(t%(p.CellWidth/8))*8,oy=int(t/(p.CellWidth/8))*8;
								auto key=Position(tx+ox,ty+oy); auto part=cells.find(key); if(part==cells.end() || owners.count(key)) { valid=false; break; }
								const auto& tt=tileAt(part->second); const auto& ss=sampleAt(part->second);
								if(ss.Layer!=p.Layer || ss.X-tt.LocalX!=sourceX+ox || ss.Y-tt.LocalY!=sourceY+oy
									|| (p.MatchKind ? tt.GraphicsAddress : tt.TileIndex)!=pattern.Tiles[t] || (pattern.Palette>=0 && tt.Palette!=pattern.Palette)) { valid=false; break; }
								for(size_t earlier=0;earlier<priority;earlier++) { if(active[earlier]->Patterns.empty() && active[earlier]->Find(tt,ss)) { valid=false; break; } }
							}
							if(valid) { matched=&pattern; break; }
						}
						if(matched) { break; }
					}
					if(!matched) { continue; }
					const auto* glyph=&p.Glyphs[matched->Glyph];
					_tokens.push_back({tx,ty,sourceX,sourceY,matched->Glyph,&p,glyph,&tile}); _matches.push_back({index,&p,glyph}); targetScale=std::max(targetScale,uint32_t(p.Scale));
					for(int oy=0;oy<p.CellHeight;oy+=8) { for(int ox=0;ox<p.CellWidth;ox+=8) { owners.emplace(Position(tx+ox,ty+oy),priority); } }
				}
			}
		}
		std::unordered_set<uint64_t> tokenKeys;
		for(uint32_t y=0;y<height;y++) { for(uint32_t x=0;x<width;x++) {
			size_t index=(size_t(y)+cropTop)*frame.Width+x+cropLeft;
			const auto& tile=tileAt(index); const auto& sample=sampleAt(index);
			if(!tile.Valid || !sample.Valid) { continue; }
			if(multi && owners.count(Position(int(x)+cropLeft-tile.LocalX,int(y)+cropTop-tile.LocalY))) { continue; }
			if(hasRegion || !cached || tile.TileIndex!=lastTile || tile.GraphicsAddress!=lastAddress || tile.Palette!=lastPalette || sample.Layer!=lastLayer) {
				lastProfile=nullptr; lastGlyph=nullptr;
				for(const auto* profile:active) { if(auto glyph=profile->Find(tile,sample)) { lastProfile=profile; lastGlyph=glyph; break; } }
				lastTile=tile.TileIndex; lastAddress=tile.GraphicsAddress; lastPalette=tile.Palette; lastLayer=sample.Layer; cached=tile.LocalX<minWidth && tile.LocalY<minHeight;
			}
			if(lastGlyph && tile.LocalX<lastProfile->CellWidth && tile.LocalY<lastProfile->CellHeight) {
				_matches.push_back({index,lastProfile,lastGlyph}); targetScale=std::max(targetScale,uint32_t(lastProfile->Scale));
				if(lastProfile->Unclipped) {
					int tx=int(x)+cropLeft-tile.LocalX,ty=int(y)+cropTop-tile.LocalY;
					uint64_t key=(uint64_t(uint32_t(lastProfile->Id))<<32)|(uint64_t(uint32_t(tx+4096))<<13)|uint32_t(ty+4096);
					if(tokenKeys.insert(key).second) { _tokens.push_back({tx,ty,sample.X-tile.LocalX,sample.Y-tile.LocalY,uint32_t(lastGlyph-lastProfile->Glyphs.data()),lastProfile,lastGlyph,&tile}); }
				}
			}
		} }
		if(_matches.empty()) { return source; } // Unmatched definitions never upscale the game.
		uint32_t inputWidth=width*outputScale,outWidth=width*targetScale,outHeight=height*targetScale;
		_output.resize(size_t(outWidth)*outHeight); auto* output=_output.data();
		if(targetScale==outputScale) { std::copy_n(source,_output.size(),output); }
		else {
			_columns.resize(outWidth); auto* columns=_columns.data();
			for(uint32_t x=0;x<outWidth;x++) { columns[x]=x*outputScale/targetScale; }
			for(uint32_t y=0;y<outHeight;y++) {
				const auto* row=source+size_t(y*outputScale/targetScale)*inputWidth; auto* destination=output+size_t(y)*outWidth;
				for(uint32_t x=0;x<outWidth;x++) { destination[x]=row[columns[x]]; }
			}
		}
		for(const auto& match:_matches) {
			const auto& profile=*match.Profile; const auto& tile=tileAt(match.Index);
			if(profile.Unclipped) { continue; }
			uint32_t x=uint32_t(match.Index%frame.Width)-cropLeft,y=uint32_t(match.Index/frame.Width)-cropTop;
			uint32_t ink=profile.PaletteColor ? nativeColor(tile.InkColors[profile.InkIndex-1]) : 0;
			for(uint32_t sy=0;sy<targetScale;sy++) {
				auto* row=output+(size_t(y)*targetScale+sy)*outWidth+size_t(x)*targetScale;
				for(uint32_t sx=0;sx<targetScale;sx++) {
					uint32_t color=profile.FillEnabled ? Blend(profile.FillColor,row[sx]) : row[sx];
					row[sx]=Blend(profile.Sample(*match.Glyph,tile,sx,sy,targetScale,ink),color);
				}
			}
		}
		DrawRuns(active,outWidth,outHeight,targetScale,cropLeft,cropTop,nativeColor);
		outputScale=targetScale; return output;
	}
public:
	uint32_t* Apply(uint32_t* source,uint32_t width,uint32_t height,const MetaSpriteFrame& frame,int cropLeft,int cropTop,
		uint32_t& outputScale,const std::vector<MetaTextReplacement>& profiles,const std::function<uint32_t(uint16_t)>& nativeColor)
	{
		if(!frame.TextLayerMask) { return ApplyPlane(source,width,height,frame,cropLeft,cropTop,outputScale,profiles,nativeColor,-1); }
		for(int layer=0;layer<4;layer++) {
			if(frame.TextLayers[layer].empty()) { continue; }
			if(std::none_of(profiles.begin(),profiles.end(),[&](const auto& p) { return p.Layer==layer; })) { continue; }
			// Keep the preceding plane alive if this pass needs to resize its output.
			if(source==_output.data()) { _previousOutput.swap(_output); source=_previousOutput.data(); }
			auto* result=ApplyPlane(source,width,height,frame,cropLeft,cropTop,outputScale,profiles,nativeColor,layer);
			if(result==_previousOutput.data() && !_previousOutput.empty()) { _output.swap(_previousOutput); source=_output.data(); } else { source=result; }
		}
		return source;
	}
};
