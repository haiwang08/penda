using System.Text;

namespace Penda.Core;

// Versioned UTF-16LE format consumed directly by the native TSF DLL; no CLR in target apps.
public static class NativeSnapshot
{
    public static string UserPath => Path.Combine(Storage.Root, "engine.pdat");
    public static void Write(string path, Preferences preferences, IReadOnlyDictionary<Language, Lexeme[]> dictionaries)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(path))!);
        var temporary = path + ".tmp";
        using (var stream = new FileStream(temporary, FileMode.Create, FileAccess.Write, FileShare.None))
        using (var writer = new BinaryWriter(stream, Encoding.Unicode))
        {
            void Text(string value) { writer.Write((uint)value.Length); writer.Write(Encoding.Unicode.GetBytes(value)); }
            writer.Write(0x31444E50u); // PND1
            writer.Write(1u);
            writer.Write((preferences.Chinese ? 1u : 0u) | (preferences.Japanese ? 2u : 0u) | (preferences.English ? 4u : 0u));
            writer.Write((uint)preferences.LatinWidth); writer.Write((uint)preferences.NumberWidth); writer.Write((uint)preferences.KanaWidth);
            foreach (var language in Enum.GetValues<Language>())
            {
                var rows = Lexicon.Normalize(dictionaries[language]); writer.Write((uint)rows.Length);
                foreach (var row in rows) { Text(row.Key); Text(row.Text); writer.Write(row.Frequency); }
            }
            writer.Write((uint)Romaji.TransliterationMap.Count);
            foreach (var pair in Romaji.TransliterationMap.OrderBy(p => p.Key, StringComparer.Ordinal)) { Text(pair.Key); Text(pair.Value); }
            stream.Flush(true);
        }
        File.Move(temporary, path, true);
        if(OperatingSystem.IsWindows() && Path.GetFullPath(path).Equals(Path.GetFullPath(UserPath),StringComparison.OrdinalIgnoreCase)) RuntimeDictionary.Publish(path);
    }
}
