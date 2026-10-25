param([ValidateSet('x64','Win32','Both')][string]$Architecture = 'Both')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$locator = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path -LiteralPath $locator)) { throw '请安装 Visual Studio 2022 的“使用 C++ 的桌面开发”工作负载。' }
$msbuildPath = & $locator -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (!$msbuildPath) { throw '未找到 C++ MSBuild 工具。' }
$platforms = if ($Architecture -eq 'Both') { @('x64','Win32') } else { @($Architecture) }
foreach ($platformName in $platforms) {
    & $msbuildPath (Join-Path $projectRoot 'src\Penda.Tsf\Penda.Tsf.vcxproj') /t:Build /p:Configuration=Release "/p:Platform=$platformName" /nologo /verbosity:minimal
    if ($LASTEXITCODE -ne 0) { throw "TSF $platformName 构建失败。" }
}
