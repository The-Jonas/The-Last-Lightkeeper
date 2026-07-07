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

.NOTES
    Run from anywhere; it locates the repo root relative to this script.
    Requires mingw32-make (and ideally objdump) on PATH.
#>

$ErrorActionPreference = 'Stop'

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
Write-Host "==> mingw32-make dist (build estatico + bundle base)..." -ForegroundColor Cyan
& mingw32-make dist
if ($LASTEXITCODE -ne 0) { Fail "mingw32-make dist falhou (codigo $LASTEXITCODE)." }

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

if (Get-Command objdump -ErrorAction SilentlyContinue) {
    $imports = & objdump -p $exe 2>$null |
               Select-String -Pattern 'DLL Name:\s*(\S+)' |
               ForEach-Object { $_.Matches[0].Groups[1].Value }
    $needed = $imports | Where-Object { $runtimeDlls -contains $_ } | Sort-Object -Unique
    if ($needed) {
        foreach ($dll in $needed) {
            if (Test-Path (Join-Path $dist $dll)) { continue }
            $src = Find-RuntimeDll $dll
            if ($src) {
                Copy-Item $src (Join-Path $dist $dll) -Force
                Write-Host "    + incluida runtime DLL: $dll" -ForegroundColor Green
            } else {
                Fail "JOGO.exe precisa de $dll, mas nao consegui localiza-la (proximo ao gcc/g++). Copie-a manualmente para dist\."
            }
        }
    } else {
        Write-Host "    OK: nenhuma DLL de runtime do MinGW necessaria." -ForegroundColor Green
    }
} else {
    Write-Host "    AVISO: objdump nao encontrado; nao foi possivel verificar runtime DLLs." -ForegroundColor Yellow
    Write-Host "           Se o jogo nao abrir em outra maquina, copie libwinpthread-1.dll para dist\." -ForegroundColor Yellow
}

# --- 4. stage into a date-stamped folder and zip it ---
$stamp   = Get-Date -Format 'yyyy-MM-dd'
$bundle  = "A-Luz-do-Farol_$stamp"
$staging = Join-Path $repoRoot $bundle
$zipPath = Join-Path $repoRoot "$bundle.zip"

Write-Host ""
Write-Host "==> Empacotando $bundle.zip..." -ForegroundColor Cyan
if (Test-Path $staging) { Remove-Item $staging -Recurse -Force }
if (Test-Path $zipPath) { Remove-Item $zipPath -Force }

New-Item -ItemType Directory -Force -Path $staging | Out-Null
Copy-Item (Join-Path $dist '*') $staging -Recurse -Force

try {
    Compress-Archive -Path $staging -DestinationPath $zipPath -Force
} finally {
    # leave dist\ as the on-disk working bundle; remove only the staging copy
    if (Test-Path $staging) { Remove-Item $staging -Recurse -Force }
}

if (-not (Test-Path $zipPath)) { Fail "Falha ao criar o .zip." }

$zipSize = "{0:N1} MB" -f ((Get-Item $zipPath).Length / 1MB)
Write-Host ""
Write-Host "Deploy concluido!" -ForegroundColor Green
Write-Host "  Pasta : $dist"
Write-Host "  Zip   : $zipPath ($zipSize)"
Write-Host ""
Write-Host "Envie o .zip. O destinatario extrai e executa JOGO.exe (Win10/11)."
