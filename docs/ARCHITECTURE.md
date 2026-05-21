# Architecture — A Luz do Farol

Estado alto nível do projeto após o refactor de paridade (mesmo comportamento de gameplay; código mais seguro e modular).

## Layout de código

Código organizado por domínio em `src/` e `include/` (mesma árvore). Includes usam caminhos relativos a `include/`, por exemplo `#include "core/Game.h"`.

| Pasta | Conteúdo |
|-------|----------|
| `core/` | `Game`, `State`, `Resources`, `InputManager`, `LevelManager`, `Timer`, `GameData` |
| `math/` | `Vec2`, `Rect` |
| `engine/` | `GameObject`, `Component`, `Camera`, sprites e animação |
| `audio/` | `Music`, `Sound` |
| `ui/` | `Text`, `FadeEffect`, `VideoPlayer` |
| `world/` | `TileMap`, `TileSet`, `Collider`, `Collision` |
| `lighting/` | sombras, máscaras de luz, overlays |
| `gameplay/` | personagem, inventário, interações (caixas, escadas, reparos) |
| `states/` | telas (`Title`, `Loading`, `End`, `Cutscene`) |
| `states/stage/` | gameplay do estágio (`StageState` e subsistemas) |

Na raiz de `include/` ficam apenas dependências compartilhadas: `SDL_include.h`, `pl_mpeg.h`, `nlohmann/`.

## Fluxo do loop

1. **`main`** empilha `TitleState` e chama `Game::Run`.
2. **`Game::Run`** atualiza/`Render` no estado atual (`stateStack.top()`).
3. **`StageState`** é onde rodam mapa, dupla, inventário, luzes e HUD.

Fluxo típico de telas: **Title → Loading → Stage** (gameplay).

## Acesso seguro ao estágio de jogo

Componentes que antes faziam cast direto para `StageState*` a partir do estado atual podiam causar **comportamento indefinido** fora da fase.

- Use sempre **`Game::TryGetStageState()`** (estático): retorna **`nullptr`** se o estado atual não for `StageState`.
- Faça **early return** quando for `nullptr` (equivalente a “não estamos na fase”).

```cpp
StageState* stage = Game::TryGetStageState();
if (!stage) {
    return;
}
// usar stage com segurança
```

## `StageState` — divisão por arquivos

Um único header (`include/states/stage/StageState.h`). Implementações agrupadas por domínio em `src/states/stage/`:

| Arquivo | Responsabilidade principal |
|--------|-----------------------------|
| `StageState.cpp` | Includes compartilhados / TU enxuto |
| `InternalHelpers.cpp` | Helpers gráficos (`stage_internal`), debounce OST silenciosa, cursor preso à janela |
| `Load.cpp` | Construtor/destrutor, `LoadAssets`, bind do áudio do mar |
| `Update.cpp` | `Update` — input global, física/colisões, música/ambiente |
| `Render.cpp` | `Render`, overlays de debug de mapa |
| `Lifecycle.cpp` | `Start`, `Pause`, `Resume` |
| `Lighting.cpp` | Coordenadas tela↔mundo usadas pela lanterna fixa/cursor, `CreateLightAtCursor` |
| `Navigation.cpp` | Tiles, A*, clamp de pickups, limites do mapa |
| `PartyInput.cpp` | Dupla: TAB/F, swap, HUD posição, seguir/companion |
| `OceanAmbientController.cpp` | Loop de ondas (`Mix_Chunk` no canal reservado) |
| `FirstLoadLoader.cpp` | Leitura de `stage_first_load.json` com fallback embutido |

## Subsistema de áudio das ondas (`StageOceanAmbientController`)

Encapsula o mesmo uso de **`Mix_PlayChannel`** no canal reservado e **`Mix_Volume`** em função do volume mestre / mute.

- **`StageState`** mantém `oceanWavesChunk` e `oceanMixerChannel`; o controlador só opera sobre ponteiros configurados no construtor (`Bind`).

## Configuração

### `.env` (na pasta de trabalho ao rodar o `.exe`)

- Suporta **`MASTER_VOLUME`** (`0`–`100`).
- Linhas com **`#`** no início (após espaços) são ignoradas.
- Chaves/valores são **trimados**; valores inválidos em `MASTER_VOLUME` são **ignorados** (sem encerrar o jogo).

### `config/settings.json` (opcional)

Se existir na pasta de trabalho:

- **`window_width`** / **`window_height`** (inteiros; faixa aceita no código: largura ≥ 320, altura ≥ 240).
- Se o arquivo estiver ausente ou com JSON inválido, usa-se **`Game::WINDOW_WIDTH`** / **`Game::WINDOW_HEIGHT`** (`include/core/Game.h`).

Veja exemplo em **`config/settings.example.json`** (copie para `config/settings.json` se quiser).

## Dados da primeira carga do estágio

- Arquivo opcional: **`Recursos/data/stage_first_load.json`** (OST, mapa, grade de navegação, ciclo de itens no chão, lanterna inicial, caminhos das ondas).
- Se o arquivo falhar ou estiver malformado, o jogo **registra no stderr** e usa um **fallback embutido** em `FirstLoadLoader.cpp` com os mesmos valores históricos (paridade).

## Recursos (`Resources`)

Caches por `std::shared_ptr`. Os clears só removem entradas cuja **`use_count() == 1`** (única referência é a tabela), substituindo o antigo `unique()` deprecado.

Política de **unload agressivo** não foi introduzida: só documentação aqui para futuras otimizações se houver pressão de memória.

## Build / distribuição

- **`mingw32-make debug`** — desenvolvimento (console no Windows).
- **`mingw32-make dist`** — release empacotado em **`dist/`**.

Em builds **release no Windows**, o link pode usar **`-static-libgcc -static-libstdc++`** para reduzir dependência de DLLs MinGW em máquinas limpas.

Se em algum build **não** estiver estático e aparecer erro por falta de **`libgcc_s_*.dll`** / **`libstdc++-6.dll`**, instale o runtime MinGW correspondente ou use `mingw32-make dist` com o makefile atual.

## Verificação antes de PR / push

Recomendação local (equivalente simples a CI quando não há runner Windows no GitHub):

```bat
mingw32-make debug
```

Smoke manual rápido: título → carregar → estágio → pegar item / mover / luz.
