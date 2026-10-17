using System;
using System.Collections.Generic;
using System.IO;

namespace Mesen.Debugger.Utilities;

public enum MetaPngPlayback { Loop, PingPong, PlayOnce }

public sealed class MetaPngAnimationFrame
{
	public string Path { get; set; } = "";
	public int Duration { get; set; } = 1; // Emulated frames, never wall-clock time.
	public int? OriginX { get; set; }
	public int? OriginY { get; set; }
}

public sealed class MetaPngAnimation
{
	public bool Enabled { get; set; }
	public MetaPngPlayback Playback { get; set; }
	public bool RestartOnActivation { get; set; }
	public List<MetaPngAnimationFrame> Frames { get; set; } = new();

	public void Validate()
	{
		if(!Enum.IsDefined(typeof(MetaPngPlayback), Playback) || Frames == null || Frames.Count > 256 || (Enabled && Frames.Count == 0)) {
			throw new InvalidDataException("PNG animation requires a valid playback mode and 1–256 frames when enabled.");
		}
		foreach(var frame in Frames) {
			if(frame == null || string.IsNullOrWhiteSpace(frame.Path) || frame.Duration < 1 || frame.Duration > 3600000
				|| frame.OriginX is < -4096 or > 4096 || frame.OriginY is < -4096 or > 4096) {
				throw new InvalidDataException("Animation frames require a PNG path, positive duration and valid optional origins.");
			}
			string normalized = frame.Path.Replace('\\', '/');
			if(normalized.StartsWith('/') || normalized.Contains(':') || Array.Exists(normalized.Split('/'), part => part == "..")
				|| !normalized.EndsWith(".png", StringComparison.OrdinalIgnoreCase)) {
				throw new InvalidDataException("Animation PNG paths must stay relative to the pack folder.");
			}
		}
	}
}

// Immutable timing data shared by editor previews. Runtime integration must use
// the same emulated-frame boundaries; decoded pixels are not copied on a tick.
public sealed class MetaPngTimeline
{
	private readonly ulong[] _ends;
	private readonly int[] _indices;
	private readonly MetaPngPlayback _playback;
	public ulong Duration => _ends.Length == 0 ? 0 : _ends[^1];

	public MetaPngTimeline(MetaPngAnimation animation)
	{
		animation.Validate(); _playback = animation.Playback;
		int count = animation.Enabled ? animation.Frames.Count : 0;
		int visits = count + (_playback == MetaPngPlayback.PingPong && count > 2 ? count - 2 : 0);
		_ends = new ulong[visits]; _indices = new int[visits]; ulong end = 0;
		for(int i = 0; i < visits; i++) {
			int index = i < count ? i : count * 2 - 2 - i;
			end = checked(end + (ulong)animation.Frames[index].Duration);
			_ends[i] = end; _indices[i] = index;
		}
	}

	public int SelectFrame(ulong elapsedEmulatedFrames)
	{
		if(_ends.Length == 0) { return -1; }
		if(_playback == MetaPngPlayback.PlayOnce && elapsedEmulatedFrames >= Duration) { return _indices[^1]; }
		ulong tick = elapsedEmulatedFrames % Duration;
		int low = 0, high = _ends.Length - 1;
		while(low < high) {
			int middle = low + (high - low) / 2;
			if(tick < _ends[middle]) { high = middle; } else { low = middle + 1; }
		}
		return _indices[low];
	}
}
