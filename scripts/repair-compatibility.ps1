#Requires -RunAsAdministrator
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$package = Join-Path $root 'artifacts\tsf-package'
$result = Join-Path $root 'artifacts\validation\compatibility-registration-result.json'
try {
    # Read/execute only on this installed package; no browser or global security changes.
    $targets = @($package,(Join-Path $package 'x64'),(Join-Path $package 'x86'),(Join-Path $package 'settings'))
    foreach ($architecture in @('x64','x86')) {
        $targets += Join-Path $package ($architecture+'\Penda.Tsf.dll')
        $targets += Join-Path $package ($architecture+'\engine.pdat')
    }
    $targets += Join-Path $package 'settings\Penda.SettingsHost.exe'
    foreach($target in $targets) {
        if(!(Test-Path -LiteralPath $target)) { throw ('Missing package file: '+$target) }
        $acl = Get-Acl -LiteralPath $target
        foreach($sid in @('S-1-15-2-1','S-1-15-2-2')) {
            $rule=[Security.AccessControl.FileSystemAccessRule]::new([Security.Principal.SecurityIdentifier]::new($sid),[Security.AccessControl.FileSystemRights]::ReadAndExecute,[Security.AccessControl.AccessControlType]::Allow)
            $acl.AddAccessRule($rule)
        }
        Set-Acl -LiteralPath $target -AclObject $acl
    }
    & (Join-Path $PSScriptRoot 'register-tsf.ps1') -PackageDirectory $package
    $service='{245A7FC9-923C-4C9C-A863-8602A83C3752}'
    # Verify using the categories installed by DllRegisterServer in both registry views.
    foreach($view in @([Microsoft.Win32.RegistryView]::Registry64,[Microsoft.Win32.RegistryView]::Registry32)) {
        $registry=[Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::LocalMachine,$view)
        try {
            foreach($category in @('{49D2F9CF-1F5E-11D7-A6D3-00065B84435C}','{13A016DF-560B-46CD-947A-4C3AF1E0E35D}')) {
                $key=$registry.OpenSubKey('SOFTWARE\Microsoft\CTF\TIP\'+$service+'\Category\Category\'+$category+'\'+$service)
                if(!$key) { throw ('Compatibility category missing: '+$category) }; $key.Dispose()
            }
        } finally {$registry.Dispose()}
    }
    @{Success=$true;Version='0.9.0';TimeUtc=[DateTime]::UtcNow.ToString('o')} | ConvertTo-Json | Set-Content -LiteralPath $result -Encoding UTF8
    exit 0
} catch {
    @{Success=$false;Error=$_.Exception.Message;TimeUtc=[DateTime]::UtcNow.ToString('o')} | ConvertTo-Json | Set-Content -LiteralPath $result -Encoding UTF8
    exit 1
}
