$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
& (Join-Path $PSScriptRoot 'build-tsf.ps1')
& dotnet run --project (Join-Path $projectRoot 'tests\Penda.Tests\Penda.Tests.csproj') -c Release
if ($LASTEXITCODE -ne 0) { throw '托管引擎测试失败。' }
& dotnet run --project (Join-Path $projectRoot 'tools\Penda.Build\Penda.Build.csproj') -c Release -- fixtures (Join-Path $projectRoot 'artifacts\tests\fixtures')
if ($LASTEXITCODE -ne 0) { throw '测试夹具生成失败。' }
$locator = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$msbuildPath = & $locator -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
foreach ($platformName in @('x64','Win32')) {
    & $msbuildPath (Join-Path $projectRoot 'tests\Penda.NativeTests\Penda.NativeTests.vcxproj') /p:Configuration=Release "/p:Platform=$platformName" /nologo /verbosity:minimal
    if ($LASTEXITCODE -ne 0) { throw "原生测试 $platformName 构建失败。" }
    Copy-Item -LiteralPath (Join-Path $projectRoot 'artifacts\tests\fixtures\case0.pdat') -Destination (Join-Path $projectRoot "artifacts\tsf\$platformName\engine.pdat")
    & (Join-Path $projectRoot "artifacts\tests\$platformName\Penda.NativeTests.exe") (Join-Path $projectRoot 'artifacts\tests\fixtures') (Join-Path $projectRoot "artifacts\tsf\$platformName\Penda.Tsf.dll")
    if ($LASTEXITCODE -ne 0) { throw "原生 TSF $platformName 测试失败。" }
}
