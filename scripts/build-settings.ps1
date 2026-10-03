param([ValidateSet('Debug','Release')][string]$Configuration = 'Release', [switch]$Restore, [string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -version '[17.14,18.0)' -products '*' -property installationPath
if (!$installation) { throw 'Visual Studio Build Tools 2022 non trovato.' }
$start = [Diagnostics.ProcessStartInfo]::new((Join-Path $installation 'MSBuild\Current\Bin\MSBuild.exe'))
$start.UseShellExecute = $false
$start.Environment.Clear()
$unique = [Collections.Generic.Dictionary[string,string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach ($entry in [Environment]::GetEnvironmentVariables().GetEnumerator()) { $unique[$entry.Key] = $entry.Value }
foreach ($entry in $unique.GetEnumerator()) { $start.Environment[$entry.Key] = $entry.Value }
$arguments = @((Join-Path $repo 'apps\settings\CrosshairNative.Settings.vcxproj'),'/m',"/p:Configuration=$Configuration",'/p:Platform=x64','/verbosity:minimal')
if ($OutputDirectory) { $arguments += "/p:OutDir=$([IO.Path]::GetFullPath($OutputDirectory))\" }
if ($Restore) { $arguments += '/t:Restore'; $arguments += "/p:RestoreConfigFile=$(Join-Path $repo 'NuGet.Config')"; $arguments += '/p:RestoreLockedMode=true' } else { $arguments += '/t:Build' }
foreach ($argument in $arguments) { $start.ArgumentList.Add($argument) }
$process = [Diagnostics.Process]::Start($start)
$process.WaitForExit()
if ($process.ExitCode) { throw "Build Settings fallita: $($process.ExitCode)." }
