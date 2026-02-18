# install-openocd.ps1
# PowerShell 5.1+ (Windows 10/11). Запускати з кореня проекту.

param(
  [string]$Version = "0.12.0-7"
)

$ZipName    = "xpack-openocd-$Version-win32-x64.zip"
$StartUrl   = "https://github.com/xpack-dev-tools/openocd-xpack/releases/download/v$Version/$ZipName"

$ProjectRoot  = (Get-Location).Path
$PackagesDir  = Join-Path $ProjectRoot "packages"
$DownloadPath = Join-Path $ProjectRoot $ZipName
$ExtractDir   = Join-Path $PackagesDir "xpack-openocd-$Version"

$OpenOcdExe   = Join-Path $ExtractDir "bin\openocd.exe"
$ScriptsDir   = Join-Path $ExtractDir "openocd\scripts"

function Write-Utf8NoBomFile([string]$Path, [string]$Text) {
  $utf8NoBom = New-Object System.Text.UTF8Encoding($false) # false => без BOM
  [System.IO.File]::WriteAllText($Path, $Text, $utf8NoBom)
}

function Ensure-PackageJson([string]$Dir, [string]$Ver) {
  $manifestPath = Join-Path $Dir "package.json"

  $manifest = [ordered]@{
    name        = "tool-openocd"
    version     = $Ver
    description = "xPack OpenOCD local wrapper for PlatformIO"
    keywords    = @("openocd","debug","stm32","stlink")
    system      = @("windows_amd64")
  }

  $json = ($manifest | ConvertTo-Json -Depth 10)

  # Завжди перезаписуємо без BOM (так надійніше)
  Write-Utf8NoBomFile -Path $manifestPath -Text $json
}

Write-Host "Creating packages directory..."
New-Item -ItemType Directory -Force -Path $PackagesDir | Out-Null

# Якщо вже встановлено — лише перевіряємо/виправляємо package.json (без BOM) і виходимо
if (Test-Path $OpenOcdExe) {
  if (-not (Test-Path $ScriptsDir)) {
    throw "Installed, but scripts folder not found: $ScriptsDir"
  }

  Write-Host "Already installed: $ExtractDir"

  Write-Host "Ensuring package.json (UTF-8 без BOM)..."
  Ensure-PackageJson -Dir $ExtractDir -Ver $Version

  Write-Host "OpenOCD:  $OpenOcdExe"
  Write-Host "Scripts:  $ScriptsDir"
  exit 0
}

Write-Host "Downloading OpenOCD..."
Invoke-WebRequest -Uri $StartUrl -OutFile $DownloadPath -UseBasicParsing
Write-Host "Downloaded: $DownloadPath"

Write-Host "Extracting archive..."
Expand-Archive -Force -Path $DownloadPath -DestinationPath $PackagesDir

if (-not (Test-Path $ExtractDir)) {
  throw "Extraction failed. Expected folder: $ExtractDir"
}

# Перевірки структури після розпаковки
if (-not (Test-Path $OpenOcdExe)) {
  throw "openocd.exe not found: $OpenOcdExe"
}
if (-not (Test-Path $ScriptsDir)) {
  throw "scripts folder not found: $ScriptsDir"
}

Write-Host "Creating package.json (UTF-8 без BOM)..."
Ensure-PackageJson -Dir $ExtractDir -Ver $Version

Write-Host "Cleaning up zip..."
Remove-Item -Force -ErrorAction SilentlyContinue $DownloadPath

Write-Host "`nDONE."
Write-Host "OpenOCD:  $OpenOcdExe"
Write-Host "Scripts:  $ScriptsDir"

# Для platform_packages = tool-openocd@file:///... потрібні слеші вперед
$ExtractDirUri = ($ExtractDir -replace '\\','/')

Write-Host "`nplatformio.ini variant (може спрацювати):"
Write-Host "platform_packages ="
Write-Host "    tool-openocd@file:///$ExtractDirUri"

Write-Host "`nНайнадійніше (через custom + -s scripts):"
Write-Host @"
upload_protocol = custom
upload_command =
  $OpenOcdExe
  -s $ScriptsDir
  -f interface/stlink.cfg
  -f target/stm32c0x.cfg
  -c "program `$SOURCE verify reset exit"
"@
