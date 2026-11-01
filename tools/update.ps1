[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$InstallDirectory,
    [int]$ParentProcessId = 0,
    [switch]$AutoConfirm,
    [switch]$NoRestart,
    [switch]$TestMode,
    [string]$TestPackagePath,
    [string]$TestExpectedSha256,
    [string]$TestReleaseVersion,
    [switch]$TestFailAfterFirstReplace,
    [switch]$TestFailAfterInstall
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.IO.Compression.FileSystem
$script:ProductTitle = 'Hostinger Simulator — Atualizador'
$script:AllowedFiles = @('simulator_desktop.exe','simulator.exe','VERSION.txt','ui/index.html','ui/app.js','ui/styles.css','ui/favicon.ico','tools/update.ps1')
$script:ApiUrl = 'https://api.github.com/repos/the-forbbiden-knowledge/hostinger-simulator-mcp/releases?per_page=20'
$script:AssetName = 'hostinger-simulator-windows-x64.zip'
$script:AssetUrlPrefix = 'https://github.com/the-forbbiden-knowledge/hostinger-simulator-mcp/releases/download/'

function Show-Notice {
    param([string]$Message, [System.Windows.Forms.MessageBoxIcon]$Icon = [System.Windows.Forms.MessageBoxIcon]::Information)
    if ($TestMode) { Write-Output $Message; return }
    [void][System.Windows.Forms.MessageBox]::Show($Message,$script:ProductTitle,[System.Windows.Forms.MessageBoxButtons]::OK,$Icon)
}
function ConvertTo-ReleaseVersion {
    param([string]$Value)
    if ($Value -notmatch '^v?(\d+)\.(\d+)\.(\d+)$') { throw "Versão inválida: '$Value'. O formato esperado é MAJOR.MINOR.PATCH." }
    return [version]("$($Matches[1]).$($Matches[2]).$($Matches[3])")
}
function Get-CurrentVersion {
    param([string]$Directory)
    $path=Join-Path $Directory 'VERSION.txt'
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { return [version]'0.0.0' }
    $raw=(Get-Content -LiteralPath $path -Raw).Trim()
    if ([string]::IsNullOrWhiteSpace($raw)) { throw 'VERSION.txt está vazio; atualização cancelada.' }
    return ConvertTo-ReleaseVersion $raw
}
function Get-LatestRelease {
    $headers=@{Accept='application/vnd.github+json';'User-Agent'='HostingerSimulator-Updater';'X-GitHub-Api-Version'='2022-11-28'}
    $releases=Invoke-RestMethod -Method Get -Uri $script:ApiUrl -Headers $headers -TimeoutSec 25
    $candidates=@($releases | Where-Object {
        -not $_.draft -and $_.tag_name -match '^v\d+\.\d+\.\d+$' -and
        @($_.assets | Where-Object {$_.name -eq $script:AssetName}).Count -eq 1
    })
    if ($candidates.Count -eq 0) { throw 'Nenhum Release publicado com o pacote Windows x64 esperado foi encontrado.' }
    $release=$candidates | Sort-Object -Property @{Expression={ConvertTo-ReleaseVersion $_.tag_name};Descending=$true} | Select-Object -First 1
    $asset=@($release.assets | Where-Object {$_.name -eq $script:AssetName})[0]
    if ($asset.digest -notmatch '^sha256:[a-f0-9]{64}$') { throw 'O GitHub não forneceu um digest SHA-256 válido. Atualização bloqueada.' }
    if (-not $asset.browser_download_url.StartsWith($script:AssetUrlPrefix,[StringComparison]::Ordinal)) { throw 'URL do pacote fora do repositório de Releases autorizado.' }
    return [pscustomobject]@{
        Version=ConvertTo-ReleaseVersion $release.tag_name
        Tag=[string]$release.tag_name
        Url=[string]$asset.browser_download_url
        Sha256=[string]$asset.digest.Substring(7)
    }
}
function Assert-SafeArchive {
    param([string]$ArchivePath,[string]$ExpectedVersion)
    $archive=[System.IO.Compression.ZipFile]::OpenRead($ArchivePath)
    try {
        $seen=[System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
        foreach($entry in $archive.Entries) {
            $name=$entry.FullName.Replace('\','/')
            if($name.EndsWith('/')) { continue }
            if([string]::IsNullOrWhiteSpace($name) -or $name.StartsWith('/') -or $name.Contains(':') -or $name -match '(^|/)\.\.?(/|$)' -or $name -match '^[A-Za-z]:') {
                throw "Caminho inseguro dentro do pacote: '$name'."
            }
            if($script:AllowedFiles -notcontains $name) { throw "Arquivo não autorizado no pacote: '$name'." }
            if(-not $seen.Add($name)) { throw "Arquivo duplicado no pacote: '$name'." }
        }
        foreach($required in $script:AllowedFiles) {
            if(-not $seen.Contains($required)) { throw "Arquivo obrigatório ausente no pacote: '$required'." }
        }
    } finally { $archive.Dispose() }
    $stageParent=Join-Path $env:TEMP ('HostingerSimulatorUpdate-'+[guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $stageParent -Force | Out-Null
    $stage=Join-Path $stageParent 'package'
    [System.IO.Compression.ZipFile]::ExtractToDirectory($ArchivePath,$stage)
    $versionText=(Get-Content -LiteralPath (Join-Path $stage 'VERSION.txt') -Raw).Trim()
    $packageVersion=ConvertTo-ReleaseVersion $versionText
    $expected=ConvertTo-ReleaseVersion $ExpectedVersion
    if($packageVersion -ne $expected) { throw "Versão interna $packageVersion não corresponde ao Release $expected." }
    foreach($binary in @('simulator_desktop.exe','simulator.exe')) {
        $file=Join-Path $stage $binary
        if(-not (Test-Path -LiteralPath $file -PathType Leaf) -or (Get-Item -LiteralPath $file).Length -lt 10240) {
            throw "Binário obrigatório ausente ou pequeno demais: $binary."
        }
    }
    return [pscustomobject]@{Root=$stage;Parent=$stageParent}
}
function Install-PackageTransaction {
    param([string]$StageDirectory,[string]$Directory,[version]$OldVersion,[switch]$FailAfterFirstReplace)
    $backupRoot=Join-Path $env:LOCALAPPDATA 'HostingerSimulator\rollback'
    New-Item -ItemType Directory -Path $backupRoot -Force | Out-Null
    $stamp=(Get-Date).ToUniversalTime().ToString('yyyyMMddTHHmmssZ')
    $backup=Join-Path $backupRoot ("{0}-{1}" -f $OldVersion,$stamp)
    New-Item -ItemType Directory -Path $backup -Force | Out-Null
    $items=@('simulator_desktop.exe','simulator.exe','VERSION.txt','ui','tools/update.ps1')
    $touched=[System.Collections.Generic.List[string]]::new()
    try {
        foreach($item in $items) {
            $source=Join-Path $StageDirectory $item
            $destination=Join-Path $Directory $item
            $saved=Join-Path $backup $item
            $touched.Add($item)
            if(Test-Path -LiteralPath $destination) {
                $savedParent=Split-Path -Parent $saved
                New-Item -ItemType Directory -Path $savedParent -Force | Out-Null
                Move-Item -LiteralPath $destination -Destination $saved
            }
            $destinationParent=Split-Path -Parent $destination
            if(-not (Test-Path -LiteralPath $destinationParent)) { New-Item -ItemType Directory -Path $destinationParent -Force | Out-Null }
            Move-Item -LiteralPath $source -Destination $destination
            if($FailAfterFirstReplace -and $touched.Count -eq 1) { throw 'Falha de teste solicitada após a primeira substituição.' }
        }
        return $backup
    } catch {
        $failure=$_
        for($index=$touched.Count-1;$index -ge 0;$index--) {
            $item=$touched[$index]
            $destination=Join-Path $Directory $item
            $saved=Join-Path $backup $item
            if(Test-Path -LiteralPath $destination) { Remove-Item -LiteralPath $destination -Recurse -Force }
            if(Test-Path -LiteralPath $saved) {
                $destinationParent=Split-Path -Parent $destination
                if(-not (Test-Path -LiteralPath $destinationParent)) { New-Item -ItemType Directory -Path $destinationParent -Force | Out-Null }
                Move-Item -LiteralPath $saved -Destination $destination
            }
        }
        throw "Instalação revertida; a versão anterior foi restaurada. Causa: $failure"
    }
}
function Restore-PackageTransaction {
    param([string]$BackupDirectory,[string]$Directory)
    $items=@('simulator_desktop.exe','simulator.exe','VERSION.txt','ui','tools/update.ps1')
    for($index=$items.Count-1;$index -ge 0;$index--) {
        $item=$items[$index]
        $destination=Join-Path $Directory $item
        $saved=Join-Path $BackupDirectory $item
        if(Test-Path -LiteralPath $destination) { Remove-Item -LiteralPath $destination -Recurse -Force }
        if(Test-Path -LiteralPath $saved) {
            $destinationParent=Split-Path -Parent $destination
            if(-not (Test-Path -LiteralPath $destinationParent)) { New-Item -ItemType Directory -Path $destinationParent -Force | Out-Null }
            Move-Item -LiteralPath $saved -Destination $destination
        }
    }
}
try {
    $install=[System.IO.Path]::GetFullPath($InstallDirectory).TrimEnd('\')
    if(-not (Test-Path -LiteralPath $install -PathType Container) -or
       -not (Test-Path -LiteralPath (Join-Path $install 'simulator_desktop.exe') -PathType Leaf) -or
       -not (Test-Path -LiteralPath (Join-Path $install 'ui\index.html') -PathType Leaf)) {
        throw 'Diretório de instalação não corresponde ao Hostinger Simulator Desktop.'
    }
    $currentVersion=Get-CurrentVersion $install
    if($TestMode) {
        if(-not $TestPackagePath -or $TestExpectedSha256 -notmatch '^[a-f0-9]{64}$' -or -not $TestReleaseVersion) { throw 'Parâmetros incompletos para o teste local do atualizador.' }
        $release=[pscustomobject]@{Version=(ConvertTo-ReleaseVersion $TestReleaseVersion);Tag=('v'+$TestReleaseVersion);Url='';Sha256=$TestExpectedSha256}
        $archivePath=[System.IO.Path]::GetFullPath($TestPackagePath)
    } else {
        $release=Get-LatestRelease
        if($currentVersion -ge $release.Version) {
            Show-Notice "O aplicativo já está atualizado.$([Environment]::NewLine)Versão instalada: $currentVersion$([Environment]::NewLine)Release disponível: $($release.Version)"
            return
        }
        if(-not $AutoConfirm) {
            $choice=[System.Windows.Forms.MessageBox]::Show(
                "Uma atualização está disponível.$([Environment]::NewLine)$([Environment]::NewLine)Versão instalada: $currentVersion$([Environment]::NewLine)Nova versão: $($release.Version)$([Environment]::NewLine)$([Environment]::NewLine)O pacote será verificado por SHA-256 antes da instalação. A versão atual será mantida para rollback.$([Environment]::NewLine)$([Environment]::NewLine)Deseja baixar e instalar agora?",
                $script:ProductTitle,[System.Windows.Forms.MessageBoxButtons]::YesNo,[System.Windows.Forms.MessageBoxIcon]::Question
            )
            if($choice -ne [System.Windows.Forms.DialogResult]::Yes) { return }
        }
        $downloadRoot=Join-Path $env:TEMP ('HostingerSimulatorDownload-'+[guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory -Path $downloadRoot -Force | Out-Null
        $archivePath=Join-Path $downloadRoot $script:AssetName
        Invoke-WebRequest -Method Get -Uri $release.Url -OutFile $archivePath -TimeoutSec 180 -MaximumRedirection 5 -UseBasicParsing
    }
    $actualSha256=(Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()
    if($actualSha256 -ne $release.Sha256) { throw "SHA-256 do pacote não confere. Esperado: $($release.Sha256); obtido: $actualSha256." }
    $staged=Assert-SafeArchive -ArchivePath $archivePath -ExpectedVersion $release.Version.ToString()
    try {
        if($ParentProcessId -gt 0) {
            $parent=Get-Process -Id $ParentProcessId -ErrorAction Stop
            if($parent.ProcessName -ne 'simulator_desktop') { throw 'O processo informado não é o Hostinger Simulator Desktop.' }
            if(-not $parent.CloseMainWindow()) { throw 'Não foi possível solicitar o fechamento normal do aplicativo. Feche-o e tente novamente.' }
            if(-not $parent.WaitForExit(30000)) { throw 'O aplicativo não encerrou em 30 segundos; instalação cancelada sem forçar o encerramento.' }
        } else {
            $running=@(Get-Process -Name 'simulator_desktop' -ErrorAction SilentlyContinue |
                Where-Object {$_.Path -and [System.IO.Path]::GetFullPath($_.Path) -ieq (Join-Path $install 'simulator_desktop.exe')})
            if($running.Count -gt 0) { throw 'O aplicativo ainda está aberto. Feche-o antes de executar a atualização manualmente.' }
        }
        $backup=Install-PackageTransaction -StageDirectory $staged.Root -Directory $install -OldVersion $currentVersion -FailAfterFirstReplace:$TestFailAfterFirstReplace
        if($TestMode -and $TestFailAfterInstall) {
            Restore-PackageTransaction -BackupDirectory $backup -Directory $install
            throw 'Falha de inicialização simulada; a versão anterior foi restaurada.'
        }
        if(-not $NoRestart -and -not $TestMode) {
            try {
                $app=Start-Process -FilePath (Join-Path $install 'simulator_desktop.exe') -WorkingDirectory $install -PassThru
                Start-Sleep -Seconds 5
                $app.Refresh()
                if($app.HasExited) { throw 'A nova versão encerrou logo após a inicialização.' }
            } catch {
                $launchFailure=$_
                Restore-PackageTransaction -BackupDirectory $backup -Directory $install
                Start-Process -FilePath (Join-Path $install 'simulator_desktop.exe') -WorkingDirectory $install | Out-Null
                throw "A nova versão não iniciou corretamente; a versão anterior foi restaurada. Causa: $launchFailure"
            }
        }
        Show-Notice "Atualização instalada com sucesso.$([Environment]::NewLine)Versão: $($release.Version)$([Environment]::NewLine)Rollback preservado em:$([Environment]::NewLine)$backup"
        Write-Output "UPDATE_SUCCESS version=$($release.Version) backup=$backup"
    } finally {
        if($staged -and (Test-Path -LiteralPath $staged.Parent)) { Remove-Item -LiteralPath $staged.Parent -Recurse -Force -ErrorAction SilentlyContinue }
    }
} catch {
    $message="A atualização não foi concluída.$([Environment]::NewLine)$([Environment]::NewLine)$($_.Exception.Message)"
    if($TestMode) { [Console]::Error.WriteLine($message) }
    else { [void][System.Windows.Forms.MessageBox]::Show($message,$script:ProductTitle,[System.Windows.Forms.MessageBoxButtons]::OK,[System.Windows.Forms.MessageBoxIcon]::Error) }
    exit 1
}
