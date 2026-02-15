param(
  [string]$PyVersion = "3.12.8"
)

$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$Dest = Join-Path $Root "third_party\python-win"
$PyExe = Join-Path $Dest "python.exe"

if (Test-Path $PyExe) {
  Write-Host "Portable Python already present: $PyExe"
  exit 0
}

New-Item -ItemType Directory -Force -Path $Dest | Out-Null

$ZipName = "python-$PyVersion-embed-amd64.zip"
$Url = "https://www.python.org/ftp/python/$PyVersion/$ZipName"
$ZipPath = Join-Path $env:TEMP $ZipName

Write-Host "Downloading portable Python $PyVersion..."
Invoke-WebRequest -Uri $Url -OutFile $ZipPath

Write-Host "Extracting to $Dest..."
Expand-Archive -Path $ZipPath -DestinationPath $Dest -Force

Remove-Item $ZipPath -Force

if (!(Test-Path $PyExe)) {
  throw "Portable python.exe not found after extraction."
}

Write-Host "Portable Python ready: $PyExe"
