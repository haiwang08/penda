# Penda 日语词库（Mozc 开放数据）

来源：https://github.com/google/mozc/tree/master/src/data/dictionary_oss

导入文件：penda-japanese-mozc.tsv。使用全部 dictionary00.txt 至 dictionary09.txt，合并重复的读音—词形记录，并保留较低词成本的排序权重。读取 Mozc id.def 标注，为明确类型的动词增加常用礼貌形、否定形、过去形、て形、条件形等；未猜测未标注词语的词性。合并后共 1,340,573 条读音—词形记录，其中 937,015 条含汉字；这不是独立汉字个数或独立词语个数。

在 Penda 0.3.0 设置中选择“日语词库 → 导入并合并”，选择本目录 TSV。旧版原生 DLL 不支持此假名索引，请在更新后注销并重新登录，再使用。设置程序旧托盘也应退出后重新打开。导入不会修改中文或英语词库。尚未替用户自动导入。

罗马字 → 假名读音 → 汉字候选，例如 nihongo 日本語、toukyou 東京、kanji 漢字、gakkou 学校、tabeta 食べた、kaita 書いた、yonda 読んだ、shimbun 新聞。输入过程中按 Tab 切到日语组；1–9 选择上行，Shift+1–9 选择下行；PgUp/PgDn 翻页。日语最多返回 64 项候选。日语长音可在当前日语组使用连字符，例如 ko-hi-。

限制：这是离线读音词典加有限的活用展开，不是完整 Mozc 转换引擎。未加载完整词性连接成本、整句文节解码、上下文语言模型或用户学习；某些长句、活用、不规则读音、数字元编码、人名地名仍可能缺失或排序不佳。原始开放 Mozc 词库不等同于 Google 日语输入法商业词库。

保留本目录 DICTIONARY-LICENSE.txt 与 MOZC-LICENSE.txt，包括 IPAdic、Okinawa 等来源的原始版权和许可说明。转换工具源码见 tools/Penda.Japanese 和 src/Penda.Core/JapaneseDictionaryReader.cs / JapaneseInflections.cs。

重新生成：在项目目录执行

dotnet run --project tools/Penda.Japanese/Penda.Japanese.csproj -c Release -- data/japanese/mozc data/japanese/ready