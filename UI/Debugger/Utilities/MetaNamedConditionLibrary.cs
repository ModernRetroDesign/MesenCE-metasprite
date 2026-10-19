using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Text.RegularExpressions;

namespace Mesen.Debugger.Utilities;

public interface IMetaMemoryConditionOwner
{
	List<MetaMemoryCondition> MemoryConditions { get; set; }
	string? MemoryConditionExpression { get; set; }
	bool NamedConditionsResolved { get; set; }
}

public static class MetaNamedConditionBindings
{
	public static bool UsesNames(string? expression)=>expression?.Contains('[')==true;
	public static string? SavedExpression(string text)=>UsesNames(text) ? text : null;
	public static string Text(IMetaMemoryConditionOwner owner)=>owner.MemoryConditionExpression ?? MetaMemoryConditionText.Format(owner.MemoryConditions);
	public static List<MetaMemoryCondition> Parse(string text,string packPath,string romHash,string system,IMetaMemoryConditionOwner? owner=null)
	{
		var library=UsesNames(text) ? MetaNamedConditionLibrary.Load(Path.Combine(Path.GetDirectoryName(Path.GetFullPath(packPath))!,MetaNamedConditionLibrary.FileName),romHash,system) : null;
		var parsed=MetaMemoryConditionText.Parse(text,library);
		if(owner!=null) { owner.MemoryConditionExpression=SavedExpression(text); owner.NamedConditionsResolved=true; }
		return parsed;
	}
	public static void ValidateCompiled(IMetaMemoryConditionOwner owner)
	{
		if(owner.MemoryConditionExpression!=null && !owner.NamedConditionsResolved) { throw new InvalidDataException("Resolve the named condition expression against the current game's library before applying replacements."); }
	}
}

public sealed class MetaNamedCondition
{
	public string Name { get; set; } = "";
	public string MemoryDomain { get; set; } = "Cpu";
	public uint Address { get; set; }
	public int? Value { get; set; }
	public int? Mask { get; set; }
	public MetaMemoryComparison? Comparison { get; set; }
	public override string ToString()=>Value.HasValue ? $"[{Name}]  ${Address:X} {MetaMemoryConditionText.Operator(Comparison ?? MetaMemoryComparison.Equal)} ${Value:X2} / ${Mask ?? 255:X2}" : $"[{Name}]  ${Address:X} (address alias)";

	public static void ValidateName(string name)
	{
		if(string.IsNullOrWhiteSpace(name) || name!=name.Trim() || name.Length>64 || name.Any(c=>char.IsControl(c) || c=='[' || c==']')) {
			throw new InvalidDataException("Condition names require 1-64 characters without brackets, line breaks or surrounding spaces.");
		}
	}
	internal void Validate(uint maximumAddress)
	{
		ValidateName(Name);
		if(!string.Equals(MemoryDomain,"Cpu",StringComparison.Ordinal)) { throw new InvalidDataException($"[{Name}] uses an unsupported memory domain. Named replacement conditions currently require the owning system's CPU address space."); }
		if(Address>maximumAddress) { throw new InvalidDataException($"[{Name}] is outside this system's CPU address space."); }
		if(Value is <0 or >255 || Mask is <0 or >255 || (Comparison.HasValue && !Enum.IsDefined(Comparison.Value))) {
			throw new InvalidDataException($"[{Name}] has an invalid byte value, mask or comparison.");
		}
	}
}

// Shared by all replacement editors. Expressions compile to the existing numeric
// conditions; the emulator still observes only their configured CPU addresses.
public sealed class MetaNamedConditionLibrary
{
	public const string FileName="conditions.json";
	private const int MaximumFileBytes=4*1024*1024;
	public int Version { get; set; } = 1;
	public string RomHash { get; set; } = "";
	public string System { get; set; } = "";
	public List<MetaNamedCondition> Conditions { get; set; } = new();

	public void Validate()
	{
		uint maximumAddress=System switch {
			"Nes" or "Gameboy" or "Pce" or "Sms" => 0xFFFF,
			"Snes" => 0xFFFFFF, "Ws" => 0xFFFFF, "Gba" => uint.MaxValue,
			_ => throw new InvalidDataException("Unsupported named-condition CPU system.")
		};
		if(Version!=1 || string.IsNullOrWhiteSpace(RomHash) || Conditions==null || Conditions.Count>4096 || Conditions.Any(c=>c==null)) { throw new InvalidDataException("Invalid named-condition library version, ROM identity or entries."); }
		var names=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
		foreach(var condition in Conditions) {
			condition.Validate(maximumAddress);
			if(!names.Add(condition.Name)) { throw new InvalidDataException($"Duplicate condition name [{condition.Name}]. Names must be unique; addresses may be shared."); }
		}
	}

	public MetaMemoryCondition ResolveExpression(string expression)
	{
		Validate();
		if(expression.Length>4096) { throw new InvalidDataException("A named condition expression is too long."); }
		const string hex=@"(?:\$?[0-9A-Fa-f]+|0[xX][0-9A-Fa-f]+)";
		var match=Regex.Match(expression,@"^\s*\[(?<name>[^\[\]\r\n]+)\]\s*(?<op>==|!=|>=|<=|>|<|=)?\s*(?<value>"+hex+@")?\s*(?:/\s*(?<mask>"+hex+@"))?\s*$",RegexOptions.CultureInvariant|RegexOptions.NonBacktracking);
		if(!match.Success) { throw new InvalidDataException("Use [Name], [Name] OP VALUE / MASK, or a numeric condition."); }
		string name=match.Groups["name"].Value;
		MetaNamedCondition.ValidateName(name);
		var preset=Conditions.SingleOrDefault(c=>string.Equals(c.Name,name,StringComparison.OrdinalIgnoreCase))
			?? throw new InvalidDataException($"Unknown named condition [{name}].");
		int? value=match.Groups["value"].Success ? Byte(match.Groups["value"].Value) : preset.Value;
		if(!value.HasValue) { throw new InvalidDataException($"[{preset.Name}] is an address alias. Supply a comparison value, for example [{preset.Name}] == $04 / $FF."); }
		int mask=match.Groups["mask"].Success ? Byte(match.Groups["mask"].Value) : preset.Mask ?? 255;
		MetaMemoryComparison comparison=match.Groups["op"].Success ? match.Groups["op"].Value switch {
			"!=" => MetaMemoryComparison.NotEqual, ">" => MetaMemoryComparison.Greater, "<" => MetaMemoryComparison.Less,
			">=" => MetaMemoryComparison.GreaterOrEqual, "<=" => MetaMemoryComparison.LessOrEqual, _ => MetaMemoryComparison.Equal
		} : preset.Comparison ?? MetaMemoryComparison.Equal;
		return new() {Address=preset.Address,Value=value.Value,Mask=mask,Comparison=comparison};
	}
	private static int Byte(string value)
	{
		if(value.StartsWith('$')) { value=value[1..]; }
		else if(value.StartsWith("0x",StringComparison.OrdinalIgnoreCase)) { value=value[2..]; }
		if(!uint.TryParse(value,global::System.Globalization.NumberStyles.AllowHexSpecifier,global::System.Globalization.CultureInfo.InvariantCulture,out uint number) || number>255) {
			throw new InvalidDataException("Named condition values and masks must be hexadecimal bytes from 00 through FF.");
		}
		return (int)number;
	}

	public static MetaNamedConditionLibrary Load(string path,string romHash,string system)
		=>Load(path,romHash,system,out _);
	public static MetaNamedConditionLibrary Load(string path,string romHash,string system,out string? contents)
	{
		contents=null;
		if(!File.Exists(path)) { var empty=new MetaNamedConditionLibrary {RomHash=romHash,System=system}; empty.Validate(); return empty; }
		if(new FileInfo(path).Length>MaximumFileBytes) { throw new InvalidDataException("Named-condition library exceeds 4 MiB."); }
		contents=File.ReadAllText(path);
		var library=JsonSerializer.Deserialize(contents,MetaNamedConditionSerializerContext.Default.MetaNamedConditionLibrary)
			?? throw new InvalidDataException("Empty named-condition library.");
		if(library.RomHash!=romHash || library.System!=system) { throw new InvalidDataException("The named-condition library belongs to a different ROM or CPU system."); }
		library.Validate(); return library;
	}
	public string ToJson()
	{
		Validate(); string contents=JsonSerializer.Serialize(this,MetaNamedConditionSerializerContext.Default.MetaNamedConditionLibrary);
		if(Encoding.UTF8.GetByteCount(contents)>MaximumFileBytes) { throw new InvalidDataException("Named-condition library exceeds 4 MiB."); }
		return contents;
	}

	// A caller keeps the exact contents loaded by its editor. A second editor or
	// process cannot overwrite those changes silently. Null means a new library.
	public void Save(string path,string? expectedContents=null)
	{
		string contents=ToJson();
		Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(path))!);
		using var editLock=new FileStream(path+".lock",FileMode.OpenOrCreate,FileAccess.ReadWrite,FileShare.None);
		string? current=File.Exists(path) ? File.ReadAllText(path) : null;
		if(!string.Equals(current,expectedContents,StringComparison.Ordinal)) { throw new InvalidDataException("Named conditions changed in another editor. Reload before saving."); }
		string temporary=path+".tmp."+Guid.NewGuid().ToString("N");
		try { File.WriteAllText(temporary,contents); File.Move(temporary,path,true); }
		finally { if(File.Exists(temporary)) { File.Delete(temporary); } }
	}
}

[JsonSerializable(typeof(MetaNamedConditionLibrary))]
[JsonSourceGenerationOptions(WriteIndented=true)]
internal partial class MetaNamedConditionSerializerContext : JsonSerializerContext { }
