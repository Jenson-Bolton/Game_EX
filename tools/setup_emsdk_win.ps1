param(
  [string]$Version = "latest"
)

$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

# Ensure portable Python exists
$PyExe = Join-Path $Root "third_party\python-win\python.exe"
if (!(Test-Path $PyExe)) {
  & (Join-Path $Root "tools\setup_python_win.ps1")
}

$EmsdkDir = Join-Path $Root "third_party\emsdk"
$EmsdkPy  = Join-Path $EmsdkDir "emsdk.py"

if (!(Test-Path $EmsdkPy)) {
  throw "emsdk.py not found at $EmsdkPy. Did you init submodules?"
}

Push-Location $EmsdkDir

# Install + activate using portable Python
& $PyExe $EmsdkPy install $Version
& $PyExe $EmsdkPy activate $Version

# Point EMSDK to this repo location for this session
$env:EMSDK = $EmsdkDir

# Load emsdk environment variables into this PowerShell session
cmd /c "`"$($EmsdkDir)\emsdk_env.bat`" >nul && set" |
  ForEach-Object {
    if ($_ -match "^(.*?)=(.*)$") {
      [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
    }
  }

Write-Host "EMSDK ready at $env:EMSDK"
Pop-Location
