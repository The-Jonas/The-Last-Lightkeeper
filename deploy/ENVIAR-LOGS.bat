@echo off
rem ===========================================================================
rem  A LUZ DO FAROL - juntar os registos do playtest num unico .zip
rem
rem  Clique duas vezes neste ficheiro DEPOIS de jogar. Ele cria um .zip na
rem  mesma pasta com:
rem     logs\playtest\*.jsonl   o que aconteceu em cada partida
rem     logs\latest.log         o registo tecnico da ultima execucao
rem     crash-reports\          relatorios, se o jogo tiver fechado sozinho
rem  Envie esse .zip para a equipa. Nada e enviado pela internet por este
rem  ficheiro: ele so faz o .zip.
rem ===========================================================================
setlocal
title A Luz do Farol - juntar registos de playtest
cd /d "%~dp0"

echo.
echo  A juntar os registos...
echo.

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$stamp = Get-Date -Format 'yyyyMMdd_HHmmss';" ^
  "$name = 'playtest_' + $env:COMPUTERNAME + '_' + $stamp + '.zip';" ^
  "$items = @('logs', 'crash-reports') | Where-Object { Test-Path $_ };" ^
  "if (-not $items) { Write-Host '  Nao encontrei nenhuma pasta de registos. Jogou pelo menos uma vez nesta pasta?' -ForegroundColor Yellow; exit 1 };" ^
  "Compress-Archive -Path $items -DestinationPath $name -Force;" ^
  "Write-Host ('  Pronto: ' + $name) -ForegroundColor Green;" ^
  "Write-Host '  Envie este ficheiro para a equipa.'"

echo.
pause
