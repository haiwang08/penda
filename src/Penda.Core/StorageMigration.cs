using System.Security.Cryptography;
using System.Text;

namespace Penda.Core;

public static class StorageMigration
{
    // AppData is virtualized in packaged hosts. The user-profile root is shared.
    // Keep originals and exact backups; each source is imported only once.
    public static void Import(string source, string target)
    {
        source=Path.GetFullPath(source); target=Path.GetFullPath(target);
        if(source.Equals(target,StringComparison.OrdinalIgnoreCase) || !Directory.Exists(source))return;
        var metadata=new StringBuilder(source.ToUpperInvariant());
        foreach(string name in new[]{"engine.pdat","settings.json","preferred-language.txt","learned-chinese.tsv","learned-japanese.tsv","learned-english.tsv","dictionaries/chinese.json","dictionaries/japanese.json","dictionaries/english.json"})
        {
            var file=new FileInfo(Path.Combine(source,name));
            if(file.Exists)metadata.Append('|').Append(name).Append(':').Append(file.Length).Append(':').Append(file.LastWriteTimeUtc.Ticks);
        }
        string id=Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(metadata.ToString())))[..16];
        string backup=Path.Combine(target,"migration-backups",id);
        string done=Path.Combine(backup,"completed.txt");
        if(File.Exists(done))return;
        Directory.CreateDirectory(backup);
        foreach(var name in new[]{"settings.json","preferred-language.txt","learned-chinese.tsv","learned-japanese.tsv","learned-english.tsv"})
        {
            string from=Path.Combine(source,name),to=Path.Combine(target,name);
            if(!File.Exists(from))continue;
            if(!File.Exists(Path.Combine(backup,name)))File.Copy(from,Path.Combine(backup,name),false);
            if(name.StartsWith("learned-",StringComparison.Ordinal))
            {
                var existing=File.Exists(to)?File.ReadAllLines(to):[];
                var counts=existing.GroupBy(x=>x).ToDictionary(g=>g.Key,g=>g.Count());
                var seen=new Dictionary<string,int>();var result=new List<string>(existing);
                foreach(string line in File.ReadAllLines(from))
                {
                    if(!line.Contains('\t'))continue;
                    seen.TryGetValue(line,out int n);seen[line]=++n;
                    if(n>counts.GetValueOrDefault(line))result.Add(line);
                }
                Storage.AtomicWrite(to,string.Join('\n',result)+"\n");
            }
            else if(!File.Exists(to))File.Copy(from,to,false);
        }
        foreach(var language in Enum.GetValues<Language>())
        {
            string name=language.ToString().ToLowerInvariant()+".json";
            string from=Path.Combine(source,"dictionaries",name),to=Path.Combine(target,"dictionaries",name);
            if(!File.Exists(from))continue;
            Directory.CreateDirectory(Path.Combine(backup,"dictionaries"));
            if(!File.Exists(Path.Combine(backup,"dictionaries",name)))File.Copy(from,Path.Combine(backup,"dictionaries",name),false);
            var words=Lexicon.Read(from);
            if(File.Exists(to))words=Lexicon.Normalize(words.Concat(Lexicon.Read(to)));
            Lexicon.Write(to,words);
        }
        if(!File.Exists(Path.Combine(target,"engine.pdat")) && File.Exists(Path.Combine(source,"engine.pdat")))File.Copy(Path.Combine(source,"engine.pdat"),Path.Combine(target,"engine.pdat"),false);
        File.WriteAllText(done,"Imported without removing original files.");
    }
}
