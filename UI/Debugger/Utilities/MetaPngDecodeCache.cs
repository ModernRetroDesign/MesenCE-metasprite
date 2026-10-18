using System;
using System.Collections.Generic;
using System.IO;

namespace Mesen.Debugger.Utilities;

// Scoped to one Apply: file edits are seen on the next application. This bounds
// retained decoded arrays, not packet buffers, Skia scratch space or process RSS.
internal sealed class MetaPngDecodeCache
{
	private sealed record Entry(string Path,int Width,int Height,uint[] Pixels);
	private readonly long _maximumBytes;
	private readonly int _maximumEntries;
	private readonly Func<string,Action<long>,(int Width,int Height,uint[] Pixels)> _decode;
	private readonly Dictionary<string,LinkedListNode<Entry>> _entries;
	private readonly LinkedList<Entry> _recent = new();
	public long RetainedBytes { get; private set; }
	public int Count => _entries.Count;
	public MetaPngDecodeCache(long maximumBytes,int maximumEntries,
		Func<string,Action<long>,(int Width,int Height,uint[] Pixels)> decode)
	{
		if(maximumBytes<0) { throw new ArgumentOutOfRangeException(nameof(maximumBytes)); }
		if(maximumEntries<1) { throw new ArgumentOutOfRangeException(nameof(maximumEntries)); }
		_maximumBytes=maximumBytes; _maximumEntries=maximumEntries; _decode=decode;
		_entries=new(OperatingSystem.IsWindows() ? StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal);
	}
	public (int Width,int Height,uint[] Pixels) Get(string path)
	{
		string fullPath=Path.GetFullPath(path);
		if(_entries.TryGetValue(fullPath,out var found)) {
			_recent.Remove(found); _recent.AddFirst(found);
			return (found.Value.Width,found.Value.Height,found.Value.Pixels);
		}
		long reserved=-1;
		var image=_decode(fullPath,bytes => {
			if(reserved!=-1 || bytes<4 || bytes%4!=0) { throw new InvalidDataException("Invalid decoded PNG reservation."); }
			reserved=bytes;
			// The codec calls this after validating dimensions, before allocating
			// bitmap/pixel buffers. Oversized single frames bypass retention.
			while(_recent.Last is {} last && (bytes>_maximumBytes-RetainedBytes || Count>=_maximumEntries)) {
				RetainedBytes-=last.Value.Pixels.LongLength*4; _entries.Remove(last.Value.Path); _recent.RemoveLast();
			}
		});
		if(image.Width<1 || image.Height<1 || (long)image.Width*image.Height!=image.Pixels.LongLength
			|| reserved!=image.Pixels.LongLength*4) { throw new InvalidDataException("Decoded PNG did not match its reserved canvas."); }
		if(reserved<=_maximumBytes) {
			var node=_recent.AddFirst(new Entry(fullPath,image.Width,image.Height,image.Pixels));
			_entries.Add(fullPath,node); RetainedBytes+=reserved;
		}
		return image;
	}
}
