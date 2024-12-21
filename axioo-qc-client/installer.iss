#define MyAppName "Axioo QC System"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Axioo"
#define MyAppExeName "Axioo QC System.exe"

[Setup]
AppId={{B89F4A71-0C36-4F8A-9B48-E56F6F79C67A}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
OutputDir=release\installer
OutputBaseFilename={#MyAppName}-{#MyAppVersion}-setup
Compression=lzma2
SolidCompression=yes
PrivilegesRequired=admin

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "startupicon"; Description: "Start with Windows"; GroupDescription: "Windows Startup"

[Files]
Source: "release\portable\Axioo QC System\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon
Name: "{userstartup}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: startupicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
