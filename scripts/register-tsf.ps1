#Requires -RunAsAdministrator
param([string]$PackageDirectory = '')
$ErrorActionPreference = 'Stop'
if (!$PackageDirectory) { $PackageDirectory = if (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'x64')) { $PSScriptRoot } else { Join-Path (Split-Path -Parent $PSScriptRoot) 'artifacts\tsf-package' } }
$packagePath = (Resolve-Path -LiteralPath $PackageDirectory).Path
$pairs = @(
    @{ HostPath = "$env:WINDIR\System32\regsvr32.exe"; DllPath = (Join-Path $packagePath 'x64\Penda.Tsf.dll') },
    @{ HostPath = "$env:WINDIR\SysWOW64\regsvr32.exe"; DllPath = (Join-Path $packagePath 'x86\Penda.Tsf.dll') }
)
foreach ($pair in $pairs) { if (!(Test-Path -LiteralPath $pair.DllPath)) { throw "缺少 $($pair.DllPath)，请先发布 TSF 版。" } }
foreach ($pair in $pairs) {
    $registration = Start-Process -FilePath $pair.HostPath -ArgumentList @('/s', ('"' + $pair.DllPath + '"')) -WindowStyle Hidden -Wait -PassThru
    if ($registration.ExitCode -ne 0) { throw "注册失败，退出码 $($registration.ExitCode)：$($pair.DllPath)。请执行 unregister-tsf.ps1 回滚注册。" }
}
Write-Host '已注册 Penda TSF。注销并重新登录后，在 Windows 输入法列表选择 Penda 中日英。'
Write-Host '注册目录必须保持不动；设置程序不必常驻，输入法由 Windows 加载。'
