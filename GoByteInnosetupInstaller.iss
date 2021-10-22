; InnoSetup script file created by Jason Champion (https://github.com/xangis)
; GBX: GNyGaHXBUvvUQ8Uv2u5j1o8j3MkkDVGWVx
;
; InnoSetup is free and can be downloaded at: https://jrsoftware.org/isdl.php
;
; This installer expects to be compiled in a directory containing a folder
; with the same name as BuildDir defined below. It should contain the same
; files that the 64-bit .zip file distribution would contain. 

#define MyAppName "GoByte Core"
#define MyAppVersion "0.16.1"
#define MyAppPublisher "GoByte.Network"
#define MyAppURL "https://gobyte.network/"
#define MyAppExeName "bin\gobyte-qt.exe"
#define BuildDir "gobytecore-0.16.1"

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
AppId={{DA2DF930-6820-42A6-A4FB-25B702C17A19}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}/community/
AppUpdatesURL={#MyAppURL}
DefaultDirName={commonpf}\{#MyAppName}
DefaultGroupName={#MyAppName}
; We can display a license file in the installer if we want to create one.
;LicenseFile={BuildDir}\License.txt
OutputDir=installer
OutputBaseFilename=GoByteCore{#MyAppVersion}Setup
SetupIconFile=installer\gobyte.ico
UninstallDisplayIcon={app}\gobyte.ico
Compression=lzma
SolidCompression=yes
ChangesAssociations=yes
ArchitecturesInstallIn64BitMode=x64
ArchitecturesAllowed=x64

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "installer\gobyte.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs
 
[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\GoByte Website"; Filename: "{#MyAppUrl}"
Name: "{commondesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,GoByte}"; Flags: nowait postinstall skipifsilent

