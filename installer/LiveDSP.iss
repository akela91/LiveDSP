; ===========================================================================
; LiveDSP - Inno Setup telepítő szkript
; ---------------------------------------------------------------------------
; Next-next-finish telepítő, amely a célgépre telepíti:
;   - LiveDSP.exe (statikus MSVC runtime -> NINCS VC++ redist függőség)
;   - models\  (NAM modellek + IR-ek, az exe mellé)
;   - favs\    (gyári presetek, az exe mellé)
; A modellek/presetek az exe melletti mappákból töltődnek (lásd AppPaths.h).
;
; Fordítás (miután az Inno Setup telepítve van):
;   "C:\Program Files\Inno Setup 6\ISCC.exe" installer\LiveDSP.iss
; A kész telepítő: installer\Output\LiveDSP-Setup-<verzió>.exe
;
; FONTOS: előbb Release buildet kell csinálni, hogy létezzen a lenti exe.
; ===========================================================================

#define AppName       "LiveDSP"
; A verziót a CMake POST_BUILD a /DAppVersion=... kapcsolóval adja át; önálló
; (kézi) ISCC-fordításnál ez az alapérték lép életbe.
#ifndef AppVersion
  #define AppVersion  "0.1.0"
#endif
#define AppPublisher  "Qulto"
#define AppExeName    "LiveDSP.exe"

; A szkript mappájához (installer\) képest relatív útvonalak.
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
; A telepítő egyetlen, tömörített exe legyen:
OutputDir=Output
OutputBaseFilename=LiveDSP-Setup-{#AppVersion}
Compression=lzma2/max
SolidCompression=yes
; 64-bites alkalmazás: csak x64 Windowsra települ, Program Files (nem x86) alá.
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Nincs adminjog kötelezően, de Program Files-hoz kell -> admin.
PrivilegesRequired=admin
WizardStyle=modern
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\{#AppExeName}
; A LiveDSP GPLv3 alatt terjeszthető (GPL függőségek: JUCE, Rubber Band, ASIO
; SDK) — a varázsló kiírja a licencet a telepítés előtt.
LicenseFile={#RepoRoot}\LICENSE

[Languages]
Name: "hungarian"; MessagesFile: "compiler:Languages\Hungarian.isl"
Name: "english";   MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
; Opcionális: az ingyenes ASIO4ALL univerzális ASIO driver. NEM csomagoljuk
; (a szerző nem ad nyilvános újraterjesztési engedélyt), helyette a telepítés
; végén a HIVATALOS letöltőoldalt nyitjuk meg. ASIO driver csak akkor kell, ha
; a hangkártyának nincs saját ASIO drivere (pl. Focusrite-nak van).
Name: "asio4all"; Description: "ASIO4ALL ingyenes ASIO driver letöltése (hivatalos oldal megnyitása a végén)"; GroupDescription: "Alacsony latenciás ASIO driver:"; Flags: unchecked

[Files]
; A futtatható.
Source: "{#ExeSource}"; DestDir: "{app}"; Flags: ignoreversion
; NAM modellek + IR-ek (rekurzívan).
Source: "{#RepoRoot}\models\*"; DestDir: "{app}\models"; Flags: ignoreversion recursesubdirs createallsubdirs
; Gyári presetek (rekurzívan).
Source: "{#RepoRoot}\favs\*";   DestDir: "{app}\favs";   Flags: ignoreversion recursesubdirs createallsubdirs
; Licenc + harmadik féltől származó jegyzékek (GPLv3 megfelelőség).
Source: "{#RepoRoot}\LICENSE";                  DestDir: "{app}"; Flags: ignoreversion
Source: "{#RepoRoot}\THIRD-PARTY-NOTICES.md";   DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}";        Filename: "{app}\{#AppExeName}"
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}";  Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
; Az ASIO4ALL HIVATALOS letöltőoldalának megnyitása, ha a felhasználó kérte
; (a böngészőben, a felhasználó jogaival — nem a telepítő admin-jogával).
Filename: "https://asio4all.org/"; Description: "ASIO4ALL letöltőoldal megnyitása"; Flags: shellexec runasoriginaluser nowait postinstall skipifsilent; Tasks: asio4all
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent
