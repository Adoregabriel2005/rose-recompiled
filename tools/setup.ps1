$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/native.ps1"
$root = Split-Path $PSScriptRoot -Parent
$pin = (Get-Content "$root/engine.lock" -Raw).Trim()
if (!(Test-Path "$root/external/nesrecomp/.git")) {
    Invoke-HiddenNative -File (Get-Command git).Source -Directory $root -Arguments @('clone','https://github.com/mstan/nesrecomp.git','external/nesrecomp')
}
Invoke-HiddenNative -File (Get-Command git).Source -Directory $root -Arguments @('-C','external/nesrecomp','checkout','--detach',$pin)
$notes = Get-Item -Force "$root/assets/system/.cache/.notes.txt"
$notes.Attributes = $notes.Attributes -bor [IO.FileAttributes]::Hidden
