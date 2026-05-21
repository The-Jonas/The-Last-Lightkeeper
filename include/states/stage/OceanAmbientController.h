#ifndef STAGE_OCEAN_AMBIENT_CONTROLLER_H
#define STAGE_OCEAN_AMBIENT_CONTROLLER_H

#include <memory>

struct Mix_Chunk;

namespace StageOceanAudio {
constexpr int kNominalPercent = 100; // % de MIX_MAX × master nas ondas (relativo ao slider).
/// Canal reservado em Game.cpp (Mix_ReserveChannels) — não usar Mix_PlayChannel(-1) aqui.
constexpr int kAmbientWavesChannel = 0;
}

/// Loop de ondas via Mix_Chunk (mesmo comportamento anterior em `StageState`).
class StageOceanAmbientController {
public:
    void Bind(std::shared_ptr<Mix_Chunk>* wavesChunk, int* mixerChannel, bool* musicMuted);

    void RefreshVolume();
    void EnsurePlaying();

private:
    std::shared_ptr<Mix_Chunk>* wavesChunk_ = nullptr;
    int* mixerChannel_ = nullptr;
    bool* musicMuted_ = nullptr;
};

#endif
