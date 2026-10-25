#Requires -RunAsAdministrator
param([string]$PackageDirectory = '')
$ErrorActionPreference = 'Stop'
if (!$PackageDirectory) { $PackageDirectory = if (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'x64')) { $PSScriptRoot } else { Join-Path (Split-Path -Parent $PSScriptRoot) 'artifacts\tsf-package' } }
$packagePath = (Resolve-Path -LiteralPath $PackageDirectory).Path
$pairs = @(
    @{ HostPath = "$env:WINDIR\SysWOW64\regsvr32.exe"; DllPath = (Join-Path $packagePath 'x86\Penda.Tsf.dll') },
    @{ HostPath = "$env:WINDIR\System32\regsvr32.exe"; DllPath = (Join-Path $packagePath 'x64\Penda.Tsf.dll') }
)
foreach ($pair in $pairs) {
    if (!(Test-Path -LiteralPath $pair.DllPath)) { throw "找不到 DLL：$($pair.DllPath)" }
    $registration = Start-Process -FilePath $pair.HostPath -ArgumentList @('/u','/s',('"' + $pair.DllPath + '"')) -WindowStyle Hidden -Wait -PassThru
    if ($registration.ExitCode -ne 0) { throw "取消注册失败：$($pair.DllPath)，退出码 $($registration.ExitCode)" }
}
Write-Host '已取消 TSF 注册。用户词库及程序文件未删除。注销后所有应用释放 DLL。'
