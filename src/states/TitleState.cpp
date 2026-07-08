#include "states/TitleState.h"
#include "states/LoadingState.h"
#include "core/SaveManager.h"
#include "states/stage/StageState.h"
#include "core/Game.h"
#include "engine/GameObject.h"
#include "engine/SpriteRenderer.h"
#include "core/InputManager.h"
#include "engine/Camera.h"
#include "ui/Text.h"
#include "core/Resources.h"
#include "states/CutsceneState.h"
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <fstream>
#include "nlohmann/json.hpp"

static const char* kMenuLabels[5] = {
    "Novo Jogo",
    "Continuar",
    "Configurações",
    "Créditos",
    "Sair"
};

// ─────────────────────────────────────────────────────────────────────────────
//  Helper interno: desenha texto com SDL_ttf
// ─────────────────────────────────────────────────────────────────────────────
static void DrawText(SDL_Renderer* r, TTF_Font* font, const char* str,
                     int x, int y, SDL_Color col, int align = 0, int refW = 0) {
    if (!r || !font || !str) return;
    SDL_Surface* sf = TTF_RenderUTF8_Blended(font, str, col);
    if (!sf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, sf);
    int w = sf->w, h = sf->h;
    SDL_FreeSurface(sf);
    if (!tex) return;
    int dx = x;
    if (align == 1) dx = x + (refW - w) / 2;
    else if (align == 2) dx = x - w;
    SDL_Rect dst{dx, y, w, h};
    SDL_RenderCopy(r, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Construtor / Destrutor
// ─────────────────────────────────────────────────────────────────────────────
TitleState::TitleState() : State() {
    InitSliders();
    for (int i = 0; i < 5; i++) menuTexts[i] = nullptr;
}

TitleState::~TitleState() {
    delete armGO; armGO = nullptr;
    for (int i = 0; i < 5; i++) { delete menuTexts[i]; menuTexts[i] = nullptr; }
}

// ─────────────────────────────────────────────────────────────────────────────
//  LoadConfig
// ─────────────────────────────────────────────────────────────────────────────
void TitleState::LoadConfig() {
    std::ifstream f("Recursos/img/menu/menu_config.json");
    if (!f.is_open()) { printf("[TitleState] menu_config.json nao encontrado\n"); return; }
    try {
        nlohmann::json j; f >> j;
        auto getf = [&](nlohmann::json& o, const char* k, float& v) {
            if (o.contains(k)) v = o[k].get<float>();
        };
        if (j.contains("character")) { auto& c=j["character"];
            getf(c,"centerX",kCharCX); getf(c,"bottomY",kCharBottomY);
            getf(c,"width",kCharW);    getf(c,"height",kCharH);
            getf(c,"shoulderDX",kShoulderDX); getf(c,"shoulderDY",kShoulderDY); }
        if (j.contains("logo")) { auto& l=j["logo"];
            getf(l,"x",kLogoX); getf(l,"y",kLogoY);
            if (l.contains("width")) { kLogoW=l["width"].get<float>(); kLogoH=kLogoW*(809.0f/2444.0f); } }
        if (j.contains("menu")) { auto& m=j["menu"];
            getf(m,"x",kMenuX); getf(m,"startY",kMenuStartY); getf(m,"spacing",kMenuSpacing);
            if (m.contains("fontSize")) kMenuFontSize=m["fontSize"].get<int>(); }
        if (j.contains("arm")) { auto& a=j["arm"];
            getf(a,"width",kArmW); getf(a,"height",kArmH);
            getf(a,"lanternAlong",kLanternAlong); getf(a,"lanternPerp",kLanternPerp); }
        if (j.contains("darkness")) kDarknessAlpha=static_cast<Uint8>(j["darkness"].get<int>());
        printf("[TitleState] Config carregada\n");
    } catch(...) { printf("[TitleState] Erro ao parsear menu_config.json\n"); }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Sliders
// ─────────────────────────────────────────────────────────────────────────────
void TitleState::InitSliders() {
    sliders[0]={VolumeSliderKind::Master, "Master",   Game::masterVolumePercent};
    sliders[1]={VolumeSliderKind::Ambient,"Fundo",    Game::ambientVolumePercent};
    sliders[2]={VolumeSliderKind::Vfx,   "Efeitos",  Game::sfxVolumePercent};
    sliders[3]={VolumeSliderKind::Voice, "Dublagem",  Game::voiceVolumePercent};
}

void TitleState::ApplySliderValue(VolumeSliderKind kind, int percent) {
    switch(kind) {
    case VolumeSliderKind::Master:  Game::SetMasterVolume(percent);  break;
    case VolumeSliderKind::Ambient: Game::SetAmbientVolume(percent); break;
    case VolumeSliderKind::Vfx:     Game::SetSfxVolume(percent);     break;
    case VolumeSliderKind::Voice:   Game::SetVoiceVolume(percent);   break;
    case VolumeSliderKind::Count:   break;
    }
}

TitleState::VolumeSliderUi* TitleState::FindSliderAtPoint(int mx, int my) {
    SDL_Point pt{mx,my};
    for (auto& s:sliders)
        if(SDL_PointInRect(&pt,&s.bar)||SDL_PointInRect(&pt,&s.handle)) return &s;
    return nullptr;
}

void TitleState::RecalcSliders(int panelX, int panelY, int panelW) {
    int baseX = panelX + (panelW - kSliderW) / 2;
    int baseY = panelY + 120;
    for (int i=0;i<static_cast<int>(VolumeSliderKind::Count);i++) {
        auto& s=sliders[i];
        s.bar={baseX, baseY+i*kSliderRowH, kSliderW, kSliderH};
        int frac=(s.value*kSliderW)/100;
        s.handle={s.bar.x+frac-kHandleW/2, s.bar.y-5, kHandleW, kSliderH+10};
    }
}

void TitleState::HandleSliderInput(int mx, int my, InputManager& input) {
    if(input.MousePress(SDL_BUTTON_LEFT))
        if(auto* h=FindSliderAtPoint(mx,my)) h->dragging=true;
    for(auto& s:sliders) {
        if(!s.dragging) continue;
        if(input.IsMouseDown(SDL_BUTTON_LEFT)) {
            float frac=std::max(0.0f,std::min(1.0f,
                static_cast<float>(mx-s.bar.x)/kSliderW));
            s.value=static_cast<int>(frac*100.0f);
            ApplySliderValue(s.kind,s.value);
        } else s.dragging=false;
    }
}

void TitleState::RenderSliders(SDL_Renderer* r, int panelX, int panelY) {
    auto font=Resources::GetFont("Recursos/font/Broadsheet_0.ttf",16);
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
    for(const auto& s:sliders) {
        SDL_SetRenderDrawColor(r,50,50,50,220);
        SDL_RenderFillRect(r,&s.bar);
        SDL_SetRenderDrawColor(r,100,100,100,255);
        SDL_RenderDrawRect(r,&s.bar);
        // Preenchimento do slider
        SDL_Rect filled={s.bar.x,s.bar.y,(s.value*kSliderW)/100,kSliderH};
        SDL_SetRenderDrawColor(r,180,150,70,220);
        SDL_RenderFillRect(r,&filled);
        SDL_SetRenderDrawColor(r,220,190,100,255);
        SDL_RenderFillRect(r,&s.handle);
        if(!font) continue;
        char buf[64];
        std::snprintf(buf,sizeof(buf),"%s: %d%%",s.label,s.value);
        DrawText(r,font.get(),buf,s.bar.x,s.bar.y-26,{200,190,160,255});
    }
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_NONE);
    (void)panelX; (void)panelY;
}

// ─────────────────────────────────────────────────────────────────────────────
//  LoadAssets
// ─────────────────────────────────────────────────────────────────────────────
void TitleState::LoadAssets() {
    LoadConfig();
    for(int i=1;i<=kCharFrames;i++) {
        char path[128];
        std::snprintf(path,sizeof(path),
            "Recursos/img/menu/irmao_costas/Costas_irmaozao_%04d.png",i);
        Resources::GetImage(path);
    }
    
    // Fundo
    { auto* go=new GameObject(); go->z=0;
      auto* sr=new SpriteRenderer(*go,"Recursos/img/menu/parede.png");
      go->AddComponent(sr); sr->SetCameraFollower(true);
      go->box={0,0,1920.0f,1080.0f}; AddObject(go); bg=go; }
      
    // Logo
    { auto* go=new GameObject(); go->z=5;
      auto* sr=new SpriteRenderer(*go,"Recursos/img/menu/LOGO_BRANCA_1.png");
      go->AddComponent(sr); sr->SetCameraFollower(true);
      go->box={kLogoX,kLogoY,kLogoW,kLogoH}; AddObject(go); logoGO=go; }
      
    // Personagem
    { auto* go=new GameObject(); go->z=10;
      auto* sr=new SpriteRenderer(*go,"Recursos/img/menu/irmao_costas/Costas_irmaozao_0001.png");
      go->AddComponent(sr); sr->SetCameraFollower(true);
      go->box={0,0,kCharW,kCharH}; AddObject(go); charBodyGO=go; }
      
    // Braço — fora do objectArray
    { auto* go=new GameObject();
      auto* sr=new SpriteRenderer(*go,"Recursos/img/menu/braco_irmaozao.png");
      go->AddComponent(sr); sr->SetCameraFollower(true);
      go->box={0,0,kArmW,kArmH}; armGO=go; }
      
    // Textos — fora do objectArray
    for(int i=0;i<5;i++) {
        auto* go=new GameObject();
        SDL_Color col={220,200,150,0};
        go->AddComponent(new Text(*go,"Recursos/font/Broadsheet_0.ttf",
            kMenuFontSize,Text::BLENDED,kMenuLabels[i],col));
        menuTexts[i]=go;
    }
    
    hasContinueSave = SaveManager::HasSave();
    
    // Configura a seleção inicial corretamente
    menuSelection = hasContinueSave ? 1 : 0; 
    
    charFrameIndex=0; charAnimTimer=0.0f;
    fadeTimer=Timer(); fadeAlpha=0.0f; pulseTimer=0.0f;
    configOpen=false; configTab=0; awaitingRebind=false; controlsSelection=0;
    
    // Isso calcula todas as posições (textos, ombro, etc) para usarmos no cálculo abaixo
    LayoutAll();
    
    // =========================================================================
    //  O SNAP DA LANTERNA: Força o braço a nascer mirando no lugar certo
    // =========================================================================
    const auto& op = optionPositions[menuSelection];
    float dx = op.cx - shoulderX;
    float dy = op.cy - shoulderY;
    
    constexpr float kArmAngleOffset = -10.0f; 
    
    armAngle = std::atan2(dy, dx) * (180.0f / 3.14159265f) + 180.0f + kArmAngleOffset;
}

// ─────────────────────────────────────────────────────────────────────────────
//  LayoutAll
// ─────────────────────────────────────────────────────────────────────────────
void TitleState::LayoutAll() {
    if(bg) bg->box={Camera::pos.x,Camera::pos.y,1920.0f,1080.0f};
    if(logoGO) logoGO->box={Camera::pos.x+kLogoX,Camera::pos.y+kLogoY,kLogoW,kLogoH};
    float charX=Camera::pos.x+kCharCX-kCharW*0.5f;
    float charY=Camera::pos.y+kCharBottomY-kCharH;
    if(charBodyGO) charBodyGO->box={charX,charY,kCharW,kCharH};
    shoulderX=kCharCX+kShoulderDX;
    shoulderY=(kCharBottomY-kCharH)+kCharH+kShoulderDY;
    for(int i=0;i<5;i++) {
        if(!menuTexts[i]) continue;
        float y=kMenuStartY+i*kMenuSpacing;
        menuTexts[i]->box.x=Camera::pos.x+kMenuX;
        menuTexts[i]->box.y=Camera::pos.y+y;
        optionPositions[i]={kMenuX+menuTexts[i]->box.w*0.5f, y+menuTexts[i]->box.h*0.5f};
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Animacao
// ─────────────────────────────────────────────────────────────────────────────
void TitleState::UpdateCharAnim(float dt) {
    charAnimTimer+=dt;
    if(charAnimTimer<kCharFrameSeconds) return;
    charAnimTimer-=kCharFrameSeconds;
    charFrameIndex=(charFrameIndex+1)%kCharFrames;
    if(!charBodyGO) return;
    auto* sr=charBodyGO->GetComponent<SpriteRenderer>();
    if(!sr) return;
    char path[128];
    std::snprintf(path,sizeof(path),
        "Recursos/img/menu/irmao_costas/Costas_irmaozao_%04d.png",charFrameIndex+1);
    Rect saved=charBodyGO->box;
    sr->Open(path);
    charBodyGO->box=saved;
}

void TitleState::UpdateArm(float dt) {
    const auto& op = optionPositions[menuSelection];
    float dx = op.cx - shoulderX, dy = op.cy - shoulderY;
    
    // COMPENSAÇÃO DE ÂNGULO:
    // Se muito pra baixo aumente o valor, se muito pra cima abaixe o valor
    constexpr float kArmAngleOffset = -10.0f; 

    armAngleTarget = std::atan2(dy, dx) * (180.0f / 3.14159265f) + 180.0f + kArmAngleOffset;
    
    float diff = armAngleTarget - armAngle;
    while(diff > 180.0f) diff -= 360.0f;
    while(diff < -180.0f) diff += 360.0f;
    armAngle += diff * std::min(1.0f, 7.0f * dt);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Painel de Configuracoes
// ─────────────────────────────────────────────────────────────────────────────
void TitleState::OpenConfig() {
    configOpen=true;
    configTab=0;
    awaitingRebind=false;
    controlsSelection=0;
}

void TitleState::UpdateConfig(InputManager& input) {
    if(!configOpen) return;

    int mx=input.GetMouseX(), my=input.GetMouseY();
    SDL_Point mp{mx,my};

    // Botao X fechar
    if(input.MousePress(SDL_BUTTON_LEFT) && SDL_PointInRect(&mp,&configCloseBtn)) {
        configOpen=false; Game::SaveSettings(); return;
    }
    if(input.KeyPress(SDLK_ESCAPE) && !awaitingRebind) {
        configOpen=false; Game::SaveSettings(); return;
    }

    // Abas
    if(input.MousePress(SDL_BUTTON_LEFT)) {
        if(SDL_PointInRect(&mp,&configTabVolume))    { configTab=0; awaitingRebind=false; }
        if(SDL_PointInRect(&mp,&configTabControles)) { configTab=1; awaitingRebind=false; }
    }

    if(configTab==0) {
        // Volume
        const int winW=Game::GetInstance().GetWindowsWidth();
        const int winH=Game::GetInstance().GetWindowsHeight();
        const int pw=640, ph=std::max(560, 90 + kControlsRowCount*(36+4) + 30);
        const int px=(winW-pw)/2, py=(winH-ph)/2;
        RecalcSliders(px,py,pw);
        HandleSliderInput(mx,my,input);
    } else {
        // Controles — captura de tecla
        if(awaitingRebind) {
            if(input.KeyPress(SDLK_ESCAPE)) { awaitingRebind=false; return; }
            int key=input.PollAnyKeyPressed();
            if(key!=0) { input.SetBinding(rebindAction,key); awaitingRebind=false; }
            return;
        }
        if(input.KeyPress(SDLK_UP)||input.KeyPress(SDLK_w))
            controlsSelection=(controlsSelection+kControlsRowCount-1)%kControlsRowCount;
        if(input.KeyPress(SDLK_DOWN)||input.KeyPress(SDLK_s))
            controlsSelection=(controlsSelection+1)%kControlsRowCount;
        // Clique nas linhas
        for(int i=0;i<kControlsRowCount;i++)
            if(SDL_PointInRect(&mp,&controlsRowRects[i]))
                controlsSelection=i;
        bool activate=input.KeyPress(SDLK_RETURN)||input.KeyPress(SDLK_f);
        if(input.MousePress(SDL_BUTTON_LEFT))
            for(int i=0;i<kControlsRowCount;i++)
                if(SDL_PointInRect(&mp,&controlsRowRects[i])) { controlsSelection=i; activate=true; }
        if(activate) {
            if(controlsSelection<InputManager::ActionCount) {
                awaitingRebind=true;
                rebindAction=static_cast<GameAction>(controlsSelection);
            } else if(controlsSelection==InputManager::ActionCount) {
                input.ResetBindingsToDefault();
            } else {
                configOpen=false; Game::SaveSettings();
            }
        }
    }
}

void TitleState::RenderConfigVolume(SDL_Renderer* r, int px, int py, int pw, int ph) {
    RenderSliders(r, px, py);
    (void)ph;
    auto font=Resources::GetFont("Recursos/font/Broadsheet_0.ttf",14);
    if(font) DrawText(r,font.get(),"Ajuste os volumes com o mouse",
        px+pw/2,py+ph-40,{150,150,150,200},1,0);
}

void TitleState::RenderConfigControles(SDL_Renderer* r, int px, int py, int pw, int ph) {
    auto labelFont=Resources::GetFont("Recursos/font/Broadsheet_0.ttf",18);
    if(!labelFont) return;
    const int rowH=36, gap=4, contentTop=10;
    for(int i=0;i<kControlsRowCount;i++) {
        const int rowY=py+contentTop+i*(rowH+gap);
        SDL_Rect rr{px+20,rowY,pw-40,rowH};
        controlsRowRects[i]=rr;
        bool sel=(i==controlsSelection);
        if(sel) {
            SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(r,60,55,40,220);
            SDL_RenderFillRect(r,&rr);
            SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_NONE);
        }
        SDL_Color lc=sel?SDL_Color{245,225,180,255}:SDL_Color{180,180,180,255};
        int ty=rowY+(rowH-20)/2;
        if(i<InputManager::ActionCount) {
            auto a=static_cast<GameAction>(i);
            DrawText(r,labelFont.get(),InputManager::ActionLabel(a),rr.x+12,ty,lc);
            bool capturing=(awaitingRebind&&rebindAction==a);
            const char* kn=capturing?"Pressione uma tecla...":
                SDL_GetKeyName(InputManager::GetInstance().GetBinding(a));
            SDL_Color kc=capturing?SDL_Color{230,200,90,255}:lc;
            DrawText(r,labelFont.get(),kn,rr.x+rr.w-12,ty,kc,2,0);
        } else if(i==InputManager::ActionCount) {
            DrawText(r,labelFont.get(),"Restaurar padrao",rr.x+12,ty,lc);
        } else {
            DrawText(r,labelFont.get(),"Voltar",rr.x+12,ty,lc);
        }
        // separador
        SDL_SetRenderDrawColor(r,60,60,60,120);
        SDL_RenderDrawLine(r,rr.x,rowY+rowH+gap/2,rr.x+rr.w,rowY+rowH+gap/2);
    }
    (void)ph;
}

void TitleState::RenderConfig(SDL_Renderer* r) {
    if(!configOpen) return;
    const int winW=Game::GetInstance().GetWindowsWidth();
    const int winH=Game::GetInstance().GetWindowsHeight();
    // kControlsRowCount linhas de 36+4px + 70 header + 20 padding
    const int pw=640, ph=std::max(560, 90 + kControlsRowCount*(36+4) + 30);
    const int px=(winW-pw)/2, py=(winH-ph)/2;

    // Fundo escuro semi-transparente
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r,0,0,0,160);
    SDL_Rect full{0,0,winW,winH};
    SDL_RenderFillRect(r,&full);

    // Painel
    SDL_SetRenderDrawColor(r,25,25,30,245);
    SDL_Rect panel{px,py,pw,ph};
    SDL_RenderFillRect(r,&panel);
    SDL_SetRenderDrawColor(r,160,140,90,255);
    SDL_RenderDrawRect(r,&panel);
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_NONE);

    auto titleFont=Resources::GetFont("Recursos/font/Broadsheet_0.ttf",28);
    auto tabFont  =Resources::GetFont("Recursos/font/Broadsheet_0.ttf",18);

    // Titulo
    if(titleFont)
        DrawText(r,titleFont.get(),"Configuracoes",px,py+18,{220,200,140,255},1,pw);

    // Botao X — canto superior INTERNO do painel
    configCloseBtn={px+pw-36,py+8,28,28};
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r,120,50,50,200);
    SDL_RenderFillRect(r,&configCloseBtn);
    SDL_SetRenderDrawColor(r,200,100,100,255);
    SDL_RenderDrawRect(r,&configCloseBtn);
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_NONE);
    if(tabFont) DrawText(r,tabFont.get(),"X",
        configCloseBtn.x,
        configCloseBtn.y+4,{255,200,200,255},1,configCloseBtn.w);

    // Abas
    const int tabY=py+56, tabH=30, tabW=pw/2;
    configTabVolume   ={px,        tabY,tabW,tabH};
    configTabControles={px+tabW,   tabY,tabW,tabH};

    auto drawTab=[&](SDL_Rect tab, const char* label, bool active){
        SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r,
            active?50:30, active?48:28, active?40:25, active?230:180);
        SDL_RenderFillRect(r,&tab);
        SDL_SetRenderDrawColor(r, active?160:80, active?140:70, active?90:50,255);
        SDL_RenderDrawRect(r,&tab);
        SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_NONE);
        SDL_Color tc=active?SDL_Color{240,220,160,255}:SDL_Color{150,150,150,255};
        if(tabFont) DrawText(r,tabFont.get(),label,tab.x,tab.y+6,tc,1,tab.w);
    };
    drawTab(configTabVolume,   "Volume",    configTab==0);
    drawTab(configTabControles,"Controles", configTab==1);

    // Linha separadora das abas
    SDL_SetRenderDrawColor(r,160,140,90,180);
    SDL_RenderDrawLine(r,px,tabY+tabH,px+pw,tabY+tabH);

    // Conteudo da aba
    const int contentPY=tabY+tabH+10;
    if(configTab==0) RenderConfigVolume(r,px,contentPY,pw,ph-(contentPY-py));
    else             RenderConfigControles(r,px,contentPY,pw,ph-(contentPY-py));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Update
// ─────────────────────────────────────────────────────────────────────────────
void TitleState::Update(float dt) {
    InputManager& input=InputManager::GetInstance();
    hasContinueSave=SaveManager::HasSave();
    if(!hasContinueSave&&menuSelection==1) menuSelection=0;

    // Config intercepta input primeiro
    if(configOpen) {
        UpdateConfig(input);
        fadeTimer.Update(dt);
        pulseTimer+=dt;
        UpdateCharAnim(dt);
        UpdateArray(dt);
        return;
    }

    if(input.QuitRequested()||input.KeyPress(ESCAPE_KEY)) quitRequested=true;

    auto isDisabled=[&](int idx){ return idx==1&&!hasContinueSave; };
    if(input.KeyPress(SDLK_w)||input.KeyPress(SDLK_UP)) {
        int next=(menuSelection+4)%5;
        while(isDisabled(next)&&next!=menuSelection) next=(next+4)%5;
        menuSelection=next;
    }
    if(input.KeyPress(SDLK_s)||input.KeyPress(SDLK_DOWN)) {
        int next=(menuSelection+1)%5;
        while(isDisabled(next)&&next!=menuSelection) next=(next+1)%5;
        menuSelection=next;
    }
    if(input.KeyPress(SDLK_f)||input.KeyPress(SPACE_KEY)||input.KeyPress(SDLK_RETURN))
        ActivateMenuSelection();

    fadeTimer.Update(dt);
    float t=std::min(1.0f,fadeTimer.Get()/kFadeDuration);
    fadeAlpha=t*255.0f;
    Uint8 a=static_cast<Uint8>(fadeAlpha);
    pulseTimer+=dt;
    const float pulse=(std::sin(pulseTimer*2.0f)+1.0f)*0.5f;

    for(int i=0;i<5;i++) {
        if(!menuTexts[i]) continue;
        Text* txt=menuTexts[i]->GetComponent<Text>();
        if(!txt) continue;
        bool sel=(i==menuSelection), dis=isDisabled(i);
        if(dis) txt->SetColor({80,80,80,static_cast<Uint8>(a/3)});
        else if(sel) {
            txt->SetColor({
                static_cast<Uint8>(200.0f+pulse*55.0f),
                static_cast<Uint8>(160.0f+pulse*55.0f),
                static_cast<Uint8>(40.0f +pulse*40.0f), a});
        } else txt->SetColor({130,130,130,a});
    }

    LayoutAll(); UpdateCharAnim(dt); UpdateArm(dt); UpdateArray(dt);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Acoes do menu
// ─────────────────────────────────────────────────────────────────────────────
void TitleState::StartNewGame() {
    SaveManager::DeleteSave();
    Game::GetInstance().Push(new LoadingState(StageState::LoadMode::NewGame));
}
void TitleState::StartContinue() {
    if(!SaveManager::HasSave()) return;
    Game::GetInstance().Push(new LoadingState(StageState::LoadMode::Continue));
}
void TitleState::ActivateMenuSelection() {
    switch(menuSelection) {
    case 0: StartNewGame(); break;
    case 1: if(hasContinueSave) StartContinue(); break;
    case 2: OpenConfig(); break;
    case 3: /* creditos */ break;
    case 4: quitRequested=true; break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Render helpers
// ─────────────────────────────────────────────────────────────────────────────
void TitleState::RenderLanternCone(SDL_Renderer* r) {
    if(fadeAlpha < 10.0f || !r || configOpen) return;

    float rad = (armAngle + 180.0f) * (3.14159265f / 180.0f);
    float ax = std::cos(rad), ay = std::sin(rad);
    float perpX = -ay, perpY = ax; 
    
    float lx = shoulderX + ax * (kArmW * kLanternAlong) + perpX * kLanternPerp;
    float ly = shoulderY + ay * (kArmW * kLanternAlong) + perpY * kLanternPerp;
    const auto& op = optionPositions[menuSelection];

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_ADD);

    Uint8 a = static_cast<Uint8>(fadeAlpha * 0.35f);

    float ba = std::atan2(op.cy - ly, op.cx - lx);

    // ── 1. O FEIXE DE LUZ (Retângulo Torto) ──────────────────────
    constexpr float kLensRadius = 54.0f;  // Ajuste se a luz vazar pelas laterais da lanterna
    constexpr float kSpotRadius = 130.0f; 
    
    float pCos = std::cos(ba + 1.57079632f); 
    float pSin = std::sin(ba + 1.57079632f);

    SDL_Vertex v[6];
    SDL_Color colLens = {255, 230, 140, a};     
    SDL_Color colSpot = {255, 220, 100, 0};     

    v[0] = {{lx + pCos * kLensRadius, ly + pSin * kLensRadius}, colLens, {0,0}};
    v[1] = {{lx - pCos * kLensRadius, ly - pSin * kLensRadius}, colLens, {0,0}};
    v[2] = {{op.cx + pCos * kSpotRadius, op.cy + pSin * kSpotRadius}, colSpot, {0,0}};
    
    v[3] = v[1]; 
    v[4] = {{op.cx - pCos * kSpotRadius, op.cy - pSin * kSpotRadius}, colSpot, {0,0}};
    v[5] = v[2]; 

    SDL_RenderGeometry(r, nullptr, v, 6, nullptr, 0);

    // ── 2. O CÍRCULO ILUMINADO NO TEXTO ───────────────────────────
    constexpr int kSegs = 24;
    SDL_Vertex sv[3];
    sv[0] = {{op.cx, op.cy}, {255, 230, 140, static_cast<Uint8>(a * 0.8f)}, {0,0}};
    for(int i = 0; i < kSegs; i++) {
        float a0 = (static_cast<float>(i)   / kSegs) * 2.0f * 3.14159265f;
        float a1 = (static_cast<float>(i+1) / kSegs) * 2.0f * 3.14159265f;
        sv[1] = {{op.cx + std::cos(a0) * kSpotRadius, op.cy + std::sin(a0) * kSpotRadius}, {255,220,100,0}, {0,0}};
        sv[2] = {{op.cx + std::cos(a1) * kSpotRadius, op.cy + std::sin(a1) * kSpotRadius}, {255,220,100,0}, {0,0}};
        SDL_RenderGeometry(r, nullptr, sv, 3, nullptr, 0);
    }

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

void TitleState::RenderArm(SDL_Renderer* r) {
    if(!armGO||!r) return;
    auto tex=Resources::GetImage("Recursos/img/menu/braco_irmaozao.png");
    if(!tex) return;
    
    SDL_Point pivot={static_cast<int>(kArmW),static_cast<int>(kArmH*0.5f)};
    SDL_Rect dst={
        static_cast<int>(shoulderX+Camera::pos.x-kArmW),
        static_cast<int>(shoulderY+Camera::pos.y-kArmH*0.5f),
        static_cast<int>(kArmW), static_cast<int>(kArmH)};
        
    SDL_SetTextureAlphaMod(tex.get(), 255);
    
    SDL_RenderCopyEx(r,tex.get(),nullptr,&dst,
        static_cast<double>(armAngle),&pivot,SDL_FLIP_NONE);
}

void TitleState::RenderMenuTexts(SDL_Renderer*) {
    for(int i=0;i<5;i++) if(menuTexts[i]) menuTexts[i]->Render();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Render
// ─────────────────────────────────────────────────────────────────────────────
void TitleState::Render() {
    SDL_Renderer* r = Game::GetInstance().GetRenderer();
    
    // 1. Fundo e Logo (Lá atrás)
    if(bg)     bg->Render();
    if(logoGO) logoGO->Render();
    
    // 2. Película Escura (Escurece o fundo e a logo)
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, kDarknessAlpha);
    SDL_Rect full{0, 0, Game::GetInstance().GetWindowsWidth(),
                      Game::GetInstance().GetWindowsHeight()};
    SDL_RenderFillRect(r, &full);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    
    // 3. CONE DE LUZ (Ilumina o fundo ANTES do braço aparecer)
    RenderLanternCone(r);
    
    // 4. Textos do Menu (Brilhando no fim da luz)
    RenderMenuTexts(r);
    
    // 5. O BRAÇO (Desenhado por CIMA da luz, tampando a ponta reta!)
    RenderArm(r);
    
    // 6. O CORPO (Desenhado por CIMA do braço, tampando o ombro!)
    if(charBodyGO) charBodyGO->Render();
    
    // 7. Configurações (Sempre por cima de tudo para não quebrar a UI)
    RenderConfig(r);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Start / Pause / Resume
// ─────────────────────────────────────────────────────────────────────────────
void TitleState::Start() {
    LoadAssets(); StartArray();
    music.Open("Recursos/audio/soundtracks/ES_Make up Your Mind - Hanna Lindgren.mp3");
    music.Play();
    started=true;
}
void TitleState::Pause() {}
void TitleState::Resume() {
    Camera::pos=Vec2(0,0);
    hasContinueSave=SaveManager::HasSave();
    if(!hasContinueSave) menuSelection=0;
    InitSliders(); fadeTimer=Timer(); fadeAlpha=0.0f;
    configOpen=false;
}