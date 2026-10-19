using Avalonia;
using Avalonia.Controls;
using Avalonia.Data;
using Avalonia.Media;
using Mesen.Debugger.Utilities;
using Mesen.Interop;
using System;
using System.IO;
using System.Linq;

namespace Mesen.Debugger.Controls;

// Editor-only resolution. No RAM is read and no gameplay definitions are changed.
public sealed class NamedConditionPicker : StackPanel
{
	public static readonly StyledProperty<string> ExpressionProperty=AvaloniaProperty.Register<NamedConditionPicker,string>(nameof(Expression),"",defaultBindingMode:BindingMode.TwoWay);
	public static readonly StyledProperty<int> MemorySourceProperty=AvaloniaProperty.Register<NamedConditionPicker,int>(nameof(MemorySource));
	public static readonly StyledProperty<int> MaximumConditionsProperty=AvaloniaProperty.Register<NamedConditionPicker,int>(nameof(MaximumConditions),64);
	public string Expression { get=>GetValue(ExpressionProperty); set=>SetValue(ExpressionProperty,value); }
	public int MemorySource { get=>GetValue(MemorySourceProperty); set=>SetValue(MemorySourceProperty,value); }
	public int MaximumConditions { get=>GetValue(MaximumConditionsProperty); set=>SetValue(MaximumConditionsProperty,value); }
	private readonly ComboBox _choices=new() {Width=180,PlaceholderText="Saved conditions"};
	private readonly Button _insert=new() {Content="Add condition"};
	private readonly TextBlock _preview=new() {TextWrapping=TextWrapping.Wrap,FontSize=12};
	private MetaNamedConditionLibrary? _library;
	private string? _hash;
	private CpuType _cpu;
	public string ResolvedPreview=>_preview.Text ?? "";
	public NamedConditionPicker()
	{
		Spacing=4;
		var row=new WrapPanel(); row.Children.Add(_choices); row.Children.Add(_insert);
		var reload=new Button {Content="Refresh names"}; row.Children.Add(reload);
		Children.Add(row); Children.Add(_preview);
		_insert.Click+=(_,_)=>InsertSelectedCondition(); reload.Click+=(_,_)=>ReloadLibrary();
		_choices.DropDownOpened+=(_,_)=>ReloadLibrary();
	}
	protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs e)
	{
		base.OnAttachedToVisualTree(e); if(!Design.IsDesignMode) { ReloadLibrary(); }
	}
	protected override void OnPropertyChanged(AvaloniaPropertyChangedEventArgs change)
	{
		base.OnPropertyChanged(change);
		if(change.Property==ExpressionProperty || change.Property==MemorySourceProperty || change.Property==MaximumConditionsProperty) { UpdatePreview(); }
	}
	private void CheckGame()
	{
		var info=EmuApi.GetRomInfo(); var cpu=info.ConsoleType.GetMainCpuType();
		if(!info.CpuTypes.Contains(cpu) || cpu is not (CpuType.Nes or CpuType.Gameboy or CpuType.Gba or CpuType.Pce or CpuType.Sms or CpuType.Snes or CpuType.Ws)) { throw new InvalidDataException("Load a supported game to select named conditions."); }
		string hash=EmuApi.GetRomHash(HashType.Sha1);
		if(_hash!=null && (_hash!=hash || _cpu!=cpu)) { throw new InvalidDataException("The game changed. Reopen this replacement editor."); }
		_hash=hash; _cpu=cpu;
	}
	public void ReloadLibrary()
	{
		try {
			CheckGame(); string? selection=(_choices.SelectedItem as MetaNamedCondition)?.Name;
			string path=Path.Combine(Path.GetDirectoryName(MetaSpriteRuntimeLoader.GetPackPath(_hash!,_cpu))!,MetaNamedConditionLibrary.FileName);
			_library=MetaNamedConditionLibrary.Load(path,_hash!,_cpu.ToString());
			var entries=_library.Conditions.OrderBy(c=>c.Name,StringComparer.OrdinalIgnoreCase).ToArray();
			_choices.ItemsSource=entries;
			_choices.SelectedItem=entries.FirstOrDefault(c=>string.Equals(c.Name,selection,StringComparison.OrdinalIgnoreCase)) ?? entries.FirstOrDefault();
			UpdatePreview();
		} catch(Exception ex) { _library=null; _choices.ItemsSource=null; _insert.IsEnabled=false; _preview.Text=ex.Message; }
	}
	public void InsertSelectedCondition()
	{
		try {
			CheckGame(); if(MemorySource!=0) { throw new InvalidDataException("Named conditions use main CPU memory. Select the main CPU source first."); }
			string? selectedName=(_choices.SelectedItem as MetaNamedCondition)?.Name;
			// Re-read before insertion so a removed or changed preset is not copied from a stale list.
			ReloadLibrary();
			if(_library==null || _choices.SelectedItem is not MetaNamedCondition entry) { return; }
			if(selectedName!=null && !string.Equals(entry.Name,selectedName,StringComparison.OrdinalIgnoreCase)) { throw new InvalidDataException("The selected name changed or was removed. Choose a condition again."); }
			string source=Expression ?? "";
			if(source.Split('\n').Count(line=>!string.IsNullOrWhiteSpace(line))>=MaximumConditions) { throw new InvalidDataException($"Use at most {MaximumConditions} condition(s) in this field."); }
			string reference=$"[{entry.Name}]"+(entry.Value.HasValue ? "" : " == ");
			SetCurrentValue(ExpressionProperty,source+(string.IsNullOrWhiteSpace(source) || source.EndsWith('\n') ? "" : Environment.NewLine)+reference);
			UpdatePreview();
		} catch(Exception ex) { _preview.Text=ex.Message; }
	}
	private void UpdatePreview()
	{
		if(_preview==null) { return; }
		_choices.IsEnabled=MemorySource==0; _insert.IsEnabled=MemorySource==0 && _library?.Conditions.Count>0;
		try {
			string text=Expression ?? "";
			if(MemorySource!=0 && MetaNamedConditionBindings.UsesNames(text)) { throw new InvalidDataException("Named conditions use main CPU memory; SPC/I/O fields require numeric addresses."); }
			var conditions=MetaMemoryConditionText.Parse(text,_library);
			if(conditions.Count>MaximumConditions) { throw new InvalidDataException($"Use at most {MaximumConditions} condition(s) in this field."); }
			_preview.Text=conditions.Count>0 ? "Resolved: "+MetaMemoryConditionText.Format(conditions) : _library?.Conditions.Count==0 ? "No saved conditions. Use Manage conditions to create one." : "No conditions entered.";
		} catch(Exception ex) { _preview.Text="Condition needs attention: "+ex.Message; }
	}
}
