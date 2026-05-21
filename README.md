# A-Luz-do-Farol---Jogo
Um jogo desenvolvido em grupo, para a disciplina de Introdução a Desenvolvimento de Jogos da Universidade de Brasília

## Arquitetura (resumo)

- Pilha de estados (`Game`): típico fluxo **Title → Loading → Stage**; o gameplay pesado está em **`StageState`**.
- **`Game::TryGetStageState()`** devolve **`nullptr`** quando o estado atual não é o estágio — componentes devem sempre checar antes de usar (evita casts inseguros).
- **`StageState`** está dividido em vários `.cpp` em `src/states/stage/` (Load, Update, Render, Navigation, Lighting, party input, lifecycle); mapa em **[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)**.
- Código-fonte organizado por domínio em pastas (`core/`, `engine/`, `gameplay/`, `states/stage/`, etc.) — ver **[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)**.
- **`StageOceanAmbientController`** concentra o loop de ondas (`Mix_Chunk` no canal reservado), mantendo os mesmos volumes/comportamentos de antes.

## Configuração

| Fonte | Conteúdo |
|-------|-----------|
| **`.env`** (opcional, pasta ao rodar o exe) | `MASTER_VOLUME=0–100`. Linhas `#` ignoradas; chaves/valores trimados; valores ruins são ignorados. |
| **`config/settings.json`** (opcional) | `window_width`, `window_height`. Ausente ou JSON inválido → usa `Game::WINDOW_WIDTH` / `Game::WINDOW_HEIGHT` em **`include/Game.h`**. Exemplo: **`config/settings.example.json`**. |

## Dados (`Recursos/data`)

- **`Recursos/data/stage_first_load.json`** — define OST, mapa, navegação, ciclo de pickups no chão, lanterna inicial e candidatos do áudio das ondas.
- Se o arquivo faltar ou falhar ao ler, o jogo usa **fallback embutido** com os mesmos valores antigos e registra aviso no stderr (`src/states/stage/FirstLoadLoader.cpp`).

## Build de desenvolvimento (Windows)

```bat
mingw32-make debug
```

## Pasta `dist/` — build de release para compartilhar

Gera **`dist\JOGO.exe`**, as DLLs do SDL em **`dist\`** e a cópia de **`dist\Recursos\`** com os mesmos caminhos do jogo (`Recursos\...`). O jogador deve **executar o `.exe` a partir dentro da pasta `dist`** (ou zipar **`dist`** inteira e enviar).

**Release no Windows:** o `makefile` pode ligar **`-static-libgcc -static-libstdc++`** nos alvos `release` / `package` / `dist`, para máquinas sem runtime MinGW. Se algum binário não estático reclamar de **`libgcc_s_*.dll`** / **`libstdc++-6.dll`**, use **`mingw32-make dist`** conforme o makefile ou instale o runtime correspondente.

**Comando principal (clean + release + cópia):**

```bat
mingw32-make dist
```

(Na pasta raiz do projeto, onde está o `makefile`.)

**Alternativas:**

| Comando | Efeito |
|--------|--------|
| `mingw32-make package` ou `mingw32-make ship` | Só empacota; não apaga objetos intermediários primeiro |
| `mingw32-make dist DIST_DIR=pasta_qualquer` | Coloca o bundle em `pasta_qualquer\` |

**Via script:**

```bat
scripts\build-dist.bat
```

**Depois:** compacte **`dist`** em um `.zip` e publique em itch.io, Drive, etc. Inclua na descrição: “descompactar e rodar **`JOGO.exe`** dentro da pasta”.

## Verificação local (pre-push)

Sem CI obrigatório no repositório: antes de enviar alterações, rode **`mingw32-make debug`** e um smoke rápido (menu → carregar → estágio → inventário/luz/movimento). Detalhes em **[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)**.
