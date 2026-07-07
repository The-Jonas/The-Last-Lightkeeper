#include "gameplay/RadioAsset.h"
#include "gameplay/Character.h"
#include "audio/GameSfx.h"
#include "core/Game.h"
#include <iostream>

RadioAsset::RadioAsset(GameObject& associated, const std::string& soundPath)
    : Component(associated) {
    radioSound.Open(soundPath.c_str());
}

    void RadioAsset::Update(float dt) {
        if (!isPlaying) return;
        playTimer += dt;

        // TESTE TEMPORÁRIO — comenta o SetChannelSpatial
        // GameSfx::SetChannelSpatial(5, radioPos.x, radioPos.y, playerPos.x, playerPos.y);

        if (playTimer >= playDuration) {
            radioSound.Stop();
            GameSfx::ClearChannelSpatial(5);
            isPlaying = false;
            playTimer = 0.0f;
        }
    }

void RadioAsset::Toggle() {
    
    if (isPlaying) {
        radioSound.Stop();
        GameSfx::ClearChannelSpatial(5);
        isPlaying = false;  
        playTimer = 0.0f;

    } else {
        if (radioSound.IsOpen()) {
            radioSound.PlayLoopedOnChannel(5);
            isPlaying = true;
            playTimer = 0.0f;

            // Força pan na esquerda DEPOIS de tudo
            SDL_Delay(50); // espera 50ms
            int r = Mix_SetPanning(5, 255, 0);
            std::cout << "[RADIO] Pan forçado result=" << r << "\n"; 
               
            if (Character::player) {
                Vec2 playerPos = Character::player->GetAssociated().box.Center();
                Vec2 radioPos  = associated.box.Center();
                GameSfx::SetChannelSpatial(5,
                    radioPos.x,  radioPos.y,
                    playerPos.x, playerPos.y);
            }
        }
    }
}