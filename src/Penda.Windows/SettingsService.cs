using Penda.Core;

namespace Penda.Windows;

// Settings/preview only. Windows loads the native TSF DLL to handle input.
internal sealed class SettingsService
{
    internal Preferences Preferences { get; set; }
    internal CandidateEngine Engine { get; set; }
    internal SettingsService(Preferences preferences, CandidateEngine engine) { Preferences = preferences; Engine = engine; }
}
