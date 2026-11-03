# install_build_tools.ps1 - installs what "Compile Epic Pinball tables" needs, for the current user only:
#   Python 3 (python.org package), the GCC C compiler (WinLibs MinGW-w64) and the capstone Python module.
# Run by the launcher's "Install build tools" button; uses winget (App Installer, part of Windows 10/11).
# Prints "PROGRESS <0..1> <step>" lines for the launcher's progress bar and ends with DONE or ERROR.

$ErrorActionPreference = 'Continue'
$inv = [Globalization.CultureInfo]::InvariantCulture
function Progress([double]$f, [string]$text) { Write-Output ("PROGRESS " + $f.ToString("0.000", $inv) + " " + $text) }

function Refresh-Path {
    $user = [Environment]::GetEnvironmentVariable('Path', 'User')
    $machine = [Environment]::GetEnvironmentVariable('Path', 'Machine')
    $env:Path = "$machine;$user;$env:LOCALAPPDATA\Microsoft\WinGet\Links"
}

function Find-Python {
    Refresh-Path
    $py = Get-Command py -ErrorAction SilentlyContinue
    if ($py) { & $py.Source -3 -c "import sys" 2>$null; if ($LASTEXITCODE -eq 0) { return @($py.Source, '-3') } }
    foreach ($p in (Get-ChildItem "$env:LOCALAPPDATA\Programs\Python\Python3*\python.exe" -ErrorAction SilentlyContinue |
                    Sort-Object FullName -Descending)) { return @($p.FullName) }
    $python = Get-Command python -ErrorAction SilentlyContinue
    # the Microsoft Store stub in WindowsApps only opens the Store; it is not a real Python
    if ($python -and $python.Source -notlike '*WindowsApps*') { return @($python.Source) }
    return $null
}

function Have-Compiler {
    Refresh-Path
    if (Get-Command gcc -ErrorAction SilentlyContinue) { return $true }
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($vs) { return $true }
    }
    return $false
}

Progress 0.0 'Checking what is already installed'
if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
    Write-Output "ERROR: winget is not available. Install 'App Installer' from the Microsoft Store, or install Python 3 and MinGW-w64 yourself."
    exit 1
}
$wingetArgs = @('--exact', '--silent', '--accept-package-agreements', '--accept-source-agreements', '--disable-interactivity')

$python = Find-Python
if ($python) {
    Write-Output "Python is already installed: $($python[0])"
} else {
    Progress 0.05 'Installing Python 3 (winget, a minute or two)'
    winget install --id Python.Python.3.12 --scope user @wingetArgs
    $python = Find-Python
    if (-not $python) { Write-Output 'ERROR: Python did not install. Install it from python.org and try again.'; exit 1 }
}

if (Have-Compiler) {
    Write-Output 'A C compiler is already installed.'
} else {
    Progress 0.40 'Installing the GCC C compiler (WinLibs, a few minutes)'
    winget install --id BrechtSanders.WinLibs.POSIX.UCRT @wingetArgs
    if (-not (Have-Compiler)) { Write-Output 'ERROR: the C compiler did not install. Install MinGW-w64 (or Build Tools for Visual Studio) and try again.'; exit 1 }
}

Progress 0.85 'Installing the capstone module for Python'
$pyExe = $python[0]
$pyArgs = @()
if ($python.Count -gt 1) { $pyArgs += $python[1..($python.Count - 1)] }
& $pyExe @pyArgs -m pip install --user --upgrade capstone
if ($LASTEXITCODE -ne 0) { Write-Output 'ERROR: pip could not install capstone (check the internet connection).'; exit 1 }

Progress 1.0 'Done'
Write-Output 'DONE: build tools installed. Click "Compile Epic Pinball tables" next.'
