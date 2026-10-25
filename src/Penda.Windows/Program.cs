using Penda.Core;
using Microsoft.Win32;

namespace Penda.Windows;

internal static class Program
{
    [STAThread]
    private static void Main(string[] args)
    {
        // Compatibility with an old login entry: settings no longer run in the background.
        using(var key=Registry.CurrentUser.OpenSubKey(@"Software\Microsoft\Windows\CurrentVersion\Run",true))
        {
            if(key?.GetValue("Penda") is string entry && (entry.Contains("Penda.exe",StringComparison.OrdinalIgnoreCase)||entry.Contains("Penda.SettingsHost.exe",StringComparison.OrdinalIgnoreCase))) key.DeleteValue("Penda",false);
        }
        if(args.Contains("--tray")) return;
        if(args.Contains("--migrate-only")) { StorageMigration.Import(Storage.LegacyRoot,Storage.Root);RuntimeDictionary.Publish(NativeSnapshot.UserPath);return; }
        using var mutex = new Mutex(true, @"Local\Penda.Settings.Window.0.6", out bool first);
        if (!first) { try { using var signal=EventWaitHandle.OpenExisting(@"Local\Penda.Settings.Open.0.6"); signal.Set(); } catch(WaitHandleCannotBeOpenedException) {} return; }
        ApplicationConfiguration.Initialize();
        try { Application.Run(new PendaContext()); }
        catch(Exception ex) { MessageBox.Show("无法打开输入法设置："+ex.Message+"\n数据目录："+Storage.Root,"Penda",MessageBoxButtons.OK,MessageBoxIcon.Error); }
    }
}

internal sealed class PendaContext : ApplicationContext
{
    private readonly SettingsWindow settings;
    private readonly EventWaitHandle openSignal = new(false,EventResetMode.AutoReset,@"Local\Penda.Settings.Open.0.6");
    private readonly System.Windows.Forms.Timer signalTimer = new() { Interval=200 };
    internal PendaContext()
    {
        StorageMigration.Import(Storage.LegacyRoot,Storage.Root);
        var dictionaries=Lexicon.Load();
        var preferences=Storage.LoadPreferences();
        var service=new SettingsService(preferences,new CandidateEngine(dictionaries,true));
        settings=new SettingsWindow(service,dictionaries);
        MainForm=settings;
        signalTimer.Tick+=(_,_) => { if(openSignal.WaitOne(0)) { if(settings.WindowState==FormWindowState.Minimized)settings.WindowState=FormWindowState.Normal; settings.Show();settings.Activate(); } };
        signalTimer.Start();settings.Show();
    }
    protected override void ExitThreadCore()
    {
        signalTimer.Dispose();openSignal.Dispose();base.ExitThreadCore();
    }
}
