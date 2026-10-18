using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.Cryptography;

namespace Mesen.Debugger.Utilities;

// Hashes index a bounded metadata cache; full packet-byte comparisons decide
// sharing. No decoded arrays are retained here, including after cache eviction.
internal sealed class MetaPngPacketResources
{
	private sealed record Entry(string Hash,uint Id,long Offset,int PixelCount);
	private readonly MetaReplacementPacketStream _stream;
	private readonly int _maximumEntries;
	private readonly Func<uint[],string> _fingerprint;
	private readonly Dictionary<string,List<LinkedListNode<Entry>>> _buckets=new();
	private readonly LinkedList<Entry> _recent=new();
	private uint _nextId=1;
	internal int Count => _recent.Count;
	internal MetaPngPacketResources(MetaReplacementPacketStream stream,int maximumEntries=4096,Func<uint[],string>? fingerprint=null)
	{
		if(maximumEntries<1) { throw new ArgumentOutOfRangeException(nameof(maximumEntries)); }
		_stream=stream; _maximumEntries=maximumEntries;
		_fingerprint=fingerprint ?? (pixels => Convert.ToHexString(SHA256.HashData(MemoryMarshal.AsBytes(pixels.AsSpan()))));
	}
	internal void Write(BinaryWriter writer,uint[] pixels)
	{
		if(!ReferenceEquals(writer.BaseStream,_stream)) { throw new InvalidOperationException("Artwork resources require their owning packet stream."); }
		string hash=_fingerprint(pixels);
		if(_buckets.TryGetValue(hash,out var bucket)) {
			foreach(var node in bucket) {
				if(node.Value.PixelCount==pixels.Length && _stream.MatchesPixels(node.Value.Offset,pixels)) {
					_recent.Remove(node); _recent.AddFirst(node); writer.Write(node.Value.Id); return;
				}
			}
		}
		writer.Write(0u); long offset=_stream.Length;
		MetaReplacementPacketStream.WritePixels(writer,pixels);
		uint id=_nextId; _nextId=checked(_nextId+1);
		if(_recent.Count==_maximumEntries) {
			var last=_recent.Last!; var oldBucket=_buckets[last.Value.Hash]; oldBucket.Remove(last);
			if(oldBucket.Count==0) { _buckets.Remove(last.Value.Hash); } _recent.RemoveLast();
		}
		if(!_buckets.TryGetValue(hash,out bucket)) { bucket=new(); _buckets.Add(hash,bucket); }
		bucket.Add(_recent.AddFirst(new Entry(hash,id,offset,pixels.Length)));
	}
}
