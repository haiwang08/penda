using Penda.Core;
using System.Drawing.Drawing2D;

namespace Penda.Windows;

internal sealed class CandidateWindow : Form
{
    private readonly CandidateTheme theme = CandidateTheme.Midnight;
    private Candidate[] rows = [];
    private string composition = "";
    private int selected;
    private int page;
    private uint languageMask = 7;
    private int preferred;
    internal const int PageSize = 10;
    internal event Action<int>? Pick;
    private float DpiScale => DeviceDpi / 96f;
    private int S(int value) => (int)(value * DpiScale);
    internal CandidateWindow()
    {
        FormBorderStyle = FormBorderStyle.None; ShowInTaskbar = false; TopMost = true;
        StartPosition = FormStartPosition.Manual; DoubleBuffered = true; BackColor = theme.Background;
        AccessibleName = "Penda 三语候选";
    }
    protected override bool ShowWithoutActivation => true;
    protected override CreateParams CreateParams { get { var p = base.CreateParams; p.ExStyle |= 0x08000000 | 0x80; p.ClassStyle |= 0x20000; return p; } }
    protected override void WndProc(ref Message message)
    {
        if (message.Msg == 0x21) { message.Result = 3; return; } // MA_NOACTIVATE
        base.WndProc(ref message);
    }
    internal void Present(string input, Candidate[] candidates, int selection, nint target, uint enabledLanguages = 7, int priority = 0)
    {
        composition = input; rows = candidates; selected = selection;
        preferred = priority; languageMask = enabledLanguages; page = 0;
        var point = Native.CaretPoint(target);
        var area = Screen.FromPoint(point).WorkingArea;
        int count = Enumerable.Range(0,3).Count(l => (languageMask & (1u<<l)) != 0);
        Size = new Size(Math.Min(S(760),area.Width-16), S(132+118*Math.Max(1,count)));
        Location = new Point(Math.Clamp(point.X, area.Left, Math.Max(area.Left, area.Right - Width)),
            point.Y + Height <= area.Bottom ? Math.Max(area.Top, point.Y) : Math.Max(area.Top, point.Y - Height - S(30)));
        if (!Visible) Show();
        Native.SetWindowPos(Handle, -1, Left, Top, Width, Height, 0x10 | 0x40);
        Invalidate();
    }
    private int At(int language, int row, int column) => rows.Select((candidate,index) => (candidate,index)).Where(pair => (int)pair.candidate.Language == language).Skip(page*10+row*5+column).Select(pair => pair.index).FirstOrDefault(-1);
    private Rectangle Cell(int language, int row, int column)
    {
        var groups = Enumerable.Range(0,3).OrderBy(l => l == preferred ? -1 : l).Where(l => (languageMask & (1u<<l)) != 0).ToArray();
        int position = Array.IndexOf(groups,language); if (position < 0) return Rectangle.Empty;
        int cellWidth = (Width-S(24))/5;
        return new(S(12)+column*cellWidth,S(124+position*118+row*46),cellWidth-S(4),S(42));
    }
    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e); var g = e.Graphics; g.SmoothingMode = SmoothingMode.AntiAlias;
        using var border = new Pen(theme.Border); g.DrawRectangle(border, 0, 0, Width - 1, Height - 1);
        using var title = new Font("Segoe UI", 8.5f, FontStyle.Bold);
        using var inputFont = new Font("Segoe UI", 13.5f, FontStyle.Bold);
        using var wordFont = new Font("Microsoft YaHei UI", 12);
        using var small = new Font("Segoe UI", 8.5f);
        void Text(string text, Font font, Color color, Rectangle rect) => TextRenderer.DrawText(g, text, font, rect, color, TextFormatFlags.Left | TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis | TextFormatFlags.NoPrefix);
        Text("PENDA  /  三语输入", title, theme.Muted, new(S(18), S(8), S(300), S(22)));
        Text(composition + "│", inputFont, theme.Text, new(S(18), S(30), S(350), S(32)));
        if (rows.Length > 0) Text("候选："+rows[selected].Text,wordFont,theme.Accent,new(S(18),S(62),Width-S(36),S(30)));
        for (int language = 0; language < 3; language++)
        {
            if ((languageMask & (1u<<language)) == 0) continue;
            var (label,color) = language switch { 0 => ("中文",theme.Chinese), 1 => ("日本語",theme.Japanese), _ => ("English",theme.English) };
            var first = Cell(language,0,0); var last = Cell(language,0,4);
            Text(label,title,color,new(first.Left,first.Top-S(26),last.Right-first.Left,S(24)));
            for (int row = 0; row < 2; row++) for (int column = 0; column < 5; column++)
            {
                var cell = Cell(language,row,column); int index = At(language,row,column);
                if (index == selected) { using var brush = new SolidBrush(theme.Surface); g.FillRectangle(brush,cell); using var accent = new SolidBrush(color); g.FillRectangle(accent,cell.Left,cell.Bottom-S(3),cell.Width,S(3)); }
                Text((row == 1 ? "⇧" : "")+(column+1),small,color,new(cell.Left+S(4),cell.Top,S(24),cell.Height));
                Text(index >= 0 ? rows[index].Text : "—",wordFont,index >= 0 ? theme.Text : theme.Muted,new(cell.Left+S(29),cell.Top,cell.Width-S(33),cell.Height));
            }
        }
        Text("Tab 切换语言 · 数字选上行 · Shift+数字选下行 · 空格确认",small,theme.Muted,new(S(18),Height-S(30),Width-S(30),S(23)));
    }
    protected override void OnMouseDown(MouseEventArgs e)
    {
        base.OnMouseDown(e);
        if (e.Button != MouseButtons.Left) return;
        for (int language = 0; language < 3; language++) for (int row = 0; row < 2; row++) for (int column = 0; column < 5; column++) if (Cell(language,row,column).Contains(e.Location)) { int index = At(language,row,column); if (index >= 0) Pick?.Invoke(index); }
    }
}
