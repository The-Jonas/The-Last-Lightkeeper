#include "ui/Text.h"
#include "core/Game.h"
#include "core/Resources.h"
#include "engine/Camera.h"

Text::Text(GameObject& associated, std::string fontFile, int fontSize, TextStyle style, std::string text, SDL_Color color)
    : Component(associated), text(text), style(style), fontFile(fontFile), fontSize(fontSize), color(color) {
    
    texture = nullptr;
    RemakeTexture();
}

Text::~Text() {
    if (texture) {
        SDL_DestroyTexture(texture);
    }
}

void Text::Update(float dt) {
    // Nada por enquanto
}

void Text::Render() {
    if (texture) {
        // ClipRect é o tamanho total da textura
        SDL_Rect clipRect = {0, 0, (int)associated.box.w, (int)associated.box.h};

        // DestRect considera a posição da câmera
        SDL_Rect dstRect = {
            (int)(associated.box.x - Camera::pos.x),
            (int)(associated.box.y - Camera::pos.y),
            (int)associated.box.w,
            (int)associated.box.h
        };
        
        SDL_RenderCopyEx(Game::GetInstance().GetRenderer(), texture, &clipRect, &dstRect, associated.angleDeg, nullptr, SDL_FLIP_NONE);
    }
}

void Text::SetText(std::string text) {
    this->text = text;
    RemakeTexture();
}

void Text::SetColor(SDL_Color color) {
    this->color = color;
    RemakeTexture();
}

void Text::SetStyle(TextStyle style) {
    this->style = style;
    RemakeTexture();
}

void Text::SetFontFile(std::string fontFile) {
    this->fontFile = fontFile;
    RemakeTexture();
}

void Text::SetFontSize(int fontSize) {
    this->fontSize = fontSize;
    RemakeTexture();
}

void Text::RemakeTexture() {
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }

    font = Resources::GetFont(fontFile, fontSize);
    if (!font) return;

    SDL_Surface* surface = nullptr;

    // Renderiza o texto na superfície dependendo do estilo
    if (style == SOLID) {
        surface = TTF_RenderText_Solid(font.get(), text.c_str(), color);
    }
    else if (style == SHADED) {
        surface = TTF_RenderText_Shaded(font.get(), text.c_str(), color, {0, 0, 0, 255});               // Fundo preto padrão
    }
    else if (style == BLENDED) {
        surface = TTF_RenderText_Blended(font.get(), text.c_str(), color);
    }

    if (surface) {
        texture = SDL_CreateTextureFromSurface(Game::GetInstance().GetRenderer(), surface);

        // Atualiza o tamanho da box do GameObject para caber o texto
        associated.box.w = surface->w;
        associated.box.h = surface->h;

        SDL_FreeSurface(surface);
    }
}