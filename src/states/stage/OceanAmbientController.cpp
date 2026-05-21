#include "states/stage/OceanAmbientController.h"
#include "core/Game.h"

#define INCLUDE_SDL_MIXER
#include "SDL_include.h"

#include <iostream>

void StageOceanAmbientController::Bind(std::shared_ptr<Mix_Chunk>* wavesChunk, int* mixerChannel, bool* musicMuted) {
    wavesChunk_ = wavesChunk;
    mixerChannel_ = mixerChannel;
    musicMuted_ = musicMuted;
}

void StageOceanAmbientController::RefreshVolume() {
    if (!wavesChunk_ || !mixerChannel_ || !musicMuted_ || !*wavesChunk_ || *mixerChannel_ < 0) {
        return;
    }
    const int nominalCap = (MIX_MAX_VOLUME * StageOceanAudio::kNominalPercent) / 100;
    int v = (nominalCap * Game::masterVolumePercent) / 100;
    if (*musicMuted_) {
        v = 0;
    }
    Mix_Volume(*mixerChannel_, v);
}

void StageOceanAmbientController::EnsurePlaying() {
    if (!wavesChunk_ || !mixerChannel_ || !*wavesChunk_) {
        return;
    }
    if (*mixerChannel_ >= 0 && Mix_Playing(*mixerChannel_)) {
        return;
    }

    if (*mixerChannel_ >= 0) {
        Mix_HaltChannel(*mixerChannel_);
        *mixerChannel_ = -1;
    }

    *mixerChannel_ =
        Mix_PlayChannel(StageOceanAudio::kAmbientWavesChannel, wavesChunk_->get(), -1);
    if (*mixerChannel_ < 0) {
        *mixerChannel_ = -1;
        std::cerr << "Erro ao reproduzir ambiente das ondas (Mix_PlayChannel canal "
                  << StageOceanAudio::kAmbientWavesChannel << "): " << Mix_GetError() << std::endl;
        return;
    }

    RefreshVolume();
}
