param([switch]$SelfContained)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$outputPath = Join-Path $projectRoot 'artifacts\tsf-package'
$settingsOutput = Join-Path $outputPath 'settings'
$projectPath = Join-Path $projectRoot 'src\Penda.Windows\Penda.Windows.csproj'
& (Join-Path $PSScriptRoot 'build-tsf.ps1')
if ($SelfContained) {
    & dotnet publish $projectPath -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -o $settingsOutput
} else {
    & dotnet publish $projectPath -c Release --self-contained false -o $settingsOutput
}
if ($LASTEXITCODE -ne 0) { throw '发布失败。' }
$snapshot = Join-Path $projectRoot 'artifacts\tsf\engine.pdat'
& dotnet run --project (Join-Path $projectRoot 'tools\Penda.Build\Penda.Build.csproj') -c Release -- snapshot $snapshot
if ($LASTEXITCODE -ne 0) { throw '默认原生词库生成失败。' }
foreach ($pair in @(@{ Source = 'x64'; Target = 'x64' }, @{ Source = 'Win32'; Target = 'x86' })) {
    $nativeOutput = Join-Path $outputPath $pair.Target
    New-Item -ItemType Directory -Force -Path $nativeOutput | Out-Null
    Copy-Item -LiteralPath (Join-Path $projectRoot "artifacts\tsf\$($pair.Source)\Penda.Tsf.dll") -Destination $nativeOutput
    Copy-Item -LiteralPath $snapshot -Destination $nativeOutput
}
# Windows PowerShell 5.1 requires a BOM to recognize UTF-8 scripts with Chinese text.
foreach ($scriptName in @('register-tsf.ps1', 'unregister-tsf.ps1')) {
    $scriptText = [IO.File]::ReadAllText((Join-Path $PSScriptRoot $scriptName), [Text.Encoding]::UTF8)
    [IO.File]::WriteAllText((Join-Path $outputPath $scriptName), $scriptText, [Text.UTF8Encoding]::new($true))
}
Copy-Item -LiteralPath (Join-Path $projectRoot 'README.md') -Destination $outputPath
Write-Host "已输出到 $outputPath"
Write-Host '原生 TSF DLL 不依赖 .NET。设置程序的 SelfContained 版自带运行环境；普通版需要 .NET 9 Desktop Runtime。'
