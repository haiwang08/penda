using System.Text;

namespace Penda.Core;

public static class Romaji
{
    private static readonly Dictionary<string, string> Map = Build();
    public static IReadOnlyDictionary<string, string> TransliterationMap => Map;
    private static Dictionary<string, string> Build()
    {
        var map = new Dictionary<string, string>(StringComparer.Ordinal);
        void Row(string consonant, string kana)
        {
            var vowels = "aiueo";
            for (int i = 0; i < 5; i++) map[consonant + vowels[i]] = kana[i].ToString();
        }
        Row("", "あいうえお"); Row("k", "かきくけこ"); Row("s", "さしすせそ"); Row("t", "たちつてと");
        Row("n", "なにぬねの"); Row("h", "はひふへほ"); Row("m", "まみむめも"); Row("r", "らりるれろ");
        Row("g", "がぎぐげご"); Row("z", "ざじずぜぞ"); Row("d", "だぢづでど"); Row("b", "ばびぶべぼ"); Row("p", "ぱぴぷぺぽ");
        foreach (var (key, value) in new[] { ("ya","や"),("yu","ゆ"),("yo","よ"),("wa","わ"),("wo","を"),("wi","うぃ"),("we","うぇ"),("shi","し"),("chi","ち"),("tsu","つ"),("fu","ふ"),("ji","じ"),("ja","じゃ"),("ju","じゅ"),("jo","じょ"),("sha","しゃ"),("shu","しゅ"),("sho","しょ"),("cha","ちゃ"),("chu","ちゅ"),("cho","ちょ"),("nn","ん"),("n'","ん"),("-","ー") }) map[key] = value;
        foreach (var (stem, kana) in new[] { ("ky","き"),("gy","ぎ"),("sy","し"),("zy","じ"),("ty","ち"),("dy","ぢ"),("ny","に"),("hy","ひ"),("by","び"),("py","ぴ"),("my","み"),("ry","り") })
            foreach (var (vowel, small) in new[] { ("a","ゃ"),("u","ゅ"),("o","ょ") }) map[stem + vowel] = kana + small;
        foreach (var prefix in new[] { "x", "l" })
        {
            Row(prefix, "ぁぃぅぇぉ"); map[prefix + "tsu"] = "っ"; map[prefix + "tu"] = "っ";
            map[prefix + "ya"] = "ゃ"; map[prefix + "yu"] = "ゅ"; map[prefix + "yo"] = "ょ";
        }
        map["va"] = "ゔぁ"; map["vi"] = "ゔぃ"; map["vu"] = "ゔ"; map["ve"] = "ゔぇ"; map["vo"] = "ゔぉ";
        using var stream=typeof(Romaji).Assembly.GetManifestResourceStream("Penda.Romaji.Mozc") ?? throw new InvalidDataException("Missing built-in Japanese romaji table.");
        using var reader=new StreamReader(stream,Encoding.UTF8);
        while(reader.ReadLine() is { } line)
        {
            var fields=line.Split('\t');
            // Pending-input and symbol commands belong to the input state machine,
            // not the syllable map. Preserve existing n/nn and punctuation behavior.
            if(fields.Length<2 || fields.Length>2&&!string.IsNullOrEmpty(fields[2]))continue;
            string key=fields[0],value=JapaneseDictionaryReader.Reading(fields[1]);
            if(key.Length is <1 or >4 || key.Any(c=>c is not (>= 'a' and <= 'z') and not '\'' and not '-'))continue;
            if(value.Length==0 || value.Any(c=>c is not (>= '\u3041' and <= '\u3096') and not '\u30FC'))continue;
            map.TryAdd(key,value);
        }
        return map;
    }
    public static string Hiragana(string input)
    {
        input = input.ToLowerInvariant();
        var result = new StringBuilder();
        for (int i = 0; i < input.Length;)
        {
            var c = input[i];
            if(c=='n' && i+2<input.Length && input[i+1]=='n' && input[i+2]=='\'') {result.Append('ん');i+=3;continue;}
            if (c == 'm' && i + 1 < input.Length && input[i+1] is 'b' or 'p') { result.Append('ん'); i++; continue; }
            if(c=='t' && i+2<input.Length && input[i+1]=='c' && input[i+2]=='h') { result.Append('っ');i++;continue; }
            if (i + 1 < input.Length && c == input[i + 1] && c != 'n' && "bcdfghjklmpqrstvwxyz".Contains(c)) { result.Append('っ'); i++; continue; }
            if (c == 'n' && (i + 1 == input.Length || (input[i + 1] != 'n' && !"aiueoy'".Contains(input[i + 1])))) { result.Append('ん'); i++; continue; }
            // nna -> んな, while final nn -> ん.
            if (c == 'n' && i + 2 < input.Length && input[i + 1] == 'n' && "aiueoy".Contains(input[i + 2])) { result.Append('ん'); i++; continue; }
            bool found = false;
            for (int length = Math.Min(4, input.Length - i); length > 0; length--)
                if (Map.TryGetValue(input.Substring(i, length), out var kana)) { result.Append(kana); i += length; found = true; break; }
            if (!found) { result.Append(c); i++; }
        }
        return result.ToString();
    }
    public static string[] HiraganaReadings(string input)
    {
        input=input.ToLowerInvariant();
        var spellings=new List<string>{input};
        for(int i=0;i+2<input.Length;++i)
        {
            if(input[i]!='n'||input[i+1]!='n'||!"aiueoy".Contains(input[i+2])||(i>0&&input[i-1]=='n'))continue;
            int count=spellings.Count;
            for(int j=0;j<count&&spellings.Count<8;++j)
                spellings.Add(spellings[j][..i]+"n'"+spellings[j][(i+2)..]);
        }
        return spellings.Select(Hiragana).Distinct(StringComparer.Ordinal).ToArray();
    }
    public static string PreferredHiragana(string input,IReadOnlyList<Lexeme> words) => PreferredReading(HiraganaReadings(input),words);
    public static string PreferredReading(IReadOnlyList<string> readings,IReadOnlyList<Lexeme> words)
    {
        string preferred=readings[0];int best=-1;
        for(int i=0;i<readings.Count;++i)
        {
            string key=JapaneseDictionaryReader.Reading(readings[i]);int low=0,high=words.Count;
            while(low<high){int mid=(low+high)/2;if(string.CompareOrdinal(words[mid].Key,key)<0)low=mid+1;else high=mid;}
            if(low==words.Count||words[low].Key!=key)continue;
            if(i==0)return readings[0]; // Keep established nna/nni/nya spellings when valid.
            int frequency=0;for(int j=low;j<words.Count&&words[j].Key==key;++j)frequency=Math.Max(frequency,words[j].Frequency);
            if(frequency>best){best=frequency;preferred=readings[i];}
        }
        return preferred;
    }
    public static string Katakana(string text) => new(text.Select(c => c is >= '\u3041' and <= '\u3096' ? (char)(c + 0x60) : c).ToArray());
}
