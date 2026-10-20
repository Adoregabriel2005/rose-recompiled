param(
    [Parameter(Mandatory=$true)][string]$RomPath,
    [string]$PythonExe,
    [int]$Frames=6000,
    [switch]$Verify
)
$ErrorActionPreference='Stop'
. "$PSScriptRoot/tools/native.ps1"
if (!$PythonExe) {
    $bundled=Join-Path $env:USERPROFILE '.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
    if (Test-Path $bundled) { $PythonExe=$bundled }
    else { $PythonExe=(Get-Command python.exe -ErrorAction Stop).Source }
}
$argsList=@("$PSScriptRoot/tools/build_port.py",'--rom',(Resolve-Path -LiteralPath $RomPath).Path,'--frames',"$Frames")
if ($Verify) { $argsList+=@('--oracle','--align','0','1','2','3') }
Invoke-HiddenNative -File $PythonExe -Arguments $argsList -Directory $PSScriptRoot
