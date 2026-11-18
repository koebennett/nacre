# Installs Nacre.vst3 into the standard Windows VST3 folder, where FL Studio looks by default.
# Right-click > "Run with PowerShell" (it asks for administrator rights).
$ErrorActionPreference = "Stop"
if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Start-Process powershell "-ExecutionPolicy Bypass -File `"$PSCommandPath`"" -Verb RunAs
    exit
}
$src = Join-Path $PSScriptRoot "Nacre.vst3"
$dll = Join-Path $src "Contents\x86_64-win\Nacre.vst3"
if (-not (Test-Path $dll)) { Write-Host "Nacre.vst3 must sit next to this script, with Contents\x86_64-win\Nacre.vst3 inside it." -ForegroundColor Red; Read-Host "Press Enter"; exit 1 }
$dst = Join-Path $env:CommonProgramFiles "VST3"
New-Item -ItemType Directory -Force -Path $dst | Out-Null
Remove-Item -Recurse -Force (Join-Path $dst "Nacre.vst3") -ErrorAction SilentlyContinue
Copy-Item -Recurse -Force $src $dst
Get-ChildItem -Recurse (Join-Path $dst "Nacre.vst3") | Unblock-File      # removes the "downloaded from the internet" block
Write-Host "Installed to $dst\Nacre.vst3"
Write-Host "In FL Studio: Options > Manage plugins > Find installed plugins (tick 'Verify plugins' if it does not appear)."
Write-Host "If Nacre still does not load, send me the file $env:TEMP\Nacre-log.txt"
Read-Host "Press Enter to close"
