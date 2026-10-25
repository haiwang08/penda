using Penda.Core;

if (args.Length != 2) throw new ArgumentException("Usage: SOURCE_DIRECTORY OUTPUT_DIRECTORY");
Directory.CreateDirectory(args[1]);
var parts = Enumerable.Range(0,10).Select(i => Path.Combine(args[0], $"dictionary{i:D2}.txt")).ToArray();
var words = Lexicon.Normalize(parts.SelectMany(JapaneseDictionaryReader.Read).Concat(Lexicon.Seed(Language.Japanese)));
var tsv = Path.Combine(args[1], "penda-japanese-mozc.tsv");
Lexicon.Write(tsv,words);
Console.WriteLine($"Merged Japanese entries: {words.Length:N0}");
Console.WriteLine($"Kanji entries: {words.Count(w => w.Text.EnumerateRunes().Any(r => r.Value is >= 0x3400 and <= 0x9FFF or >= 0x20000 and <= 0x323AF)):N0}");
if (words.Length > 2_000_000) throw new InvalidDataException("Native dictionary limit exceeded.");
var dictionaries = Enum.GetValues<Language>().ToDictionary(l => l, Lexicon.Seed);
dictionaries[Language.Japanese] = words;
var preferences = new Preferences { Chinese = false, English = false };
var engine = new CandidateEngine(dictionaries);
foreach (var (input,expected) in new[] { ("nihongo","日本語"),("toukyou","東京"),("kanji","漢字"),("gakkou","学校"),("sensei","先生"),("watashi","私"),("taberu","食べる"),("tabeta","食べた"),("benkyou","勉強"),("kyou","今日"),("hashi","橋") })
{
    var rows = engine.Query(input,preferences);
    bool exists = words.Any(w => w.Key == JapaneseDictionaryReader.Reading(Romaji.Hiragana(input)) && w.Text == expected);
    Console.WriteLine($"{input} -> {expected}: dictionary={exists}, candidates={rows.Any(c => c.Text == expected)}");
    if (input is "nihongo" or "toukyou" or "kanji" or "gakkou" or "sensei" or "watashi" or "benkyou")
        if (!rows.Any(c => c.Text == expected)) throw new Exception("Missing required kanji candidate: " + input);
}
NativeSnapshot.Write(Path.Combine(args[1], "japanese-validation.pdat"),preferences,dictionaries);
Console.WriteLine(tsv);
