namespace Penda.Core;

internal static class JapaneseInflections
{
    internal static IEnumerable<Lexeme> Expand(string reading,string word,int frequency,string tag)
    {
        var fields = tag.Split(',');
        if (fields.Length < 6 || fields[0] != "動詞" || fields[1] != "自立" || fields[5] != "基本形" || reading.Length < 2 || word.Length < 2) yield break;
        if (fields[4].StartsWith("サ変",StringComparison.Ordinal) && reading.EndsWith("する",StringComparison.Ordinal) && word.EndsWith("する",StringComparison.Ordinal))
        {
            foreach (var suffix in new[] {"した","して","します","しました","しません","しない","しなかった","したい","したかった","すれば","しよう"}) yield return new(reading[..^2]+suffix,word[..^2]+suffix,Math.Max(0,frequency-3000));
            yield break;
        }
        if (fields[4].StartsWith("カ変",StringComparison.Ordinal) && reading.EndsWith("くる",StringComparison.Ordinal) && word.EndsWith("来る",StringComparison.Ordinal))
        {
            foreach (var (sound,surface) in new[] {("きた","来た"),("きて","来て"),("きます","来ます"),("きました","来ました"),("こない","来ない"),("こなかった","来なかった"),("くれば","来れば"),("こよう","来よう")}) yield return new(reading[..^2]+sound,word[..^2]+surface,Math.Max(0,frequency-3000));
            yield break;
        }
        var stemReading = reading[..^1]; var stemWord = word[..^1];
        string? i = null, a = null, e = null, o = null, te = null, ta = null;
        if (fields[4] == "一段" && reading.EndsWith('る') && word.EndsWith('る')) { i = a = stemReading; e = stemReading+"れ"; o = stemReading+"よ"; te = stemReading+"て"; ta = stemReading+"た"; }
        else if (fields[4].StartsWith("五段",StringComparison.Ordinal))
        {
            var index = "うくぐすつぬぶむる".IndexOf(reading[^1]);
            if (index < 0 || word[^1] != reading[^1]) yield break;
            i = stemReading+"いきぎしちにびみり"[index]; a = stemReading+"わかがさたなばまら"[index]; e = stemReading+"えけげせてねべめれ"[index]; o = stemReading+"おこごそとのぼもろ"[index];
            var teEnd = new[] {"って","いて","いで","して","って","んで","んで","んで","って"}[index];
            if (word.EndsWith("行く",StringComparison.Ordinal)) teEnd = "って";
            if (fields[4].Contains("ウ音便",StringComparison.Ordinal)) teEnd = "うて";
            te = stemReading+teEnd; ta = te[..^1]+(te.EndsWith('で') ? "だ" : "た");
        }
        else yield break;
        string Surface(string form) => stemWord+form[stemReading.Length..];
        foreach (var value in new[] {i+"ます",i+"ました",i+"ません",i+"ませんでした",i+"たい",i+"たかった",a+"ない",a+"なかった",e+"ば",o+"う",te,ta})
        {
            if (reading == "ある" && (value == a+"ない" || value == a+"なかった")) continue;
            yield return new(value,Surface(value),Math.Max(0,frequency-3000));
        }
    }
}
