#include "gameplay/RadioAsset.h"
#include "gameplay/Character.h"
#include "audio/GameSfx.h"
#include "core/Game.h"
#include <iostream>

namespace {
/// Canal fixo do radio (era um 5 solto no meio do codigo).
constexpr int kRadioChannel = 5;
/// Alcance audivel. O radio e uma pista de sitio: ouve-se de longe, mas so de
/// perto e que enche.
constexpr float kRadioAudibleDist = 1800.0f;
}

RadioAsset::RadioAsset(GameObject& associated, const std::string& soundPath)
    : Component(associated) {
    radioSound.Open(soundPath.c_str());
}

void RadioAsset::Update(float dt) {
    if (!isPlaying) return;

    // O radio toca em LOOP enquanto o jogador anda pelo andar, por isso a
    // panoramica tem de ser refrescada a cada frame — ao contrario de um som
    // curto, que so precisa de ser posicionado no instante em que comeca.
    if (Character::player) {
        const Vec2 here = associated.box.Center();
        const Vec2 ear = Character::player->GetAssociated().box.Center();
        GameSfx::SetChannelSpatial(kRadioChannel, here.x, here.y, ear.x, ear.y, kRadioAudibleDist);
    }

    playTimer += dt;
    if (playTimer >= playDuration) {
        radioSound.Stop();
        isPlaying = false;
        playTimer = 0.0f;
    }
}

void RadioAsset::Toggle() {
    if (isPlaying) {
        radioSound.Stop();
        isPlaying = false;
        playTimer = 0.0f;
    } else {
        if (radioSound.IsOpen()) {
            radioSound.PlayLoopedOnChannel(kRadioChannel);
            isPlaying = true;
            playTimer = 0.0f;
        }
    }
}