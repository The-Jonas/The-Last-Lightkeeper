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
            radioSound.PlayLoopedOnChannel(5);
            isPlaying = true;
            playTimer = 0.0f;
        }
    }
}