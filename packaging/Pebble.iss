#define AppName "Pebble"
#define AppVersion "0.3.2"
#define AppPublisher "Pebble project"
#define AppExeName "pebble_studio.exe"

[Setup]
AppId={{A7B3C1E2-7D54-4F6A-9C10-0A0B0C0D0E0F}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\Pebble
DefaultGroupName=Pebble
OutputDir=output
OutputBaseFilename=Pebble-{#AppVersion}-Windows-x64-Setup
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\bin\pebble_studio.exe

[Files]
Source: "..\stage\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Pebble Studio"; Filename: "{app}\bin\pebble_studio.exe"; WorkingDir: "{userdocs}"
Name: "{autodesktop}\Pebble Studio"; Filename: "{app}\bin\pebble_studio.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"
Name: "addtopath"; Description: "Add Pebble to PATH"; GroupDescription: "Environment:"

[Run]
Filename: "{app}\bin\pebble_studio.exe"; Description: "Launch Pebble Studio"; Flags: postinstall nowait skipifsilent

[Registry]
Root: HKLM; Subkey: "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"; ValueType: expandsz; ValueName: "Path"; ValueData: "{olddata};{app}\bin"; Tasks: addtopath; Check: NeedsAddPath('{app}\bin')

[Code]
function NeedsAddPath(Param: string): boolean;
var
  OrigPath: string;
begin
  if not RegQueryStringValue(HKEY_LOCAL_MACHINE,
    'SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment',
    'Path', OrigPath) then
  begin
    Result := true;
    exit;
  end;
  Result := Pos(';' + Param + ';', ';' + OrigPath + ';') = 0;
end;
