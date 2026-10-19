using Avalonia.Interactivity;
using Avalonia.Markup.Xaml;
using Mesen.Debugger.ViewModels;
using Mesen.Utilities;
using System;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using Avalonia.Controls;
using Mesen.Interop;
using Mesen.Debugger.Utilities;

namespace Mesen.Debugger.Windows;

public class NamedConditionWindow : MesenWindow
{
	public static bool CanOpenForCurrentGame()
	{
		var info=EmuApi.GetRomInfo();
		return info.CpuTypes.Any(cpu=>cpu is CpuType.Nes or CpuType.Gameboy or CpuType.Gba or CpuType.Pce or CpuType.Sms or CpuType.Snes or CpuType.Ws);
	}
	public static async Task OpenForCurrentGame(Window owner)
	{
		try {
			var info=EmuApi.GetRomInfo();
			CpuType cpu=info.CpuTypes.First(cpu=>cpu is CpuType.Nes or CpuType.Gameboy or CpuType.Gba or CpuType.Pce or CpuType.Sms or CpuType.Snes or CpuType.Ws);
			string hash=EmuApi.GetRomHash(HashType.Sha1),path=MetaSpriteRuntimeLoader.GetPackPath(hash,cpu);
			var model=new NamedConditionEditorViewModel(path,hash,cpu.ToString(),0,0,"Named conditions for the current game.",
				()=>EmuApi.GetRomInfo().CpuTypes.Contains(cpu) && EmuApi.GetRomHash(HashType.Sha1)==hash,
				()=>MetaSpriteRuntimeLoader.Apply(MetaSpritePack.Load(path,hash,cpu.ToString()),path));
			if(model.Entries.Count>0) { model.SelectedEntry=model.Entries[0]; }
			else { model.AddressHex=""; }
			await new NamedConditionWindow(model).ShowDialog(owner);
		} catch(Exception ex) { await MesenMsgBox.ShowException(ex); }
	}
	private readonly NamedConditionEditorViewModel _model;
	[Obsolete("For designer only")]
	public NamedConditionWindow() : this(new NamedConditionEditorViewModel(Path.Combine(Path.GetTempPath(),"named-condition-designer","groups.json"),"designer","Nes",0,0,"CPU address $00",()=>false)) { }
	public NamedConditionWindow(NamedConditionEditorViewModel model) { AvaloniaXamlLoader.Load(this); _model=model; DataContext=model; }
	private void NewClicked(object? sender,RoutedEventArgs e)=>_model.NewPreset();
	private void SaveClicked(object? sender,RoutedEventArgs e)=>_model.Save();
	private void RemoveClicked(object? sender,RoutedEventArgs e)=>_model.Remove();
	private void ReloadClicked(object? sender,RoutedEventArgs e)=>_model.Reload();
	private void CloseClicked(object? sender,RoutedEventArgs e)=>Close();
}
