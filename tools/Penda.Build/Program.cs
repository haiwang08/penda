using Penda.Core;
using System.Text;

if(args.Length==2 && args[0]=="migrate-storage") { StorageMigration.Import(Path.GetFullPath(args[1]),Storage.Root);Console.WriteLine("Shared storage migration completed: "+Storage.Root);return; }
if(args.Length==2 && args[0]=="publish-runtime") { RuntimeDictionary.Publish(Path.GetFullPath(args[1]));Console.WriteLine("Read-only app-container runtime dictionary updated.");return; }
if(args.Length==2 && args[0]=="prepare-japanese")
{
    var stage=Path.GetFullPath("artifacts/updates/japanese-install");Directory.CreateDirectory(stage);
    var source=NativeSnapshot.UserPath;
    using var input=new BinaryReader(File.OpenRead(source),Encoding.Unicode);
    if(input.ReadUInt32()!=0x31444E50u || input.ReadUInt32()!=1)throw new InvalidDataException("Unsupported native dictionary snapshot.");
    uint mask=input.ReadUInt32();
    var prefs=new Preferences { Chinese=(mask&1)!=0,Japanese=(mask&2)!=0,English=(mask&4)!=0,LatinWidth=(WidthMode)input.ReadUInt32(),NumberWidth=(WidthMode)input.ReadUInt32(),KanaWidth=(WidthMode)input.ReadUInt32() };
    string Text() { uint count=input.ReadUInt32();if(count>4096)throw new InvalidDataException("Invalid dictionary string.");var data=input.ReadBytes(checked((int)count*2));if(data.Length!=count*2)throw new EndOfStreamException();return Encoding.Unicode.GetString(data); }
    var current=new Dictionary<Language,Lexeme[]>();
    foreach(var language in Enum.GetValues<Language>()) {uint count=input.ReadUInt32();if(count>2_000_000)throw new InvalidDataException("Invalid dictionary count.");var rows=new Lexeme[count];for(int i=0;i<rows.Length;i++)rows[i]=new(Text(),Text(),input.ReadInt32());current[language]=rows;Console.WriteLine($"Existing {language}: {count:N0}");}
    var before=Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(File.ReadAllBytes(source)));
    var japanese=Lexicon.Read(Path.GetFullPath(args[1]));
    var saved=Storage.DictionaryPath(Language.Japanese);
    current[Language.Japanese]=Lexicon.Normalize(current[Language.Japanese].Concat(japanese).Concat(File.Exists(saved)?Lexicon.Read(saved):Array.Empty<Lexeme>()));
    Lexicon.Write(Path.Combine(stage,"japanese.json"),current[Language.Japanese]);
    NativeSnapshot.Write(Path.Combine(stage,"engine.pdat"),prefs,current);
    File.WriteAllText(Path.Combine(stage,"source-engine.sha256"),before);
    Console.WriteLine($"Prepared Japanese: {current[Language.Japanese].Length:N0}; Chinese and English copied from active index.");
    var candidateEngine=new CandidateEngine(current);
    foreach(var pair in new[]{("nihongo","日本語"),("toukyou","東京"),("kanji","漢字"),("gakkou","学校"),("tabeta","食べた"),("kaita","書いた"),("yonda","読んだ"),("shimbun","新聞")}) { if(!candidateEngine.Query(pair.Item1,prefs).Any(c=>c.Language==Language.Japanese&&c.Text==pair.Item2))throw new InvalidDataException("Missing Japanese validation candidate: "+pair.Item1); }
    Console.WriteLine("Prepared active-index Japanese kanji queries passed.");return;
}
if (args.Length != 2 || args[0] is not ("snapshot" or "fixtures")) throw new ArgumentException("Usage: snapshot PATH | fixtures DIRECTORY");
var dictionaries = Enum.GetValues<Language>().ToDictionary(l => l, Lexicon.Seed);
if (args[0] == "snapshot") { NativeSnapshot.Write(Path.GetFullPath(args[1]), new(), dictionaries); return; }
dictionaries[Language.Japanese] = Lexicon.Normalize(dictionaries[Language.Japanese].Concat(new[] { new Lexeme("あの","あの",1000000),new Lexeme("ふたり","二人",940960),new Lexeme("ふたり","ふたり",929232),new Lexeme("おごる","奢る",922464),new Lexeme("おごる","驕る",910368),new Lexeme("しんぶん","新聞",900000), new Lexeme("たべた","食べた",900000),new Lexeme("こーひー","珈琲",900000) }));
Directory.CreateDirectory(args[1]);
dictionaries[Language.Chinese]=Lexicon.Normalize(dictionaries[Language.Chinese].Concat(new[]{new Lexeme("haoxiang","好像",900000),new Lexeme("haoxiang","好想",800000)}));
var cases = new[] { new Preferences(), new Preferences { LatinWidth = WidthMode.Full, NumberWidth = WidthMode.Full, KanaWidth = WidthMode.Half }, new Preferences { Chinese = false, Japanese = false }, new Preferences { Chinese = false, Japanese = false, English = false } };
for (int index = 0; index < cases.Length; index++)
{
    var preferences = cases[index]; var stem = Path.Combine(args[1], "case" + index);
    NativeSnapshot.Write(stem + ".pdat", preferences, dictionaries);
    using var writer = new BinaryWriter(File.Create(stem + ".golden"), Encoding.Unicode);
    void Text(string value) { writer.Write((uint)value.Length); writer.Write(Encoding.Unicode.GetBytes(value)); }
    var inputs = new[] { "ni", "nihao", "nihongo", "hello", "konnichiwa", "konnichiha", "gakkou", "shinjuku", "kanpai", "kanna", "nya", "n'ya", "nn", "xtsu", "ryokou", "Az09漢字がパヴ", "Ａｚ０９", "", "xxyzz", "xi'an", "ｶﾞﾊﾟｳﾞ｡｢｣､･", "カ・ナ。が", "shinbun","sinbun","shimbun","tabeta","ko-hi-","ogoruna","おごるな","ogorunayo","anofutari","あのふたり","xyzfutari","haox","zhongguor" };
    var engine = new CandidateEngine(dictionaries);
    writer.Write((uint)inputs.Length);
    foreach (var input in inputs)
    {
        Text(input); Text(Romaji.Hiragana(input)); Text(WidthConverter.Convert(input, preferences));
        var rows = engine.Query(input, preferences); writer.Write((uint)rows.Length);
        foreach (var row in rows) { Text(WidthConverter.Convert(row.Text, preferences)); writer.Write(row.Language.GetHashCode()); }
    }
}
Console.WriteLine("Native parity fixtures generated.");
