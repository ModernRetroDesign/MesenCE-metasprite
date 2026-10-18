#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

// Same bounded sequential parser source for interop arrays and owned packet
// files. File input retains one 64 KiB read buffer, not a whole-packet copy.
class MetaReplacementPacketReader
{
	const uint8_t* _data = nullptr;
	size_t _length = 0, _offset = 0, _bufferOffset = 0, _bufferCount = 0;
	std::ifstream _file;
	std::vector<uint8_t> _buffer;
	std::array<uint64_t,2> _identity = {14695981039346656037ULL,7809847782465536322ULL};
public:
	MetaReplacementPacketReader(const uint8_t* data,size_t length) : _data(data), _length(length) {}
	explicit MetaReplacementPacketReader(const std::string& path)
	{
		_file.open(std::filesystem::u8path(path),std::ios::binary);
		if(!_file) { throw std::runtime_error("Could not open replacement packet file"); }
		_file.seekg(0,std::ios::end); std::streamoff length=_file.tellg();
		if(length<0 || length>536870912) { throw std::runtime_error("Invalid replacement packet file size"); }
		_length=(size_t)length; _file.seekg(0,std::ios::beg);
		if(!_file) { throw std::runtime_error("Could not seek replacement packet file"); }
		_buffer.resize(65536);
	}
	bool IsValid() const { return _data || _file.is_open(); }
	size_t Length() const { return _length; }
	size_t Remaining() const { return _length-_offset; }
	uint32_t Read()
	{
		if(!IsValid() || Remaining()<4) { throw std::runtime_error("Truncated replacement packet"); }
		const uint8_t* source;
		if(_data) { source=_data+_offset; }
		else {
			if(_bufferOffset==_bufferCount) {
				_bufferCount=std::min(_buffer.size(),Remaining()); _bufferOffset=0;
				_file.read(reinterpret_cast<char*>(_buffer.data()),(std::streamsize)_bufferCount);
				if(_file.gcount()!=(std::streamsize)_bufferCount) { throw std::runtime_error("Truncated replacement packet file"); }
			}
			if(_bufferCount-_bufferOffset<4) { throw std::runtime_error("Unaligned replacement packet file"); }
			source=_buffer.data()+_bufferOffset; _bufferOffset+=4;
		}
		uint32_t value=uint32_t(source[0]) | (uint32_t(source[1])<<8) | (uint32_t(source[2])<<16) | (uint32_t(source[3])<<24);
		for(int i=0;i<4;i++) { _identity[0]=(_identity[0]^source[i])*1099511628211ULL; _identity[1]=(_identity[1]^source[i])*14029467366897019727ULL; }
		_offset+=4; return value;
	}
	std::array<uint64_t,2> Identity()
	{
		if(Remaining()!=0 || (!_data && _file.peek()!=std::char_traits<char>::eof())) { throw std::runtime_error("Trailing replacement packet data"); }
		return _identity;
	}
};
