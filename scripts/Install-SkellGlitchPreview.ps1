[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'Medium')]
param(
    [ValidateSet('Install', 'Rollback')]
    [string]$Action = 'Install',
    [string]$PackagePath = $PSScriptRoot,
    [string]$Destination,
    [string]$BackupRoot,
    [string]$BackupPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:Product = 'SkellGlitch'
$script:BundleName = 'SkellGlitch.vst3'
$script:ModuleRelativePath = 'Contents/x86_64-win/SkellGlitch.vst3'
$script:ModuleInfoRelativePath = 'Contents/Resources/moduleinfo.json'
$script:InstallerName = 'Install-SkellGlitchPreview.ps1'
$script:NoticeName = 'WINDOWS-VST3-PRIVATE-PREVIEW.txt'

function Get-FullPath([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw 'A required path is empty.'
    }
    return [System.IO.Path]::GetFullPath($Path)
}

function Test-IsDescendantPath([string]$Path, [string]$Root) {
    $rootFull = (Get-FullPath $Root).TrimEnd([char[]]@('\', '/'))
    $pathFull = Get-FullPath $Path
    $prefix = $rootFull + [System.IO.Path]::DirectorySeparatorChar
    return $pathFull.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)
}

function Assert-NotReparsePoint([string]$Path, [string]$Description) {
    $item = Get-Item -LiteralPath $Path -Force -ErrorAction SilentlyContinue
    if ($null -ne $item -and
        (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0)) {
        throw ('Reparse point rejected for ' + $Description + ': ' + $Path)
    }
}

function Assert-NoReparsePoints([string]$Root, [string]$Description) {
    $rootFull = Get-FullPath $Root
    $rootItem = Get-Item -LiteralPath $rootFull -Force -ErrorAction Stop
    if (-not $rootItem.PSIsContainer) {
        throw "$Description is not a directory: $rootFull"
    }
    Assert-NotReparsePoint $rootFull $Description

    $pending = [System.Collections.Generic.Stack[string]]::new()
    $pending.Push($rootFull)
    while ($pending.Count -gt 0) {
        $current = $pending.Pop()
        foreach ($item in @(Get-ChildItem -LiteralPath $current -Force -ErrorAction Stop)) {
            if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw ('Reparse point rejected inside ' + $Description + ': ' + $item.FullName)
            }
            if ($item.PSIsContainer) {
                $pending.Push($item.FullName)
            }
        }
    }
}

function Get-RegularFiles([string]$Root) {
    $rootFull = Get-FullPath $Root
    $files = [System.Collections.Generic.List[System.IO.FileInfo]]::new()
    $pending = [System.Collections.Generic.Stack[string]]::new()
    $pending.Push($rootFull)
    while ($pending.Count -gt 0) {
        $current = $pending.Pop()
        foreach ($item in @(Get-ChildItem -LiteralPath $current -Force -ErrorAction Stop)) {
            if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "Reparse point rejected while inventorying: $($item.FullName)"
            }
            if ($item.PSIsContainer) {
                $pending.Push($item.FullName)
            }
            else {
                $files.Add([System.IO.FileInfo]$item)
            }
        }
    }
    return $files.ToArray()
}

function Get-FileHashHex([string]$Path) {
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Get-TreeHash([string]$Root) {
    Assert-NoReparsePoints $Root 'VST3 bundle'
    $rootFull = Get-FullPath $Root
    $records = [System.Collections.Generic.List[string]]::new()
    foreach ($file in @(Get-RegularFiles $rootFull)) {
        $relative = [System.IO.Path]::GetRelativePath($rootFull, $file.FullName).Replace('\', '/')
        $records.Add($relative + ' ' + (Get-FileHashHex $file.FullName))
    }
    $sorted = @($records.ToArray() | Sort-Object)
    $payload = [string]::Join([string][char]10, $sorted) + [string][char]10
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($payload)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        return [System.Convert]::ToHexString($sha.ComputeHash($bytes)).ToLowerInvariant()
    }
    finally {
        $sha.Dispose()
    }
}

function Get-BundleVersion([string]$BundlePath) {
    $moduleInfo = Join-Path $BundlePath $script:ModuleInfoRelativePath
    $text = [System.IO.File]::ReadAllText($moduleInfo)
    $match = [System.Text.RegularExpressions.Regex]::Match($text, '"Version"\s*:\s*"([^"]+)"')
    if ($match.Success) {
        return $match.Groups[1].Value
    }
    return 'unknown'
}

function Assert-ValidBundle([string]$BundlePath) {
    $bundleFull = Get-FullPath $BundlePath
    if (-not (Test-Path -LiteralPath $bundleFull -PathType Container)) {
        throw "Missing VST3 bundle: $bundleFull"
    }
    Assert-NoReparsePoints $bundleFull 'VST3 bundle'

    $module = Join-Path $bundleFull $script:ModuleRelativePath
    $moduleInfo = Join-Path $bundleFull $script:ModuleInfoRelativePath
    if (-not (Test-Path -LiteralPath $module -PathType Leaf)) {
        throw "Missing Windows x64 VST3 module: $module"
    }
    if (-not (Test-Path -LiteralPath $moduleInfo -PathType Leaf)) {
        throw "Missing VST3 module metadata: $moduleInfo"
    }

    $files = @(Get-RegularFiles $bundleFull)
    $modules = @($files | Where-Object { $_.Extension -ieq '.vst3' })
    if ($modules.Count -ne 1 -or
        -not $modules[0].FullName.Equals($module, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw 'The bundle must contain exactly one Windows x64 VST3 module.'
    }
    if (@($files | Where-Object { $_.Extension -ieq '.exe' }).Count -gt 0) {
        throw 'Standalone executables are not allowed in the VST3 bundle.'
    }

    return [pscustomobject]@{
        Version = Get-BundleVersion $bundleFull
        TreeHash = Get-TreeHash $bundleFull
        ModulePath = $module
        ModuleInfoPath = $moduleInfo
    }
}

function Get-LocalAppDataRoot {
    if ([string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
        throw 'LOCALAPPDATA is unavailable; refusing to guess a per-user install path.'
    }
    $root = Get-FullPath $env:LOCALAPPDATA
    if (-not (Test-Path -LiteralPath $root -PathType Container)) {
        throw "LOCALAPPDATA does not exist: $root"
    }
    Assert-NotReparsePoint $root 'LOCALAPPDATA'
    return $root
}

function Assert-SafeDescendant([string]$Path, [string]$Root, [string]$Description) {
    $rootFull = Get-FullPath $Root
    $pathFull = Get-FullPath $Path
    if (-not (Test-IsDescendantPath $pathFull $rootFull)) {
        throw "$Description must be inside the current user's LOCALAPPDATA directory: $pathFull"
    }

    Assert-NotReparsePoint $rootFull 'LOCALAPPDATA'
    $relative = $pathFull.Substring($rootFull.Length).TrimStart([char[]]@('\', '/'))
    $current = $rootFull
    foreach ($segment in ($relative -split '[\\/]+')) {
        if ([string]::IsNullOrWhiteSpace($segment)) {
            continue
        }
        $current = Join-Path $current $segment
        $item = Get-Item -LiteralPath $current -Force -ErrorAction SilentlyContinue
        if ($null -ne $item) {
            if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "Reparse point rejected in $Description path: $current"
            }
            if (-not $item.PSIsContainer) {
                throw "$Description path contains a non-directory component: $current"
            }
        }
    }
    return $pathFull
}

function Ensure-SafeDirectory([string]$Path, [string]$Root, [string]$Description) {
    $full = Assert-SafeDescendant $Path $Root $Description
    $rootFull = Get-FullPath $Root
    $relative = $full.Substring($rootFull.Length).TrimStart([char[]]@('\', '/'))
    $current = $rootFull
    foreach ($segment in ($relative -split '[\\/]+')) {
        if ([string]::IsNullOrWhiteSpace($segment)) {
            continue
        }
        $current = Join-Path $current $segment
        $item = Get-Item -LiteralPath $current -Force -ErrorAction SilentlyContinue
        if ($null -eq $item) {
            New-Item -ItemType Directory -Path $current -ErrorAction Stop | Out-Null
            $item = Get-Item -LiteralPath $current -Force -ErrorAction Stop
        }
        if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Reparse point rejected in $Description path: $current"
        }
        if (-not $item.PSIsContainer) {
            throw "$Description path contains a non-directory component: $current"
        }
    }
    return $full
}

function Get-ChecksumMap([string]$PackageRoot) {
    $checksumPath = Join-Path $PackageRoot 'SHA256SUMS.txt'
    if (-not (Test-Path -LiteralPath $checksumPath -PathType Leaf)) {
        throw 'SHA256SUMS.txt is missing.'
    }
    $map = [System.Collections.Generic.Dictionary[string, string]]::new(
        [System.StringComparer]::OrdinalIgnoreCase
    )
    foreach ($line in [System.IO.File]::ReadAllLines($checksumPath)) {
        if ([string]::IsNullOrWhiteSpace($line)) {
            continue
        }
        $match = [System.Text.RegularExpressions.Regex]::Match($line, '^([0-9a-fA-F]{64})  (.+)$')
        if (-not $match.Success) {
            throw "Invalid SHA256SUMS.txt entry: $line"
        }
        $relative = $match.Groups[2].Value
        if ($relative.StartsWith('/') -or $relative.Contains('\') -or $relative.Contains(':')) {
            throw "Unsafe checksum path: $relative"
        }
        $segments = $relative.Split('/')
        if (@($segments | Where-Object { $_ -in @('', '.', '..') }).Count -gt 0) {
            throw "Unsafe checksum path: $relative"
        }
        if ($relative -ieq 'SHA256SUMS.txt') {
            throw 'SHA256SUMS.txt must not checksum itself.'
        }
        if ($map.ContainsKey($relative)) {
            throw "Duplicate checksum path: $relative"
        }
        $map.Add($relative, $match.Groups[1].Value.ToLowerInvariant())
    }
    if ($map.Count -eq 0) {
        throw 'SHA256SUMS.txt contains no file checksums.'
    }
    return $map
}

function Assert-ValidPackage([string]$PackageRoot) {
    $root = Get-FullPath $PackageRoot
    if (-not (Test-Path -LiteralPath $root -PathType Container)) {
        throw "Preview package directory does not exist: $root"
    }
    Assert-NoReparsePoints $root 'preview package'

    $expectedTop = @(
        $script:BundleName,
        'release-manifest.json',
        'SHA256SUMS.txt',
        $script:NoticeName,
        $script:InstallerName
    ) | Sort-Object
    $actualTop = @(Get-ChildItem -LiteralPath $root -Force | ForEach-Object { $_.Name } | Sort-Object)
    if (@(Compare-Object -ReferenceObject $expectedTop -DifferenceObject $actualTop -CaseSensitive).Count -gt 0) {
        throw "Preview package contents are unexpected. Expected: $($expectedTop -join ', ')"
    }

    $manifestPath = Join-Path $root 'release-manifest.json'
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    foreach ($field in @(
        'schema_version', 'product', 'version', 'vendor', 'platform', 'bundle',
        'module', 'module_size_bytes', 'module_sha256', 'moduleinfo_sha256',
        'source_sha', 'workflow_run_id', 'workflow_run_attempt', 'workflow_event', 'workflow_ref'
    )) {
        if ($null -eq $manifest.$field -or [string]::IsNullOrWhiteSpace([string]$manifest.$field)) {
            throw "Release manifest is missing required field: $field"
        }
    }
    if ([int]$manifest.schema_version -ne 1 -or
        $manifest.product -cne $script:Product -or
        $manifest.vendor -cne 'Skelly' -or
        $manifest.platform -cne 'windows-x64' -or
        $manifest.bundle -cne $script:BundleName -or
        $manifest.module -cne $script:ModuleRelativePath) {
        throw 'Release manifest product, vendor, platform or bundle identity is invalid.'
    }
    if ([string]$manifest.source_sha -notmatch '^[0-9a-fA-F]{40}([0-9a-fA-F]{24})?$' -or
        [string]$manifest.module_sha256 -notmatch '^[0-9a-fA-F]{64}$' -or
        [string]$manifest.moduleinfo_sha256 -notmatch '^[0-9a-fA-F]{64}$' -or
        [string]$manifest.workflow_run_id -notmatch '^[0-9]+$' -or
        [string]$manifest.workflow_run_attempt -notmatch '^[0-9]+$') {
        throw 'Release manifest provenance or SHA-256 fields are invalid.'
    }

    $bundlePath = Join-Path $root $script:BundleName
    $bundleInfo = Assert-ValidBundle $bundlePath
    if ($bundleInfo.Version -cne [string]$manifest.version) {
        throw 'VST3 moduleinfo version does not match release-manifest.json.'
    }
    $moduleHash = Get-FileHashHex $bundleInfo.ModulePath
    $moduleInfoHash = Get-FileHashHex $bundleInfo.ModuleInfoPath
    if ($moduleHash -cne [string]$manifest.module_sha256 -or
        $moduleInfoHash -cne [string]$manifest.moduleinfo_sha256 -or
        (Get-Item -LiteralPath $bundleInfo.ModulePath).Length -ne [long]$manifest.module_size_bytes) {
        throw 'VST3 module bytes or moduleinfo metadata do not match the release manifest.'
    }

    $checksums = Get-ChecksumMap $root
    $actualPaths = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::OrdinalIgnoreCase
    )
    foreach ($file in @(Get-RegularFiles $root)) {
        $relative = [System.IO.Path]::GetRelativePath($root, $file.FullName).Replace('\', '/')
        if ($relative -ieq 'SHA256SUMS.txt') {
            continue
        }
        if (-not $actualPaths.Add($relative)) {
            throw "Duplicate package path: $relative"
        }
    }
    if ($actualPaths.Count -ne $checksums.Count) {
        throw 'SHA256SUMS.txt does not describe the complete package file inventory.'
    }
    foreach ($relative in $actualPaths) {
        if (-not $checksums.ContainsKey($relative)) {
            throw "Unlisted package file: $relative"
        }
        $path = Join-Path $root ($relative.Replace('/', [System.IO.Path]::DirectorySeparatorChar))
        if ((Get-FileHashHex $path) -cne $checksums[$relative]) {
            throw "Package checksum mismatch: $relative"
        }
    }
    foreach ($relative in $checksums.Keys) {
        if (-not $actualPaths.Contains($relative)) {
            throw "Checksum references a missing package file: $relative"
        }
    }
    return $manifest
}

function New-BackupPath([string]$Root) {
    $name = 'SkellGlitch-' + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ') +
        '-' + [Guid]::NewGuid().ToString('N').Substring(0, 8)
    $path = Join-Path $Root $name
    New-Item -ItemType Directory -Path $path -ErrorAction Stop | Out-Null
    return $path
}

function Write-BackupInfo([string]$Path, [bool]$HadExisting, [string]$Version, [string]$TreeHash, [string]$SourceSha, [string]$Target) {
    $info = [ordered]@{
        schema_version = 1
        product = $script:Product
        created_utc = [DateTime]::UtcNow.ToString('o')
        had_existing = $HadExisting
        previous_version = $Version
        previous_bundle_sha256 = $TreeHash
        source_sha = $SourceSha
        install_target = $Target
    }
    $json = ($info | ConvertTo-Json -Depth 5) + [string][char]10
    [System.IO.File]::WriteAllText(
        (Join-Path $Path 'backup-info.json'),
        $json,
        [System.Text.UTF8Encoding]::new($false)
    )
}

function Read-BackupInfo([string]$Path) {
    Assert-NoReparsePoints $Path 'backup record'
    $infoPath = Join-Path $Path 'backup-info.json'
    if (-not (Test-Path -LiteralPath $infoPath -PathType Leaf)) {
        throw "Backup record is missing backup-info.json: $Path"
    }
    $info = Get-Content -LiteralPath $infoPath -Raw | ConvertFrom-Json
    if ([int]$info.schema_version -ne 1 -or
        $info.product -cne $script:Product -or
        $null -eq $info.had_existing) {
        throw "Backup record identity or schema is invalid: $Path"
    }
    return $info
}

function Assert-BackupPath([string]$Path, [string]$BackupRoot, [string]$LocalRoot) {
    $full = Assert-SafeDescendant $Path $LocalRoot 'rollback backup'
    $rootFull = (Get-FullPath $BackupRoot).TrimEnd([char[]]@('\', '/'))
    $parent = ([System.IO.Path]::GetDirectoryName($full)).TrimEnd([char[]]@('\', '/'))
    if (-not $parent.Equals($rootFull, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw 'Rollback backup must be a direct child of the configured SkellGlitch backup directory.'
    }
    if (-not (Test-Path -LiteralPath $full -PathType Container)) {
        throw "Rollback backup does not exist: $full"
    }
    Assert-NoReparsePoints $full 'rollback backup'
    return $full
}

function Install-Preview {
    $localRoot = Get-LocalAppDataRoot
    if ([string]::IsNullOrWhiteSpace($Destination)) {
        $Destination = Join-Path $localRoot 'Programs\Common\VST3'
    }
    if ([string]::IsNullOrWhiteSpace($BackupRoot)) {
        $BackupRoot = Join-Path $localRoot 'SkellGlitch\Backups'
    }
    $destinationFull = Assert-SafeDescendant $Destination $localRoot 'VST3 destination'
    $backupRootFull = Assert-SafeDescendant $BackupRoot $localRoot 'backup root'
    if ((Test-IsDescendantPath $backupRootFull $destinationFull) -or
        (Test-IsDescendantPath $destinationFull $backupRootFull)) {
        throw 'The VST3 destination and backup root must be separate directories.'
    }

    $manifest = Assert-ValidPackage $PackagePath
    $packageBundle = Join-Path (Get-FullPath $PackagePath) $script:BundleName
    $packageTreeHash = Get-TreeHash $packageBundle
    $target = Join-Path $destinationFull $script:BundleName
    $existing = $false
    $existingInfo = $null
    if (Test-Path -LiteralPath $target) {
        $existing = $true
        $existingInfo = Assert-ValidBundle $target
        if ($existingInfo.TreeHash -ceq $packageTreeHash) {
            return [pscustomobject]@{
                Status = 'NoChange'
                Product = $script:Product
                Version = [string]$manifest.version
                Destination = $target
                BackupPath = $null
                SourceCommit = [string]$manifest.source_sha
            }
        }
    }

    if (-not $PSCmdlet.ShouldProcess($target, "Install SkellGlitch $($manifest.version) as a per-user VST3")) {
        return [pscustomobject]@{
            Status = 'WhatIf'
            Product = $script:Product
            Version = [string]$manifest.version
            Destination = $target
            BackupPath = $null
            SourceCommit = [string]$manifest.source_sha
        }
    }

    $null = Ensure-SafeDirectory $destinationFull $localRoot 'VST3 destination'
    $null = Ensure-SafeDirectory $backupRootFull $localRoot 'backup root'
    $stageRoot = Join-Path $destinationFull ('.SkellGlitch-stage-' + [Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $stageRoot -ErrorAction Stop | Out-Null
    $backupPathCreated = $null
    $backupBundle = $null
    $movedExisting = $false
    $installed = $false
    try {
        Copy-Item -LiteralPath $packageBundle -Destination $stageRoot -Recurse -ErrorAction Stop
        $stagedBundle = Join-Path $stageRoot $script:BundleName
        $stagedInfo = Assert-ValidBundle $stagedBundle
        if ($stagedInfo.TreeHash -cne $packageTreeHash) {
            throw 'Staged VST3 bytes differ from the verified preview package.'
        }

        $backupPathCreated = New-BackupPath $backupRootFull
        $backupBundle = Join-Path $backupPathCreated $script:BundleName
        $previousVersion = ''
        $previousHash = ''
        if ($existing) {
            $previousVersion = $existingInfo.Version
            $previousHash = $existingInfo.TreeHash
            Move-Item -LiteralPath $target -Destination $backupBundle -ErrorAction Stop
            $movedExisting = $true
        }
        Write-BackupInfo -Path $backupPathCreated -HadExisting $existing -Version $previousVersion -TreeHash $previousHash -SourceSha ([string]$manifest.source_sha) -Target $target

        Move-Item -LiteralPath $stagedBundle -Destination $target -ErrorAction Stop
        $installed = $true
        $installedInfo = Assert-ValidBundle $target
        if ($installedInfo.TreeHash -cne $packageTreeHash) {
            throw 'Installed VST3 bytes differ from the verified preview package.'
        }

        return [pscustomobject]@{
            Status = if ($existing) { 'Updated' } else { 'Installed' }
            Product = $script:Product
            Version = [string]$manifest.version
            Destination = $target
            BackupPath = $backupPathCreated
            PreviousVersion = $previousVersion
            SourceCommit = [string]$manifest.source_sha
        }
    }
    catch {
        $failure = $_
        try {
            if ($installed -and (Test-Path -LiteralPath $target)) {
                Remove-Item -LiteralPath $target -Recurse -Force -ErrorAction Stop
            }
            if ($movedExisting -and (Test-Path -LiteralPath $backupBundle)) {
                Move-Item -LiteralPath $backupBundle -Destination $target -ErrorAction Stop
            }
            if ($null -ne $backupPathCreated -and (Test-Path -LiteralPath $backupPathCreated)) {
                Remove-Item -LiteralPath $backupPathCreated -Recurse -Force -ErrorAction SilentlyContinue
            }
        }
        catch {
            throw "Install failed: $($failure.Exception.Message). Automatic restoration also failed; inspect the preserved backup at $backupPathCreated. Restoration error: $($_.Exception.Message)"
        }
        throw $failure
    }
    finally {
        if (Test-Path -LiteralPath $stageRoot) {
            Remove-Item -LiteralPath $stageRoot -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
}

function Rollback-Preview {
    $localRoot = Get-LocalAppDataRoot
    if ([string]::IsNullOrWhiteSpace($Destination)) {
        $Destination = Join-Path $localRoot 'Programs\Common\VST3'
    }
    if ([string]::IsNullOrWhiteSpace($BackupRoot)) {
        $BackupRoot = Join-Path $localRoot 'SkellGlitch\Backups'
    }
    if ([string]::IsNullOrWhiteSpace($BackupPath)) {
        throw 'Rollback requires -BackupPath from a previous install result.'
    }

    $destinationFull = Assert-SafeDescendant $Destination $localRoot 'VST3 destination'
    $backupRootFull = Assert-SafeDescendant $BackupRoot $localRoot 'backup root'
    if ((Test-IsDescendantPath $backupRootFull $destinationFull) -or
        (Test-IsDescendantPath $destinationFull $backupRootFull)) {
        throw 'The VST3 destination and backup root must be separate directories.'
    }
    $backupFull = Assert-BackupPath $BackupPath $backupRootFull $localRoot
    $info = Read-BackupInfo $backupFull
    if (-not ([string]$info.install_target).Equals(
        (Join-Path $destinationFull $script:BundleName),
        [System.StringComparison]::OrdinalIgnoreCase
    )) {
        throw 'Rollback backup belongs to a different VST3 destination.'
    }

    $restoreBundle = Join-Path $backupFull $script:BundleName
    $restoreInfo = $null
    if ([bool]$info.had_existing) {
        $restoreInfo = Assert-ValidBundle $restoreBundle
        if ($restoreInfo.TreeHash -cne [string]$info.previous_bundle_sha256) {
            throw 'Rollback backup VST3 checksum does not match backup-info.json.'
        }
    }
    elseif (Test-Path -LiteralPath $restoreBundle) {
        throw 'A first-install rollback record must not contain a previous VST3 bundle.'
    }

    $target = Join-Path $destinationFull $script:BundleName
    $currentExists = Test-Path -LiteralPath $target
    $currentInfo = $null
    if ($currentExists) {
        $currentInfo = Assert-ValidBundle $target
    }
    if (-not [bool]$info.had_existing -and -not $currentExists) {
        return [pscustomobject]@{
            Status = 'NoChange'
            Product = $script:Product
            Destination = $target
            BackupPath = $backupFull
            RestoredVersion = $null
        }
    }

    $restoreVersion = if ($restoreInfo) { $restoreInfo.Version } else { $null }
    if (-not $PSCmdlet.ShouldProcess($target, "Rollback SkellGlitch to $restoreVersion")) {
        return [pscustomobject]@{
            Status = 'WhatIf'
            Product = $script:Product
            Destination = $target
            BackupPath = $backupFull
            RestoredVersion = $restoreVersion
        }
    }

    $null = Ensure-SafeDirectory $destinationFull $localRoot 'VST3 destination'
    $null = Ensure-SafeDirectory $backupRootFull $localRoot 'backup root'
    $stageRoot = Join-Path $destinationFull ('.SkellGlitch-rollback-' + [Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $stageRoot -ErrorAction Stop | Out-Null
    $safetyPath = $null
    $safetyBundle = $null
    $movedCurrent = $false
    $restored = $false
    try {
        if ([bool]$info.had_existing) {
            Copy-Item -LiteralPath $restoreBundle -Destination $stageRoot -Recurse -ErrorAction Stop
            $stagedBundle = Join-Path $stageRoot $script:BundleName
            $stagedInfo = Assert-ValidBundle $stagedBundle
            if ($stagedInfo.TreeHash -cne $restoreInfo.TreeHash) {
                throw 'Staged rollback bundle differs from the preserved backup.'
            }
        }

        $safetyPath = New-BackupPath $backupRootFull
        $safetyBundle = Join-Path $safetyPath $script:BundleName
        if ($currentExists) {
            Move-Item -LiteralPath $target -Destination $safetyBundle -ErrorAction Stop
            $movedCurrent = $true
        }
        $safetyVersion = if ($currentInfo) { $currentInfo.Version } else { '' }
        $safetyHash = if ($currentInfo) { $currentInfo.TreeHash } else { '' }
        Write-BackupInfo -Path $safetyPath -HadExisting $currentExists -Version $safetyVersion -TreeHash $safetyHash -SourceSha ([string]$info.source_sha) -Target $target

        if ([bool]$info.had_existing) {
            Move-Item -LiteralPath (Join-Path $stageRoot $script:BundleName) -Destination $target -ErrorAction Stop
            $restored = $true
            $finalInfo = Assert-ValidBundle $target
            if ($finalInfo.TreeHash -cne $restoreInfo.TreeHash) {
                throw 'Restored VST3 bytes differ from the selected rollback backup.'
            }
        }

        return [pscustomobject]@{
            Status = 'RolledBack'
            Product = $script:Product
            Destination = $target
            BackupPath = $backupFull
            SafetyBackupPath = $safetyPath
            RestoredVersion = $restoreVersion
        }
    }
    catch {
        $failure = $_
        try {
            if ($restored -and (Test-Path -LiteralPath $target)) {
                Remove-Item -LiteralPath $target -Recurse -Force -ErrorAction Stop
            }
            if ($movedCurrent -and (Test-Path -LiteralPath $safetyBundle)) {
                Move-Item -LiteralPath $safetyBundle -Destination $target -ErrorAction Stop
            }
            if ($null -ne $safetyPath -and (Test-Path -LiteralPath $safetyPath)) {
                Remove-Item -LiteralPath $safetyPath -Recurse -Force -ErrorAction SilentlyContinue
            }
        }
        catch {
            throw "Rollback failed: $($failure.Exception.Message). Current installation could not be restored; inspect safety backup $safetyPath. Restoration error: $($_.Exception.Message)"
        }
        throw $failure
    }
    finally {
        if (Test-Path -LiteralPath $stageRoot) {
            Remove-Item -LiteralPath $stageRoot -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
}

if (-not $IsWindows) {
    throw 'The SkellGlitch preview installer supports Windows only.'
}
if ($PSVersionTable.PSVersion.Major -lt 7) {
    throw 'PowerShell 7 or newer is required.'
}

if ($Action -eq 'Install') {
    Install-Preview
}
else {
    Rollback-Preview
}
