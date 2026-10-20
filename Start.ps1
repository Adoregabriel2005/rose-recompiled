param([string]$Rom)
$ErrorActionPreference='Stop'
. "$PSScriptRoot/tools/native.ps1"
if (!$Rom) {
    $cache="$PSScriptRoot/build/game/CMakeCache.txt"
    if (Test-Path -LiteralPath $cache) {
        $entry=Get-Content -LiteralPath $cache | Where-Object { $_ -match '^ROSE_ROM:FILEPATH=' } | Select-Object -First 1
        if ($entry) { $Rom=$entry.Substring('ROSE_ROM:FILEPATH='.Length) }
    }
    if (!$Rom -or !(Test-Path -LiteralPath $Rom)) {
        $files=@(Get-ChildItem "$PSScriptRoot/private/roms/*/game.nes")
        if ($files.Count -ne 1) { throw 'Informe -Rom com o caminho da revisao compilada.' }
        $Rom=$files[0].FullName
    }
}
Invoke-HiddenNative -File "$PSScriptRoot/build/game/RoseRecompiled.exe" -Arguments @((Resolve-Path -LiteralPath $Rom).Path) -Directory $PSScriptRoot
