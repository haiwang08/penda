namespace Penda.Windows;

internal static class Branding
{
    internal static readonly Icon Icon = LoadIcon();
    private static Icon LoadIcon()
    {
        using var stream = typeof(Branding).Assembly.GetManifestResourceStream("Penda.AppIcon")
            ?? throw new InvalidOperationException("Penda icon resource is missing.");
        using var source = new Icon(stream, SystemInformation.SmallIconSize);
        return (Icon)source.Clone();
    }
}
