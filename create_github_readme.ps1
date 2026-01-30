# PowerShell script to create README.md for GitHub
# This reads all units and generates a comprehensive README

$sourceDir = "platform\nts-1_mkii"
$readmePath = "github_upload\README.md"

# Function to get unit type
function Get-UnitType {
    param($unitDir)
    $manifestPath = Join-Path $unitDir "manifest.json"
    if (Test-Path $manifestPath) {
        $manifest = Get-Content $manifestPath | ConvertFrom-Json
        return $manifest.header.module
    }
    return "osc"
}

# Function to read HOW_TO file
function Get-UnitInfo {
    param($unitDir)
    
    $howToFile = Get-ChildItem -Path $unitDir.FullName -Filter "HOW_TO_*.txt" | Select-Object -First 1
    if ($howToFile) {
        $content = Get-Content $howToFile.FullName -Raw
        return $content
    }
    return $null
}

# Find all units
$unitDirs = Get-ChildItem -Path $sourceDir -Directory | Where-Object {
    $_.Name -notmatch "^(common|ld|inc|backup|finetuning|dummy-|\(dummy\)|waves|Thuis)" -and
    (Test-Path (Join-Path $_.FullName "*.nts1mkiiunit"))
}

$oscUnits = @()
$modfxUnits = @()
$delfxUnits = @()
$revfxUnits = @()

foreach ($unitDir in $unitDirs) {
    $unitName = $unitDir.Name
    $unitType = Get-UnitType -unitDir $unitDir.FullName
    $unitFile = Get-ChildItem -Path $unitDir.FullName -Filter "*.nts1mkiiunit" | Select-Object -First 1
    $info = Get-UnitInfo -unitDir $unitDir.FullName
    
    $unitInfo = [PSCustomObject]@{
        Name = $unitName
        File = $unitFile.Name
        Info = $info
    }
    
    switch ($unitType) {
        "osc" { $oscUnits += $unitInfo }
        "modfx" { $modfxUnits += $unitInfo }
        "delfx" { $delfxUnits += $unitInfo }
        "revfx" { $revfxUnits += $unitInfo }
        default { $oscUnits += $unitInfo }
    }
}

# Generate README
$readme = @"
# Korg NTS-1 mkII Custom Units

A collection of custom oscillators, modulation effects, delay effects, and reverb effects for the Korg NTS-1 mkII synthesizer.

## 📦 Installation

1. Download the `.nts1mkiiunit` file for the unit you want to install
2. Connect your NTS-1 mkII to your computer via USB
3. Copy the `.nts1mkiiunit` file to the appropriate folder on your NTS-1 mkII:
   - **Oscillators**: `USER_OSC/`
   - **Modulation Effects**: `USER_MODFX/`
   - **Delay Effects**: `USER_DELFX/`
   - **Reverb Effects**: `USER_REVFX/`
4. Disconnect and restart your NTS-1 mkII
5. Select the unit from the menu

## 📚 Documentation

Each unit includes a `HOW_TO_*.txt` file with detailed information about:
- Features
- Hardware controls
- All parameters

## 🎹 Oscillators ($($oscUnits.Count) units)

"@

foreach ($unit in $oscUnits | Sort-Object Name) {
    $readme += "`n### $($unit.Name)`n"
    if ($unit.Info) {
        $features = ($unit.Info -split "`n" | Where-Object { $_ -match "^FEATURES:" -or ($_ -match "^- " -and $_ -notmatch "^HARDWARE" -and $_ -notmatch "^PARAMETERS") }) -join "`n"
        if ($features) {
            $readme += $features + "`n"
        }
    }
    $readme += "`n**File**: \````$($unit.File)````"
    if ($unit.Info) {
        $readme += "`n**Documentation**: \`````HOW_TO_$($unit.Name).txt````"
    }
    $readme += "`n"
}

$readme += @"

## 🎛️ Modulation Effects ($($modfxUnits.Count) units)

"@

foreach ($unit in $modfxUnits | Sort-Object Name) {
    $readme += "`n### $($unit.Name)`n"
    if ($unit.Info) {
        $features = ($unit.Info -split "`n" | Where-Object { $_ -match "^FEATURES:" -or ($_ -match "^- " -and $_ -notmatch "^HARDWARE" -and $_ -notmatch "^PARAMETERS") }) -join "`n"
        if ($features) {
            $readme += $features + "`n"
        }
    }
    $readme += "`n**File**: \`````$($unit.File)````"
    if ($unit.Info) {
        $readme += "`n**Documentation**: \`````HOW_TO_$($unit.Name).txt````"
    }
    $readme += "`n"
}

$readme += @"

## ⏱️ Delay Effects ($($delfxUnits.Count) units)

"@

foreach ($unit in $delfxUnits | Sort-Object Name) {
    $readme += "`n### $($unit.Name)`n"
    if ($unit.Info) {
        $features = ($unit.Info -split "`n" | Where-Object { $_ -match "^FEATURES:" -or ($_ -match "^- " -and $_ -notmatch "^HARDWARE" -and $_ -notmatch "^PARAMETERS") }) -join "`n"
        if ($features) {
            $readme += $features + "`n"
        }
    }
    $readme += "`n**File**: \`````$($unit.File)````"
    if ($unit.Info) {
        $readme += "`n**Documentation**: \`````HOW_TO_$($unit.Name).txt````"
    }
    $readme += "`n"
}

$readme += @"

## 🎚️ Reverb Effects ($($revfxUnits.Count) units)

"@

foreach ($unit in $revfxUnits | Sort-Object Name) {
    $readme += "`n### $($unit.Name)`n"
    if ($unit.Info) {
        $features = ($unit.Info -split "`n" | Where-Object { $_ -match "^FEATURES:" -or ($_ -match "^- " -and $_ -notmatch "^HARDWARE" -and $_ -notmatch "^PARAMETERS") }) -join "`n"
        if ($features) {
            $readme += $features + "`n"
        }
    }
    $readme += "`n**File**: \`````$($unit.File)````"
    if ($unit.Info) {
        $readme += "`n**Documentation**: \`````HOW_TO_$($unit.Name).txt````"
    }
    $readme += "`n"
}

$readme += @"

## 📝 License

These units are provided as-is for use with the Korg NTS-1 mkII. Please refer to individual unit documentation for specific usage instructions.

## 🙏 Credits

Created for the Korg NTS-1 mkII community.

---

**Total Units**: $($oscUnits.Count + $modfxUnits.Count + $delfxUnits.Count + $revfxUnits.Count)
- Oscillators: $($oscUnits.Count)
- Modulation Effects: $($modfxUnits.Count)
- Delay Effects: $($delfxUnits.Count)
- Reverb Effects: $($revfxUnits.Count)
"@

# Save README
New-Item -ItemType Directory -Force -Path (Split-Path $readmePath) | Out-Null
$readme | Out-File -FilePath $readmePath -Encoding UTF8

Write-Host "README.md created at: $readmePath" -ForegroundColor Green

