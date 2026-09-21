#ifndef DIALOGUE_TUNING_H
#define DIALOGUE_TUNING_H

#include <string>
#include <cstddef>

// Parâmetros de ajuste fino da caixa de diálogo, lidos de config/dialogue_ui.json
// na inicialização do jogo. Se o arquivo não existir ou uma chave estiver faltando,
// o valor default abaixo é usado — nunca quebra por causa de config incompleta.
struct DialogueTuning {
    static void Load(const std::string& path = "config/dialogue_ui.json");

    // Estes valores sao os MESMOS de config/dialogue_ui.json de proposito: o
    // jogo tem de ficar igual com ou sem o ficheiro. Eles descrevem a arte
    // real (caixa_dialogo.png, 1798x707), nao uma caixa imaginaria.
    static inline float boxScaleMul = 0.4f;

    static inline float charsPerSecond   = 42.0f;       // velocidade da digitação
    static inline float autoAdvanceDelay = 3.0f;        // segundos parado c/ texto completo até avançar sozinho
    static inline size_t maxCharsPerPage = 220;         // tamanho máx de cada "página" de texto

    static inline int   margin = 40;                    // distância da borda da tela até a caixa

    static inline float portraitHeightMul  = 1.0f;      // 1.0 = retrato do tamanho exato da caixa; >1.0 = "espia" por cima
    static inline float portraitOverlapMul = 0.15f;     // quanto do retrato invade a caixa (0 = totalmente fora; 0.5 = metade dentro)

    // TODAS as medidas abaixo estao em PIXEIS DA ARTE (a caixa tem 1798x707).
    // O desenho converte-as com um unico factor de escala, por isso continuam
    // certas em qualquer resolucao.
    static inline int nameFontSize = 80;
    static inline int textFontSize = 65;
    static inline int namePadX = 30;
    static inline int namePadY = 10;
    static inline int textPadX = 65;
    static inline int textPadY = 155;

    static inline int nameBoxW = 300;                   // largura do "quadrado do nome" na arte
    static inline int nameBoxH = 120;                   // altura do "quadrado do nome" na arte

    static inline int textBoxW = 1690;                  // largura da área de texto na arte
    static inline int textBoxH = 200;                   // altura da área de texto na arte (só usada se quiserem cortar overflow depois)

    static inline int blipEveryNChars = 3;              // toca 1 blip a cada N letras reveladas
};

#endif