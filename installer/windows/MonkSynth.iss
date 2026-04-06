#ifndef MyAppVersion
  #define MyAppVersion "0.0.0-dev"
#endif

[Setup]
AppId={{B8F3A1E2-7C4D-4E5F-9A6B-1D2E3F4A5B6C}
AppName=MonkSynth
AppVersion={#MyAppVersion}
AppPublisher=Odd Laundry Digital
AppPublisherURL=https://github.com/JonET/monksynth
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
OutputBaseFilename=MonkSynth-{#MyAppVersion}-Windows-Setup
LicenseFile=..\..\LICENSE
UninstallFilesDir={commonpf64}\MonkSynth
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible

[Files]
Source: "staging\MonkSynth.vst3\*"; DestDir: "{commoncf64}\VST3\MonkSynth.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\MonkSynth.vst3"
