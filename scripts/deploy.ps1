<#
.SYNOPSIS
    Builds a self-contained Windows distribution of "A Luz do Farol" and zips it.

.DESCRIPTION
    Produces a folder + a ready-to-send .zip containing the statically-linked
    game executable, the SDL2 runtime DLLs, all game resources, and the player
    config/readme. The result can be extracted and run on a clean Windows 10/11
    machine with no toolchain, MinGW runtime, or other installs.

    Steps:
      1. Build via `mingw32-make dist` (clean + optimized release with
         -static-libgcc/-static-libstdc++, then copies JOGO.exe + SDL DLLs +
         Recursos into .\dist).
      2. Add .env, config\settings.json and LEIA-ME.txt to .\dist.
      3. Verify JOGO.exe has no MinGW runtime DLL dependencies (objdump).
      4. Stage into a date-stamped folder and Compress-Archive it.

.PARAMETER Debug
    Build the DEBUG variant instead of the optimized release: uses
    `mingw32-make debug-dist` (-ggdb -O0 -DDEBUG, console window enabled),
    drops a DEBUG-BUILD.txt marker into the bundle, and names the folder/zip
    with a _DEBUG tag (e.g. A-Luz-do-Farol_DEBUG_2026-06-30.zip).

.NOTES
    Run from anywhere; it locates the repo root relative to this script.
    Requires mingw32-make (and ideally objdump) on PATH.
#>

param(
    [switch]$Debug
)

$ErrorActionPreference = 'Stop'

# --- release vs debug variant selection ---
if ($Debug) {
    $variant  = 'DEBUG'
    $makeGoal = 'debug-dist'
    $tag      = '_DEBUG'
} else {
    $variant  = 'release'
    $makeGoal = 'dist'
    $tag      = ''
}

function Fail($msg) {
    Write-Host ""
    Write-Host "ERRO: $msg" -ForegroundColor Red
    exit 1
}

# --- locate repo root (parent of this script's folder) ---
$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot
Write-Host "Repo: $repoRoot"

# --- tooling check ---
if (-not (Get-Command mingw32-make -ErrorAction SilentlyContinue)) {
    Fail "mingw32-make nao encontrado no PATH. Instale o MinGW e adicione ao PATH."
}

# --- 1. build (clean + static release + copy exe/DLLs/Recursos into .\dist) ---
Write-Host ""
Write-Host "==> mingw32-make $makeGoal (build $variant + bundle base)..." -ForegroundColor Cyan
& mingw32-make $makeGoal
if ($LASTEXITCODE -ne 0) { Fail "mingw32-make $makeGoal falhou (codigo $LASTEXITCODE)." }

$dist = Join-Path $repoRoot 'dist'
$exe  = Join-Path $dist 'JOGO.exe'
if (-not (Test-Path $exe)) { Fail "dist\JOGO.exe nao foi gerado." }

# --- 2. add config + readme into the bundle ---
Write-Host ""
Write-Host "==> Adicionando .env, config\settings.json e LEIA-ME.txt..." -ForegroundColor Cyan

# .env: prefer repo-root .env, fall back to .env.example
$envSrc = Join-Path $repoRoot '.env'
if (-not (Test-Path $envSrc)) {
    $envExample = Join-Path $repoRoot '.env.example'
    if (Test-Path $envExample) {
        Copy-Item $envExample $envSrc
        Write-Host "    (.env ausente; criado a partir de .env.example)"
    }
}
if (Test-Path $envSrc) {
    Copy-Item $envSrc (Join-Path $dist '.env') -Force
} else {
    Write-Host "    AVISO: nenhum .env disponivel; o jogo usara o volume padrao." -ForegroundColor Yellow
}

# config/settings.json: prefer existing, fall back to settings.example.json
$distConfig = Join-Path $dist 'config'
New-Item -ItemType Directory -Force -Path $distConfig | Out-Null
$settingsSrc = Join-Path $repoRoot 'config\settings.json'
if (-not (Test-Path $settingsSrc)) {
    $settingsExample = Join-Path $repoRoot 'config\settings.example.json'
    if (Test-Path $settingsExample) {
        $settingsSrc = $settingsExample
        Write-Host "    (config\settings.json ausente; usando settings.example.json)"
    } else {
        $settingsSrc = $null
        Write-Host "    AVISO: nenhum settings.json/example; o jogo usara o tamanho padrao." -ForegroundColor Yellow
    }
}
if ($settingsSrc) {
    Copy-Item $settingsSrc (Join-Path $distConfig 'settings.json') -Force
}

# readme (tracked template)
$readme = Join-Path $repoRoot 'deploy\LEIA-ME.txt'
if (Test-Path $readme) {
    Copy-Item $readme (Join-Path $dist 'LEIA-ME.txt') -Force
} else {
    Write-Host "    AVISO: deploy\LEIA-ME.txt nao encontrado; bundle sem readme." -ForegroundColor Yellow
}

# debug marker: make it obvious (inside the bundle, not just in the zip name)
# that this is a debug build with the console/log output enabled.
if ($Debug) {
    $marker = @(
        "BUILD DE DEBUG",
        "",
        "Este pacote foi compilado com -ggdb -O0 -DDEBUG e janela de console",
        "habilitada (saida/logs de DEBUG visiveis). Nao e a build de release.",
        "Use apenas para diagnostico; para jogar/enviar use a build normal."
    ) -join "`r`n"
    Set-Content -Path (Join-Path $dist 'DEBUG-BUILD.txt') -Value $marker -Encoding UTF8
    Write-Host "    + marcador DEBUG-BUILD.txt incluido." -ForegroundColor Green
}

# --- 3. ensure the MinGW runtime DLLs the exe needs are bundled ---
# -static-libgcc/-static-libstdc++ remove libgcc/libstdc++, but the POSIX-threads
# build still imports libwinpthread-1.dll dynamically. Bundle any such runtime
# DLL the exe actually depends on so the folder is self-contained.
Write-Host ""
Write-Host "==> Verificando/incluindo DLLs de runtime do JOGO.exe..." -ForegroundColor Cyan

$runtimeDlls = @(
    'libwinpthread-1.dll',
    'libgcc_s_dw2-1.dll',
    'libgcc_s_seh-1.dll',
    'libstdc++-6.dll'
)

function Find-RuntimeDll($name) {
    # 1) next to gcc/g++ (the MinGW bin dir) — most reliable for the .dll itself
    $cc = Get-Command gcc -ErrorAction SilentlyContinue
    if (-not $cc) { $cc = Get-Command g++ -ErrorAction SilentlyContinue }
    if ($cc) {
        $cand = Join-Path (Split-Path $cc.Source) $name
        if (Test-Path $cand) { return $cand }
        # 2) gcc's own library search path
        $p = (& $cc.Source -print-file-name=$name 2>$null)
        if ($p -and [System.IO.Path]::IsPathRooted($p) -and (Test-Path $p)) { return $p }
    }
    return $null
}

# Decide which runtime DLLs to bundle.
#   - With objdump: read the exe's real imports (precise) and treat a missing
#     required DLL as fatal.
#   - Without objdump: we cannot inspect imports, so bundle EVERY known MinGW
#     runtime DLL we can locate. Shipping an unused DLL is harmless; a missing
#     one crashes the game on the player's machine ("libwinpthread-1.dll nao
#     encontrada"). This is why the debug build failed before: objdump was
#     absent, so nothing got bundled.
$haveObjdump = [bool](Get-Command objdump -ErrorAction SilentlyContinue)
if ($haveObjdump) {
    $imports = & objdump -p $exe 2>$null |
               Select-String -Pattern 'DLL Name:\s*(\S+)' |
               ForEach-Object { $_.Matches[0].Groups[1].Value }
    $needed = @($imports | Where-Object { $runtimeDlls -contains $_ })
} else {
    Write-Host "    AVISO: objdump nao encontrado; incluindo todas as runtime DLLs conhecidas por precaucao." -ForegroundColor Yellow
    $needed = @($runtimeDlls)
}

# libwinpthread-1.dll is imported even by -static-libgcc/-static-libstdc++
# builds (MinGW POSIX threads), so always ensure it is bundled.
if ($needed -notcontains 'libwinpthread-1.dll') { $needed += 'libwinpthread-1.dll' }
$needed = $needed | Sort-Object -Unique

$bundledAny = $false
foreach ($dll in $needed) {
    if (Test-Path (Join-Path $dist $dll)) { $bundledAny = $true; continue }
    $src = Find-RuntimeDll $dll
    if ($src) {
        Copy-Item $src (Join-Path $dist $dll) -Force
        Write-Host "    + incluida runtime DLL: $dll" -ForegroundColor Green
        $bundledAny = $true
    } elseif ($haveObjdump) {
        # objdump said the exe imports this one, but we couldn't find it -> fatal.
        Fail "JOGO.exe precisa de $dll, mas nao consegui localiza-la (proximo ao gcc/g++). Copie-a manualmente para dist\."
    } else {
        # No objdump: this candidate simply isn't part of this toolchain
        # (e.g. the other libgcc variant, or a fully static build). Skip it.
        Write-Host "    (ignorada: $dll nao encontrada perto do gcc; provavelmente nao e necessaria)"
    }
}
if (-not $bundledAny) {
    Write-Host "    OK: nenhuma DLL de runtime do MinGW necessaria." -ForegroundColor Green
}

# --- 4. zip the bundle CONTENTS (no wrapper folder) ---
# IMPORTANT: zip the files at the archive ROOT, not inside a folder named after
# the bundle. Windows Explorer's "Extract All" already creates a folder named
# after the .zip, so if the archive ALSO contained a same-named folder the
# extracted tree would be doubly nested (A-Luz.../A-Luz.../Recursos/...). That
# extra level pushes deep resource paths past Windows' 260-char MAX_PATH limit
# and triggers "Erro 0x80010135: Caminho muito longo" on extraction. Zipping the
# contents directly keeps a single level and shortens every path by ~32 chars.
$stamp   = Get-Date -Format 'yyyy-MM-dd'
$bundle  = "A-Luz-do-Farol${tag}_$stamp"
$zipPath = Join-Path $repoRoot "$bundle.zip"

Write-Host ""
Write-Host "==> Empacotando $bundle.zip..." -ForegroundColor Cyan
if (Test-Path $zipPath) { Remove-Item $zipPath -Force }

# Compress the bundle's top-level items (JOGO.exe, Recursos\, config\, DLLs,
# .env, LEIA-ME.txt, ...) so they land at the zip ROOT. Use -Force so hidden
# files such as .env are included (a bare "dist\*" wildcard would skip them).
$items = Get-ChildItem -LiteralPath $dist -Force
if (-not $items) { Fail "dist\ esta vazio; nada para empacotar." }
Compress-Archive -Path $items.FullName -DestinationPath $zipPath -Force

if (-not (Test-Path $zipPath)) { Fail "Falha ao criar o .zip." }

$zipSize = "{0:N1} MB" -f ((Get-Item $zipPath).Length / 1MB)
Write-Host ""
Write-Host "Deploy concluido! (build: $variant)" -ForegroundColor Green
Write-Host "  Pasta : $dist"
Write-Host "  Zip   : $zipPath ($zipSize)"
Write-Host ""
Write-Host "Envie o .zip. O destinatario extrai e executa JOGO.exe (Win10/11)."
