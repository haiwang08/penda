using System.Text;
using System.Text.Json;

namespace Penda.Core;

public static class Lexicon
{
    public static Lexeme[] Normalize(IEnumerable<Lexeme> entries) => entries
        .Select(e => e with { Key = e.Key.Trim().ToLowerInvariant().Replace(" ", "").Replace("’", "").Replace("'", ""), Text = e.Text.Trim(), Frequency = Math.Clamp(e.Frequency, 0, 1_000_000) })
        .Where(e => e.Key.Length is > 0 and <= 512 && e.Text.Length is > 0 and <= 1024 && e.Key.All(c => char.IsAsciiLetterOrDigit(c) || c is '\'' or '-' or >= '\u3041' and <= '\u3096' or '\u30FC'))
        .GroupBy(e => (e.Key, e.Text)).Select(g => g.MaxBy(e => e.Frequency)!)
        .OrderBy(e => e.Key, StringComparer.Ordinal).ThenByDescending(e => e.Frequency).ToArray();

    public static Lexeme[] Read(string path)
    {
        long sizeLimit = Path.GetExtension(path).Equals(".json",StringComparison.OrdinalIgnoreCase) ? 512L*1024*1024 : 128L*1024*1024;
        if (new FileInfo(path).Length > sizeLimit) throw new InvalidDataException("词库文件过大，请拆分后导入。");
        if (Path.GetExtension(path).Equals(".yaml", StringComparison.OrdinalIgnoreCase) || Path.GetExtension(path).Equals(".yml", StringComparison.OrdinalIgnoreCase)) return RimeDictionaryReader.Read(path);
        if (Path.GetExtension(path).Equals(".scel", StringComparison.OrdinalIgnoreCase)) return ScelReader.Read(File.ReadAllBytes(path));
        if (Path.GetExtension(path).Equals(".json", StringComparison.OrdinalIgnoreCase))
            return Normalize(JsonSerializer.Deserialize<Lexeme[]>(File.ReadAllText(path), Storage.Json) ?? throw new InvalidDataException("JSON 词库为空。"));
        var entries = new List<Lexeme>();
        int lineNumber = 0;
        foreach (var line in File.ReadLines(path, Encoding.UTF8))
        {
            lineNumber++;
            if (string.IsNullOrWhiteSpace(line) || line.StartsWith('#')) continue;
            var fields = line.Split('\t');
            if (fields.Length is < 2 or > 3 || fields.Length == 3 && !int.TryParse(fields[2], out _))
                throw new InvalidDataException($"第 {lineNumber} 行格式不正确。需要：编码 TAB 词语 TAB 词频（可选）。");
            entries.Add(new(fields[0], fields[1], fields.Length == 3 ? int.Parse(fields[2]) : 100));
        }
        return Normalize(entries);
    }
    public static void Write(string path, IEnumerable<Lexeme> entries)
    {
        var normalized = Normalize(entries);
        Storage.AtomicWrite(path, Path.GetExtension(path).Equals(".json", StringComparison.OrdinalIgnoreCase)
            ? JsonSerializer.Serialize(normalized, Storage.Json)
            : "# Penda: key\ttext\tfrequency\n" + string.Join('\n', normalized.Select(e => $"{e.Key}\t{e.Text}\t{e.Frequency}")) + "\n");
    }
    public static Dictionary<Language, Lexeme[]> Load()
    {
        var result = new Dictionary<Language, Lexeme[]>();
        foreach (var language in Enum.GetValues<Language>())
        {
            string path = Storage.DictionaryPath(language);
            result[language] = File.Exists(path) ? Read(path) : Seed(language);
            if(language==Language.Chinese) result[language]=Normalize(result[language].Concat(UserLexicon.ReadChinese()));
        }
        return result;
    }
    public static Lexeme[] Seed(Language language)
    {
        var rows = language switch
        {
            Language.Chinese => "nihao|你好;nimen|你们;ni|你;hao|好;zhongwen|中文;zhongguo|中国;shurufa|输入法;shuru|输入;xiexie|谢谢;zaijian|再见;wo|我;women|我们;shi|是;de|的;bu|不;le|了;zai|在;you|有;ren|人;tianqi|天气;jintian|今天;mingtian|明天;gongzuo|工作;xuexi|学习;diannao|电脑;shijie|世界;riyu|日语;yingyu|英语;zhong|中;ri|日;ying|英;beijing|北京;shanghai|上海;dongjing|东京;riben|日本;shuzi|数字;quan jiao|全角;ban jiao|半角;kaifa|开发;ceshi|测试;huanying|欢迎;pengyou|朋友;shijian|时间;wenjian|文件;shezhi|设置;baocun|保存;daoru|导入;daochu|导出;kuaisu|快速;shuangyu|双语;sanyu|三语;ma|吗;ne|呢;hen|很;keyi|可以;meiyou|没有",
            Language.Japanese => "nihongo|日本語;nihon|日本;konnichiwa|こんにちは;arigatou|ありがとう;ohayou|おはよう;konbanwa|こんばんは;sayounara|さようなら;watashi|私;watashitachi|私たち;kyou|今日;ashita|明日;kinou|昨日;tenki|天気;tokyo|東京;toukyou|東京;kanji|漢字;kana|仮名;hiragana|ひらがな;katakana|カタカナ;nyuuryoku|入力;henkan|変換;settei|設定;hozon|保存;shigoto|仕事;benkyou|勉強;gakkou|学校;gakusei|学生;sensei|先生;tomodachi|友達;sekai|世界;neko|猫;inu|犬;hana|花;mizu|水;yama|山;kawa|川;hon|本;kotoba|言葉;ongaku|音楽;denwa|電話;jikan|時間;daijoubu|大丈夫;sumimasen|すみません;onegaishimasu|お願いします;hajimemashite|はじめまして;yoroshiku|よろしく;hai|はい;iie|いいえ;sushi|寿司;taberu|食べる;nomu|飲む;miru|見る;iku|行く;kuru|来る;kai|会;ai|愛",
            _ => "hello|hello;help|help;hey|hey;world|world;welcome|welcome;windows|Windows;input|input;method|method;language|language;english|English;chinese|Chinese;japanese|Japanese;thank|thank;thanks|thanks;thankyou|thank you;good|good;morning|morning;night|night;work|work;word|word;write|write;read|read;settings|settings;save|save;import|import;export|export;computer|computer;code|code;coding|coding;test|test;performance|performance;dictionary|dictionary;development|development;design|design;time|time;today|today;tomorrow|tomorrow;yes|yes;no|no;please|please;penda|Penda;apple|apple;application|application;app|app;open|open;close|close;file|file;folder|folder;number|number;name|name"
        };
        return Normalize(rows.Split(';').Select((row, index) => { var pair = row.Split('|'); return new Lexeme(pair[0], pair[1], 1000 - index); }));
    }
}

public sealed class CandidateEngine
{
    private readonly IReadOnlyDictionary<Language, Lexeme[]> dictionaries;
    private readonly bool userWeights;
    public CandidateEngine(IReadOnlyDictionary<Language, Lexeme[]> dictionaries,bool userWeights=false) { this.dictionaries=dictionaries;this.userWeights=userWeights; }
    public Candidate[] Query(string input, Preferences preferences)
    {
        if (string.IsNullOrEmpty(input)) return [];
        var key = input.ToLowerInvariant().Replace("'", "");
        var groups = new List<Candidate[]>();
        foreach (var language in Enum.GetValues<Language>())
        {
            if (!preferences.Enabled(language)) continue;
            var matches = new List<Candidate>();
            var usage=userWeights?UserLexicon.ReadLanguage(language):Array.Empty<Lexeme>();
            var counts=usage.ToDictionary(w=>(w.Key,w.Text),w=>w.Frequency);
            if(language==Language.Chinese)matches.AddRange(usage.Where(w=>key.StartsWith(w.Key,StringComparison.Ordinal)&&w.Text.Length<=8).Select(w=>new Candidate(w.Text,language,w.Key,1_700_000)));
            if(language==Language.Japanese)matches.AddRange(usage.Where(w=>w.Key==key).Select(w=>new Candidate(w.Text,language,w.Key,3_200_000)));
            if (dictionaries.TryGetValue(language, out var rows))
            {
                foreach (var lookupKey in (language == Language.Japanese ? new[] { key, JapaneseDictionaryReader.Reading(Romaji.Hiragana(input)) }.Distinct(StringComparer.Ordinal) : new[] { key }))
                {
                int low = 0, high = rows.Length;
                while (low < high) { int mid = (low + high) / 2; if (string.CompareOrdinal(rows[mid].Key, lookupKey) < 0) low = mid + 1; else high = mid; }
                // Bound work per keystroke even for million-entry imported dictionaries.
                for (int i = low, count = 0; i < rows.Length && rows[i].Key.StartsWith(lookupKey, StringComparison.Ordinal) && (rows[i].Key==lookupKey || count < 192); i++, count++)
                {
                    var row = rows[i];
                    matches.Add(new(row.Text, language, language==Language.Chinese?lookupKey:row.Key, (row.Key == lookupKey ? 2_000_000 : 0) + row.Frequency));
                }
                }
            }
            if (language == Language.Japanese)
            {
                var kana = Romaji.Hiragana(input);
                if(dictionaries.TryGetValue(language,out var japaneseRows) && !matches.Any(c=>c.Score>=2_000_000)) matches.AddRange(JapaneseDecoder.Decode(japaneseRows,kana,key));
                matches.Add(new(kana, language, input, 1_500_000));
                matches.Add(new(Romaji.Katakana(kana), language, input, 1_400_000));
            }
            if (language == Language.Chinese && key.Length>=1 && key.Length<=512 && dictionaries.TryGetValue(language,out var chineseRows)) matches.AddRange(ChineseDecoder.Decode(chineseRows,key));
            if (language == Language.English) matches.Add(new(input, language, input, 1_600_000));
            groups.Add(matches.Where(c => language != Language.Chinese || c.Text.Length<=8).OrderByDescending(c => language==Language.Chinese && key.StartsWith(c.Key,StringComparison.Ordinal) ? c.Key.Length : 0).ThenByDescending(c => counts.GetValueOrDefault((c.Key,c.Text))).ThenByDescending(c => c.Score).DistinctBy(c => c.Text).ToArray());
        }
        // Interleave each language so large dictionaries cannot crowd the others out.
        var result = new List<Candidate>();
        for (int rank = 0; rank < groups.Select(g => g.Length).DefaultIfEmpty().Max(); rank++) foreach (var group in groups) if (rank < group.Length) result.Add(group[rank]);
        return result.DistinctBy(c => (c.Text, c.Language)).ToArray();
    }
}
