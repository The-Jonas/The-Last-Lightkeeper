// ─────────────────────────────────────────────────────────────────────────────
//  StageState — ECO DOS PASSOS DO MONSTRO.
//
//  Desde que o monstro so aparece onde ha luz, o jogador as escuras deixou de
//  ter qualquer ideia de onde ele esta. Isso nao assusta: irrita. Este ficheiro
//  devolve-lhe a informacao pelo unico canal que faz sentido na escuridao — o
//  SOM. De tantos em tantos passos o monstro abre um circulo branco no chao,
//  que cresce e apaga, como a onda de um pingo de agua.
//
//  QUATRO REGRAS decidem quando ha onda (ver `Monster::Update`, que as aplica):
//    • de X em X pixeis andados, nao a cada passada — uma onda por passo dizia
//      de mais e enchia o ecra;
//    • o dobro da distancia entre ondas quando ele CORRE (perseguicao ou fuga):
//      a essa altura o som e a musica ja contam a historia;
//    • nada quando ele esta COLADO aos irmaos: a essa distancia ja se ouve e ja
//      se sente, e um anel por cima deles so atrapalhava;
//    • nada quando ele esta A VISTA (ver `RenderMonsterEchoes`): as ondas sao
//      um substituto de o ver, nao um acrescento.
//  A forca de cada onda copia a curva de distancia do som da passada
//  (`GameSfx::PlayMonsterStep`): perto e forte, longe e um sussurro.
//
//  E quando a FRENTE de uma onda passa por cima de um irmao, o monstro aparece
//  por um instante mesmo que ninguem esteja a olhar para ele — a onda tocou-os,
//  e isso chega para saberem onde ele esta.
//
//  DESENHO: depois do pos-processamento (ver `StageState::Render`). A malha de
//  escuridao apagaria os circulos, e o desfoque do campo de visao borra-los-ia
//  fora do cone — mas a coisa toda existe precisamente para funcionar fora do
//  cone e no escuro.
// ─────────────────────────────────────────────────────────────────────────────
#include "states/stage/StageState.h"

#include "core/Game.h"
#include "engine/Camera.h"
#include "engine/GameObject.h"
#include "gameplay/Character.h"
#include "gameplay/Monster.h"

#include <algorithm>
#include <cmath>

namespace {

/// Quanto tempo um circulo demora a crescer e apagar.
constexpr float kEchoLifeSec = 1.35f;
/// Raio inicial e final, em pixeis de MUNDO (o zoom trata da conversao).
/// O raio final tem de ser GRANDE o bastante para a onda alcancar um irmao que
/// esteja a uma distancia normal: e nesse toque que o monstro se revela.
constexpr float kEchoStartRadiusPx = 16.0f;
constexpr float kEchoEndRadiusPx = 560.0f;
/// Espessura do traco, em pixeis de ECRA. Um anel de um pixel desaparecia
/// contra o ruido da imagem; desenha-se como varios aneis encostados.
constexpr float kEchoStrokePx = 3.0f;
/// Opacidade no instante da passada, para uma passada colada ao jogador.
constexpr float kEchoPeakAlpha = 185.0f;
/// Uma passada mais fraca do que isto nao chega a dar circulo: sao os passos no
/// limite do alcance, que so encheriam o ecra de ruido.
constexpr float kEchoMinStrength = 0.06f;
/// Tecto de circulos vivos ao mesmo tempo. Passa-se disto so com o monstro a
/// correr colado ao jogador, e ai ja ha coisas mais urgentes no ecra.
constexpr size_t kMaxEchoes = 24;

/// Um anel de um pixel. O numero de segmentos acompanha o raio: um circulo
/// pequeno com 96 segmentos e desperdicio, um grande com 24 fica com cara de
/// poligono.
void DrawThinRing(SDL_Renderer* renderer, float cx, float cy, float radius, Uint8 alpha) {
    if (radius < 2.0f || alpha == 0) {
        return;
    }
    const int segments = std::max(24, std::min(128, static_cast<int>(radius * 0.7f)));
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, alpha);
    float prevX = cx + radius;
    float prevY = cy;
    for (int i = 1; i <= segments; i++) {
        const float a = (static_cast<float>(i) / static_cast<float>(segments)) * 2.0f * static_cast<float>(M_PI);
        const float x = cx + std::cos(a) * radius;
        const float y = cy + std::sin(a) * radius;
        SDL_RenderDrawLineF(renderer, prevX, prevY, x, y);
        prevX = x;
        prevY = y;
    }
}

/// Anel GROSSO: varios aneis finos encostados, meio pixel de distancia entre
/// eles para nao deixar buracos. O SDL_Renderer nao sabe desenhar linhas com
/// espessura, e um anel de um pixel perde-se contra a imagem.
void DrawRing(SDL_Renderer* renderer, float cx, float cy, float radius, Uint8 alpha, float thickness) {
    if (!renderer || radius < 2.0f || alpha == 0) {
        return;
    }
    const int passes = std::max(1, static_cast<int>(std::lround(thickness * 2.0f)));
    const float start = radius - thickness * 0.5f;
    for (int i = 0; i < passes; i++) {
        const float r = start + static_cast<float>(i) * 0.5f;
        DrawThinRing(renderer, cx, cy, r, alpha);
    }
}

/// Raio da onda a esta idade. Sai daqui, e nao do desenho, porque a deteccao do
/// toque no jogador precisa exactamente do mesmo numero.
float EchoRadiusAt(float age) {
    const float t = std::max(0.0f, std::min(1.0f, age / kEchoLifeSec));
    // Abre depressa e trava no fim (cubica a sair), como uma onda que perde
    // forca. Crescer a velocidade constante parecia um alvo de mira.
    const float ease = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
    return kEchoStartRadiusPx + (kEchoEndRadiusPx - kEchoStartRadiusPx) * ease;
}

}  // namespace

void StageState::SpawnMonsterEcho(const Vec2& worldFootPos, float strength01) {
    const float strength = std::max(0.0f, std::min(1.0f, strength01));
    if (strength < kEchoMinStrength) {
        return;   // passada longe de mais para se ouvir; nao ha o que mostrar
    }
    if (monsterEchoes.size() >= kMaxEchoes) {
        monsterEchoes.erase(monsterEchoes.begin());   // deita fora a mais velha
    }
    monsterEchoes.push_back(MonsterEcho{worldFootPos, 0.0f, strength});
}

/// O monstro deste andar, ou nullptr. Guardado de um frame para o outro porque
/// tanto o envelhecer das ondas como o desenho precisam dele. A cache e fraca:
/// se o objecto morrer, o `lock()` falha e a procura recomeca, em vez de ficar
/// um ponteiro pendurado.
Monster* StageState::FindMonster() const {
    if (std::shared_ptr<GameObject> cached = monsterCache.lock()) {
        if (!cached->IsDead()) {
            if (Monster* m = cached->GetComponent<Monster>()) {
                return m;
            }
        }
        monsterCache.reset();
    }
    for (const auto& goPtr : objectArray) {
        GameObject* go = goPtr.get();
        if (!go || go->IsDead()) continue;
        if (Monster* m = go->GetComponent<Monster>()) {
            monsterCache = goPtr;
            return m;
        }
    }
    return nullptr;
}

void StageState::UpdateMonsterEchoes(float dt) {
    if (monsterEchoes.empty()) {
        return;
    }

    Monster* monster = FindMonster();

    // Posicoes dos dois irmaos, para saber quando a onda lhes passa por cima.
    Vec2 brothers[2];
    int brotherCount = 0;
    if (bigCharacterObject) brothers[brotherCount++] = bigCharacterObject->box.Center();
    if (smallCharacterObject) brothers[brotherCount++] = smallCharacterObject->box.Center();

    for (MonsterEcho& echo : monsterEchoes) {
        const float prevRadius = EchoRadiusAt(echo.age);
        echo.age += dt;
        const float radius = EchoRadiusAt(echo.age);

        // ── A ONDA TOCOU NUM IRMAO ──────────────────────────────────────────
        // A frente da onda passou por cima dele NESTE frame (estava dentro,
        // ficou fora). Nao chega ter o irmao dentro do circulo: o que revela o
        // monstro e a passagem da frente, uma unica vez por onda.
        if (monster && !echo.touched) {
            for (int i = 0; i < brotherCount; i++) {
                const float d = echo.worldPos.Distance(brothers[i]);
                if (prevRadius < d && radius >= d) {
                    echo.touched = true;
                    monster->TriggerEchoReveal();
                    break;
                }
            }
        }
    }

    monsterEchoes.erase(std::remove_if(monsterEchoes.begin(), monsterEchoes.end(),
                                       [](const MonsterEcho& e) { return e.age >= kEchoLifeSec; }),
                        monsterEchoes.end());
}

void StageState::RenderMonsterEchoes(SDL_Renderer* renderer) const {
    if (!renderer || monsterEchoes.empty()) {
        return;
    }

    // COM O MONSTRO A VISTA NAO HA ONDAS. Elas sao um substituto de o ver; com
    // as duas coisas ao mesmo tempo o ecra so diria duas vezes a mesma coisa, e
    // a onda ate roubava a atencao ao proprio monstro.
    if (Monster* monster = FindMonster()) {
        if (VisibilityOfObject(monster->GetAssociated()) > 0.02f) {
            return;
        }
    }

    SDL_BlendMode oldBlend;
    SDL_GetRenderDrawBlendMode(renderer, &oldBlend);
    Uint8 dr, dg, db, da;
    SDL_GetRenderDrawColor(renderer, &dr, &dg, &db, &da);
    // ADD em vez de BLEND: o branco SOMA-SE ao que esta por baixo, por isso o
    // anel le-se tanto contra o chao claro como contra o preto do quarto sem
    // luz — que e onde ele tem mesmo de ser visto.
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);

    const float zoom = Camera::GetZoom();

    for (const MonsterEcho& echo : monsterEchoes) {
        const float t = std::max(0.0f, std::min(1.0f, echo.age / kEchoLifeSec));
        const float screenRadius = EchoRadiusAt(echo.age) * zoom;

        // Apaga-se mais depressa do que cresce: o fim da vida do anel e quase
        // invisivel, e nao fica um risco branco parado no ecra.
        const float fade = (1.0f - t) * (1.0f - t);
        const Uint8 alpha = static_cast<Uint8>(std::max(0.0f, std::min(255.0f, kEchoPeakAlpha * fade * echo.strength)));
        if (alpha == 0) {
            continue;
        }

        const Vec2 screen = WorldToScreen(echo.worldPos);
        DrawRing(renderer, screen.x, screen.y, screenRadius, alpha, kEchoStrokePx);
        // Segundo anel, um pouco atras do primeiro e mais fraco: da espessura a
        // onda sem a transformar num disco.
        DrawRing(renderer, screen.x, screen.y, screenRadius * 0.88f,
                 static_cast<Uint8>(alpha * 0.45f), kEchoStrokePx * 0.7f);
    }

    SDL_SetRenderDrawBlendMode(renderer, oldBlend);
    SDL_SetRenderDrawColor(renderer, dr, dg, db, da);
}
