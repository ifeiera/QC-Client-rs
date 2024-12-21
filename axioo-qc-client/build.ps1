# Build and packaging script for Axioo QC System

# Application configuration
$APP_NAME = "Axioo QC System"
$VERSION = "1.0.0"

# Configure static linking for release build
$env:RUSTFLAGS = "-C target-feature=+crt-static"

Write-Host "Building release version..."
cargo build --release

# Define output directory structure
$RELEASE_DIR = "release"
$PORTABLE_DIR = "$RELEASE_DIR\portable\$APP_NAME"
$PORTABLE_UPX_DIR = "$RELEASE_DIR\portable_upx\$APP_NAME"
$INSTALLER_DIR = "$RELEASE_DIR\installer"

# Create output directories
New-Item -ItemType Directory -Force -Path $PORTABLE_DIR
New-Item -ItemType Directory -Force -Path $PORTABLE_UPX_DIR
New-Item -ItemType Directory -Force -Path $INSTALLER_DIR

# Copy release files to output directories
Copy-Item "target\release\axioo-qc-client.exe" -Destination "$PORTABLE_DIR\$APP_NAME.exe"
Copy-Item "target\release\axioo-qc-client.exe" -Destination "$PORTABLE_UPX_DIR\$APP_NAME.exe"

# Copy required DLLs and dependencies
Copy-Item "dll\*" -Destination $PORTABLE_DIR -Recurse
Copy-Item "dll\*" -Destination $PORTABLE_UPX_DIR -Recurse

# Create UPX compressed version if UPX is available
$UPX_PATH = "tools\upx.exe"
if (Test-Path $UPX_PATH) {
    Write-Host "Compressing with UPX..."
    & $UPX_PATH --best --lzma "$PORTABLE_UPX_DIR\$APP_NAME.exe"
    
    # Package UPX compressed version
    Compress-Archive -Path "$PORTABLE_UPX_DIR\*" -DestinationPath "$RELEASE_DIR\$APP_NAME-$VERSION-portable-upx.zip" -Force
    Write-Host "UPX compressed version created"
} else {
    Write-Host "UPX not found in tools directory. Skipping UPX compression."
}

# Create standard portable package
Compress-Archive -Path "$PORTABLE_DIR\*" -DestinationPath "$RELEASE_DIR\$APP_NAME-$VERSION-portable.zip" -Force

# Generate Inno Setup installer script
$INNO_SCRIPT = @"
#define MyAppName "$APP_NAME"
#define MyAppVersion "$VERSION"
#define MyAppPublisher "Axioo"
#define MyAppExeName "$APP_NAME.exe"

[Setup]
AppId={{B89F4A71-0C36-4F8A-9B48-E56F6F79C67A}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
OutputDir=$INSTALLER_DIR
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
Source: "$PORTABLE_DIR\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon
Name: "{userstartup}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: startupicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
"@

# Save installer script
$INNO_SCRIPT | Out-File -FilePath "installer.iss" -Encoding UTF8

# Build installer if Inno Setup is available
$INNO_COMPILER = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
if (Test-Path $INNO_COMPILER) {
    Write-Host "Building installer..."
    & $INNO_COMPILER "installer.iss"
} else {
    Write-Host "Inno Setup not found. Please install it to build the installer."
}

# Display build results
Write-Host "`nBuild complete!"
Write-Host "Regular portable version: $RELEASE_DIR\$APP_NAME-$VERSION-portable.zip"
if (Test-Path $UPX_PATH) {
    Write-Host "UPX compressed version: $RELEASE_DIR\$APP_NAME-$VERSION-portable-upx.zip"
}
if (Test-Path $INNO_COMPILER) {
    Write-Host "Installer: $INSTALLER_DIR\$APP_NAME-$VERSION-setup.exe"
} 