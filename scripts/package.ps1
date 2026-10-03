param([string]$ApplicationDirectory = 'out/settings/Release')
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$source = [IO.Path]::GetFullPath((Join-Path $repo $ApplicationDirectory))
foreach ($required in @('CrosshairNative.Settings.exe','CrosshairNative.Engine.exe','CrosshairNative.TestTarget.exe','catalog/index.json')) {
    if (!(Test-Path -LiteralPath (Join-Path $source $required))) { throw "File richiesto mancante: $required" }
}
# A new staging directory also prevents obsolete DLLs from entering a new package.
$output = Join-Path $repo ('out/package/' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$stage = Join-Path $output 'app'
New-Item -ItemType Directory -Path $stage -Force | Out-Null
foreach ($file in Get-ChildItem -LiteralPath $source -File -Recurse) {
    if ($file.Extension -in '.pdb','.ilk','.lib','.exp' -or $file.Name -like '*WebView2*') { continue }
    $relative = [IO.Path]::GetRelativePath($source,$file.FullName)
    if ($relative -eq 'catalog\comparison.png') { continue }
    $destination = Join-Path $stage $relative
    New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
    Copy-Item -LiteralPath $file.FullName -Destination $destination
}
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs = & $vswhere -latest -version '[17.14,18.0)' -products '*' -property installationPath
$crt = Join-Path $vs 'VC/Redist/MSVC/14.44.35112/x64/Microsoft.VC143.CRT'
if (!(Test-Path -LiteralPath $crt)) { throw 'MSVC CRT 14.44.35112 non trovato.' }
Copy-Item -Path (Join-Path $crt '*.dll') -Destination $stage
$notices = Join-Path $stage 'licenses'
New-Item -ItemType Directory -Path $notices | Out-Null
$lock = Get-Content (Join-Path $repo 'apps/settings/packages.lock.json') -Raw | ConvertFrom-Json
foreach ($package in $lock.dependencies.'native,Version=v0.0'.PSObject.Properties) {
    $packageRoot = Join-Path $env:USERPROFILE ('.nuget/packages/' + $package.Name.ToLowerInvariant() + '/' + $package.Value.resolved)
    foreach ($file in Get-ChildItem -LiteralPath $packageRoot -File | Where-Object Name -Match '(license|notice)') {
        Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $notices ($package.Name + '-' + $file.Name))
    }
}
Copy-Item -LiteralPath (Join-Path $repo 'docs/DEPENDENCIES.md') -Destination $notices
Copy-Item -LiteralPath (Join-Path $repo 'docs/CATALOG_PROVENANCE.md') -Destination $notices
Copy-Item -LiteralPath (Join-Path $repo 'apps/settings/packages.lock.json') -Destination $notices
Copy-Item -LiteralPath (Join-Path $repo 'LICENSE'),(Join-Path $repo 'NOTICE') -Destination $notices
Set-Content -LiteralPath (Join-Path $notices 'LOCAL-DEVELOPMENT.txt') -Value 'Build di sviluppo non firmata. Codice originale e catalogo: PolyForm Noncommercial 1.0.0, vedere LICENSE e NOTICE. Le licenze Microsoft incluse restano applicabili ai componenti Microsoft. I controlli per una release pubblica restano in docs/RELEASE_CHECKLIST.md.' -Encoding utf8

$msi = Join-Path $output 'CrosshairNative-0.1.0-dev-x64.msi'
$installer = New-Object -ComObject WindowsInstaller.Installer
$db = $installer.OpenDatabase($msi,3)
function Invoke-Sql([string]$sql) {
    $view = $db.OpenView($sql)
    try { $view.Execute() } finally { $view.Close(); [Runtime.InteropServices.Marshal]::FinalReleaseComObject($view) | Out-Null }
}
function Add-Row([string]$table,[string[]]$columns,[object[]]$values) {
    $names = ($columns | ForEach-Object { '`' + $_ + '`' }) -join ','
    $parameters = (@('?') * $values.Count) -join ','
    $view = $db.OpenView(('INSERT INTO `{0}` ({1}) VALUES ({2})' -f $table,$names,$parameters))
    $record = $installer.CreateRecord($values.Count)
    try {
        for ($i=0; $i -lt $values.Count; $i++) {
            if ($null -eq $values[$i]) { continue }
            $member = if ($values[$i] -is [int]) { 'IntegerData' } else { 'StringData' }
            $record.GetType().InvokeMember($member,'SetProperty',$null,$record,@(($i+1),$values[$i])) | Out-Null
        }
        $view.Execute($record)
    } finally { $view.Close(); [Runtime.InteropServices.Marshal]::FinalReleaseComObject($view) | Out-Null; [Runtime.InteropServices.Marshal]::FinalReleaseComObject($record) | Out-Null }
}
function Stable-Guid([string]$value) {
    $bytes = [Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes('CrosshairNative.dev.x64/' + $value.ToLowerInvariant()))
    return '{' + ([Guid]::new([byte[]]$bytes[0..15])).ToString().ToUpperInvariant() + '}'
}
Invoke-Sql 'CREATE TABLE `Property` (`Property` CHAR(72) NOT NULL, `Value` CHAR(0) NOT NULL PRIMARY KEY `Property`)'
Invoke-Sql 'CREATE TABLE `Directory` (`Directory` CHAR(72) NOT NULL, `Directory_Parent` CHAR(72), `DefaultDir` CHAR(255) NOT NULL PRIMARY KEY `Directory`)'
Invoke-Sql 'CREATE TABLE `Component` (`Component` CHAR(72) NOT NULL, `ComponentId` CHAR(38), `Directory_` CHAR(72) NOT NULL, `Attributes` SHORT NOT NULL, `Condition` CHAR(255), `KeyPath` CHAR(72) PRIMARY KEY `Component`)'
Invoke-Sql 'CREATE TABLE `File` (`File` CHAR(72) NOT NULL, `Component_` CHAR(72) NOT NULL, `FileName` CHAR(255) NOT NULL, `FileSize` LONG NOT NULL, `Version` CHAR(72), `Language` CHAR(20), `Attributes` SHORT, `Sequence` SHORT NOT NULL PRIMARY KEY `File`)'
Invoke-Sql 'CREATE TABLE `Feature` (`Feature` CHAR(38) NOT NULL, `Feature_Parent` CHAR(38), `Title` CHAR(64), `Description` CHAR(255), `Display` SHORT, `Level` SHORT NOT NULL, `Directory_` CHAR(72), `Attributes` SHORT NOT NULL PRIMARY KEY `Feature`)'
Invoke-Sql 'CREATE TABLE `FeatureComponents` (`Feature_` CHAR(38) NOT NULL, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Feature_`,`Component_`)'
Invoke-Sql 'CREATE TABLE `Media` (`DiskId` SHORT NOT NULL, `LastSequence` SHORT NOT NULL, `DiskPrompt` CHAR(64), `Cabinet` CHAR(255), `VolumeLabel` CHAR(32), `Source` CHAR(72) PRIMARY KEY `DiskId`)'
Invoke-Sql 'CREATE TABLE `Shortcut` (`Shortcut` CHAR(72) NOT NULL, `Directory_` CHAR(72) NOT NULL, `Name` CHAR(128) NOT NULL, `Component_` CHAR(72) NOT NULL, `Target` CHAR(255) NOT NULL, `Arguments` CHAR(255), `Description` CHAR(255), `Hotkey` SHORT, `Icon_` CHAR(72), `IconIndex` SHORT, `ShowCmd` SHORT, `WkDir` CHAR(72) PRIMARY KEY `Shortcut`)'
Invoke-Sql 'CREATE TABLE `Registry` (`Registry` CHAR(72) NOT NULL, `Root` SHORT NOT NULL, `Key` CHAR(255) NOT NULL, `Name` CHAR(255), `Value` CHAR(0), `Component_` CHAR(72) NOT NULL PRIMARY KEY `Registry`)'
Invoke-Sql 'CREATE TABLE `LaunchCondition` (`Condition` CHAR(255) NOT NULL, `Description` CHAR(255) NOT NULL PRIMARY KEY `Condition`)'
Invoke-Sql 'CREATE TABLE `RegLocator` (`Signature_` CHAR(72) NOT NULL, `Root` SHORT NOT NULL, `Key` CHAR(255) NOT NULL, `Name` CHAR(255), `Type` SHORT PRIMARY KEY `Signature_`)'
Invoke-Sql 'CREATE TABLE `Signature` (`Signature` CHAR(72) NOT NULL, `FileName` CHAR(255) NOT NULL, `MinVersion` CHAR(20), `MaxVersion` CHAR(20), `MinSize` LONG, `MaxSize` LONG, `MinDate` LONG, `MaxDate` LONG, `Languages` CHAR(255) PRIMARY KEY `Signature`)'
Invoke-Sql 'CREATE TABLE `AppSearch` (`Property` CHAR(72) NOT NULL, `Signature_` CHAR(72) NOT NULL PRIMARY KEY `Property`,`Signature_`)'
foreach ($table in @('InstallExecuteSequence','InstallUISequence')) {
    Invoke-Sql ('CREATE TABLE `{0}` (`Action` CHAR(72) NOT NULL, `Condition` CHAR(255), `Sequence` SHORT PRIMARY KEY `Action`)' -f $table)
}
$productCode = Stable-Guid 'Product.0.1.0'
$properties = @{ProductCode=$productCode;ProductName='Crosshair Native (sviluppo locale)';ProductVersion='0.1.0';ProductLanguage='1040';Manufacturer='Progetto locale - titolare da definire';UpgradeCode=(Stable-Guid 'Upgrade');INSTALLLEVEL='1';ARPNOMODIFY='1';ARPNOREPAIR='1';MSIDISABLERMRESTART='1'}
foreach ($entry in $properties.GetEnumerator()) { Add-Row Property @('Property','Value') @($entry.Key,$entry.Value) }
Add-Row LaunchCondition @('Condition','Description') @('NOT ALLUSERS','Installazione solo per utente: non impostare ALLUSERS.')
Add-Row RegLocator @('Signature_','Root','Key','Name','Type') @('WindowsBuild',2,'SOFTWARE\Microsoft\Windows NT\CurrentVersion','CurrentBuildNumber',18)
Add-Row AppSearch @('Property','Signature_') @('OSBUILD','WindowsBuild')
Add-Row LaunchCondition @('Condition','Description') @('Installed OR (VersionNT64 AND OSBUILD >= 22000)','Richiesto Windows 11 x64, build 22000 o successiva.')
Add-Row Directory @('Directory','Directory_Parent','DefaultDir') @('TARGETDIR',$null,'SourceDir')
Add-Row Directory @('Directory','Directory_Parent','DefaultDir') @('LocalAppDataFolder','TARGETDIR','.')
Add-Row Directory @('Directory','Directory_Parent','DefaultDir') @('Programs','LocalAppDataFolder','Programs')
Add-Row Directory @('Directory','Directory_Parent','DefaultDir') @('INSTALLDIR','Programs','CrosshairNative')
Add-Row Directory @('Directory','Directory_Parent','DefaultDir') @('ProgramMenuFolder','TARGETDIR','.')
Add-Row Feature @('Feature','Title','Level','Directory_','Attributes') @('Main','Crosshair Native',1,'INSTALLDIR',0)
$directories = @{''='INSTALLDIR'}
$directoryIndex = 0
foreach ($directory in Get-ChildItem -LiteralPath $stage -Directory -Recurse | Sort-Object { $_.FullName.Length }) {
    $relative = [IO.Path]::GetRelativePath($stage,$directory.FullName)
    $parent = [IO.Path]::GetDirectoryName($relative)
    $id = 'D' + (++$directoryIndex)
    $directories[$relative] = $id
    Add-Row Directory @('Directory','Directory_Parent','DefaultDir') @($id,$directories[$parent],$directory.Name)
}
$ddf = [Collections.Generic.List[string]]::new()
$ddf.Add('.OPTION EXPLICIT'); $ddf.Add('.Set CabinetNameTemplate=payload.cab'); $ddf.Add('.Set DiskDirectoryTemplate="' + $output + '"')
$ddf.Add('.Set CompressionType=MSZIP'); $ddf.Add('.Set Cabinet=on'); $ddf.Add('.Set Compress=on'); $ddf.Add('.Set MaxDiskSize=0')
$ddf.Add('.Set RptFileName="' + (Join-Path $output 'payload.rpt') + '"'); $ddf.Add('.Set InfFileName="' + (Join-Path $output 'payload.inf') + '"')
$sequence=0; $inventory=@(); $settingsComponent=''
foreach ($file in Get-ChildItem -LiteralPath $stage -File -Recurse | Sort-Object FullName) {
    $relative=[IO.Path]::GetRelativePath($stage,$file.FullName); $id='F' + (++$sequence); $component='C'+$sequence
    $directory=$directories[[IO.Path]::GetDirectoryName($relative)]
    Add-Row Component @('Component','ComponentId','Directory_','Attributes','KeyPath') @($component,(Stable-Guid $relative),$directory,256,$id)
    Add-Row File @('File','Component_','FileName','FileSize','Attributes','Sequence') @($id,$component,$file.Name,[int]$file.Length,16384,$sequence)
    Add-Row FeatureComponents @('Feature_','Component_') @('Main',$component)
    $ddf.Add('"'+$file.FullName+'" '+$id)
    $inventory += [ordered]@{path=$relative;bytes=$file.Length;sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash}
    if ($relative -eq 'CrosshairNative.Settings.exe') { $settingsComponent=$component; $settingsFile=$id }
}
# Empty Run value means disabled. The owned value is removed on uninstall, including after the app enables it.
Add-Row Component @('Component','ComponentId','Directory_','Attributes','KeyPath') @('Startup',(Stable-Guid 'Startup'),'INSTALLDIR',388,'StartupValue')
Add-Row Registry @('Registry','Root','Key','Name','Value','Component_') @('StartupValue',1,'Software\Microsoft\Windows\CurrentVersion\Run','CrosshairNative',$null,'Startup')
Add-Row FeatureComponents @('Feature_','Component_') @('Main','Startup')
Add-Row Shortcut @('Shortcut','Directory_','Name','Component_','Target','ShowCmd','WkDir') @('Settings','ProgramMenuFolder','Crosshair Native',$settingsComponent,('[#'+$settingsFile+']'),1,'INSTALLDIR')
Add-Row Media @('DiskId','LastSequence','Cabinet') @(1,$sequence,'#payload.cab')
$actions = [ordered]@{LaunchConditions=100;ValidateProductID=700;CostInitialize=800;FileCost=900;CostFinalize=1000;InstallValidate=1400;InstallInitialize=1500;ProcessComponents=1600;UnpublishFeatures=1800;RemoveRegistryValues=2600;RemoveShortcuts=3200;RemoveFiles=3500;InstallFiles=4000;CreateShortcuts=4500;WriteRegistryValues=5000;RegisterUser=6000;RegisterProduct=6100;PublishFeatures=6300;PublishProduct=6400;InstallFinalize=6600}
foreach ($entry in $actions.GetEnumerator()) { Add-Row InstallExecuteSequence @('Action','Sequence') @($entry.Key,[int]$entry.Value) }
foreach ($name in @('LaunchConditions','CostInitialize','FileCost','CostFinalize')) { Add-Row InstallUISequence @('Action','Sequence') @($name,[int]$actions[$name]) }
Add-Row InstallUISequence @('Action','Sequence') @('AppSearch',50)
Add-Row InstallExecuteSequence @('Action','Sequence') @('AppSearch',50)
Add-Row InstallUISequence @('Action','Sequence') @('ExecuteAction',1300)
$directive = Join-Path $output 'payload.ddf'; $ddf | Set-Content -LiteralPath $directive -Encoding ascii
& "$env:SystemRoot/System32/makecab.exe" /F $directive | Out-File (Join-Path $output 'makecab.log')
if ($LASTEXITCODE) { throw "makecab: $LASTEXITCODE" }
$view=$db.OpenView('INSERT INTO `_Streams` (`Name`,`Data`) VALUES (''payload.cab'',?)')
$record=$installer.CreateRecord(1); $record.SetStream(1,(Join-Path $output 'payload.cab')); $view.Execute($record); $view.Close()
$summary=$db.SummaryInformation(20)
foreach ($entry in @{1=1252;2='Installation Database';3='Crosshair Native sviluppo locale';7='x64;1040';9=('{'+[Guid]::NewGuid().ToString().ToUpperInvariant()+'}');14=500;15=10;18='Crosshair Native package.ps1';19=2}.GetEnumerator()) {
    $summary.GetType().InvokeMember('Property','SetProperty',$null,$summary,@([int]$entry.Key,$entry.Value)) | Out-Null
}
$summary.Persist(); $db.Commit()
foreach ($object in @($view,$record,$summary,$db,$installer)) { [Runtime.InteropServices.Marshal]::FinalReleaseComObject($object) | Out-Null }
$inventory | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $output 'inventory.json') -Encoding utf8
[ordered]@{msi=$msi;productCode=$productCode;files=$sequence;payloadBytes=($inventory.bytes | Measure-Object -Sum).Sum;msiBytes=(Get-Item -LiteralPath $msi).Length;sha256=(Get-FileHash -LiteralPath $msi).Hash} | ConvertTo-Json | Tee-Object -FilePath (Join-Path $output 'package.json')
