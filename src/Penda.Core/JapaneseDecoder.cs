namespace Penda.Core;

internal static class JapaneseDecoder
{
    private sealed record Path(string Text,int Cost,bool Changed);
    public static IEnumerable<Candidate> Decode(Lexeme[] words,string reading,string key)
    {
        const int beam=12,variants=8,maxWord=64;
        var paths=Enumerable.Range(0,reading.Length+1).Select(_=>new List<Path>()).ToArray();
        paths[0].Add(new("",0,false));
        void Add(int position,Path value)
        {
            if(value.Text.Length>1024)return;
            var list=paths[position];int same=list.FindIndex(p=>p.Text==value.Text);
            if(same>=0){if(list[same].Cost<=value.Cost)return;list.RemoveAt(same);}
            int at=list.FindIndex(p=>p.Cost>value.Cost||(p.Cost==value.Cost&&string.CompareOrdinal(p.Text,value.Text)>0));
            list.Insert(at<0?list.Count:at,value);if(list.Count>beam)list.RemoveAt(list.Count-1);
        }
        for(int i=0;i<reading.Length;++i)
        {
            foreach(var path in paths[i])Add(i+1,new(path.Text+reading[i],path.Cost+1200,path.Changed));
            for(int length=1;length<=Math.Min(maxWord,reading.Length-i);++length)
            {
                string part=reading.Substring(i,length);int low=0,high=words.Length;
                while(low<high){int mid=(low+high)/2;if(string.CompareOrdinal(words[mid].Key,part)<0)low=mid+1;else high=mid;}
                if(low==words.Length||!words[low].Key.StartsWith(part,StringComparison.Ordinal))break;
                for(int at=low,count=0;at<words.Length&&words[at].Key==part&&count<variants;++at,++count)
                {
                    var word=words[at];bool kanji=word.Text.Any(c=>c is >= '\u3400' and <= '\u9FFF');
                    int cost=90+(1_000_000-word.Frequency)/1000-(kanji&&length>=2?20:0);
                    foreach(var path in paths[i])Add(i+length,new(path.Text+word.Text,path.Cost+cost,path.Changed||word.Text!=part));
                }
            }
        }
        return paths[^1].Where(p=>p.Changed).Select(p=>new Candidate(p.Text,Language.Japanese,key,3_100_000-p.Cost));
    }
}
