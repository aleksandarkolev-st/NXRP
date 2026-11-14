param([string]$Name = 'TheHedge-Windows-x64')
$ErrorActionPreference = 'Stop'
if ($Name -notmatch '^[A-Za-z0-9_-]+$') { throw 'Use a plain release name.' }
$repo = Split-Path $PSScriptRoot -Parent
$binary = Join-Path $repo 'build/bin/Release/Maze.Graphics.exe'
if (!(Test-Path -LiteralPath $binary)) { throw 'Build Release/x64 first.' }
$destination = Join-Path $repo ('build/packages/' + $Name)
if (Test-Path -LiteralPath $destination) { throw 'Package directory exists. Choose a new name to preserve it.' }
New-Item -ItemType Directory -Path $destination | Out-Null
Copy-Item -LiteralPath $binary -Destination $destination
Copy-Item -LiteralPath (Join-Path $repo 'assets') -Destination $destination -Recurse
Copy-Item -LiteralPath (Join-Path $repo 'README.md') -Destination $destination
Copy-Item -LiteralPath (Join-Path $repo 'docs/PLAY.md') -Destination $destination
$sourceCommit = git -C $repo rev-parse HEAD
$sourceChanges = git -C $repo status --porcelain
$sourceState = if ($sourceChanges) { 'Includes uncommitted working-tree changes.' } else { 'Clean source checkout.' }
"Source base commit: $sourceCommit`n$sourceState`nBuilt for Windows x64; co-op protocol 15." |
    Set-Content -LiteralPath (Join-Path $destination 'VERSION.txt') -Encoding ascii
$notices = Join-Path $destination 'licenses'
New-Item -ItemType Directory -Path $notices | Out-Null
$licenseFiles = @{
    'SFML.md' = 'build/sfml-source/SFML-3.1.0/license.md'
    'HarfBuzz.txt' = 'build/dependencies/_deps/harfbuzz-src/COPYING'
    'FreeType.txt' = 'build/dependencies/_deps/freetype-src/docs/FTL.TXT'
    'SheenBidi.txt' = 'build/dependencies/_deps/sheenbidi-src/LICENSE'
}
foreach ($entry in $licenseFiles.GetEnumerator()) {
    Copy-Item -LiteralPath (Join-Path $repo $entry.Value) -Destination (Join-Path $notices $entry.Key)
}
'This software uses the FreeType Project (https://freetype.org/), portions copyright The FreeType Project. All rights reserved. See FreeType.txt.' |
    Set-Content -LiteralPath (Join-Path $notices 'NOTICE.txt') -Encoding utf8
# App-local MSVC runtime from the installed redistributable payload.
$redistRoot = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/2022/BuildTools/VC/Redist/MSVC'
$runtime = Get-ChildItem -LiteralPath $redistRoot -Directory | Where-Object Name -Match '^14\.' |
    Sort-Object Name -Descending | ForEach-Object { Join-Path $_.FullName 'x64/Microsoft.VC143.CRT' } |
    Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (!$runtime) { throw 'MSVC x64 redistributable payload not found.' }
Get-ChildItem -LiteralPath $runtime -Filter '*.dll' | Copy-Item -Destination $destination
@'
@echo off
cd /d "%~dp0"
start "" "Maze.Graphics.exe"
'@ | Set-Content -LiteralPath (Join-Path $destination 'Play.cmd') -Encoding ascii
$manifest = Get-ChildItem -LiteralPath $destination -File -Recurse | ForEach-Object {
    [pscustomobject]@{ Path = $_.FullName.Substring($destination.Length + 1); SHA256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
}
$manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $destination 'manifest.json') -Encoding utf8
$zip = $destination + '.zip'
Compress-Archive -LiteralPath $destination -DestinationPath $zip
Get-Item -LiteralPath $zip | Select-Object FullName, Length
Get-FileHash -LiteralPath $zip -Algorithm SHA256
