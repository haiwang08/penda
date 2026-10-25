using System.Text.Json;

namespace Penda.Core;

public enum Language { Chinese, Japanese, English }
public enum WidthMode { Half, Full }
public sealed record Lexeme(string Key, string Text, int Frequency = 100);
public sealed record Candidate(string Text, Language Language, string Key, int Score);
public sealed class Preferences
{
    public bool Chinese { get; set; } = true;
    public bool Japanese { get; set; } = true;
    public bool English { get; set; } = true;
    public WidthMode LatinWidth { get; set; } = WidthMode.Half;
    public WidthMode NumberWidth { get; set; } = WidthMode.Half;
    public WidthMode KanaWidth { get; set; } = WidthMode.Full;
    public Language PreferredLanguage { get; set; } = Language.Chinese;
    public string ThemeId { get; set; } = "midnight";
    public bool Enabled(Language language) => language switch { Language.Chinese => Chinese, Language.Japanese => Japanese, _ => English };
    public void Toggle(Language language)
    {
        switch (language) { case Language.Chinese: Chinese = !Chinese; break; case Language.Japanese: Japanese = !Japanese; break; default: English = !English; break; }
    }
}

public static class Storage
{
    public static string Root { get; } = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), ".penda");
    public static string LegacyRoot => Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "Penda");
    public static readonly JsonSerializerOptions Json = new() { WriteIndented = true, PropertyNameCaseInsensitive = true };
    public static string DictionaryPath(Language language) => Path.Combine(Root, "dictionaries", language.ToString().ToLowerInvariant() + ".json");
    public static Preferences LoadPreferences()
    {
        var path = Path.Combine(Root, "settings.json");
        var preferences = File.Exists(path) ? JsonSerializer.Deserialize<Preferences>(File.ReadAllText(path), Json) ?? new() : new();
        preferences.PreferredLanguage = LoadPriority(preferences.PreferredLanguage);
        return preferences;
    }
    public static Language LoadPriority(Language fallback = Language.Chinese) => File.Exists(Path.Combine(Root,"preferred-language.txt")) && int.TryParse(File.ReadAllText(Path.Combine(Root,"preferred-language.txt")).Trim(),out int value) && value is >= 0 and < 3 ? (Language)value : fallback;
    public static void SavePriority(Language language) => AtomicWrite(Path.Combine(Root,"preferred-language.txt"),((int)language).ToString());
    public static void SavePreferences(Preferences preferences) { AtomicWrite(Path.Combine(Root, "settings.json"), JsonSerializer.Serialize(preferences, Json)); SavePriority(preferences.PreferredLanguage); }
    public static void AtomicWrite(string path, string text)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(path))!);
        var temp = path + ".tmp";
        File.WriteAllText(temp, text, new System.Text.UTF8Encoding(false));
        File.Move(temp, path, true);
    }
}
