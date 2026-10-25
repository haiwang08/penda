using System.Globalization;
using System.Text;

namespace Penda.Core;

/// <summary>Reads the tab-separated body of a Rime dictionary; never executes YAML tags or imports.</summary>
public static class RimeDictionaryReader
{
    public static Lexeme[] Read(string path)
    {
        var entries = new List<Lexeme>();
        bool headerStarted = false, bodyStarted = false, readingColumns = false;
        var columns = new List<string>();
        int lineNumber = 0;
        foreach (var raw in File.ReadLines(path, new UTF8Encoding(false, true)))
        {
            lineNumber++;
            var trimmed = raw.Trim().TrimStart('\uFEFF');
            if (trimmed.Length == 0 || trimmed.StartsWith('#')) continue;
            if (!headerStarted)
            {
                if (trimmed != "---") throw Error(lineNumber, "缺少 Rime 词典头（---）。请下载原始 .dict.yaml 文件，而非网页。");
                headerStarted = true; continue;
            }
            if (!bodyStarted)
            {
                if (trimmed == "...")
                {
                    bodyStarted = true;
                    if (columns.Count == 0) columns.AddRange(["text", "code", "weight"]);
                    if (columns.Distinct(StringComparer.Ordinal).Count() != columns.Count || !columns.Contains("text") || !columns.Contains("code"))
                        throw Error(lineNumber, "columns 必须包含唯一的 text 与 code 列。");
                    continue;
                }
                if (trimmed.StartsWith("columns:", StringComparison.Ordinal))
                {
                    readingColumns = true; columns.Clear();
                    var inline = trimmed[8..].Split('#')[0].Trim();
                    if (inline.Length > 0)
                    {
                        if (!inline.StartsWith('[') || !inline.EndsWith(']')) throw Error(lineNumber, "不支持此 columns 格式。");
                        columns.AddRange(inline[1..^1].Split(',').Select(Unquote)); readingColumns = false;
                    }
                }
                else if (readingColumns && trimmed.StartsWith("- ", StringComparison.Ordinal)) columns.Add(Unquote(trimmed[2..].Split('#')[0]));
                else readingColumns = false;
                continue;
            }
            var fields = raw.Split('\t');
            int textColumn = columns.IndexOf("text"), codeColumn = columns.IndexOf("code"), weightColumn = columns.IndexOf("weight");
            if (fields.Length <= Math.Max(textColumn, codeColumn) || fields.Length > columns.Count)
                throw Error(lineNumber, "词条应使用 Tab 分隔词语、拼音和可选词频。");
            var word = fields[textColumn].Trim();
            var code = string.Concat(fields[codeColumn].ToLowerInvariant().Replace("u:", "v").Replace('ü', 'v').Where(c => !char.IsWhiteSpace(c) && c is not '\'' and not '’'));
            if (word.Length is < 1 or > 256 || code.Length is < 1 or > 128 || !code.All(char.IsAsciiLetter))
                throw Error(lineNumber, "词语或全拼编码无效；本入口支持无声调全拼词典。");
            int frequency = 100;
            if (weightColumn >= 0 && weightColumn < fields.Length)
            {
                var weight = fields[weightColumn].Split('#')[0].Trim();
                if (weight.Length > 0)
                {
                    if (!long.TryParse(weight, NumberStyles.None, CultureInfo.InvariantCulture, out var value)) throw Error(lineNumber, "词频应为非负整数。");
                    // Monotonic compression preserves order above the engine's one-million frequency limit.
                    frequency = value <= 100_000 ? (int)value : (int)Math.Min(1_000_000, 100_000 + Math.Log(1 + (value - 100_000) / 100_000.0) * 100_000);
                }
            }
            entries.Add(new(code, word, frequency));
            if (entries.Count > 2_000_000) throw Error(lineNumber, "词条数超过 200 万，请拆分词库。");
        }
        if (!bodyStarted) throw new InvalidDataException("缺少 Rime 词典头结束标记（...）。");
        if (entries.Count == 0) throw new InvalidDataException("文件没有带拼音的词条；请选 8105.dict.yaml 或 base.dict.yaml，而非方案入口文件。");
        return Lexicon.Normalize(entries);
    }
    private static string Unquote(string value) => value.Trim().Trim('\'', '"');
    private static InvalidDataException Error(int line, string message) => new($"Rime 词库第 {line} 行：{message}");
}
