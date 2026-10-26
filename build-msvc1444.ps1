#Requires -Version 5.1
<#
    Build del cliente MU 0.97k SIEMPRE con MSVC 14.44.35207 (toolset v143).

    Motivo: en esta maquina el toolset "v143" por defecto resuelve a 14.43.34808
    (Microsoft.VCToolsVersion.v143.default.txt) y el default global es 14.51.36231.
    Sin pin explicito el binario queda compilado con otra version.

    Se compila con el proyecto Visual Studio (mu97k.sln / mu97k.vcxproj), que es
    el que incluye src/UI/JewelBank.cpp. El build por CMake no lo lista y falla
    con LNK2001.

    Uso:
        .\build-msvc1444.ps1                 # Release Win32
        .\build-msvc1444.ps1 -Configuration Debug
        .\build-msvc1444.ps1 -NoRebuild      # incremental
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [switch]$NoRebuild
)

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
if (-not $root) { $root = (Get-Location).Path }

$wanted        = '14.44.35207'
$wantedClMajor = '19.44.'

function Fail([string]$msg) {
    Write-Host "ERROR: $msg" -ForegroundColor Red
    exit 1
}

# 1) Instalacion de Visual Studio con herramientas C++ x86/x64
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) { Fail "vswhere.exe no encontrado en $vswhere" }

$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { Fail 'No hay instalacion de VS con herramientas C++ x86/x64' }
Write-Host "VS          : $vs"

# 2) Toolset exacto
$toolDir = Join-Path $vs "VC\Tools\MSVC\$wanted"
if (-not (Test-Path $toolDir)) { Fail "MSVC $wanted no esta instalado ($toolDir)" }

# 3) cl.exe real
$cl = Join-Path $toolDir 'bin\Hostx86\x86\cl.exe'
if (-not (Test-Path $cl)) { Fail "cl.exe no encontrado: $cl" }

$oldEap = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
$clOut = (& $cl 2>&1 | Out-String)
$ErrorActionPreference = $oldEap

if ($clOut -notmatch [regex]::Escape($wantedClMajor)) {
    Fail "cl.exe no reporta $wantedClMajor :`n$clOut"
}
$clLine = ($clOut -split "`r?`n" | Where-Object { $_.Trim() } | Select-Object -First 1).Trim()
Write-Host "CL          : $clLine"

# 4) MSBuild
$msbuild = Join-Path $vs 'MSBuild\Current\Bin\MSBuild.exe'
if (-not (Test-Path $msbuild)) { Fail "MSBuild no encontrado: $msbuild" }
Write-Host "MSBuild     : $msbuild"

$sln = Join-Path $root 'mu97k.sln'
if (-not (Test-Path $sln)) { Fail "No existe $sln" }

$outExe = Join-Path $root 'bin\Client\main.exe'

# 5) Backup con nombre nuevo del main.exe anterior
$backup = $null
if (Test-Path $outExe) {
    $h = (Get-FileHash $outExe -Algorithm SHA256).Hash
    $backup = "$outExe`_BACKUP_PRE_BUILD_$($h.Substring(0,8))"
    if (-not (Test-Path $backup)) {
        Copy-Item $outExe $backup
        Write-Host "Backup      : $backup"
    } else {
        Write-Host "Backup      : $backup (ya existia)"
    }
}

# 6) Build con pin explicito
$target = '/t:Build'
if (-not $NoRebuild) { $target = '/t:Rebuild' }

$buildArgs = @(
    $sln,
    $target,
    '/m',
    '/nologo',
    '/v:minimal',
    "/p:Configuration=$Configuration",
    '/p:Platform=Win32',
    '/p:PlatformToolset=v143',
    "/p:VCToolsVersion=$wanted"
)

Write-Host "Target      : $target  $Configuration|Win32  v143 / $wanted"
& $msbuild @buildArgs
$rc = $LASTEXITCODE

# 7) Si el link fallo y borro el exe, restaurar el anterior
if (-not (Test-Path $outExe) -and $backup -and (Test-Path $backup)) {
    Copy-Item $backup $outExe -Force
    Write-Host "main.exe restaurado desde backup tras el fallo" -ForegroundColor Yellow
}

if ($rc -ne 0) { Fail "build fallo (exit $rc)" }
if (-not (Test-Path $outExe)) { Fail "No se genero $outExe" }

# 8) Verificacion del linker del binario (14.44)
$fs = [IO.File]::OpenRead($outExe)
$br = New-Object IO.BinaryReader($fs)
try {
    $fs.Position = 0x3C
    $peOff = $br.ReadInt32()
    $fs.Position = $peOff + 24
    $magic = $br.ReadUInt16()
    $maj = $br.ReadByte()
    $min = $br.ReadByte()
} finally { $fs.Close() }

if ("$maj.$min" -ne '14.44') { Fail "El linker del exe es $maj.$min (se esperaba 14.44)" }

$fi  = Get-Item $outExe
$sha = (Get-FileHash $outExe -Algorithm SHA256).Hash

Write-Host ''
Write-Host "OK  $outExe" -ForegroundColor Green
Write-Host "    $($fi.Length) bytes"
Write-Host "    SHA-256 $sha"
Write-Host "    linker 14.44 / MSVC $wanted / VS project / $Configuration|Win32"
if ($backup) { Write-Host "    backup previo: $backup" }
