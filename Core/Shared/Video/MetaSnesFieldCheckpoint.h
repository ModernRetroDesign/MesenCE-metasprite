#pragma once
#include "MetaSpriteFrame.h"
#include <cstring>
#include <type_traits>
#include <unordered_set>

// Process-local rewind data, not a portable pack/save-state format. Only POD
// payloads are copied as bytes; container and resource pointers are never saved.
// Animation bindings are stable definition IDs supplied/resolved by the owner.
namespace MetaSnesFieldCheckpoint
{
	constexpr size_t PixelCount=256*239, MaxBytes=256*1024*1024;
	constexpr uint32_t Magic=0x31464353; // SCF1
	struct Snapshot {
		std::shared_ptr<MetaSpriteFrame> Frame;
		bool HasAnimationTiming=false;
		std::vector<uint64_t> AnimationBindings; // definition ID, elapsed tick pairs
	};
	class Writer {
	public:
		bool Valid=true;
		std::vector<uint8_t> Bytes;
		void Raw(const void* data,size_t size) {
			if(!Valid || Bytes.size()>MaxBytes || size>MaxBytes-Bytes.size()) { Valid=false; return; }
			if(size) { auto first=(const uint8_t*)data; Bytes.insert(Bytes.end(),first,first+size); }
		}
		template<class T> void Pod(const T& value) { static_assert(std::is_trivially_copyable<T>::value); Raw(&value,sizeof(T)); }
		template<class T> void Vector(const std::vector<T>& values) {
			if(values.size()>UINT32_MAX) { Valid=false; return; }
			Pod((uint32_t)values.size()); Raw(values.data(),values.size()*sizeof(T));
			static_assert(std::is_trivially_copyable<T>::value);
		}
	};
	class Reader {
		const std::vector<uint8_t>& _bytes;
		size_t _offset=0;
	public:
		bool Valid;
		Reader(const std::vector<uint8_t>& bytes):_bytes(bytes),Valid(bytes.size()<=MaxBytes) {}
		void Raw(void* data,size_t size) {
			if(!Valid || size>_bytes.size()-_offset) { Valid=false; return; }
			if(size) { memcpy(data,_bytes.data()+_offset,size); _offset+=size; }
		}
		template<class T> void Pod(T& value) { static_assert(std::is_trivially_copyable<T>::value); Raw(&value,sizeof(T)); }
		template<class T> void Vector(std::vector<T>& values,size_t limit) {
			static_assert(std::is_trivially_copyable<T>::value);
			uint32_t count=0; Pod(count);
			if(!Valid || count>limit || count>(_bytes.size()-_offset)/sizeof(T)) { Valid=false; return; }
			values.resize(count); Raw(values.data(),values.size()*sizeof(T));
		}
		bool Complete() const { return Valid && _offset==_bytes.size(); }
	};
	inline bool ValidBindings(const std::vector<uint64_t>& bindings) {
		if(bindings.size()%2 || bindings.size()>131072) { return false; }
		std::unordered_set<uint64_t> keys;
		for(size_t i=0;i<bindings.size();i+=2) { if(!keys.insert(bindings[i]).second) { return false; } }
		return true;
	}
	inline bool ValidFrame(const MetaSpriteFrame& f) {
		if(f.Width!=256 || f.Height!=239 || f.Pixels.size()!=PixelCount || !f.SnesColorEffects
			|| f.SpriteCoordinateScale!=1 || f.SnesOutputOffsetY || f.SnesHiresSource || f.SnesHiresOddSource
			|| f.GbaColorEffects || f.GameboyCapture || f.PceCapture || f.SmsCapture || f.WsCapture
			|| f.SnesFieldMemoryCaptured || f.SnesFieldAnimationTiming[0] || f.SnesFieldAnimationTiming[1]
			|| f.SnesFieldFrameNumber[0] || f.SnesFieldFrameNumber[1]
			|| !f.SnesFieldMemory[0].empty() || !f.SnesFieldMemory[1].empty()
			|| f.Rows.size()>65536 || f.NesRows.size()>65536 || f.DecodedRows.size()>65536 || f.MemorySnapshot.size()>1048576) { return false; }
		auto plane=[](size_t size) { return !size || size==PixelCount; };
		if(!plane(f.SnesBackgrounds.size()) || !plane(f.SnesSubOutput.size()) || !plane(f.BackgroundUnderlays.size()) || !plane(f.TextPixels.size()) || f.TextLayerMask>15) { return false; }
		for(const auto& text:f.TextLayers) { if(!plane(text.size())) { return false; } }
		for(const auto& p:f.Pixels) {
			if(p.GbaCoverage.size() || p.GbaNativeObjects.size() || p.SnesCoverage.size()>128 || p.SnesNativeObjects.size()>128
				|| p.SnesField || p.VisibleSprite>=128 || p.SecondarySprite>=128 || p.SnesObjectSprite>=128) { return false; }
			for(const auto& c:p.SnesCoverage) { if(c.Sprite<0 || c.Sprite>=128) { return false; } }
			for(const auto& o:p.SnesNativeObjects) { if(o.Sprite<0 || o.Sprite>=128) { return false; } }
		}
		for(const auto& row:f.DecodedRows) {
			if(row.Sprite<0 || row.Sprite>=128 || row.SnesField || row.Width<1 || row.Width>128 || row.Height<1 || row.Height>128
				|| row.Row<0 || row.Row>=row.Height || row.OffsetX<0 || row.OffsetX>row.Width || row.Pixels.size()>size_t(row.Width-row.OffsetX)
				|| (row.SnesRowStep!=1 && row.SnesRowStep!=2) || row.SnesRowPhase>=row.SnesRowStep || row.Row%row.SnesRowStep!=row.SnesRowPhase) { return false; }
		}
		return true;
	}
	inline bool Encode(const MetaSpriteFrame& f,const std::vector<uint64_t>& bindings,std::vector<uint8_t>& output) {
		if(!ValidFrame(f) || !ValidBindings(bindings) || (f.AnimationTiming ? f.AnimationTiming->Elapsed.size()!=bindings.size()/2 : !bindings.empty())) { return false; }
		Writer w; w.Pod(Magic); w.Pod((uint32_t)sizeof(MetaSpritePixelState)); w.Pod((uint32_t)sizeof(MetaSnesBackgroundPixel));
		w.Pod(f.FrameNumber); w.Pod(f.TextLayerMask); w.Pod(uint8_t(f.AnimationTiming!=nullptr)); w.Vector(bindings);
		for(const auto& p:f.Pixels) {
			w.Pod(static_cast<const MetaSpritePixelState&>(p)); w.Pod((uint32_t)p.SnesCoverage.size());
			for(const auto& c:p.SnesCoverage) { w.Pod(c); }
			w.Pod((uint32_t)p.SnesNativeObjects.size()); for(const auto& o:p.SnesNativeObjects) { w.Pod(o); }
		}
		w.Vector(f.SnesBackgrounds); w.Vector(f.SnesSubOutput); w.Vector(f.BackgroundUnderlays); w.Vector(f.TextPixels);
		for(const auto& text:f.TextLayers) { w.Vector(text); }
		w.Vector(f.Rows); w.Vector(f.NesRows); w.Pod((uint32_t)f.DecodedRows.size());
		for(const auto& row:f.DecodedRows) {
			w.Pod(row.Sprite); w.Pod(row.X); w.Pod(row.Y); w.Pod(row.Row); w.Pod(row.Width); w.Pod(row.Height); w.Vector(row.Pixels);
			w.Pod(row.OffsetX); w.Pod(row.ExpansionClass); w.Pod(row.GbaPriority); w.Pod(row.GbaSemiTransparent);
			w.Pod(row.SnesPriorityCode); w.Pod(row.SnesPalette); w.Pod(row.SnesFetchOrder); w.Pod(row.SnesField); w.Pod(row.SnesRowStep); w.Pod(row.SnesRowPhase);
		}
		w.Pod((uint32_t)f.MemorySnapshot.size()); for(const auto& entry:f.MemorySnapshot) { w.Pod(entry.first); w.Pod(entry.second); }
		if(!w.Valid) { return false; } output=std::move(w.Bytes); return true;
	}
	inline bool Decode(const std::vector<uint8_t>& bytes,Snapshot& output) {
		Reader r(bytes); uint32_t magic=0,pixelSize=0,bgSize=0; r.Pod(magic); r.Pod(pixelSize); r.Pod(bgSize);
		if(!r.Valid || magic!=Magic || pixelSize!=sizeof(MetaSpritePixelState) || bgSize!=sizeof(MetaSnesBackgroundPixel)) { return false; }
		Snapshot result; auto f=std::make_shared<MetaSpriteFrame>(256,239); f->SnesColorEffects=true;
		uint8_t timing=0; r.Pod(f->FrameNumber); r.Pod(f->TextLayerMask); r.Pod(timing); r.Vector(result.AnimationBindings,131072);
		if(timing>1 || !ValidBindings(result.AnimationBindings) || (!timing && !result.AnimationBindings.empty())) { return false; }
		result.HasAnimationTiming=timing!=0;
		for(auto& p:f->Pixels) {
			r.Pod(static_cast<MetaSpritePixelState&>(p)); uint32_t count=0; r.Pod(count);
			if(!r.Valid || count>128) { return false; }
			for(uint32_t i=0;i<count;i++) { MetaSpritePixel::SnesCoverageSample c; r.Pod(c); p.SnesCoverage.push_back(c); }
			r.Pod(count); if(!r.Valid || count>128) { return false; }
			for(uint32_t i=0;i<count;i++) { MetaSnesNativeObject o; r.Pod(o); p.SnesNativeObjects.push_back(o); }
			if(!r.Valid) { return false; }
		}
		r.Vector(f->SnesBackgrounds,PixelCount); r.Vector(f->SnesSubOutput,PixelCount); r.Vector(f->BackgroundUnderlays,PixelCount); r.Vector(f->TextPixels,PixelCount);
		for(auto& text:f->TextLayers) { r.Vector(text,PixelCount); }
		r.Vector(f->Rows,65536); r.Vector(f->NesRows,65536);
		uint32_t count=0; r.Pod(count); if(!r.Valid || count>65536) { return false; }
		f->DecodedRows.resize(count);
		for(auto& row:f->DecodedRows) {
			r.Pod(row.Sprite); r.Pod(row.X); r.Pod(row.Y); r.Pod(row.Row); r.Pod(row.Width); r.Pod(row.Height); r.Vector(row.Pixels,128);
			r.Pod(row.OffsetX); r.Pod(row.ExpansionClass); r.Pod(row.GbaPriority); r.Pod(row.GbaSemiTransparent);
			r.Pod(row.SnesPriorityCode); r.Pod(row.SnesPalette); r.Pod(row.SnesFetchOrder); r.Pod(row.SnesField); r.Pod(row.SnesRowStep); r.Pod(row.SnesRowPhase);
		}
		r.Pod(count); if(!r.Valid || count>1048576) { return false; }
		for(uint32_t i=0;i<count;i++) { uint32_t address=0; uint8_t value=0; r.Pod(address); r.Pod(value); if(!r.Valid || !f->MemorySnapshot.emplace(address,value).second) { return false; } }
		if(!r.Complete() || !ValidFrame(*f)) { return false; }
		result.Frame=std::move(f); output=std::move(result); return true;
	}
}
