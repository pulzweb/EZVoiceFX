; EZ Voice FX - Windows installer (Inno Setup 6)
; Built automatically by GitHub Actions:  ISCC.exe /DDistDir=<path to dist> EZVoiceFX.iss

#ifndef DistDir
  #define DistDir "..\dist"
#endif

#define AppName "EZ Voice FX"
#define AppVersion "1.2.0"

[Setup]
AppId={{8F1B5A3E-6C2D-4E7A-9B1F-3C5D7E9A1B2C}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=EZVibe
DefaultDirName={commonpf64}\EZVibe\EZ Voice FX
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputDir={#DistDir}
OutputBaseFilename=EZ-Voice-FX-1.2-Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#DistDir}\EZ Voice FX.vst3\*"; DestDir: "{commoncf64}\VST3\EZ Voice FX.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#DistDir}\Standalone\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

[Icons]
Name: "{autoprograms}\EZ Voice FX"; Filename: "{app}\EZ Voice FX.exe"

[Run]
Filename: "{app}\EZ Voice FX.exe"; Description: "Open EZ Voice FX (standalone)"; Flags: nowait postinstall skipifsilent unchecked
