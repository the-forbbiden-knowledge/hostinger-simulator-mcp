$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$repo = Split-Path -Parent $PSScriptRoot
$updater = Join-Path $repo 'tools\update.ps1'
$work = Join-Path $env:TEMP ('HostingerUpdaterTests-' + [guid]::NewGuid().ToString('N'))
$install = Join-Path $work 'install'
$env:LOCALAPPDATA = Join-Path $work 'localappdata'
New-Item -ItemType Directory -Path $install -Force | Out-Null

function Write-DummyBinary {
    param([string]$Path,[string]$Marker)
    $bytes = New-Object byte[] 16384
    $markerBytes = [Text.Encoding]::UTF8.GetBytes($Marker)
    [Array]::Copy($markerBytes,$bytes,$markerBytes.Length)
    [IO.File]::WriteAllBytes($Path,$bytes)
}
function New-TestPackage {
    param([string]$Version,[string]$Name)
    $source = Join-Path $work ($Name + '-source')
    $zip = Join-Path $work ($Name + '.zip')
    New-Item -ItemType Directory -Path (Join-Path $source 'ui') -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $source 'tools') -Force | Out-Null
    Write-DummyBinary (Join-Path $source 'simulator_desktop.exe') ("DESKTOP-" + $Version)
    Write-DummyBinary (Join-Path $source 'simulator.exe') ("CLI-" + $Version)
    Set-Content -LiteralPath (Join-Path $source 'VERSION.txt') -Value $Version -NoNewline -Encoding ascii
    Set-Content -LiteralPath (Join-Path $source 'ui\index.html') -Value ("<html>" + $Version + "</html>") -Encoding utf8
    Set-Content -LiteralPath (Join-Path $source 'ui\app.js') -Value ("// " + $Version) -Encoding utf8
    Set-Content -LiteralPath (Join-Path $source 'ui\styles.css') -Value ("/* " + $Version + " */") -Encoding utf8
    [IO.File]::WriteAllBytes((Join-Path $source 'ui\favicon.ico'),[byte[]](1,2,3,4,5,6,7,8))
    Copy-Item -LiteralPath $updater -Destination (Join-Path $source 'tools\update.ps1')
    [System.IO.Compression.ZipFile]::CreateFromDirectory($source,$zip)
    return [pscustomobject]@{Path=$zip;Sha=(Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()}
}
function Invoke-Updater {
    param([string]$Package,[string]$Digest,[string]$Version,[switch]$FailAfterFirstReplace,[switch]$FailAfterInstall)
    $exe = Join-Path $PSHOME 'powershell.exe'
    $args = @('-NoProfile','-ExecutionPolicy','Bypass','-File',$updater,'-InstallDirectory',$install,'-TestMode','-TestPackagePath',$Package,'-TestExpectedSha256',$Digest,'-TestReleaseVersion',$Version,'-AutoConfirm','-NoRestart')
    if($FailAfterFirstReplace) { $args += '-TestFailAfterFirstReplace' }
    if($FailAfterInstall) { $args += '-TestFailAfterInstall' }
    $proc = Start-Process -FilePath $exe -ArgumentList $args -Wait -PassThru -NoNewWindow
    return $proc.ExitCode
}
try {
    New-Item -ItemType Directory -Path (Join-Path $install 'ui') -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $install 'tools') -Force | Out-Null
    Write-DummyBinary (Join-Path $install 'simulator_desktop.exe') 'OLD-DESKTOP'
    Write-DummyBinary (Join-Path $install 'simulator.exe') 'OLD-CLI'
    Set-Content -LiteralPath (Join-Path $install 'VERSION.txt') -Value '0.0.0' -NoNewline -Encoding ascii
    Set-Content -LiteralPath (Join-Path $install 'ui\index.html') -Value '<html>old</html>' -Encoding utf8
    Set-Content -LiteralPath (Join-Path $install 'ui\app.js') -Value '// old' -Encoding utf8
    Set-Content -LiteralPath (Join-Path $install 'ui\styles.css') -Value '/* old */' -Encoding utf8
    [IO.File]::WriteAllBytes((Join-Path $install 'ui\favicon.ico'),[byte[]](8,7,6,5,4,3,2,1))
    Copy-Item -LiteralPath $updater -Destination (Join-Path $install 'tools\update.ps1')

    $v100 = New-TestPackage '0.1.0' 'success'
    $code = Invoke-Updater -Package $v100.Path -Digest $v100.Sha -Version '0.1.0'
    if($code -ne 0) { throw "Successful update returned exit code $code." }
    if((Get-Content (Join-Path $install 'VERSION.txt') -Raw).Trim() -ne '0.1.0') { throw 'Version did not update.' }
    if((Get-Content (Join-Path $install 'ui\index.html') -Raw) -notmatch '0.1.0') { throw 'UI files were not replaced.' }
    if(-not (Test-Path (Join-Path $env:LOCALAPPDATA 'HostingerSimulator\rollback'))) { throw 'Rollback directory was not created.' }

    $v101 = New-TestPackage '0.1.1' 'bad-digest'
    $code = Invoke-Updater -Package $v101.Path -Digest ('0' * 64) -Version '0.1.1'
    if($code -eq 0) { throw 'Wrong SHA-256 was accepted.' }
    if((Get-Content (Join-Path $install 'VERSION.txt') -Raw).Trim() -ne '0.1.0') { throw 'Wrong digest changed installed version.' }

    $traversal = Join-Path $work 'traversal.zip'
    $stream = [IO.File]::Open($traversal,[IO.FileMode]::Create)
    $zip = New-Object System.IO.Compression.ZipArchive($stream,[System.IO.Compression.ZipArchiveMode]::Create)
    try {
        $entry = $zip.CreateEntry('../escape.txt')
        $writer = New-Object IO.StreamWriter($entry.Open())
        $writer.Write('must-not-extract')
        $writer.Dispose()
    } finally { $zip.Dispose(); $stream.Dispose() }
    $traversalHash = (Get-FileHash -LiteralPath $traversal -Algorithm SHA256).Hash.ToLowerInvariant()
    $code = Invoke-Updater -Package $traversal -Digest $traversalHash -Version '0.1.1'
    if($code -eq 0) { throw 'Path traversal archive was accepted.' }
    if(Test-Path (Join-Path $work 'escape.txt')) { throw 'Archive traversal wrote outside staging.' }

    $v101b = New-TestPackage '0.1.1' 'rollback'
    $code = Invoke-Updater -Package $v101b.Path -Digest $v101b.Sha -Version '0.1.1' -FailAfterFirstReplace
    if($code -eq 0) { throw 'Injected transaction failure was not surfaced.' }
    if((Get-Content (Join-Path $install 'VERSION.txt') -Raw).Trim() -ne '0.1.0') { throw 'Rollback did not restore VERSION.txt.' }
    if((Get-Content (Join-Path $install 'ui\index.html') -Raw) -notmatch '0.1.0') { throw 'Rollback did not restore UI files.' }

    $v102 = New-TestPackage '0.1.2' 'startup-rollback'
    $code = Invoke-Updater -Package $v102.Path -Digest $v102.Sha -Version '0.1.2' -FailAfterInstall
    if($code -eq 0) { throw 'Simulated startup failure was not surfaced.' }
    if((Get-Content (Join-Path $install 'VERSION.txt') -Raw).Trim() -ne '0.1.0') { throw 'Startup rollback did not restore VERSION.txt.' }
    if((Get-Content (Join-Path $install 'ui\index.html') -Raw) -notmatch '0.1.0') { throw 'Startup rollback did not restore UI files.' }
    Write-Output 'UPDATE_TESTS_PASS: valid update, SHA-256 rejection, ZIP traversal rejection, partial-write rollback, startup rollback.'
}
finally {
    if(Test-Path $work) { Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue }
}
