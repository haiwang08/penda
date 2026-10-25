using System.Buffers.Binary;
using System.Text;

namespace Penda.Core;

/// <summary>Strict reader for the common Sogou SCEL 0x44/0x45 layouts. Unknown variants are rejected.</summary>
public static class ScelReader
{
    public static Lexeme[] Read(byte[] data)
    {
        if (data.Length < 0x2628 || data[0] != 0x40 || data[1] != 0x15 || data[2] != 0 || data[3] != 0)
            throw new InvalidDataException("不是受支持的搜狗 SCEL 文件。");
        int wordsOffset = data[4] switch { 0x44 => 0x2628, 0x45 => 0x26C4, _ => throw new InvalidDataException("不支持此 SCEL 版本，请先导出为 UTF-8 TSV。") };
        var syllables = new Dictionary<ushort, string>();
        int position = 0x1544;
        ushort U16(ref int p)
        {
            if (p < 0 || p + 2 > data.Length) throw new InvalidDataException("SCEL 文件截断。");
            ushort value = BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(p, 2)); p += 2; return value;
        }
        string Text(ref int p, int length)
        {
            if ((length & 1) != 0 || length < 0 || p + length > data.Length) throw new InvalidDataException("SCEL 字符串损坏。");
            string value = Encoding.Unicode.GetString(data, p, length); p += length; return value;
        }
        while (position + 4 <= wordsOffset)
        {
            ushort id = U16(ref position), length = U16(ref position);
            if (length == 0) break;
            if (position + length > wordsOffset) throw new InvalidDataException("SCEL 拼音表损坏。");
            var syllable = Text(ref position, length);
            syllables[id] = syllable;
            if (syllable == "zuo") break;
        }
        position = wordsOffset;
        var entries = new List<Lexeme>();
        while (position < data.Length)
        {
            if (data.Length - position < 4) throw new InvalidDataException("SCEL 词条头损坏。");
            int count = U16(ref position), pinyinLength = U16(ref position);
            if (count == 0 || pinyinLength == 0 || (pinyinLength & 1) != 0 || pinyinLength > 256) throw new InvalidDataException("SCEL 拼音长度无效。");
            var key = new StringBuilder();
            for (int i = 0; i < pinyinLength / 2; i++)
            {
                var id = U16(ref position);
                if (!syllables.TryGetValue(id, out var syllable)) throw new InvalidDataException("SCEL 引用了未知拼音。");
                key.Append(syllable);
            }
            for (int i = 0; i < count; i++)
            {
                var length = U16(ref position);
                string word = Text(ref position, length);
                int extension = U16(ref position);
                if (extension < 2 || position + extension > data.Length) throw new InvalidDataException("SCEL 扩展数据损坏。");
                int frequencyPosition = position;
                int frequency = U16(ref frequencyPosition);
                position += extension;
                entries.Add(new(key.ToString(), word, frequency));
                if (entries.Count > 2_000_000) throw new InvalidDataException("SCEL 词条数量超过上限。");
            }
        }
        return Lexicon.Normalize(entries);
    }
}
