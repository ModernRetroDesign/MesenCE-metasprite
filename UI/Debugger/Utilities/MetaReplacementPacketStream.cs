using System;
using System.Buffers.Binary;
using System.IO;
using System.Runtime.InteropServices;

namespace Mesen.Debugger.Utilities;

// Forward-only packet builder. Large payloads spill instead of growing a second
// contiguous managed buffer alongside the final interop array.
internal sealed class MetaReplacementPacketStream : Stream
{
	private Stream _storage;
	private readonly long _maximumLength;
	private readonly int _memoryThreshold;
	private readonly bool _fileLease;
	private bool _disposed;
	private byte[]? _comparisonBuffer;
	internal string? SpillPath { get; private set; }
	internal int MemoryCapacity => _storage is MemoryStream memory ? memory.Capacity : 0;
	public MetaReplacementPacketStream(long maximumLength=536870912,int memoryThreshold=4*1024*1024,bool fileLease=false)
	{
		if(maximumLength<0 || maximumLength>Array.MaxLength) { throw new ArgumentOutOfRangeException(nameof(maximumLength)); }
		if(memoryThreshold<0) { throw new ArgumentOutOfRangeException(nameof(memoryThreshold)); }
		_maximumLength=maximumLength; _memoryThreshold=memoryThreshold; _fileLease=fileLease;
		_storage=new MemoryStream((int)Math.Min(65536,Math.Min(maximumLength,memoryThreshold)));
	}
	public override bool CanRead => false;
	public override bool CanSeek => false;
	public override bool CanWrite => !_disposed;
	public override long Length { get { ThrowIfDisposed(); return _storage.Length; } }
	public override long Position { get => Length; set => throw new NotSupportedException(); }
	private void ThrowIfDisposed() { ObjectDisposedException.ThrowIf(_disposed,this); }
	public override void Flush() { ThrowIfDisposed(); _storage.Flush(); }
	public override void Write(byte[] buffer,int offset,int count) { ArgumentNullException.ThrowIfNull(buffer); Write(buffer.AsSpan(offset,count)); }
	public override void WriteByte(byte value) { Span<byte> data=stackalloc byte[1]; data[0]=value; Write(data); }
	public override void Write(ReadOnlySpan<byte> data)
	{
		ThrowIfDisposed();
		if(data.Length>_maximumLength-_storage.Length) { throw new InvalidDataException("Replacement packet exceeds the 512 MiB transport limit."); }
		if(_storage is MemoryStream && _storage.Length+data.Length>_memoryThreshold) { Spill(); }
		if(_storage is MemoryStream boundedMemory && boundedMemory.Capacity<_storage.Length+data.Length) {
			long capacity=Math.Max(_storage.Length+data.Length,(long)boundedMemory.Capacity*2);
			boundedMemory.Capacity=(int)Math.Min(capacity,Math.Min(_memoryThreshold,_maximumLength));
		}
		_storage.Write(data);
	}
	private void Spill()
	{
		if(_storage is not MemoryStream memory) { return; }
		string path=Path.Combine(Path.GetTempPath(),"mesen-png-packet-"+Guid.NewGuid().ToString("N")+".tmp");
		var file=new FileStream(path,FileMode.CreateNew,FileAccess.ReadWrite,_fileLease ? FileShare.Read : FileShare.None,65536,
			FileOptions.SequentialScan|(_fileLease ? FileOptions.None : FileOptions.DeleteOnClose));
		try { memory.Position=0; memory.CopyTo(file); }
		catch { file.Dispose(); if(_fileLease) { File.Delete(path); } memory.Position=memory.Length; throw; }
		_storage=file; SpillPath=path; memory.Dispose();
	}
	internal MetaReplacementPacketFile DetachFile()
	{
		ThrowIfDisposed();
		if(!_fileLease) { throw new InvalidOperationException("Packet was not built for file handoff."); }
		Spill(); _storage.Flush(); long length=_storage.Length; _storage.Dispose();
		FileStream? reader=null;
		try {
			reader=new FileStream(SpillPath!,FileMode.Open,FileAccess.Read,FileShare.Read,65536,FileOptions.SequentialScan);
			if(reader.Length!=length) { throw new InvalidDataException("Packet file changed during sealing."); }
			var file=new MetaReplacementPacketFile(reader,SpillPath!);
			_storage=Stream.Null; _disposed=true; return file;
		} catch { reader?.Dispose(); throw; }
	}
	public byte[] ToArray()
	{
		ThrowIfDisposed();
		if(_storage is MemoryStream memory) { return memory.ToArray(); }
		_storage.Flush();
		byte[] result=new byte[checked((int)_storage.Length)];
		try { _storage.Position=0; _storage.ReadExactly(result); }
		finally { _storage.Position=_storage.Length; }
		return result;
	}
	internal bool MatchesPixels(long offset,uint[] pixels)
	{
		ThrowIfDisposed();
		long bytes=pixels.LongLength*4;
		if(offset<0 || offset>Length || bytes>Length-offset) { throw new InvalidDataException("Invalid packet artwork range."); }
		bool Matches(ReadOnlySpan<byte> stored,int pixelOffset) {
			if(BitConverter.IsLittleEndian) { return stored.SequenceEqual(MemoryMarshal.AsBytes(pixels.AsSpan(pixelOffset,stored.Length/4))); }
			for(int i=0;i<stored.Length;i+=4) { if(BinaryPrimitives.ReadUInt32LittleEndian(stored.Slice(i,4))!=pixels[pixelOffset+i/4]) { return false; } }
			return true;
		}
		if(_storage is MemoryStream memory && memory.TryGetBuffer(out var buffer)) {
			return Matches(buffer.AsSpan(checked((int)offset),checked((int)bytes)),0);
		}
		_storage.Flush(); _comparisonBuffer ??= new byte[65536];
		try {
			_storage.Position=offset; int pixelOffset=0;
			while(bytes>0) {
				int count=(int)Math.Min(bytes,_comparisonBuffer.Length);
				_storage.ReadExactly(_comparisonBuffer.AsSpan(0,count));
				if(!Matches(_comparisonBuffer.AsSpan(0,count),pixelOffset)) { return false; }
				bytes-=count; pixelOffset+=count/4;
			}
			return true;
		} finally { _storage.Position=_storage.Length; }
	}
	public static void WritePixels(BinaryWriter writer,uint[] pixels)
	{
		if(BitConverter.IsLittleEndian) { writer.Write(MemoryMarshal.AsBytes(pixels.AsSpan())); }
		else { foreach(uint pixel in pixels) { writer.Write(pixel); } }
	}
	public override int Read(byte[] buffer,int offset,int count) => throw new NotSupportedException();
	public override long Seek(long offset,SeekOrigin origin) => throw new NotSupportedException();
	public override void SetLength(long value) => throw new NotSupportedException();
	protected override void Dispose(bool disposing)
	{
		if(!_disposed) { if(disposing) { _storage.Dispose(); if(_fileLease && SpillPath!=null) { File.Delete(SpillPath); } } _disposed=true; }
		base.Dispose(disposing);
	}
}

// Owns a sealed packet file until the synchronous native parse/commit returns.
// Share.Read allows native readers while preventing a second writer on Windows.
internal sealed class MetaReplacementPacketFile : IDisposable
{
	private FileStream? _file;
	public string Path { get; }
	internal MetaReplacementPacketFile(FileStream file,string path) { _file=file; Path=path; }
	public void Dispose()
	{
		if(_file==null) { return; }
		_file.Dispose(); _file=null; File.Delete(Path);
	}
}
