# Run the game as a first-time player with nothing installed, to exercise the
# intake screen.
#
# Two things hide that screen on a development machine, and both have to be
# dealt with or you get a game instead of a prompt:
#
#   - `%LOCALAPPDATA%\Raingrasp\` already holds unpacked trees from earlier
#     runs, so intake resolves one and never asks. This redirects LOCALAPPDATA
#     at a scratch directory for the child process only -- your real trees are
#     neither read nor touched.
#   - intake also scans the executable's own directory for *.jar, so the exe is
#     copied somewhere empty rather than run from bazel-bin.
#
# Drag a .jar onto the window to leave the screen. Everything lands under the
# scratch directory, so deleting it puts you back to a clean first run.

[CmdletBinding()]
param(
    # Where the pretend install lives. Wiped on each run unless -Keep is given.
    [string] $Root = (Join-Path $env:TEMP 'rg-nojar'),

    # Drop this JAR in automatically instead of waiting for a drag. Useful for
    # checking that the screen hands off to the game.
    [string] $Jar,

    # Reuse whatever the last run unpacked, rather than starting empty. With
    # this the intake screen is *skipped* -- that is the point: it is how you
    # check the "already installed" path without disturbing your real data.
    [switch] $Keep,

    # Build first. Off by default so a deliberate test of an older binary is
    # not silently replaced by a rebuild.
    [switch] $Build
)

$ErrorActionPreference = 'Stop'

$repo = Split-Path -Parent $PSScriptRoot
$built = Join-Path $repo 'bazel-bin\src\common\sdl'

if ($Build) {
    Write-Host 'Building //:raingrasp ...' -ForegroundColor Cyan
    & bazelisk build //:raingrasp
    if ($LASTEXITCODE -ne 0) { throw 'build failed' }
}

$exe = Join-Path $built 'raingrasp.exe'
if (-not (Test-Path $exe)) {
    throw "no binary at $exe -- run with -Build, or bazelisk build //:raingrasp"
}

if ((Test-Path $Root) -and -not $Keep) {
    Remove-Item -Recurse -Force $Root
}
$bin = Join-Path $Root 'bin'
New-Item -ItemType Directory -Force $bin | Out-Null

# The exe plus whatever it needs beside it. Copying the DLLs by extension keeps
# this from breaking the next time the binary picks up a new runtime dependency.
Copy-Item $exe $bin -Force
Get-ChildItem $built -Filter *.dll | ForEach-Object { Copy-Item $_.FullName $bin -Force }

$data = Join-Path $Root 'Raingrasp'
$installed = @()
if (Test-Path $data) {
    $installed = @(Get-ChildItem $data -Directory -ErrorAction SilentlyContinue |
                   Select-Object -ExpandProperty Name)
}

Write-Host ''
Write-Host "sandbox : $Root"
if ($installed.Count -gt 0) {
    Write-Host "data    : $($installed -join ', ') already unpacked -- intake screen will be SKIPPED" -ForegroundColor Yellow
} else {
    Write-Host 'data    : none -- the intake screen should appear' -ForegroundColor Green
}

$args = @()
if ($Jar) {
    $jarPath = (Resolve-Path $Jar).Path
    Write-Host "jar     : $jarPath (passed in; the screen will be skipped)" -ForegroundColor Yellow
    $args += $jarPath
} else {
    Write-Host 'jar     : none -- drag one onto the window'
}
Write-Host ''

# Scoped to this process only. Set on the current shell it would follow every
# other program launched from here, which is exactly the kind of surprise a
# test script should not leave behind.
$previous = $env:LOCALAPPDATA
try {
    $env:LOCALAPPDATA = $Root
    & (Join-Path $bin 'raingrasp.exe') @args
    $code = $LASTEXITCODE
} finally {
    $env:LOCALAPPDATA = $previous
}

Write-Host ''
Write-Host "exit $code"
if (Test-Path $data) {
    $now = @(Get-ChildItem $data -Directory -ErrorAction SilentlyContinue |
             Select-Object -ExpandProperty Name)
    if ($now.Count -gt 0) { Write-Host "unpacked: $($now -join ', ')" }
}
Write-Host "delete $Root to start clean again"
exit $code
