using Microsoft.Win32;
using Penda.Core;

namespace Penda.Windows;

internal sealed class SettingsWindow : Form
{
    private readonly SettingsService service;
    private readonly Dictionary<Language, Lexeme[]> dictionaries;
    private readonly CheckBox[] languages = new CheckBox[3];
    private readonly ComboBox latin = WidthBox(), numbers = WidthBox(), kana = WidthBox();
    private readonly ComboBox priority = new() { DropDownStyle = ComboBoxStyle.DropDownList, Width = 200 };
    private readonly ComboBox dictionaryLanguage = new() { DropDownStyle = ComboBoxStyle.DropDownList, Width = 180 };
    private readonly Label status = new() { AutoSize = true, MaximumSize = new Size(650, 0), ForeColor = Color.FromArgb(80,100,130) };
    private readonly Label counts = new() { AutoSize = true };
    private readonly Button import = new() { Text = "导入并合并…", AutoSize = true };
    private readonly Button export = new() { Text = "导出…", AutoSize = true };
    private readonly Button save = new() { Text = "保存设置", AutoSize = true, BackColor = Color.FromArgb(40,94,177), ForeColor = Color.White, FlatStyle = FlatStyle.Flat, Padding = new Padding(16,5,16,5), Margin = new Padding(0,12,0,12) };

    private readonly TextBox preview = new() { Text = "nihongo", Width = 290 };
    private readonly Label previewText = new() { AutoSize = true, MaximumSize = new Size(640, 0) };
    private bool busy;
    private Language priorityBaseline;
    private CandidateWindow? previewPopup;
    private static ComboBox WidthBox() { var box = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, Width = 200 }; box.Items.AddRange(["半角", "全角"]); return box; }
    internal SettingsWindow(SettingsService service, Dictionary<Language, Lexeme[]> dictionaries)
    {
        this.service = service; this.dictionaries = dictionaries;
        Icon = Branding.Icon; Text = "Penda · 三语输入设置"; Font = new Font("Microsoft YaHei UI", 10);
        BackColor = Color.FromArgb(246,248,252); ClientSize = new Size(770, 800); MinimumSize = new Size(680, 600);
        StartPosition = FormStartPosition.CenterScreen; AutoScaleMode = AutoScaleMode.Dpi;
        var panel = new FlowLayoutPanel { Dock = DockStyle.Fill, AutoScroll = true, FlowDirection = FlowDirection.TopDown, WrapContents = false, Padding = new Padding(30,24,24,24) };
        Controls.Add(panel);
        void Heading(string text, bool large = false) => panel.Controls.Add(new Label { Text = text, AutoSize = true, Font = new Font(Font.FontFamily, large ? 22 : 12, FontStyle.Bold), Margin = new Padding(0, large ? 0 : 18, 0, 10) });
        void Note(string text) => panel.Controls.Add(new Label { Text = text, AutoSize = true, MaximumSize = new Size(660, 0), Margin = new Padding(0, 0, 0, 10), ForeColor = Color.FromArgb(84,97,119) });
        void Row(string text, Control control)
        {
            var row = new FlowLayoutPanel { AutoSize = true, WrapContents = false, Margin = new Padding(0,3,0,4) };
            row.Controls.Add(new Label { Text = text, Width = 190, Height = 32, TextAlign = ContentAlignment.MiddleLeft }); row.Controls.Add(control); panel.Controls.Add(row);
        }
        Heading("一种输入，三种表达。", true);
        Note("PENDA TSF  0.9.7  ·  本地离线词库  ·  Ctrl + Alt + 空格：启用 / 暂停");
        Heading("候选语言");
        var languageRow = new FlowLayoutPanel { AutoSize = true };
        var names = new[] { "中文  Ctrl+Alt+1", "日本語  Ctrl+Alt+2", "English  Ctrl+Alt+3" };
        for (int i = 0; i < 3; i++) { languages[i] = new CheckBox { Text = names[i], AutoSize = true, Margin = new Padding(0,0,22,0), Checked = service.Preferences.Enabled((Language)i) }; languageRow.Controls.Add(languages[i]); }
        panel.Controls.Add(languageRow);
        Note("可同时开启 1–3 种语言；关闭全部语言时，字母仍可通过 Enter 输出原文。\n请在 Windows 输入法列表中选择“Penda 中日英”，无需启动本设置程序。");
        priority.Items.AddRange(["中文优先", "日语优先", "英语优先"]); priorityBaseline = Storage.LoadPriority(service.Preferences.PreferredLanguage); priority.SelectedIndex = (int)priorityBaseline;
        Row("起始候选语言", priority);
        Note("最近一次确认输入的语言自动排在第一行，并记住选择。Tab 切换选词组；也可从右键菜单调整初始顺序。");
        Heading("字符宽度");
        latin.SelectedIndex = (int)service.Preferences.LatinWidth; numbers.SelectedIndex = (int)service.Preferences.NumberWidth; kana.SelectedIndex = (int)service.Preferences.KanaWidth;
        Row("英文字母", latin); Row("数字", numbers); Row("假名", kana);
        Note("汉字：标准全角（Unicode 没有通用半角汉字，暂不提供无效开关）。\n半角假名会输出片假名，例如 が → ｶﾞ；平假名没有标准半角形式。");
        Heading("独立词库");
        dictionaryLanguage.Items.AddRange(["中文词库", "日语词库", "英语词库"]); dictionaryLanguage.SelectedIndex = 0;
        Row("管理语言", dictionaryLanguage); panel.Controls.Add(counts);
        var actions = new FlowLayoutPanel { AutoSize = true }; actions.Controls.Add(import); actions.Controls.Add(export); panel.Controls.Add(actions);
        Note("中文支持雾凇 8105.dict.yaml / base.dict.yaml、搜狗 SCEL、TSV / JSON。\n日语支持 Mozc 全部分片及 Penda TSV / JSON；按读音检索汉字，保留同音词。");
        Heading("候选预览"); Row("拼音 / 罗马字 / 英文", preview); panel.Controls.Add(previewText);
        var showPopup = new Button { Text = "预览实际候选窗", AutoSize = true };
        panel.Controls.Add(showPopup);
        showPopup.Click += (_, _) =>
        {
            previewPopup?.Dispose(); previewPopup = new CandidateWindow();
            var rows = service.Engine.Query(preview.Text.Trim(), Draft()).Select(c => c with { Text = WidthConverter.Convert(c.Text, Draft()) }).ToArray();
            previewPopup.Pick += index => { if (index < rows.Length) status.Text = "预览选择：" + rows[index].Text; previewPopup.Hide(); };
            var draft = Draft(); uint mask = (draft.Chinese ? 1u : 0u) | (draft.Japanese ? 2u : 0u) | (draft.English ? 4u : 0u);
            int selection = Array.FindIndex(rows, c => c.Language == draft.PreferredLanguage);
            previewPopup.Present(preview.Text.Trim(), rows, Math.Max(0,selection), Handle, mask, (int)draft.PreferredLanguage);
        };
        Heading("保存设置");
        Note("设置从输入法图标的右键菜单打开，关闭窗口即退出。输入法由 Windows 自动加载，无需设置程序开机启动。");
        panel.Controls.Add(save); panel.Controls.Add(status);
        dictionaryLanguage.SelectedIndexChanged += (_, _) => UpdateCounts();
        preview.TextChanged += (_, _) => UpdatePreview();
        import.Click += async (_, _) => await Import(); export.Click += async (_, _) => await Export();
        save.Click += async (_, _) => await Save();
        FormClosing += (_, e) => { if (busy) { e.Cancel = true; status.Text = "正在处理词库，请完成后再关闭。"; } else previewPopup?.Dispose(); };
        Deactivate += (_, _) => previewPopup?.Hide();
        Activated += (_, _) => { priority.SelectedIndex = (int)Storage.LoadPriority(service.Preferences.PreferredLanguage); };
        UpdateCounts(); UpdatePreview();
    }
    private Preferences Draft() => new() { Chinese = languages[0].Checked, Japanese = languages[1].Checked, English = languages[2].Checked, LatinWidth = (WidthMode)latin.SelectedIndex, NumberWidth = (WidthMode)numbers.SelectedIndex, KanaWidth = (WidthMode)kana.SelectedIndex, PreferredLanguage = (Language)priority.SelectedIndex, ThemeId = service.Preferences.ThemeId };
    private void UpdatePreview() => previewText.Text = string.Join("    ", service.Engine.Query(preview.Text.Trim(), Draft()).Take(6).Select(c => $"{c.Language switch { Language.Chinese => "中", Language.Japanese => "日", _ => "EN" }} · {WidthConverter.Convert(c.Text, Draft())}"));
    private void UpdateCounts() => counts.Text = $"当前词条：{dictionaries[(Language)dictionaryLanguage.SelectedIndex].Length:N0}";
    private void SetBusy(bool value) { busy = value; save.Enabled = import.Enabled = export.Enabled = dictionaryLanguage.Enabled = !value; }
    private async Task Import()
    {
        var language = (Language)dictionaryLanguage.SelectedIndex;
        using var dialog = new OpenFileDialog { Title = "选择词库", Filter = language == Language.Chinese ? "支持的中文词库|*.yaml;*.yml;*.scel;*.tsv;*.txt;*.json|Rime 拼音词库（雾凇）|*.yaml;*.yml|搜狗细胞词库|*.scel|TSV / JSON|*.tsv;*.txt;*.json" : language == Language.Japanese ? "日语 Mozc / Penda 词库|*.txt;*.tsv;*.json" : "词库|*.tsv;*.txt;*.json", CheckFileExists = true, Multiselect = language != Language.English };
        if (dialog.ShowDialog(this) != DialogResult.OK) return;
        SetBusy(true); status.Text = "正在校验、合并并建立词库索引…";
        try
        {
            var existing = dictionaries[language];
            var merged = await Task.Run(() =>
            {
                if (language != Language.Chinese && dialog.FileNames.Any(path => Path.GetExtension(path).ToLowerInvariant() is ".scel" or ".yaml" or ".yml")) throw new InvalidDataException("SCEL / Rime 导入仅用于中文词库。");
                var incoming = dialog.FileNames.SelectMany(path => language == Language.Japanese ? JapaneseDictionaryReader.Read(path) : Lexicon.Read(path)).ToArray();
                if (incoming.Length == 0) throw new InvalidDataException("未发现有效词条，请检查编码列。");
                var result = Lexicon.Normalize(existing.Concat(incoming));
                Lexicon.Write(Storage.DictionaryPath(language), result); return result;
            });
            dictionaries[language] = merged;
            service.Engine = new CandidateEngine(new Dictionary<Language, Lexeme[]>(dictionaries),true);
            await Task.Run(() => NativeSnapshot.Write(NativeSnapshot.UserPath, service.Preferences, dictionaries));
            UpdateCounts(); UpdatePreview(); status.Text = $"已导入 {dialog.FileNames.Length} 个文件，新增 {merged.Length - existing.Length:N0} 条；当前共 {merged.Length:N0} 条。下一次开始输入时加载新词库。";
        }
        catch (Exception ex) { status.Text = "导入失败：" + ex.Message; }
        finally { SetBusy(false); }
    }
    private async Task Export()
    {
        var language = (Language)dictionaryLanguage.SelectedIndex;
        using var dialog = new SaveFileDialog { Title = "导出独立词库", Filter = "UTF-8 TSV|*.tsv|JSON|*.json", FileName = language.ToString().ToLowerInvariant() + ".tsv", AddExtension = true };
        if (dialog.ShowDialog(this) != DialogResult.OK) return;
        SetBusy(true);
        try { var rows = language==Language.Chinese ? Lexicon.Normalize(dictionaries[language].Concat(UserLexicon.ReadChinese())) : dictionaries[language]; await Task.Run(() => Lexicon.Write(dialog.FileName, rows)); status.Text = $"已导出 {rows.Length:N0} 条。"; }
        catch (Exception ex) { status.Text = "导出失败：" + ex.Message; }
        finally { SetBusy(false); }
    }
    private async Task Save()
    {
        try
        {
            SetBusy(true);
            var preferences = Draft(); Storage.SavePreferences(preferences);
            await Task.Run(() => NativeSnapshot.Write(NativeSnapshot.UserPath, preferences, dictionaries));
            service.Preferences = preferences; UpdatePreview(); status.Text = "设置已保存；TSF 在下一次开始输入时加载新设置。";
        }
        catch (Exception ex) { status.Text = "保存失败：" + ex.Message; }
        finally { SetBusy(false); }
    }
}
