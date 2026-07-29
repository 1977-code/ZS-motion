; Inno Setup script for the Windows installer.
;
; Compiled by CI (see .github/workflows/windows.yml), which stages the build output
; next to this file and passes the version in:
;
;     ISCC /DAppVersion=0.3.0 /DStageDir=..\..\..\stage installer.iss
;
; Produces one ZS-motion-<version>-Windows.exe that installs the VST3 into the
; standard shared VST3 folder every Windows host scans, and optionally the
; standalone with a Start-menu entry. Both are selectable, and it uninstalls
; cleanly through Apps & features.

#define AppName      "ZS-motion"
#define Publisher    "ZS Records"
#define AppURL       "https://zsr.artspace1977.ru"

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif

#ifndef StageDir
  #define StageDir "stage"
#endif

; Where the generated brand artwork lives, relative to this script by default.
#ifndef BrandDir
  #define BrandDir "..\..\brand"
#endif

[Setup]
; Never change AppId — it is how Windows recognises an upgrade of this product.
AppId={{B676DC95-ABCF-4793-8EC2-2464F408A8BD}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#Publisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
VersionInfoVersion={#AppVersion}

; The plug-in goes to the shared VST3 folder, so the only choosable location is
; the standalone's — not worth a wizard page of its own.
DefaultDirName={autopf}\{#Publisher}\{#AppName}
DisableDirPage=yes
DisableProgramGroupPage=yes
UninstallDisplayName={#AppName} {#AppVersion}
UninstallDisplayIcon={app}\{#AppName}.exe

; 64-bit only, matching the build.
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

; Writing to Program Files\Common Files needs elevation.
PrivilegesRequired=admin

OutputDir=.
OutputBaseFilename={#AppName}-{#AppVersion}-Windows
Compression=lzma2/max
SolidCompression=yes

; Branding — all of it generated from the plug-in's own theme by the ZSmotionArt
; target, so the installer cannot end up looking like a different product.
WizardStyle=modern
SetupIconFile={#BrandDir}\installer.ico
WizardImageFile={#BrandDir}\wizard-large.png
WizardSmallImageFile={#BrandDir}\wizard-small.png
WizardImageStretch=yes
WizardResizable=no

[Languages]
Name: "ru"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full";   Description: "Всё"
Name: "custom"; Description: "Выборочно"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 (Live, REAPER, Cubase, Studio One)"; \
    Types: full custom; Flags: checkablealone
Name: "app";  Description: "Standalone — отдельное приложение"; \
    Types: full custom

[Files]
; The VST3 is a folder bundle, so the whole tree is copied.
Source: "{#StageDir}\VST3\{#AppName}.vst3\*"; \
    DestDir: "{commoncf64}\VST3\{#AppName}.vst3"; \
    Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3

Source: "{#StageDir}\Standalone\{#AppName}.exe"; \
    DestDir: "{app}"; Flags: ignoreversion; Components: app

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppName}.exe"; Components: app

[UninstallDelete]
; Remove the bundle folder itself, which Inno leaves behind once emptied.
Type: filesandordirs; Name: "{commoncf64}\VST3\{#AppName}.vst3"

[Messages]
ru.WelcomeLabel2=Будет установлен [name/ver] — кинетический модуляционный эффект студии ZS Records.%n%nЕсли DAW открыта, её нужно перезапустить, чтобы плагин появился в списке.
en.WelcomeLabel2=This will install [name/ver], a kinetic modulation effect by ZS Records.%n%nRestart your DAW after installing so it picks the plug-in up.
