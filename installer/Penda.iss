#define MyAppVersion "0.9.7"
[Setup]
AppId={{5BBF5BE5-8436-45F7-92D3-26B312589C34}
AppName=Penda TSF 三语输入
AppVersion={#MyAppVersion}
DefaultDirName={autopf}\Penda
DefaultGroupName=Penda
PrivilegesRequired=admin
ArchitecturesAllowed=x64os
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\artifacts\installer
OutputBaseFilename=Penda-TSF-Setup-{#MyAppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\settings\Penda.SettingsHost.exe
CloseApplications=yes
[Files]
Source: "..\artifacts\tsf-package\*"; DestDir: "{app}"; Excludes: "*.backup*,*.legacy-*,Penda.exe"; Flags: ignoreversion recursesubdirs createallsubdirs
[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueName: "Penda"; Flags: deletevalue
[Run]
Filename: "{app}\settings\Penda.SettingsHost.exe"; Description: "打开 Penda 设置"; Flags: nowait postinstall skipifsilent runasoriginaluser
[UninstallRun]
Filename: "{syswow64}\regsvr32.exe"; Parameters: "/u /s ""{app}\x86\Penda.Tsf.dll"""; Flags: runhidden waituntilterminated; RunOnceId: "PendaTsfX86"
Filename: "{sys}\regsvr32.exe"; Parameters: "/u /s ""{app}\x64\Penda.Tsf.dll"""; Flags: runhidden waituntilterminated; RunOnceId: "PendaTsfX64"
[Code]
procedure CurStepChanged(CurStep: TSetupStep);
var
  ResultCode: Integer;
begin
  if CurStep = ssPostInstall then
  begin
    if not Exec(ExpandConstant('{sys}\regsvr32.exe'), '/s "' + ExpandConstant('{app}\x64\Penda.Tsf.dll') + '"', '', SW_HIDE, ewWaitUntilTerminated, ResultCode) or (ResultCode <> 0) then
      RaiseException('Penda x64 TSF registration failed. Please run unregister-tsf.ps1 before retrying.');
    if not Exec(ExpandConstant('{syswow64}\regsvr32.exe'), '/s "' + ExpandConstant('{app}\x86\Penda.Tsf.dll') + '"', '', SW_HIDE, ewWaitUntilTerminated, ResultCode) or (ResultCode <> 0) then
    begin
      Exec(ExpandConstant('{sys}\regsvr32.exe'), '/u /s "' + ExpandConstant('{app}\x64\Penda.Tsf.dll') + '"', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
      RaiseException('Penda x86 TSF registration failed; x64 registration rolled back.');
    end;
  end;
end;
