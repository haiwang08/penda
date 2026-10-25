using System.Text;

namespace Penda.Core;

public static class WidthConverter
{
    // Unicode's halfwidth repertoire covers katakana; hiragana is transliterated to katakana first.
    private static readonly Dictionary<char, string> HalfKana = BuildKana();
    private static Dictionary<char, string> BuildKana()
    {
        var map = new Dictionary<char, string>();
        for (var c = '\uFF61'; c <= '\uFF9F'; c++)
        {
            var normalized = c.ToString().Normalize(NormalizationForm.FormKC);
            if (normalized.Length == 1) map[normalized[0]] = c.ToString();
        }
        foreach (var baseChar in Enumerable.Range(0xFF66, 0xFF9D - 0xFF66 + 1).Select(x => (char)x))
        {
            foreach (var mark in new[] { '\uFF9E', '\uFF9F' })
            {
                var pair = new string(new[] { baseChar, mark });
                var full = pair.Normalize(NormalizationForm.FormKC);
                if (full.Length == 1) map[full[0]] = pair;
            }
        }
        return map;
    }
    public static string Convert(string text, Preferences preferences)
    {
        if (preferences.KanaWidth == WidthMode.Full)
        {
            var expanded = new StringBuilder();
            for (int i = 0; i < text.Length;)
            {
                if (text[i] is >= '\uFF61' and <= '\uFF9F')
                {
                    int start = i;
                    while (i < text.Length && text[i] is >= '\uFF61' and <= '\uFF9F') i++;
                    expanded.Append(text[start..i].Normalize(NormalizationForm.FormKC));
                }
                else expanded.Append(text[i++]);
            }
            text = expanded.ToString();
        }
        var output = new StringBuilder();
        foreach (var original in text)
        {
            var c = original;
            if (c is >= '\uFF10' and <= '\uFF19' or >= '\uFF21' and <= '\uFF3A' or >= '\uFF41' and <= '\uFF5A') c = (char)(c - 0xFEE0);
            if (char.IsAsciiDigit(c)) output.Append(preferences.NumberWidth == WidthMode.Full ? (char)(c + 0xFEE0) : c);
            else if (char.IsAsciiLetter(c)) output.Append(preferences.LatinWidth == WidthMode.Full ? (char)(c + 0xFEE0) : c);
            else if (preferences.KanaWidth == WidthMode.Half)
            {
                if (c is >= '\u3041' and <= '\u3096') c = (char)(c + 0x60);
                output.Append(HalfKana.TryGetValue(c, out var half) ? half : c.ToString());
            }
            else output.Append(c);
        }
        return output.ToString();
    }
}
