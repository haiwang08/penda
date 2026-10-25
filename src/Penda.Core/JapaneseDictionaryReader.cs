using System.Globalization;
using System.Text;

namespace Penda.Core;

public static class JapaneseDictionaryReader
{
    public static string Reading(string value) => new(value.Normalize(NormalizationForm.FormKC).ToLowerInvariant().Where(c => !char.IsWhiteSpace(c)).Select(c => c is >= '\u30A1' and <= '\u30F6' ? (char)(c-0x60) : c).ToArray());
    public static Lexeme[] Read(string path)
    {
        if (Path.GetExtension(path).Equals(".json", StringComparison.OrdinalIgnoreCase)) return Lexicon.Read(path);
        if (new FileInfo(path).Length > 128 * 1024 * 1024) throw new InvalidDataException("日语词库文件超过 128 MB，请拆分导入。");
        var entries = new List<Lexeme>(); int line = 0;
        var tags = new Dictionary<int,string>();
        var tagPath = Path.Combine(Path.GetDirectoryName(Path.GetFullPath(path))!,"id.def");
        if (File.Exists(tagPath)) foreach (var value in File.ReadLines(tagPath)) { int split = value.IndexOf(' '); if (split > 0 && int.TryParse(value[..split],out int id)) tags[id] = value[(split+1)..].Trim(); }
        foreach (var raw in File.ReadLines(path,new UTF8Encoding(false,true)))
        {
            line++; if (string.IsNullOrWhiteSpace(raw) || raw.TrimStart().StartsWith('#')) continue;
            var fields = raw.Split('\t');
            if (fields.Length >= 5 && int.TryParse(fields[1],out _) && int.TryParse(fields[2],out _) && int.TryParse(fields[3],NumberStyles.Integer,CultureInfo.InvariantCulture,out int cost))
            {
                var key = Reading(fields[0]); var word = fields[4].Trim();
                // Mozc meta keys (# = numeric placeholders, etc.) need a decoder, not literal lookup.
                if (key.Length is < 1 or > 128 || word.Length is < 1 or > 256 || !key.All(c => char.IsAsciiLetterOrDigit(c) || c is >= '\u3041' and <= '\u3096' or '\u30FC' or '\'' or '-')) continue;
                int frequency = (int)Math.Clamp(1_000_000L-(long)cost*16,0,1_000_000);
                entries.Add(new(key,word,frequency));
                if (int.TryParse(fields[1],out int id) && tags.TryGetValue(id,out var tag)) entries.AddRange(JapaneseInflections.Expand(key,word,frequency,tag));
            }
            else if (fields.Length is 2 or 3 && (fields.Length == 2 || int.TryParse(fields[2],out _)))
                entries.Add(new(Reading(fields[0]),fields[1],fields.Length == 3 ? int.Parse(fields[2],CultureInfo.InvariantCulture) : 100));
            else throw new InvalidDataException($"日语词库第 {line} 行格式不正确。支持 Mozc 五列系统词典或 Penda TSV。");
        }
        var result = Lexicon.Normalize(entries);
        if (result.Length == 0) throw new InvalidDataException("没有可用于读音检索的日语词条。");
        return result;
    }
}
