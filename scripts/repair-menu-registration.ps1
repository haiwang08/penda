#Requires -RunAsAdministrator
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$resultPath = Join-Path $root 'artifacts\validation\menu-registration-result.json'
try {
    & (Join-Path $PSScriptRoot 'register-tsf.ps1') -PackageDirectory (Join-Path $root 'artifacts\tsf-package')
    foreach ($base in @('Registry::HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\CTF\TIP','Registry::HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\Microsoft\CTF\TIP')) {
        $category = $base + '\{245A7FC9-923C-4C9C-A863-8602A83C3752}\Category\Category\{25504FB4-7BAB-4BC1-9C69-CF81890F0EF5}\{245A7FC9-923C-4C9C-A863-8602A83C3752}'
        if (!(Test-Path -LiteralPath $category)) { throw ('Taskbar capability not registered: ' + $category) }
    }
    @{ Success=$true; Version='0.6.2'; TimeUtc=[DateTime]::UtcNow.ToString('o') } | ConvertTo-Json | Set-Content -LiteralPath $resultPath -Encoding UTF8
    exit 0
} catch {
    @{ Success=$false; Error=$_.Exception.Message; TimeUtc=[DateTime]::UtcNow.ToString('o') } | ConvertTo-Json | Set-Content -LiteralPath $resultPath -Encoding UTF8
    exit 1
}
