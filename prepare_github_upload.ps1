# PowerShell script to prepare files for GitHub upload
# This script collects all .nts1mkiiunit files and HOW_TO_*.txt files

$sourceDir = "platform\nts-1_mkii"
$outputDir = "github_upload"
$oscDir = "$outputDir\oscillators"
$modfxDir = "$outputDir\modfx"
$delfxDir = "$outputDir\delfx"
$revfxDir = "$outputDir\revfx"

# Create output directories
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
New-Item -ItemType Directory -Force -Path $oscDir | Out-Null
New-Item -ItemType Directory -Force -Path $modfxDir | Out-Null
New-Item -ItemType Directory -Force -Path $delfxDir | Out-Null
New-Item -ItemType Directory -Force -Path $revfxDir | Out-Null

Write-Host "Collecting units and documentation..." -ForegroundColor Green

# Function to determine unit type from manifest.json
function Get-UnitType {
    param($unitDir)
    
    $manifestPath = Join-Path $unitDir "manifest.json"
    if (Test-Path $manifestPath) {
        try {
            $manifest = Get-Content $manifestPath | ConvertFrom-Json
            return $manifest.header.module
        } catch {
            return $null
        }
    }
    return $null
}

# Find all unit directories
$unitDirs = Get-ChildItem -Path $sourceDir -Directory | Where-Object {
    $_.Name -notmatch "^(common|ld|inc|backup|finetuning|dummy-|\(dummy\)|waves|Thuis)" -and
    (Test-Path (Join-Path $_.FullName "*.nts1mkiiunit"))
}

$units = @()

foreach ($unitDir in $unitDirs) {
    $unitName = $unitDir.Name
    $unitFile = Get-ChildItem -Path $unitDir.FullName -Filter "*.nts1mkiiunit" | Select-Object -First 1
    $howToFile = Get-ChildItem -Path $unitDir.FullName -Filter "HOW_TO_*.txt" | Select-Object -First 1
    
    if ($unitFile) {
        $unitType = Get-UnitType -unitDir $unitDir.FullName
        
        # Determine target directory
        $targetDir = $oscDir
        switch ($unitType) {
            "osc" { $targetDir = $oscDir }
            "modfx" { $targetDir = $modfxDir }
            "delfx" { $targetDir = $delfxDir }
            "revfx" { $targetDir = $revfxDir }
            default { $targetDir = $oscDir }
        }
        
        # Copy unit file
        Copy-Item -Path $unitFile.FullName -Destination (Join-Path $targetDir $unitFile.Name) -Force
        
        # Copy HOW_TO file if exists
        if ($howToFile) {
            Copy-Item -Path $howToFile.FullName -Destination (Join-Path $targetDir $howToFile.Name) -Force
        }
        
        $units += [PSCustomObject]@{
            Name = $unitName
            Type = if ($unitType) { $unitType } else { "osc" }
            UnitFile = $unitFile.Name
            HasHowTo = $null -ne $howToFile
        }
        
        Write-Host "  OK $unitName ($unitType)" -ForegroundColor Cyan
    }
}

Write-Host ""
Write-Host "Total units collected: $($units.Count)" -ForegroundColor Green
Write-Host "  Oscillators: $(($units | Where-Object { $_.Type -eq 'osc' }).Count)" -ForegroundColor Yellow
Write-Host "  ModFX: $(($units | Where-Object { $_.Type -eq 'modfx' }).Count)" -ForegroundColor Yellow
Write-Host "  DelFX: $(($units | Where-Object { $_.Type -eq 'delfx' }).Count)" -ForegroundColor Yellow
Write-Host "  RevFX: $(($units | Where-Object { $_.Type -eq 'revfx' }).Count)" -ForegroundColor Yellow

Write-Host ""
Write-Host "Files prepared in: $outputDir" -ForegroundColor Green
Write-Host "Ready for GitHub upload!" -ForegroundColor Green
