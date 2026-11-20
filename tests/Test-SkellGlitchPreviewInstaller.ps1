param(
    [Parameter(Mandatory = $true)]
    [string]$InstallerPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not $IsWindows -or $PSVersionTable.PSVersion.Major -lt 7) {
    throw 'Installer integration tests require Windows PowerShell 7 or newer.'
}

$installerFull = [System.IO.Path]::GetFullPath($InstallerPath)
if (-not (Test-Path -LiteralPath $installerFull -PathType Leaf)) {
    throw "Installer script not found: $installerFull"
}
if ([string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
    throw 'LOCALAPPDATA is required for the per-user installer integration test.'
}

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) {
        throw "ASSERTION FAILED: $Message"
    }
}

function Assert-Equal([object]$Actual, [object]$Expected, [string]$Message) {
    if ([string]$Actual -cne [string]$Expected) {
        throw "ASSERTION FAILED: $Message. Expected '$Expected', got '$Actual'."
    }
}

function Assert-Throws([scriptblock]$Action, [string]$Message) {
    $threw = $false
    try {
        & $Action | Out-Null
    }
    catch {
        $threw = $true
    }
    if (-not $threw) {
        throw "ASSERTION FAILED: expected rejection: $Message"
    }
}

function Get-Hash([string]$Path) {
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Write-TestChecksums([string]$PackagePath) {
    $files = @(Get-ChildItem -LiteralPath $PackagePath -File -Recurse | Sort-Object FullName)
    $lines = [System.Collections.Generic.List[string]]::new()
    foreach ($file in $files) {
        $relative = [System.IO.Path]::GetRelativePath($PackagePath, $file.FullName).Replace('\', '/')
        $lines.Add((Get-Hash $file.FullName) + '  ' + $relative)
    }
    [System.IO.File]::WriteAllLines(
        (Join-Path $PackagePath 'SHA256SUMS.txt'),
        $lines.ToArray(),
        [System.Text.UTF8Encoding]::new($false)
    )
}

function New-TestPackage([string]$PackagePath, [string]$Version, [string]$Payload) {
    New-Item -ItemType Directory -Path $PackagePath -ErrorAction Stop | Out-Null
    $bundle = Join-Path $PackagePath 'SkellGlitch.vst3'
    $module = Join-Path $bundle 'Contents/x86_64-win/SkellGlitch.vst3'
    $moduleInfo = Join-Path $bundle 'Contents/Resources/moduleinfo.json'
    New-Item -ItemType Directory -Path (Split-Path -Parent $module) -Force | Out-Null
    New-Item -ItemType Directory -Path (Split-Path -Parent $moduleInfo) -Force | Out-Null
    [System.IO.File]::WriteAllBytes($module, [System.Text.Encoding]::ASCII.GetBytes('MZ-SYNTHETIC-MODULE-' + $Payload))
    $metadata = [ordered]@{
        Name = 'SkellGlitch'
        Version = $Version
        'Factory Info' = @{ Vendor = 'Skelly' }
        Classes = @(
            @{ CID = '00112233445566778899AABBCCDDEEFF'; Category = 'Audio Module Class'; Name = 'SkellGlitch'; Vendor = 'Skelly'; Version = $Version }
        )
    }
    [System.IO.File]::WriteAllText(
        $moduleInfo,
        (($metadata | ConvertTo-Json -Depth 6) + [string][char]10),
        [System.Text.UTF8Encoding]::new($false)
    )

    $manifest = [ordered]@{
        schema_version = 1
        product = 'SkellGlitch'
        version = $Version
        vendor = 'Skelly'
        platform = 'windows-x64'
        bundle = 'SkellGlitch.vst3'
        module = 'Contents/x86_64-win/SkellGlitch.vst3'
        module_size_bytes = (Get-Item -LiteralPath $module).Length
        module_sha256 = Get-Hash $module
        moduleinfo_sha256 = Get-Hash $moduleInfo
        source_sha = ('a' * 40)
        workflow_run_id = '123456'
        workflow_run_attempt = '1'
        workflow_event = 'pull_request'
        workflow_ref = 'refs/pull/1/merge'
    }
    [System.IO.File]::WriteAllText(
        (Join-Path $PackagePath 'release-manifest.json'),
        (($manifest | ConvertTo-Json -Depth 6) + [string][char]10),
        [System.Text.UTF8Encoding]::new($false)
    )
    [System.IO.File]::WriteAllText(
        (Join-Path $PackagePath 'WINDOWS-VST3-PRIVATE-PREVIEW.txt'),
        ('Synthetic test fixture only; not a distributable plugin.' + [string][char]10),
        [System.Text.UTF8Encoding]::new($false)
    )
    Copy-Item -LiteralPath $installerFull -Destination (Join-Path $PackagePath 'Install-SkellGlitchPreview.ps1')
    Write-TestChecksums $PackagePath
}

$localRoot = [System.IO.Path]::GetFullPath($env:LOCALAPPDATA)
$testRoot = Join-Path $localRoot ('SkellGlitch-Installer-Tests-' + [Guid]::NewGuid().ToString('N'))
$destination = Join-Path $testRoot 'Programs\Common\VST3'
$backupRoot = Join-Path $testRoot 'Backups'
$packageV1 = Join-Path $testRoot 'package-v1'
$packageV2 = Join-Path $testRoot 'package-v2'

try {
    New-Item -ItemType Directory -Path $testRoot -ErrorAction Stop | Out-Null
    New-TestPackage $packageV1 '1.7.3' 'baseline-v1'
    New-TestPackage $packageV2 '1.7.4' 'update-v2'

    $whatIfResult = & $installerFull -Action Install -PackagePath $packageV1 -Destination $destination -BackupRoot $backupRoot -WhatIf -Confirm:$false
    Assert-Equal $whatIfResult.Status 'WhatIf' 'WhatIf reports a dry run'
    Assert-True (-not (Test-Path -LiteralPath $destination)) 'WhatIf does not create the VST3 destination'
    Assert-True (-not (Test-Path -LiteralPath $backupRoot)) 'WhatIf does not create the backup root'

    $firstInstall = & $installerFull -Action Install -PackagePath $packageV1 -Destination $destination -BackupRoot $backupRoot -Confirm:$false
    Assert-Equal $firstInstall.Status 'Installed' 'first install status'
    Assert-True (Test-Path -LiteralPath (Join-Path $destination 'SkellGlitch.vst3')) 'first install creates the VST3 bundle'
    Assert-True (Test-Path -LiteralPath (Join-Path $firstInstall.BackupPath 'backup-info.json')) 'first install records an explicit empty-baseline rollback point'

    $backupCount = @(Get-ChildItem -LiteralPath $backupRoot -Directory).Count
    $sameInstall = & $installerFull -Action Install -PackagePath $packageV1 -Destination $destination -BackupRoot $backupRoot -Confirm:$false
    Assert-Equal $sameInstall.Status 'NoChange' 'same-hash reinstall is idempotent'
    $backupCountAfter = @(Get-ChildItem -LiteralPath $backupRoot -Directory).Count
    Assert-Equal $backupCountAfter $backupCount 'idempotent reinstall creates no backup'

    $update = & $installerFull -Action Install -PackagePath $packageV2 -Destination $destination -BackupRoot $backupRoot -Confirm:$false
    Assert-Equal $update.Status 'Updated' 'update status'
    Assert-Equal $update.PreviousVersion '1.7.3' 'update records the previous version'
    Assert-True (Test-Path -LiteralPath (Join-Path $update.BackupPath 'SkellGlitch.vst3')) 'update preserves the previous VST3 bundle'
    $installedModule = Join-Path $destination 'SkellGlitch.vst3/Contents/x86_64-win/SkellGlitch.vst3'
    Assert-Equal ([System.IO.File]::ReadAllText($installedModule)) 'MZ-SYNTHETIC-MODULE-update-v2' 'updated module payload'

    $rollback = & $installerFull -Action Rollback -Destination $destination -BackupRoot $backupRoot -BackupPath $update.BackupPath -Confirm:$false
    Assert-Equal $rollback.Status 'RolledBack' 'rollback status'
    Assert-Equal $rollback.RestoredVersion '1.7.3' 'rollback restores the prior version'
    Assert-True (Test-Path -LiteralPath $rollback.SafetyBackupPath) 'rollback preserves the version it replaced'
    Assert-Equal ([System.IO.File]::ReadAllText($installedModule)) 'MZ-SYNTHETIC-MODULE-baseline-v1' 'rollback restored original module bytes'

    $removeFirstInstall = & $installerFull -Action Rollback -Destination $destination -BackupRoot $backupRoot -BackupPath $firstInstall.BackupPath -Confirm:$false
    Assert-Equal $removeFirstInstall.Status 'RolledBack' 'first-install rollback status'
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $destination 'SkellGlitch.vst3'))) 'first-install rollback removes only the preview bundle'

    $moduleV2 = Join-Path $packageV2 'SkellGlitch.vst3/Contents/x86_64-win/SkellGlitch.vst3'
    [System.IO.File]::WriteAllText($moduleV2, 'tampered module bytes')
    Assert-Throws {
        & $installerFull -Action Install -PackagePath $packageV2 -Destination $destination -BackupRoot $backupRoot -Confirm:$false
    } 'a tampered module is rejected before installation'
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $destination 'SkellGlitch.vst3'))) 'tampered package leaves destination unchanged'

    Remove-Item -LiteralPath $packageV2 -Recurse -Force
    New-TestPackage $packageV2 '1.7.4' 'update-v2'
    $extra = Join-Path $packageV2 'UNEXPECTED.txt'
    [System.IO.File]::WriteAllText($extra, 'not allowed')
    Assert-Throws {
        & $installerFull -Action Install -PackagePath $packageV2 -Destination $destination -BackupRoot $backupRoot -Confirm:$false
    } 'unlisted package files are rejected'
    Remove-Item -LiteralPath $extra -Force

    [System.IO.File]::WriteAllText(
        (Join-Path $packageV2 'SHA256SUMS.txt'),
        (('0' * 64) + '  ../outside.txt' + [string][char]10),
        [System.Text.UTF8Encoding]::new($false)
    )
    Assert-Throws {
        & $installerFull -Action Install -PackagePath $packageV2 -Destination $destination -BackupRoot $backupRoot -Confirm:$false
    } 'checksum path traversal is rejected'

    $outsideDestination = Join-Path $env:ProgramFiles 'Common Files\VST3'
    Assert-Throws {
        & $installerFull -Action Install -PackagePath $packageV1 -Destination $outsideDestination -BackupRoot $backupRoot -Confirm:$false
    } 'machine-wide VST3 destinations are rejected'

    $outsideBackup = Join-Path $env:ProgramFiles 'SkellGlitch-Outside-Backup'
    Assert-Throws {
        & $installerFull -Action Rollback -Destination $destination -BackupRoot $backupRoot -BackupPath $outsideBackup -Confirm:$false
    } 'rollback paths outside LOCALAPPDATA are rejected'

    Write-Output 'SkellGlitch installer integration tests: PASS'
}
finally {
    if (Test-Path -LiteralPath $testRoot) {
        Remove-Item -LiteralPath $testRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
