#include "gameplay/Candlestick.h"
#include "gameplay/Character.h"
#include "states/stage/StageState.h"
#include "engine/SpriteRenderer.h"
#include "ui/Text.h"
#include "core/Game.h"

Candlestick::Candlestick(GameObject& associated, bool startsLit, const std::string& direction)
    : Component(associated), isLit(startsLit), direction(direction), myLightId(-1) {
    basePath = "Recursos/img/objetos/castical/castical_";

    std::string state = isLit ? "_aceso.png" : "_apagado.png";
    std::string fullPath = basePath + direction + state;

    SpriteRenderer* sprite = new SpriteRenderer(associated, fullPath);
    associated.AddComponent(sprite);

    textObj = new GameObject();
    SDL_Color textColor = {255, 255, 255, 255};
    Text* promptText =
        new Text(*textObj, "Recursos/font/times.ttf", 14, Text::SOLID, "[E] Acender", textColor);
    textObj->AddComponent(promptText);
}

Candlestick::~Candlestick() {
    delete textObj;
}

void Candlestick::EnsureLightRegistered(StageState& stage) {
    if (myLightId >= 0) {
        stage.SetLightEnabled(myLightId, isLit);
        return;
    }
    myLightId = stage.CreateStaticLight(associated.box.Center(), isLit);
}

void Candlestick::Start() {
    if (StageState* stage = Game::TryGetStageState()) {
        EnsureLightRegistered(*stage);
    }
}

void Candlestick::Update(float dt) {
    (void)dt;
    StageState* stage = Game::TryGetStageState();
    const bool reachable = stage && stage->GetReachableCandle() == this;
    if (reachable != showPrompt) {
        showPrompt = reachable;
    }
    if (showPrompt) {
        UpdatePromptText();
    }
}

void Candlestick::UpdatePromptText() {
    if (!textObj) {
        return;
    }
    Text* promptText = textObj->GetComponent<Text>();
    if (!promptText) {
        return;
    }
    promptText->SetText(isLit ? "[E] Apagar" : "[E] Acender");
}

void Candlestick::Render() {
    // O rótulo flutuante foi removido — a indicação agora é só o prompt
    // estilizado no rodapé central (StageState::RenderInteractionPrompt).
}

void Candlestick::SetLit(bool lit) {
    const bool stateChanged = isLit != lit;
    isLit = lit;

    if (stateChanged) {
        SpriteRenderer* sprite = associated.GetComponent<SpriteRenderer>();
        if (sprite) {
            std::string estado = isLit ? "_aceso.png" : "_apagado.png";
            sprite->Open(basePath + direction + estado);
        }
    }

    if (StageState* stage = Game::TryGetStageState()) {
        EnsureLightRegistered(*stage);
        if (myLightId >= 0) {
            stage->SetLightEnabled(myLightId, isLit);
        }
    }

    if (showPrompt) {
        UpdatePromptText();
    }
}
