using System.Text;
namespace Penda.Core;
public static class UserLexicon
{
    public static string ChinesePath => Path.Combine(Storage.Root,"learned-chinese.tsv");
    public static Lexeme[] ReadChinese(string? path=null)=>ReadLanguage(Language.Chinese,path);
    public static Lexeme[] ReadLanguage(Language language,string? path=null)
    {
        path ??= Path.Combine(Storage.Root,"learned-"+language.ToString().ToLowerInvariant()+".tsv");
        if(!File.Exists(path))return [];
        using var file=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.ReadWrite);
        if(file.Length>8*1024*1024)return [];
        using var reader=new StreamReader(file,Encoding.UTF8);
        var rows=new Dictionary<(string Key,string Text),int>();
        while(reader.ReadLine() is {} line){var fields=line.Split('\t');if(fields.Length!=2||fields[0].Length is 0 or >512||fields[1].Length is 0 or >1024)continue;var key=(fields[0],fields[1]);rows.TryGetValue(key,out int count);rows[key]=Math.Min(1_000_000,count+1);}
        return Lexicon.Normalize(rows.Select(pair=>new Lexeme(pair.Key.Key,pair.Key.Text,pair.Value)));
    }
}
