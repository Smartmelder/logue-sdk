# Rebuild all NTS-1 mkII units using Docker
# This script rebuilds all units to ensure .nts1mkiiunit files are up to date

$ErrorActionPreference = "Continue"

$units = @(
    "pan_trem",
    "freq_shift",
    "cathedral_smooth",
    "tr909",
    "td3_acid",
    "rave_engine",
    "orch_hit",
    "m1_piano_pm",
    "m1_brass_ultra",
    "m1_brass",
    "juno106",
    "jp8000",
    "gabber_bass",
    "disco_fall",
    "ultra_wide",
    "tape_wobble",
    "seq_filter",
    "rand_repeat",
    "kaoss_loop"
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$platformPath = Resolve-Path (Join-Path $scriptDir "platform")

$IMAGE_NAME = "xiashj/logue-sdk"
$IMAGE_VERSION = "latest"

$failed = @()
$succeeded = @()

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Rebuilding NTS-1 mkII units" -ForegroundColor Cyan
Write-Host "Total units: $($units.Count)" -ForegroundColor Cyan
Write-Host "Platform path: $platformPath" -ForegroundColor Gray
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

foreach ($unit in $units) {
    $unitNum = $units.IndexOf($unit) + 1
    Write-Host "[$unitNum/$($units.Count)] Building $unit..." -ForegroundColor Yellow
    
    try {
        # Use Docker with platform path mounted as /workspace
        $dockerCmd = "docker run --rm -v `"${platformPath}:/workspace`" -h logue-sdk ${IMAGE_NAME}:${IMAGE_VERSION} /app/cmd_entry build nts-1_mkii/$unit"
        
        $output = Invoke-Expression $dockerCmd 2>&1 | Out-String
        
        # Check if build was successful
        if ($LASTEXITCODE -eq 0 -and ($output -match "Done" -or $output -match "Deploying to")) {
            Write-Host "  ✓ Success" -ForegroundColor Green
            $succeeded += $unit
        } else {
            Write-Host "  ✗ Failed" -ForegroundColor Red
            # Show last few lines of output for debugging
            $outputLines = $output -split "`n" | Select-Object -Last 8
            foreach ($line in $outputLines) {
                if ($line -match "error|Error|fatal|Failed") {
                    Write-Host "    $line" -ForegroundColor Red
                } else {
                    Write-Host "    $line" -ForegroundColor Gray
                }
            }
            $failed += $unit
        }
    }
    catch {
        Write-Host "  ✗ Error: $_" -ForegroundColor Red
        $failed += $unit
    }
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Build Summary" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Succeeded: $($succeeded.Count)" -ForegroundColor Green
Write-Host "Failed: $($failed.Count)" -ForegroundColor Red

if ($succeeded.Count -gt 0) {
    Write-Host ""
    Write-Host "Successfully rebuilt:" -ForegroundColor Green
    foreach ($unit in $succeeded) {
        Write-Host "  ✓ $unit" -ForegroundColor Green
    }
}

if ($failed.Count -gt 0) {
    Write-Host ""
    Write-Host "Failed units:" -ForegroundColor Red
    foreach ($unit in $failed) {
        Write-Host "  ✗ $unit" -ForegroundColor Red
    }
    Write-Host ""
    Write-Host "Tip: Check the output above for error details." -ForegroundColor Yellow
}
