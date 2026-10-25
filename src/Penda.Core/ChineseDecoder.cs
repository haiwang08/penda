namespace Penda.Core;

internal static class ChineseDecoder
{
    public static IEnumerable<Candidate> Decode(Lexeme[] words, string key)
    {
        for(int length=Math.Min(32,key.Length);length>0;length--)
        {
            var prefix=key[..length]; int low=0, high=words.Length;
            while(low<high) { int mid=(low+high)/2; if(string.CompareOrdinal(words[mid].Key,prefix)<0) low=mid+1; else high=mid; }
            for(int index=low;index<words.Length&&words[index].Key==prefix;index++)
            {
                if(words[index].Text.Length>8)continue;
                yield return new(words[index].Text,Language.Chinese,prefix,1_700_000+length*1000+Math.Min(words[index].Frequency,999));
            }
        }
    }
}
