using CommunityToolkit.Mvvm.ComponentModel;
using Mesen.Debugger.Utilities;
using System;
using System.Collections.ObjectModel;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using System.Text.Json;
using Mesen.Config;

namespace Mesen.Debugger.ViewModels;

public partial class NamedConditionEditorViewModel : ObservableObject
{
	private readonly string _packPath,_path,_hash,_system;
	private readonly uint _capturedAddress;
	private readonly byte _capturedValue;
	private readonly Func<bool> _sameGame;
	private readonly Func<bool>? _activate;
	private MetaNamedConditionLibrary _library;
	private string? _snapshot;
	private bool _loading;
	public ObservableCollection<MetaNamedCondition> Entries { get; }=new();
	public string[] Operators { get; }={"==","!=",">","<",">=","<="};
	public string SelectionInfo { get; }
	[ObservableProperty] public partial MetaNamedCondition? SelectedEntry { get; set; }
	[ObservableProperty] public partial string ConditionName { get; set; }="";
	[ObservableProperty] public partial string AddressHex { get; set; }="";
	[ObservableProperty] public partial string ValueHex { get; set; }="";
	[ObservableProperty] public partial string MaskHex { get; set; }="FF";
	[ObservableProperty] public partial int OperatorIndex { get; set; }
	[ObservableProperty] public partial bool AddressAlias { get; set; }
	[ObservableProperty] public partial string ResolvedPreview { get; set; }="";
	[ObservableProperty] public partial string Status { get; set; }="";
	public NamedConditionEditorViewModel(string packPath,string hash,string system,uint address,byte value,string selectionInfo,Func<bool> sameGame,Func<bool>? activate=null)
	{
		_packPath=packPath; _hash=hash; _system=system; _capturedAddress=address; _capturedValue=value; _sameGame=sameGame; _activate=activate;
		_path=Path.Combine(Path.GetDirectoryName(Path.GetFullPath(packPath))!,MetaNamedConditionLibrary.FileName);
		SelectionInfo=selectionInfo+"\nSaved conditions watch the mapped CPU address shown below.";
		_library=MetaNamedConditionLibrary.Load(_path,hash,system,out _snapshot);
		RefreshEntries(); NewPreset();
	}
	private void CheckGame() { if(!_sameGame()) { throw new InvalidDataException("The game changed. Reopen named conditions for the current ROM."); } }
	public void NewPreset()
	{
		_loading=true; SelectedEntry=null;
		int number=1; while(_library.Conditions.Any(c=>string.Equals(c.Name,"Condition"+number,StringComparison.OrdinalIgnoreCase))) { number++; }
		ConditionName="Condition"+number; AddressHex=_capturedAddress.ToString("X"); ValueHex=_capturedValue.ToString("X2"); MaskHex="FF"; OperatorIndex=0; AddressAlias=false;
		_loading=false; RefreshPreview();
	}
	partial void OnSelectedEntryChanged(MetaNamedCondition? value)
	{
		if(_loading || value==null) { return; }
		_loading=true; ConditionName=value.Name; AddressHex=value.Address.ToString("X"); ValueHex=value.Value?.ToString("X2") ?? ""; MaskHex=value.Mask?.ToString("X2") ?? "";
		OperatorIndex=(int)(value.Comparison ?? MetaMemoryComparison.Equal); AddressAlias=!value.Value.HasValue;
		_loading=false; RefreshPreview();
	}
	partial void OnConditionNameChanged(string value)=>RefreshPreview();
	partial void OnAddressHexChanged(string value)=>RefreshPreview();
	partial void OnValueHexChanged(string value)=>RefreshPreview();
	partial void OnMaskHexChanged(string value)=>RefreshPreview();
	partial void OnOperatorIndexChanged(int value)=>RefreshPreview();
	partial void OnAddressAliasChanged(bool value)=>RefreshPreview();
	private static uint Hex(string text)
	{
		text=text.Trim(); if(text.StartsWith('$')) { text=text[1..]; } else if(text.StartsWith("0x",StringComparison.OrdinalIgnoreCase)) { text=text[2..]; }
		if(!uint.TryParse(text,NumberStyles.AllowHexSpecifier,CultureInfo.InvariantCulture,out uint value)) { throw new InvalidDataException("Use hexadecimal addresses, values and masks."); }
		return value;
	}
	private MetaNamedCondition Draft()
	{
		uint? value=AddressAlias || string.IsNullOrWhiteSpace(ValueHex) ? null : Hex(ValueHex);
		uint? mask=string.IsNullOrWhiteSpace(MaskHex) ? null : Hex(MaskHex);
		if(value>255 || mask>255) { throw new InvalidDataException("Values and masks must be byte values from 00 through FF."); }
		return new() {Name=ConditionName,Address=Hex(AddressHex),Value=(int?)value,Mask=(int?)mask,Comparison=(MetaMemoryComparison)OperatorIndex};
	}
	private MetaNamedConditionLibrary Candidate(MetaNamedCondition? replacement,bool remove=false)
	{
		var candidate=new MetaNamedConditionLibrary {RomHash=_hash,System=_system,Conditions=_library.Conditions.ToList()};
		if(SelectedEntry!=null) { candidate.Conditions.Remove(SelectedEntry); }
		if(!remove && replacement!=null) { candidate.Conditions.Add(replacement); }
		candidate.Validate(); return candidate;
	}
	private MetaSpritePack SavedPack()=>MetaSpritePack.Load(_packPath,_hash,_system,resolveNamedConditions:false);
	private string AudioPath=>Path.Combine(ConfigManager.HdPackFolder,"Audio",_system=="Pce" ? "PcEngine" : _system,_hash,"music.json");
	private MusicReplacementPack? SavedAudio()
	{
		if(!File.Exists(AudioPath)) { return null; }
		var pack=JsonSerializer.Deserialize(File.ReadAllText(AudioPath),MusicReplacementJson.Default.MusicReplacementPack) ?? throw new InvalidDataException("Empty audio pack.");
		if(pack.RomHash!=_hash || pack.System!=(_system=="Pce" ? "PcEngine" : _system) || pack.Rules==null || pack.Rules.Any(r=>r==null || r.Trigger==null || r.Conditions==null)) { throw new InvalidDataException("Invalid audio pack identity or rules."); }
		return pack;
	}
	private bool IsReferenced(string name)
	{
		var expressions=SavedPack().ConditionOwners().Select(o=>o.MemoryConditionExpression ?? "");
		var audio=SavedAudio(); if(audio!=null) { expressions=expressions.Concat(audio.NamedExpressions()); }
		return expressions.Any(text=>Regex.Matches(text,@"\[([^\[\]\r\n]+)\]").Any(m=>string.Equals(m.Groups[1].Value,name,StringComparison.OrdinalIgnoreCase)));
	}
	private void ValidateReferences(MetaNamedConditionLibrary candidate)
	{
		foreach(var owner in SavedPack().ConditionOwners()) {
			if(owner.MemoryConditionExpression!=null) {
				var conditions=MetaMemoryConditionText.Parse(owner.MemoryConditionExpression,candidate);
				if(owner is MetaBoundaryCondition && conditions.Count>8) { throw new InvalidDataException("Use at most eight boundary conditions per edge."); }
			}
		}
		var audio=SavedAudio();
		if(audio!=null) {
			audio.Validate(candidate);
			foreach(var rule in audio.Rules) { audio.ParseCondition(rule.Trigger,rule.TriggerSource,candidate); audio.ParseCondition(rule.Conditions,rule.ContextSource,candidate); }
		}
	}
	private void RefreshPreview()
	{
		if(_loading || _library==null) { return; }
		try { var draft=Draft(); Candidate(draft); ResolvedPreview=draft.ToString(); }
		catch(Exception ex) { ResolvedPreview=ex.Message; }
	}
	private void RefreshEntries() { Entries.Clear(); foreach(var entry in _library.Conditions.OrderBy(c=>c.Name,StringComparer.OrdinalIgnoreCase)) { Entries.Add(entry); } }
	private void Commit(MetaNamedConditionLibrary candidate,MetaNamedCondition? selected)
	{
		ValidateReferences(candidate); string snapshot=candidate.ToJson(); candidate.Save(_path,_snapshot);
		_library=candidate; _snapshot=snapshot;
		_loading=true; RefreshEntries(); SelectedEntry=selected; _loading=false;
		Status=selected==null ? "Named condition removed." : $"Saved [{selected.Name}]. Use it in replacement condition boxes.";
		try {
			if(_activate!=null) {
				if(File.Exists(_packPath)) { Status+=_activate() ? " Gameplay conditions refreshed." : " Gameplay conditions could not be refreshed."; }
				if(SavedAudio() is {} audio && audio.NamedExpressions().Any(MetaNamedConditionBindings.UsesNames)) { Status+=MusicReplacementRuntime.Apply(audio,AudioPath) ? " Audio conditions refreshed." : " Audio conditions could not be refreshed."; }
			}
		}
		catch(Exception ex) { Status+=" Could not refresh gameplay: "+ex.Message; }
	}
	public bool Save()
	{
		try {
			CheckGame(); var entry=Draft();
			if(SelectedEntry!=null && !string.Equals(SelectedEntry.Name,entry.Name,StringComparison.OrdinalIgnoreCase) && IsReferenced(SelectedEntry.Name)) { throw new InvalidDataException($"[{SelectedEntry.Name}] is referenced by saved replacements. Update those expressions before renaming it, or create a new preset."); }
			Commit(Candidate(entry),entry); return true;
		} catch(Exception ex) { Status="Could not save named condition: "+ex.Message; return false; }
	}
	public bool Remove()
	{
		try {
			CheckGame(); if(SelectedEntry==null) { throw new InvalidDataException("Select a saved named condition first."); }
			if(IsReferenced(SelectedEntry.Name)) { throw new InvalidDataException($"[{SelectedEntry.Name}] is referenced by saved replacements. Update those expressions before removing it."); }
			Commit(Candidate(null,remove:true),null); NewPreset(); return true;
		} catch(Exception ex) { Status="Could not remove named condition: "+ex.Message; return false; }
	}
	public void Reload()
	{
		try { CheckGame(); _library=MetaNamedConditionLibrary.Load(_path,_hash,_system,out _snapshot); _loading=true; RefreshEntries(); _loading=false; NewPreset(); Status="Named conditions reloaded."; }
		catch(Exception ex) { Status="Could not reload named conditions: "+ex.Message; }
	}
}
