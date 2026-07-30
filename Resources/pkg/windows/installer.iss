; Inno Setup script for the Windows installer.
;
; Compiled by CI (see .github/workflows/windows.yml), which stages the build output
; next to this file and passes the version in:
;
;     ISCC /DAppVersion=0.3.0 /DStageDir=..\..\..\stage installer.iss
;
; Produces one ZS-Motion-Bundle-<version>-Windows.exe carrying both plug-ins.
; Each VST3 goes to the shared folder every Windows host scans; each standalone is
; optional and gets its own Start-menu entry. Every part is selectable, and it
; uninstalls cleanly through Apps & features.

#define AppName      "ZS Motion Bundle"
#define Motion       "ZS-motion"
#define Fan          "ZS-MOTION-FAN"
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
DefaultDirName={autopf}\{#Publisher}
DisableDirPage=yes
DisableProgramGroupPage=yes
UninstallDisplayName={#AppName} {#AppVersion}
UninstallDisplayIcon={app}\{#Motion}\{#Motion}.exe

; 64-bit only, matching the build.
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

; Writing to Program Files\Common Files needs elevation.
PrivilegesRequired=admin

OutputDir=.
OutputBaseFilename=ZS-Motion-Bundle-{#AppVersion}-Windows
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
Name: "motion";      Description: "{#Motion} — глубокий модуляционный движок"; Types: full custom
Name: "motion\vst3"; Description: "VST3"; Types: full custom
Name: "motion\app";  Description: "Standalone"; Types: full custom
Name: "fan";         Description: "{#Fan} — кольцевая модуляция, десять ручек"; Types: full custom
Name: "fan\vst3";    Description: "VST3"; Types: full custom
Name: "fan\app";     Description: "Standalone"; Types: full custom

[Files]
; VST3s are folder bundles, so the whole tree is copied for each.
Source: "{#StageDir}\VST3\{#Motion}.vst3\*"; \
    DestDir: "{commoncf64}\VST3\{#Motion}.vst3"; \
    Flags: ignoreversion recursesubdirs createallsubdirs; Components: motion\vst3

Source: "{#StageDir}\Standalone\{#Motion}.exe"; \
    DestDir: "{app}\{#Motion}"; Flags: ignoreversion; Components: motion\app

Source: "{#StageDir}\VST3\{#Fan}.vst3\*"; \
    DestDir: "{commoncf64}\VST3\{#Fan}.vst3"; \
    Flags: ignoreversion recursesubdirs createallsubdirs; Components: fan\vst3

Source: "{#StageDir}\Standalone\{#Fan}.exe"; \
    DestDir: "{app}\{#Fan}"; Flags: ignoreversion; Components: fan\app

[Icons]
Name: "{autoprograms}\{#Motion}"; Filename: "{app}\{#Motion}\{#Motion}.exe"; Components: motion\app
Name: "{autoprograms}\{#Fan}";    Filename: "{app}\{#Fan}\{#Fan}.exe";       Components: fan\app

[UninstallDelete]
; Remove the bundle folders themselves, which Inno leaves behind once emptied.
Type: filesandordirs; Name: "{commoncf64}\VST3\{#Motion}.vst3"
Type: filesandordirs; Name: "{commoncf64}\VST3\{#Fan}.vst3"

[Messages]
ru.WelcomeLabel2=Будет установлен [name/ver] — два плагина студии ZS Records: ZS-motion (глубокий модуляционный движок) и ZS-MOTION-FAN (кольцевая модуляция, десять ручек). На следующем шаге можно выбрать, что именно ставить.%n%nЕсли DAW открыта, её нужно перезапустить, чтобы плагины появились в списке.
en.WelcomeLabel2=This will install [name/ver] — two ZS Records plug-ins: ZS-motion, the deep modulation engine, and ZS-MOTION-FAN, the ten-knob ring modulator. You can choose which parts to install on the next step.%n%nRestart your DAW afterwards so it picks them up.
