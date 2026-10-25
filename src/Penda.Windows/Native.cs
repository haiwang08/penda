using System.Runtime.InteropServices;

namespace Penda.Windows;

internal static class Native
{
    [DllImport("user32.dll")] internal static extern uint GetWindowThreadProcessId(nint window, out uint process);
    [DllImport("user32.dll")] internal static extern bool GetGUIThreadInfo(uint thread, ref GuiThreadInfo info);
    [DllImport("user32.dll")] internal static extern bool ClientToScreen(nint window, ref Point point);
    [DllImport("user32.dll")] internal static extern bool SetWindowPos(nint window, nint after, int x, int y, int width, int height, uint flags);
    [StructLayout(LayoutKind.Sequential)] internal struct Rect { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)] internal struct GuiThreadInfo { public int Size; public uint Flags; public nint Active, Focus, Capture, MenuOwner, MoveSize, Caret; public Rect CaretRect; }
    internal static GuiThreadInfo FocusInfo(nint window)
    {
        var info = new GuiThreadInfo { Size = Marshal.SizeOf<GuiThreadInfo>() };
        var thread = GetWindowThreadProcessId(window, out _);
        GetGUIThreadInfo(thread, ref info); return info;
    }
    internal static Point CaretPoint(nint window)
    {
        var info = FocusInfo(window);
        if (info.Caret != 0)
        {
            var point = new Point(info.CaretRect.Left, info.CaretRect.Bottom + 8);
            if (ClientToScreen(info.Caret, ref point)) return point;
        }
        // Modern browser controls may not expose a Win32 caret. Keep fallback away from the cursor.
        return new Point(Cursor.Position.X + 16, Cursor.Position.Y + 24);
    }
}
