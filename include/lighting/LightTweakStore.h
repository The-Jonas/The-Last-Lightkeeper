#ifndef LIGHT_TWEAK_STORE_H
#define LIGHT_TWEAK_STORE_H

#include "lighting/LightMaskTypes.h"

/// Guarda e recupera os valores do painel de afinacao (tecla `\`).
///
/// O ficheiro e `config/lighting.json` (com `lighting.json` ao lado do
/// executavel como alternativa, para o caso de a pasta `config` nao existir).
///
/// PONTO DE PARTIDA = OS DEFAULTS COMPILADOS. `Load` nao constroi nada de raiz:
/// escreve por cima dos valores que ja la estao. Uma chave em falta, mal
/// formada, fora de intervalo ou nao finita e simplesmente ignorada, e esse
/// campo fica com o default de `LightMaskParams` / `PlayerVisionParams`. Apagar
/// o ficheiro devolve o jogo aos valores de fabrica.
namespace LightTweakStore {

/// Caminho efectivamente usado na ultima operacao (para mensagens).
const char* LastPath();

/// Le o ficheiro por cima dos valores dados. Devolve false quando nao ha
/// ficheiro nenhum — o que NAO e um erro: e a primeira execucao.
bool Load(LightMaskParams& params, LightMaskShape& shape, PlayerVisionParams& vision, bool& durability);

/// Grava tudo. Devolve false quando nao conseguiu escrever em lado nenhum.
bool Save(const LightMaskParams& params, LightMaskShape shape, const PlayerVisionParams& vision, bool durability);

} // namespace LightTweakStore

#endif
