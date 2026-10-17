#pragma once
#include "MetaSpriteCompositor.h"
#include <stdexcept>
#include <set>

class MetaSpritePacket
{
	const uint8_t* _data;
	size_t _length, _offset = 0;
	MetaPngPixelPool _localPool;
	MetaPngPixelPool* _pixelPool;
	uint32_t Read()
	{
		if(_length - _offset < 4) { throw std::runtime_error("Truncated metasprite packet"); }
		uint32_t value = (uint32_t)_data[_offset] | ((uint32_t)_data[_offset + 1] << 8) | ((uint32_t)_data[_offset + 2] << 16) | ((uint32_t)_data[_offset + 3] << 24);
		_offset += 4;
		return value;
	}
	int Coordinate()
	{
		int32_t value = static_cast<int32_t>(Read());
		if(value < -4096 || value > 4096) { throw std::runtime_error("Invalid origin or member coordinate"); }
		return value;
	}
	int Dimension(int maximum)
	{
		uint32_t value = Read();
		if(value < 1 || value > (uint32_t)maximum) { throw std::runtime_error("Invalid dimension"); }
		return (int)value;
	}
	std::vector<uint32_t> Pixels(int width, int height)
	{
		size_t count = (size_t)width * height;
		if(count > (_length - _offset) / 4) { throw std::runtime_error("Invalid pixel buffer"); }
		std::vector<uint32_t> pixels;
		pixels.reserve(count);
		for(size_t i = 0; i < count; i++) { pixels.push_back(Read()); }
		return pixels;
	}

	MetaPngArtworkAnimation Animation()
	{
		MetaPngArtworkAnimation animation; uint32_t count=Read();
		if(count==0) { return animation; }
		if(count>256) { throw std::runtime_error("Too many PNG animation frames"); }
		uint32_t mode=Read(), restart=Read();
		if(mode>2 || restart>1) { throw std::runtime_error("Unsupported PNG animation playback settings"); }
		animation.RestartOnActivation=restart!=0;
		std::vector<uint32_t> durations;
		for(uint32_t i=0;i<count;i++) {
			uint32_t duration=Read(); if(duration<1 || duration>3600000) { throw std::runtime_error("Invalid PNG frame duration"); }
			durations.push_back(duration); MetaPngArtworkFrame frame;
			frame.OriginX=Coordinate(); frame.OriginY=Coordinate(); frame.Width=Dimension(8192); frame.Height=Dimension(8192);
			frame.Pixels=_pixelPool->Intern(Pixels(frame.Width,frame.Height)); animation.Frames.push_back(std::move(frame));
		}
		animation.Timeline=MetaPngTimeline((MetaPngPlayback)mode,durations); return animation;
	}
public:
	MetaSpritePacket(const uint8_t* data, size_t length, MetaPngPixelPool* pool = nullptr) : _data(data), _length(length), _pixelPool(pool ? pool : &_localPool) {}
	std::vector<MetaSpriteReplacement> Parse()
	{
		if(!_data || _length > 536870912) { throw std::runtime_error("Invalid metasprite packet"); }
		uint32_t magic = Read();
		if(magic != 0x3150534D && magic != 0x3250534D && magic != 0x3350534D && magic != 0x3450534D && magic != 0x3550534D) { throw std::runtime_error("Invalid metasprite packet"); }
		uint32_t count = Read();
		if(count > 1024) { throw std::runtime_error("Too many groups"); }
		std::vector<MetaSpriteReplacement> groups;
		std::set<int> ids;
		for(uint32_t i = 0; i < count; i++) {
			MetaSpriteReplacement group;
			group.Id = Dimension(INT32_MAX);
			if(!ids.insert(group.Id).second) { throw std::runtime_error("Duplicate group ID"); }
			group.OriginX = Coordinate(); group.OriginY = Coordinate();
			group.ArtworkOriginX = Coordinate(); group.ArtworkOriginY = Coordinate();
			group.ArtworkScale = Dimension(10);
			group.ArtworkWidth = Dimension(8192); group.ArtworkHeight = Dimension(8192);
			int members = Dimension(128);
			for(int m = 0; m < members; m++) {
				MetaSpriteGraphic member;
				member.X = Coordinate(); member.Y = Coordinate();
				member.Width = Dimension(128); member.Height = Dimension(128);
				member.Pixels = Pixels(member.Width, member.Height);
				group.Members.push_back(std::move(member));
			}
			group.Artwork = Pixels(group.ArtworkWidth, group.ArtworkHeight);
			if(magic != 0x3150534D) {
				uint32_t conditions = Read();
				if(conditions > 64) { throw std::runtime_error("Too many memory conditions"); }
				for(uint32_t c = 0; c < conditions; c++) {
					uint32_t address = Read(), value = Read(), mask = Read();
					uint32_t comparison = magic == 0x3450534D || magic == 0x3550534D ? Read() : 0;
					if(comparison > 5) { throw std::runtime_error("Invalid comparison operator"); }
					if(value > 255 || mask > 255) { throw std::runtime_error("Invalid memory condition byte"); }
					group.MemoryConditions.push_back({address, static_cast<uint8_t>(value), static_cast<uint8_t>(mask), static_cast<uint8_t>(comparison)});
				}
			}
			if(magic == 0x3350534D || magic == 0x3450534D || magic == 0x3550534D) {
				uint32_t alternates = Read();
				if(alternates > 64) { throw std::runtime_error("Too many alternate images"); }
				for(uint32_t a = 0; a < alternates; a++) {
					MetaAlternateArtwork alternate;
					alternate.ArtworkOriginX = Coordinate(); alternate.ArtworkOriginY = Coordinate();
					alternate.ArtworkScale = Dimension(10);
					alternate.ArtworkWidth = Dimension(8192); alternate.ArtworkHeight = Dimension(8192);
					alternate.Artwork = Pixels(alternate.ArtworkWidth, alternate.ArtworkHeight);
					int conditions = Dimension(64);
					for(int c = 0; c < conditions; c++) {
						uint32_t address = Read(), value = Read(), mask = Read();
						uint32_t comparison = magic == 0x3450534D || magic == 0x3550534D ? Read() : 0;
						if(comparison > 5) { throw std::runtime_error("Invalid alternate comparison operator"); }
						if(value > 255 || mask > 255) { throw std::runtime_error("Invalid alternate condition byte"); }
						alternate.MemoryConditions.push_back({address, static_cast<uint8_t>(value), static_cast<uint8_t>(mask), static_cast<uint8_t>(comparison)});
					}
					if(magic == 0x3550534D) { alternate.Animation=Animation(); }
					group.AlternateArtwork.push_back(std::move(alternate));
				}
			}
			if(magic == 0x3550534D) { group.Animation=Animation(); }
			groups.push_back(std::move(group));
		}
		if(_offset != _length) { throw std::runtime_error("Trailing packet data"); }
		auto identity=MetaPngPacketIdentity(_data,_length);
		for(auto& group : groups) {
			group.Animation.SourceIdentity=identity;
			for(auto& alternate : group.AlternateArtwork) { alternate.Animation.SourceIdentity=identity; }
		}
		return groups;
	}
};
