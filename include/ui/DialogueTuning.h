#ifndef DIALOGUE_TUNING_H
#define DIALOGUE_TUNING_H

#include <string>
#include <cstddef>

// Parâmetros de ajuste fino da caixa de diálogo, lidos de config/dialogue_ui.json
// na inicialização do jogo. Se o arquivo não existir ou uma chave estiver faltando,
// o valor default abaixo é usado — nunca quebra por causa de config incompleta.
struct DialogueTuning {
    static void Load(const std::string& path = "config/dialogue_ui.json");

    static inline float boxScaleMul = 1.0f;

    static inline float charsPerSecond   = 42.0f;       // velocidade da digitação
    static inline float autoAdvanceDelay = 1.8f;        // segundos parado c/ texto completo até avançar sozinho
    static inline size_t maxCharsPerPage = 220;         // tamanho máx de cada "página" de texto

    static inline int   margin = 40;                    // distância da borda da tela até a caixa

    static inline float portraitHeightMul  = 1.0f;      // 1.0 = retrato do tamanho exato da caixa; >1.0 = "espia" por cima
    static inline float portraitOverlapMul = 0.15f;     // quanto do retrato invade a caixa (0 = totalmente fora; 0.5 = metade dentro)

    static inline int nameFontSize = 16;
    static inline int textFontSize = 20;
    static inline int namePadX = 30;
    static inline int namePadY = 10;
    static inline int textPadX = 30;
    static inline int textPadY = 40;

    static inline int nameBoxW = 200;                   // largura do "quadrado do nome" na arte
    static inline int nameBoxH = 40;                    // altura do "quadrado do nome" na arte

    static inline int textBoxW = 500;                   // largura da área de texto na arte
    static inline int textBoxH = 100;                   // altura da área de texto na arte (só usada se quiserem cortar overflow depois)

    static inline int blipEveryNChars = 2;              // toca 1 blip a cada N letras reveladas
};

#endif