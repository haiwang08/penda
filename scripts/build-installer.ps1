param([string]$Compiler = 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (!(Test-Path -LiteralPath $Compiler)) { throw '未找到 Inno Setup 6。安装后使用 -Compiler 指定 ISCC.exe 路径。' }
& (Join-Path $PSScriptRoot 'publish.ps1') -SelfContained
& $Compiler (Join-Path $projectRoot 'installer\Penda.iss')
if ($LASTEXITCODE -ne 0) { throw '安装包编译失败。' }
Write-Host "安装包：$(Join-Path $projectRoot 'artifacts\installer\Penda-TSF-Setup-0.9.8.exe')"
