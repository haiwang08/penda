namespace Penda.Windows;

// Semantic palette and scaled drawing metrics are the future skin extension boundary.
internal sealed record CandidateTheme(Color Background, Color Surface, Color Border, Color Text, Color Muted, Color Accent, Color Chinese, Color Japanese, Color English)
{
    internal static CandidateTheme Midnight { get; } = new(Color.FromArgb(22,26,36), Color.FromArgb(38,45,62), Color.FromArgb(61,72,94), Color.FromArgb(240,244,250), Color.FromArgb(156,170,191), Color.FromArgb(127,176,255), Color.FromArgb(133,205,173), Color.FromArgb(223,171,244), Color.FromArgb(132,186,255));
}
