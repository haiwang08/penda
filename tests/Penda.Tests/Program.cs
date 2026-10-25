using System.Diagnostics;
using System.Text;
using Penda.Core;

int passed = 0;
void Check(bool condition, string name) { if (!condition) throw new Exception("FAIL: " + name); Console.WriteLine("PASS: " + name); passed++; }
var migrationRoot=Path.Combine(Path.GetTempPath(),"Penda-migration-"+Guid.NewGuid());
var migrationOld=Path.Combine(migrationRoot,"old");var migrationNew=Path.Combine(migrationRoot,"shared");
Directory.CreateDirectory(migrationOld);Directory.CreateDirectory(migrationNew);
File.WriteAllText(Path.Combine(migrationOld,"learned-chinese.tsv"),"ni\t你\nni\t你\nhao\t好\n");
File.WriteAllText(Path.Combine(migrationNew,"learned-chinese.tsv"),"ni\t你\nni\t你\nni\t你\n");
Lexicon.Write(Path.Combine(migrationOld,"dictionaries","japanese.json"),[new("おごる","奢る",900000)]);
Lexicon.Write(Path.Combine(migrationNew,"dictionaries","japanese.json"),[new("にほんご","日本語",900000)]);
StorageMigration.Import(migrationOld,migrationNew);
var migratedUsage=File.ReadAllLines(Path.Combine(migrationNew,"learned-chinese.tsv"));
Check(migratedUsage.Count(x=>x=="ni\t你")==3&&migratedUsage.Count(x=>x=="hao\t好")==1,"迁移保留词频且不重复计算共有历史");
Check(Lexicon.Read(Path.Combine(migrationNew,"dictionaries","japanese.json")).Length==2,"迁移合并两处日语词库");
StorageMigration.Import(migrationOld,migrationNew);
Check(File.ReadAllLines(Path.Combine(migrationNew,"learned-chinese.tsv")).SequenceEqual(migratedUsage),"重复迁移不增加词频");
File.AppendAllText(Path.Combine(migrationOld,"learned-chinese.tsv"),"xin\t新\n");
StorageMigration.Import(migrationOld,migrationNew);
Check(File.ReadAllLines(Path.Combine(migrationNew,"learned-chinese.tsv")).Contains("xin\t新")&&File.Exists(Path.Combine(migrationOld,"learned-chinese.tsv")),"另一文件视图后续发现的学习记录可导入且原文件保留");
var manyWords=Lexicon.Normalize(Enumerable.Range(0,300).Select(i=>new Lexeme("shi",char.ConvertFromUtf32(0x4E00+i),1000-i)));
var pagedEngine=new CandidateEngine(new Dictionary<Language,Lexeme[]> {[Language.Chinese]=manyWords});
Check(pagedEngine.Query("shi",new(){Japanese=false,English=false}).Length==300,"同音候选超过192项仍全部可翻页");
var learnedFile=Path.Combine(Path.GetTempPath(),"Penda-learned-"+Guid.NewGuid()+".tsv");
try { File.WriteAllText(learnedFile,"nihaoxiexie\t你好谢谢\nnihaoxiexie\t你好谢谢你\n");var learned=UserLexicon.ReadChinese(learnedFile);Check(learned.Length==2 && learned.All(w=>w.Key=="nihaoxiexie"&&w.Frequency==1),"多组选词用户词库保留各个词形及使用次数"); } finally {File.Delete(learnedFile);}
var longestEngine=new CandidateEngine(new Dictionary<Language,Lexeme[]>{[Language.Chinese]=Lexicon.Normalize([new("ge","个",1000000),new("gengxin","更新",1),new("geng","更",1000000)])});
Check(longestEngine.Query("gengxin",new(){Japanese=false,English=false})[0].Text=="更新","完整最长匹配优先于高频短字");
var preferences = new Preferences();
var completionEngine=new CandidateEngine(new Dictionary<Language,Lexeme[]>{[Language.Chinese]=Lexicon.Normalize([new("hao","好",1000000),new("haoxiang","好像",900000),new("haoxiang","好想",800000)])});
var completedRows=completionEngine.Query("haox",new(){Japanese=false,English=false});
Check(completedRows[0].Text=="好像"&&completedRows[0].Key=="haox","未输完整拼音优先整词补全并消耗已输入字母");
Check(completionEngine.Query("hao",new(){Japanese=false,English=false})[0].Text=="好","完整拼音精确候选优先于补全词");
var phraseEngine=new CandidateEngine(new Dictionary<Language,Lexeme[]>{[Language.Japanese]=Lexicon.Normalize([new("あの","あの",1000000),new("あの","阿野",908768),new("ふたり","二人",940960),new("ふたり","ふたり",929232),new("は","は",1000000),new("きた","来た",950000)])});
foreach(string spelling in new[]{"anofutari","あのふたり"})Check(phraseEngine.Query(spelling,new(){Chinese=false,English=false})[0].Text=="あの二人","日语整段分词转换内部汉字 "+spelling);
Check(phraseEngine.Query("anofutarihakita",new(){Chinese=false,English=false})[0].Text=="あの二人は来た","日语多个词语与助词组合");
Check(phraseEngine.Query("xyzfutari",new(){Chinese=false,English=false}).Any(c=>c.Text=="xyz二人"),"未识别片段保留且继续转换后续词语");
var japaneseSuffixEngine=new CandidateEngine(new Dictionary<Language,Lexeme[]>{[Language.Japanese]=Lexicon.Normalize([new("おごる","奢る",922464),new("おごる","驕る",910368),new("お","尾",999999)])});
foreach(string spelling in new[]{"ogoruna","おごるな"})
{
    var suffixRows=japaneseSuffixEngine.Query(spelling,new(){Chinese=false,English=false});
    Check(suffixRows[0].Text=="奢るな"&&suffixRows.Any(x=>x.Text=="驕るな")&&suffixRows.Any(x=>x.Text=="おごるな"),"日语带助词时保留最长词语汉字及假名候选 "+spelling);
}
var dictionaries = Enum.GetValues<Language>().ToDictionary(l => l, Lexicon.Seed);
var engine = new CandidateEngine(dictionaries);
Check(engine.Query("ni", preferences).Take(6).Select(c => c.Language).Distinct().Count() == 3, "三语首屏交错");
Check(engine.Query("nihao", preferences)[0].Text == "你好", "中文全拼");
var chineseDecoder = new CandidateEngine(new Dictionary<Language,Lexeme[]> { [Language.Chinese] = Lexicon.Normalize([new("wo","我",200000),new("kan","看",100000),new("kankan","看看",200000)]) });
Check(chineseDecoder.Query("wokankan",new() { Japanese=false,English=false })[0].Text=="我","长拼音先选择前一组");
Check(chineseDecoder.Query(string.Concat(Enumerable.Repeat("wokankan",12)),new() { Japanese=false,English=false })[0].Text=="我","超过64字母仍显示简短的前一组候选");
Check(engine.Query("nihongo", preferences).Any(c => c.Text == "日本語"), "日语词库");
preferences.Japanese = false; Check(engine.Query("ni", preferences).All(c => c.Language != Language.Japanese), "语言过滤");
preferences.Japanese = true;
foreach (var (input, expected) in new[] { ("konnichiwa","こんにちわ"),("konnichiha","こんにちは"),("gakkou","がっこう"),("shinjuku","しんじゅく"),("kanpai","かんぱい"),("kanna","かんな"),("nya","にゃ"),("n'ya","んや"),("nn","ん"),("xtsu","っ"),("ryokou","りょこう") }) Check(Romaji.Hiragana(input) == expected, "罗马字 " + input);
foreach(var pair in Romaji.TransliterationMap)
{
    if(Romaji.Hiragana(pair.Key)!=pair.Value || Romaji.Hiragana(pair.Key.ToUpperInvariant())!=pair.Value)throw new Exception("Invalid romaji mapping: "+pair.Key);
    var candidates=engine.Query(pair.Key,new(){Chinese=false,English=false});
    if(!candidates.Any(c=>c.Text==Romaji.Katakana(pair.Value)))throw new Exception("Missing katakana candidate: "+pair.Key);
}
Check(true,$"全部 {Romaji.TransliterationMap.Count} 条罗马字规则及大写、片假名候选");
foreach(var (input,expected) in new[]{("ofa-","おふぁー"),("fairu","ふぁいる"),("firumu","ふぃるむ"),("feri-","ふぇりー"),("fo-ku","ふぉーく"),("fyu-jon","ふゅーじょん"),("shefu","しぇふ"),("jeru","じぇる"),("chekku","ちぇっく"),("pa-thi-","ぱーてぃー"),("dhizuni-","でぃずにー"),("twu-ru","とぅーる"),("matcha","まっちゃ"),("vva","っゔぁ"),("qqa","っくぁ"),("cchi","っち"),("t'i","てぃ"),("d'yu","でゅ")})
    Check(Romaji.Hiragana(input)==expected,"外来语和扩展拼写 "+input);
Check(engine.Query("fa",new(){Chinese=false,Japanese=false})[0].Text=="fa","英语组字母不受日语扩展规则影响");
Check(WidthConverter.Convert("Abc123漢字が", preferences) == "Abc123漢字が", "默认字符宽度");
Check(WidthConverter.Convert("ｶﾞﾊﾟｳﾞ", preferences) == "ガパヴ", "导入半角假名恢复全角");
preferences.LatinWidth = WidthMode.Full; preferences.NumberWidth = WidthMode.Full; preferences.KanaWidth = WidthMode.Half;
Check(WidthConverter.Convert("Az09漢字がパヴ", preferences) == "Ａｚ０９漢字ｶﾞﾊﾟｳﾞ", "分类型宽度和浊音");
preferences.LatinWidth = WidthMode.Half; preferences.NumberWidth = WidthMode.Half;
Check(WidthConverter.Convert("Ａｚ０９", preferences) == "Az09", "全角英文数字转半角");
var normalized = Lexicon.Normalize([new("NI HAO", "你好", 1),new("nihao", "你好", 30)]);
Check(normalized.Length == 1 && normalized[0].Frequency == 30, "合并去重和词频");
byte[] Scel(byte marker)
{
    int offset = marker == 0x44 ? 0x2628 : 0x26C4;
    using var stream = new MemoryStream(); using var writer = new BinaryWriter(stream);
    writer.Write(new byte[offset]); stream.Position = 0; writer.Write(new byte[] { 0x40,0x15,0,0,marker });
    stream.Position = 0x1544;
    foreach (var (id, syllable) in new[] { (0,"ni"),(1,"hao") }) { writer.Write((ushort)id); writer.Write((ushort)(syllable.Length * 2)); writer.Write(Encoding.Unicode.GetBytes(syllable)); }
    stream.Position = offset; writer.Write((ushort)1); writer.Write((ushort)4); writer.Write((ushort)0); writer.Write((ushort)1);
    writer.Write((ushort)4); writer.Write(Encoding.Unicode.GetBytes("你好")); writer.Write((ushort)10); writer.Write((ushort)500); writer.Write(new byte[8]); return stream.ToArray();
}
foreach (byte marker in new byte[] {0x44,0x45}) Check(ScelReader.Read(Scel(marker)).Single() == new Lexeme("nihao","你好",500), $"SCEL {marker:X2} 解析");
try { ScelReader.Read(Scel(0x44)[..^1]); throw new Exception("截断文件未拒绝"); } catch (InvalidDataException) { Check(true, "拒绝截断 SCEL"); }
var temp = Path.Combine(Path.GetTempPath(), "Penda-test-" + Guid.NewGuid() + ".tsv");
try { Lexicon.Write(temp, normalized); Check(Lexicon.Read(temp).SequenceEqual(normalized), "TSV 导出再导入"); }
finally { File.Delete(temp); }
var rimeTemp = Path.Combine(Path.GetTempPath(), "Penda-rime-" + Guid.NewGuid() + ".dict.yaml");
var japaneseTemp = Path.Combine(Path.GetTempPath(),"Penda-mozc-"+Guid.NewGuid()+".txt");
try
{
    File.WriteAllText(japaneseTemp,"にほんご\t1\t2\t1000\t日本語\nかんじ\t1\t1\t2000\t漢字\nかんじ\t2\t2\t4000\t漢字\nしんぶん\t1\t1\t1000\t新聞\n#\t1\t1\t1000\t数字\n");
    var japanese = JapaneseDictionaryReader.Read(japaneseTemp);
    Check(japanese.Length == 3,"Mozc 多词性同词去重并排除元编码");
    var japaneseEngine = new CandidateEngine(new Dictionary<Language,Lexeme[]> { [Language.Japanese] = japanese });
    Check(japaneseEngine.Query("nihongo",new()).Any(c => c.Text == "日本語"),"日语罗马字按假名读音检索汉字");
    Check(japaneseEngine.Query("sinbun",new()).Any(c => c.Text == "新聞") && japaneseEngine.Query("shimbun",new()).Any(c => c.Text == "新聞"),"日语 si/shi 与鼻音 m/n 输入兼容");
    Check(japaneseEngine.Query("shinbun",new()).Any(c => c.Text == "新聞"),"日语 Hepburn 输入检索");
    Check(JapaneseDictionaryReader.Reading("ニホンゴ") == "にほんご","日语片假名读音统一");
}
finally { File.Delete(japaneseTemp); }
try
{
    File.WriteAllText(rimeTemp, "# Rime dictionary\n---\nname: sample\n...\n看\tkan\t8030267\n看看\tkan kan\t500549\n# 不导入\tbu dao ru\t100\n女儿\tnü er\t10\n𰻝\tbiang\t1\n", new UTF8Encoding(true));
    var rows = Lexicon.Read(rimeTemp);
    Check(rows.Length == 4 && rows.Any(e => e.Key == "kankan" && e.Text == "看看"), "Rime 默认列、BOM、注释与拼音空格");
    Check(rows.Any(e => e.Key == "nver") && rows.Any(e => e.Text == "𰻝"), "Rime ü 编码与扩展汉字");
    Check(rows.Single(e => e.Text == "看").Frequency > rows.Single(e => e.Text == "看看").Frequency, "Rime 高频权重保持次序");
    File.WriteAllText(rimeTemp, "---\ncolumns: [code, weight, text]\n...\nni hao\t999\t你好\n");
    Check(Lexicon.Read(rimeTemp).Single() == new Lexeme("nihao", "你好", 999), "Rime 声明列顺序");
    File.WriteAllText(rimeTemp, "---\ncolumns:\n  - text\n  - code\n...\n你好\tni hao\n");
    Check(Lexicon.Read(rimeTemp).Single() == new Lexeme("nihao", "你好", 100), "Rime 列表列与可选词频");
    File.WriteAllText(rimeTemp, "---\nname: entry\nimport_tables:\n  - cn_dicts/base\n...\n");
    try { Lexicon.Read(rimeTemp); throw new Exception("空 Rime 入口未拒绝"); } catch (InvalidDataException) { Check(true, "拒绝只有引用的 Rime 方案入口"); }
    File.WriteAllText(rimeTemp, "---\n...\n看看\tkan kan\tinvalid\n");
    try { Lexicon.Read(rimeTemp); throw new Exception("损坏 Rime 未拒绝"); } catch (InvalidDataException) { Check(true, "拒绝损坏 Rime 权重"); }
}
finally { File.Delete(rimeTemp); }
if (args.Length == 2 && args[0] == "--rime-dir")
{
    var characters = Lexicon.Read(Path.Combine(args[1], "8105.dict.yaml"));
    var words = Lexicon.Read(Path.Combine(args[1], "base.dict.yaml"));
    Check(characters.Length > 8000, "真实 8105 字典导入");
    Check(words.Length > 100000, "真实 base 词典导入");
    var imported = new CandidateEngine(new Dictionary<Language, Lexeme[]> { [Language.Chinese] = Lexicon.Normalize(characters.Concat(words)) });
    var chineseOnly = new Preferences { Japanese = false, English = false };
    Check(imported.Query("kan", chineseOnly).Any(c => c.Text == "看"), "真实字典：kan → 看");
    Check(imported.Query("kankan", chineseOnly).Any(c => c.Text == "看看"), "真实字典：kankan → 看看");
    Check(imported.Query("gongzuo", chineseOnly).Any(c => c.Text == "工作"), "真实字典：gongzuo → 工作");
    NativeSnapshot.Write(Path.Combine(args[1], "validation.pdat"), chineseOnly, new Dictionary<Language, Lexeme[]> { [Language.Chinese] = Lexicon.Normalize(characters.Concat(words)), [Language.Japanese] = [], [Language.English] = [] });
    Console.WriteLine($"真实词库：8105 {characters.Length:N0} 条，base {words.Length:N0} 条。");
}
var million = Lexicon.Normalize(Enumerable.Range(0,1_000_000).Select(i => new Lexeme("a" + i.ToString("D7"), "词" + i, i)));
var large = new CandidateEngine(new Dictionary<Language,Lexeme[]> { [Language.Chinese] = million });
preferences = new() { Japanese = false, English = false };
for (int i = 0; i < 100; i++) large.Query("a", preferences);
var samples = new double[1000];
for (int i = 0; i < samples.Length; i++) { long start = Stopwatch.GetTimestamp(); large.Query(i % 2 == 0 ? "a" : "a09999", preferences); samples[i] = Stopwatch.GetElapsedTime(start).TotalMilliseconds; }
Array.Sort(samples); Console.WriteLine($"百万词条：查询中位数 {samples[500]:F3} ms，P95 {samples[950]:F3} ms（仅引擎，不含 UI 和系统输入）");
Check(large.Query("a0999999", preferences).First().Text == "词999999", "百万词条精确查找");
Console.WriteLine($"通过 {passed} 项检查。");
