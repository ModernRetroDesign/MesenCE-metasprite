using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using Mesen.Debugger.Utilities;

internal static class MatcherBenchmark
{
	public static void Run()
	{
		uint[] pixels = Enumerable.Repeat(0xFFFFFFFFu, 64).ToArray();
		var group = new MetaSpriteGroup { OriginX = 4, OriginY = 4 };
		for(int y = 0; y < 2; y++) {
			for(int x = 0; x < 2; x++) {
				group.Members.Add(new() { X = x * 8, Y = y * 8, Width = 8, Height = 8, Pixels = pixels });
			}
		}
		var candidates = new List<MetaSpriteMember>();
		for(int copy = 0; copy < 4; copy++) {
			for(int y = 0; y < 8; y++) {
				for(int x = 0; x < 16; x++) {
					candidates.Add(new() { X = x * 8 + (copy % 2) * 256, Y = y * 8 + (copy / 2) * 256,
						Width = 8, Height = 8, Pixels = pixels, RuntimeSpriteIndex = y * 16 + x });
				}
			}
		}
		var expected = PreviousMatcher(group, candidates);
		var actual = MetaSpriteMatcher.FindMatches(group, candidates);
		if(expected.Count != 420 || actual.Count != expected.Count
			|| actual.Where((match, i) => match.X != expected[i].X || match.Y != expected[i].Y || !match.Members.SequenceEqual(expected[i].Members)).Any()) {
			throw new Exception("Dense wrapped snapshot matching changed.");
		}
		var unwrapped = candidates.Take(128).ToList();
		if(MetaSpriteMatcher.FindMatches(group, unwrapped).Count != 105) { throw new Exception("Unwrapped snapshot matching changed."); }
		Measure("previous matcher, 128 candidates", () => PreviousMatcher(group, unwrapped));
		Measure("current matcher, 128 candidates", () => MetaSpriteMatcher.FindMatches(group, unwrapped));
		Measure("previous matcher, 512 candidates with wrapped copies", () => PreviousMatcher(group, candidates));
		Measure("current matcher, 512 candidates with wrapped copies", () => MetaSpriteMatcher.FindMatches(group, candidates));
	}

	private static void Measure(string name, Func<List<(int X, int Y, int[] Members)>> match)
	{
		for(int i = 0; i < 3; i++) { match(); }
		long allocation = GC.GetAllocatedBytesForCurrentThread();
		var watch = Stopwatch.StartNew();
		int count = 0;
		for(int i = 0; i < 20; i++) { count += match().Count; }
		watch.Stop();
		Console.WriteLine($"MATCHER BENCHMARK: {name}; mean={watch.Elapsed.TotalMilliseconds / 20:F3} ms; allocated={(GC.GetAllocatedBytesForCurrentThread() - allocation) / 20} bytes/call; matches={count / 20}");
	}

	// Retained only as a benchmark baseline for the algorithm before position rejection.
	private static List<(int X, int Y, int[] Members)> PreviousMatcher(MetaSpriteGroup group, IReadOnlyList<MetaSpriteMember> candidates)
	{
		var matches = new List<(int X, int Y, int[] Members)>();
		var anchor = group.Members[0];
		bool Same(MetaSpriteMember a, MetaSpriteMember b) => a.Width == b.Width && a.Height == b.Height && a.Pixels.AsSpan().SequenceEqual(b.Pixels);
		for(int i = 0; i < candidates.Count; i++) {
			if(!Same(anchor, candidates[i])) { continue; }
			int left = candidates[i].X - anchor.X, top = candidates[i].Y - anchor.Y;
			var used = new List<int> { i };
			foreach(var member in group.Members.Skip(1)) {
				int found = -1;
				for(int j = 0; j < candidates.Count; j++) {
					bool sameHardware = candidates[j].RuntimeSpriteIndex >= 0 && used.Any(index => candidates[index].RuntimeSpriteIndex == candidates[j].RuntimeSpriteIndex);
					if(!used.Contains(j) && !sameHardware && candidates[j].X == left + member.X && candidates[j].Y == top + member.Y && Same(member, candidates[j])) { found = j; break; }
				}
				if(found < 0) { break; }
				used.Add(found);
			}
			if(used.Count == group.Members.Count) { matches.Add((left + group.OriginX, top + group.OriginY, used.ToArray())); }
		}
		return matches;
	}
}
