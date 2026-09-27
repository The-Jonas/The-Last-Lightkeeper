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

int StageOceanAmbientController::TargetVolume() const {
    const int nominalCap = (MIX_MAX_VOLUME * StageOceanAudio::kNominalPercent) / 100;
    int v = (nominalCap * Game::masterVolumePercent) / 100;
    v = (v * Game::ambientVolumePercent) / 100;
    if (musicMuted_ && *musicMuted_) {
        v = 0;
    }
    return v;
}
// Volume das ondas segundo os sliders de volume geral e de ambiente (0 se mudo).

void StageOceanAmbientController::RefreshVolume() {
    if (!wavesChunk_ || !mixerChannel_ || !musicMuted_ || !*wavesChunk_ || *mixerChannel_ < 0) {
        return;
    }
    if (Mix_FadingChannel(*mixerChannel_) == MIX_FADING_IN) {
        return;
    }
    Mix_Volume(*mixerChannel_, TargetVolume());
}
// Aplica o volume atual das ondas (ignora enquanto o fade-in está em andamento).

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

    Mix_Volume(StageOceanAudio::kAmbientWavesChannel, TargetVolume());
    *mixerChannel_ = Mix_FadeInChannel(StageOceanAudio::kAmbientWavesChannel,
                                       wavesChunk_->get(), -1, StageOceanAudio::kFadeInMs);
    if (*mixerChannel_ < 0) {
        *mixerChannel_ = -1;
        std::cerr << "Erro ao reproduzir ambiente das ondas (Mix_FadeInChannel canal "
                  << StageOceanAudio::kAmbientWavesChannel << "): " << Mix_GetError() << std::endl;
    }
}
// Garante as ondas tocando em loop; se estavam paradas, entram com fade-in no
// volume certo em vez de começar de uma vez.
