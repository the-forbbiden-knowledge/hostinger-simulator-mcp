#ifndef AppVersion
  #error "AppVersion must be supplied by the release workflow."
#endif
#ifndef SourceDir
  #error "SourceDir must point to the staged Windows package."
#endif
#ifndef OutputDir
  #error "OutputDir must point to the release output directory."
#endif

#define ProductName "Hostinger Simulator"

[Setup]
AppId={{8B3EBC52-8D8B-4F4C-A7D4-7F56F0C6B8C1}
AppName={#ProductName}
AppVersion={#AppVersion}
AppPublisher=The Forbidden Knowledge
AppPublisherURL=https://github.com/the-forbbiden-knowledge/hostinger-simulator-mcp
DefaultDirName={localappdata}\Programs\HostingerSimulator
DefaultGroupName={#ProductName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
Uninstallable=yes
OutputDir={#OutputDir}
OutputBaseFilename=HostingerSimulator-Setup-x64
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\simulator_desktop.exe
SetupLogging=yes
CloseApplications=yes
RestartApplications=no

[Tasks]
Name: "desktopicon"; Description: "Criar um atalho na área de trabalho"; GroupDescription: "Atalhos adicionais:"; Flags: unchecked

[Files]
Source: "{#SourceDir}\simulator_desktop.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\simulator.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\VERSION.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\ui\*"; DestDir: "{app}\ui"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\tools\*"; DestDir: "{app}\tools"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#ProductName}"; Filename: "{app}\simulator_desktop.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\{#ProductName}"; Filename: "{app}\simulator_desktop.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\simulator_desktop.exe"; Description: "Iniciar {#ProductName}"; Flags: postinstall nowait skipifsilent
