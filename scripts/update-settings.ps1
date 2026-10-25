$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$staging = Join-Path $projectRoot 'artifacts\updates\settings-0.9.9'
$installed = Join-Path $projectRoot 'artifacts\tsf-package\settings'
$romajiLicensePath = Join-Path $projectRoot 'artifacts\tsf-package\licenses\romaji'
New-Item -ItemType Directory -Force -Path $romajiLicensePath | Out-Null
foreach ($licenseName in @('MOZC-LICENSE.txt','SOURCE.json')) {
    Copy-Item -LiteralPath (Join-Path $projectRoot ('data\japanese\romaji\'+$licenseName)) -Destination $romajiLicensePath
}
Copy-Item -LiteralPath (Join-Path $projectRoot 'README.md') -Destination (Join-Path $projectRoot 'artifacts\tsf-package\README.md')
$packageDocs = Join-Path $projectRoot 'artifacts\tsf-package\docs'
New-Item -ItemType Directory -Force -Path $packageDocs | Out-Null
Copy-Item -LiteralPath (Join-Path $projectRoot 'docs\ROMAJI.md') -Destination $packageDocs
& dotnet publish (Join-Path $projectRoot 'src\Penda.Windows\Penda.Windows.csproj') -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -o $staging
if ($LASTEXITCODE -ne 0) { throw 'Settings publish failed.' }
New-Item -ItemType Directory -Force -Path $installed | Out-Null
$legacy = Join-Path $installed 'Penda.exe'
if (Test-Path -LiteralPath $legacy) { Move-Item -LiteralPath $legacy -Destination ($legacy + '.legacy-' + [DateTime]::Now.ToString('yyyyMMdd-HHmmss-fff')) }
$target = Join-Path $installed 'Penda.SettingsHost.exe'
$backup = Join-Path $installed ('Penda.SettingsHost.backup-' + [DateTime]::Now.ToString('yyyyMMdd-HHmmss-fff') + '.exe')
$moved = $false
try {
    if (Test-Path -LiteralPath $target) { Move-Item -LiteralPath $target -Destination $backup; $moved = $true }
    Copy-Item -LiteralPath (Join-Path $staging 'Penda.SettingsHost.exe') -Destination $target
} catch {
    if ($moved -and !(Test-Path -LiteralPath $target)) { Move-Item -LiteralPath $backup -Destination $target }
    throw "Could not update the existing EXE. Exit the settings tray and rerun this script. New build: $staging"
}
Write-Host "Updated: $target"
Write-Host 'Settings now open only from the native input-method menu and have no tray icon. Exit any old settings tray once.'
