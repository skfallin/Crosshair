param([ValidateSet('Debug','Release')][string]$Configuration = 'Release', [switch]$EngineOnly)
$ErrorActionPreference = 'Stop'
# Supply a case-insensitively unique environment to MSBuild. Some automation
# hosts inherit both PATH and Path, which its .NET Framework launcher rejects.
function Invoke-BuildTool([string]$Executable, [string[]]$Arguments) {
    $start = [Diagnostics.ProcessStartInfo]::new($Executable)
    $start.UseShellExecute = $false
    $start.Environment.Clear()
    $unique = [Collections.Generic.Dictionary[string,string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($entry in [Environment]::GetEnvironmentVariables().GetEnumerator()) { $unique[$entry.Key] = $entry.Value }
    foreach ($entry in $unique.GetEnumerator()) { $start.Environment[$entry.Key] = $entry.Value }
    foreach ($argument in $Arguments) { $start.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::Start($start)
    $process.WaitForExit()
    if ($process.ExitCode) { throw "$Executable ha restituito $($process.ExitCode)." }
}
$repo = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path -LiteralPath $vswhere)) { throw 'Installa Visual Studio 2022 Build Tools con C++ e Windows SDK; vedi docs/BUILD.md.' }
$installation = & $vswhere -latest -version '[17.14,18.0)' -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$installation) { throw 'MSVC v143 (VS 2022 17.14) non trovato.' }
$cmake = Join-Path $installation 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctest = Join-Path (Split-Path $cmake) 'ctest.exe'
if (!(Test-Path -LiteralPath $cmake)) { throw 'Manca il componente CMake di Visual Studio.' }
$sdk = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\Include\10.0.26100.0'
if (!(Test-Path -LiteralPath $sdk)) { throw 'Manca Windows SDK 10.0.26100.0.' }
$build = Join-Path $repo 'out\build'
Invoke-BuildTool $cmake @('-S',$repo,'-B',$build,'-G','Visual Studio 17 2022','-A','x64','-T','v143,version=14.44','-DCMAKE_SYSTEM_VERSION=10.0.26100.0')
Invoke-BuildTool $cmake @('--build',$build,'--config',$Configuration,'--parallel')
Invoke-BuildTool $ctest @('--test-dir',$build,'-C',$Configuration,'--output-on-failure')
Write-Host "Output: $build\$Configuration"
if (!$EngineOnly) {
    & (Join-Path $PSScriptRoot 'build-settings.ps1') -Configuration $Configuration
    $desktop = Join-Path $repo "out\settings\$Configuration"
    Copy-Item (Join-Path $build "$Configuration\CrosshairNative.Engine.exe"),(Join-Path $build "$Configuration\CrosshairNative.TestTarget.exe") -Destination $desktop
    Copy-Item (Join-Path $build 'catalog') -Destination $desktop -Recurse -Force
    Write-Host "Applicazione completa dell'incremento: $desktop\CrosshairNative.Settings.exe"
}
