# Self-test every table in the Full Tilt Native library: each table plays itself for a while (FullTiltEngine
# --selftest) and the result is listed. Run it from the Full Tilt Native folder (where FullTiltEngine.exe is):
#   powershell -ExecutionPolicy Bypass -File selftest.ps1 [-Seconds 120]
# A table FAILs when it crashes, a ball never drains, a ball gets stuck, or no ball can be launched.
param(
    [int]$Seconds = 120,
    [string]$Engine = (Join-Path $PSScriptRoot "FullTiltEngine.exe"),
    [string]$Tables = (Join-Path $PSScriptRoot "tables")
)
if (-not (Test-Path $Engine)) { Write-Error "FullTiltEngine.exe not found at $Engine"; exit 2 }
$log = Join-Path (Split-Path $Engine) "FullTiltEngine.log"
$failed = 0
foreach ($ini in Get-ChildItem -Path $Tables -Filter table.ini -Recurse -Depth 1) {
    $data = (Get-Content $ini.FullName | Where-Object { $_ -match '^Data=' } | Select-Object -First 1) -replace '^Data=', ''
    if (-not $data -or $data -notmatch '\.DAT$') { continue }   # Epic Pinball and other non-Full Tilt entries
    $folder = $ini.DirectoryName
    $before = if (Test-Path $log) { (Get-Content $log).Count } else { 0 }
    $p = Start-Process -FilePath $Engine -ArgumentList "--table $data --selftest $Seconds" -WorkingDirectory $folder -PassThru -Wait
    $lines = if (Test-Path $log) { Get-Content $log | Select-Object -Skip $before } else { @() }
    $result = $lines | Where-Object { $_ -match 'SELFTEST|CRASH' } | Select-Object -First 1
    if (-not $result) { $result = "no result (exit code $($p.ExitCode))" }
    if ($p.ExitCode -ne 0 -or $result -notmatch 'result=PASS') { $failed++ }
    Write-Output ("{0,-28} {1}" -f (Split-Path $folder -Leaf), $result)
}
if ($failed) { Write-Output "$failed table(s) failed"; exit 1 } else { Write-Output "All tables passed" }
