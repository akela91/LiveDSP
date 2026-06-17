; ===========================================================================
; LiveDSP - Inno Setup installer script
; ---------------------------------------------------------------------------
; Next-next-finish installer that installs onto the target machine:
;   - LiveDSP.exe (static MSVC runtime -> NO VC++ redist dependency)
;   - LICENSE + THIRD-PARTY-NOTICES.md (GPLv3 compliance)
;
; NAM models/rigs and presets are NOT bundled (the commercial captures cannot
; be redistributed). On first run the app creates a writable folder under
; <Documents>/LiveDSP/models; the user downloads a rig (see the in-app link and
; the README) and either extracts it there or imports it with the AMP/RIG
; panel's "Browse" button.
;
; Build (once Inno Setup is installed):
;   "C:\Program Files\Inno Setup 6\ISCC.exe" installer\LiveDSP.iss
; Resulting installer: installer\Output\LiveDSP-Setup-<version>.exe
;
; IMPORTANT: build the Release configuration first so the exe below exists.
; ===========================================================================

#define AppName       "LiveDSP"
; The version is passed by the CMake POST_BUILD via /DAppVersion=...; for a
; standalone (manual) ISCC build this default applies.
#ifndef AppVersion
  #define AppVersion  "0.1.0"
#endif
#define AppPublisher  "Qulto"
#define AppExeName    "LiveDSP.exe"

; Paths relative to the script folder (installer\).
#define RepoRoot      ".."
#define ExeSource     RepoRoot + "\build\LiveDSP_artefacts\Release\Standalone\" + AppExeName

[Setup]
AppId={{B7E5F2A1-9C3D-4E6F-8A1B-2D4C6E8F0A12}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL=https://qulto.eu
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
; The installer should be a single, compressed exe:
OutputDir=Output
OutputBaseFilename=LiveDSP-Setup-{#AppVersion}
Compression=lzma2/max
SolidCompression=yes
; 64-bit application: installs only on x64 Windows, under Program Files (not x86).
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Admin is required for Program Files.
PrivilegesRequired=admin
WizardStyle=modern
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\{#AppExeName}
; LiveDSP is distributed under GPLv3 (GPL dependencies: JUCE, Rubber Band, ASIO
; SDK) — the wizard shows the license before installing.
LicenseFile={#RepoRoot}\LICENSE

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
; Optional: the free ASIO4ALL universal ASIO driver. We do NOT bundle it (the
; author grants no public redistribution permission); instead we open the
; OFFICIAL download page at the end. An ASIO driver is only needed if the audio
; interface has no ASIO driver of its own (a Focusrite, for example, does).
Name: "asio4all"; Description: "Download the free ASIO4ALL driver (opens the official page at the end)"; GroupDescription: "Low-latency ASIO driver:"; Flags: unchecked

[Files]
; The executable.
Source: "{#ExeSource}"; DestDir: "{app}"; Flags: ignoreversion
; License + third-party notices (GPLv3 compliance).
Source: "{#RepoRoot}\LICENSE";                  DestDir: "{app}"; Flags: ignoreversion
Source: "{#RepoRoot}\THIRD-PARTY-NOTICES.md";   DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}";        Filename: "{app}\{#AppExeName}"
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}";  Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
; Open the OFFICIAL ASIO4ALL download page if the user asked for it (in the
; browser, as the original user — not with the installer's admin rights).
Filename: "https://asio4all.org/"; Description: "Open the ASIO4ALL download page"; Flags: shellexec runasoriginaluser nowait postinstall skipifsilent; Tasks: asio4all
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent
