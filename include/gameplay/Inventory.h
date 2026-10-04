#ifndef INVENTORY_H
#define INVENTORY_H

#include "gameplay/Item.h"
#include "lighting/LightMaskTypes.h"

#include <string>
#include <vector>

struct SaveGameState;

enum class HeldPropVisual { None, Lighter, Lamp };

// ─────────────────────────────────────────────────────────────────────────────
//  Mochila do irmãozão: até kMaxCapacity itens numa roda, um deles "na mão".
//
//  Fontes de luz (isqueiro, lamparina) são únicas por tipo e recarregáveis —
//  esgotadas ficam com carga 0 na bolsa. Combustível é limitado a
//  kMaxFuelUnits; [R] gasta uma unidade inteira numa luz (o que não couber se
//  perde), com a luz apagada por kReloadDuration s.
//
//  activeIndex é um contador SEM limite (a roda anima continuamente); o item na
//  mão é derivado dele por GetSelectedStackIndex — nunca indexe stacks com ele.
// ─────────────────────────────────────────────────────────────────────────────
class Inventory {
public:
    struct ItemStack {
        ItemDef def;
        int count;
        std::vector<int> durabilities;                   // uma carga por unidade (a da frente é a em uso)
    };

    static constexpr int kMaxCapacity  = 6;              // itens na bolsa (o 7º é recusado)
    static constexpr int kMaxFuelUnits = 2;              // combustíveis na bolsa
    static constexpr float kReloadDuration = 2.0f;       // s com a luz apagada recarregando
    static constexpr float kReloadSpeedMul = 0.5f;       // velocidade do irmãozão durante a recarga

    // ── Itens ────────────────────────────────────────────────────────────────
    bool AddItem(const ItemDef& def, int durability);    // false se CanAcceptItem recusar
    bool CanAcceptItem(const ItemDef& def) const;        // critério único de "posso pegar?" (fala e contorno vermelho)
    bool IsFull() const { return GetStackCount() >= kMaxCapacity; }
    bool IsFuelAtMax() const;                            // já tem kMaxFuelUnits de combustível
    bool HasItem(const std::string& name) const;
    bool TryConsumeItem(const std::string& name);        // gasta uma unidade (ex.: a tábua no conserto)
    void ClearAll();

    const ItemStack* GetStack(int index) const;
    int GetStackCount() const { return static_cast<int>(stacks.size()); }

    // ── Roda ─────────────────────────────────────────────────────────────────
    int  GetActiveIndex() const { return activeIndex; }  // contador contínuo (animação da roda)
    void CycleLeft();                                    // girar a roda apaga a luz
    void CycleRight();
    int  GetVisibleSlotCount() const;                    // sempre 3 (a roda e o gameplay usam o mesmo)
    int  GetRingSize() const;                            // posições da roda (itens ou 3, o maior)
    int  GetSelectedStackIndex() const;                  // item no centro da roda, -1 se vazio
    void SetSelectedStackIndex(int stackIndex);          // gira pelo caminho mais curto até ele
    const ItemStack* GetActiveStack() const;             // item na mão (ou nullptr)

    // ── Luz ──────────────────────────────────────────────────────────────────
    bool isLightToggledOn = false;                       // luz de mão ligada pelo jogador
    bool TryTurnLightOn();                               // liga se o item na mão for luz com carga
    bool TryActivateBestLight();                         // garante uma luz acesa (isqueiro antes da lamparina)
    bool IsUsableLightActive() const;                    // luz na mão, ligada e com carga
    bool IsActiveLightLighter() const;                   // idem, e é o isqueiro
    bool IsActiveLightLamp() const;                      // idem, e é a lamparina
    bool IsActiveItemLighter() const;                    // isqueiro na mão, aceso ou não (som de ligar)
    bool HasDepletedLighter() const;                     // isqueiro com carga 0 ("sua luz apagou")
    HeldPropVisual GetHeldPropVisual() const;            // o que o sprite segura (só acesa aparece)
    float GetSelectedLightFuelRatio() const;             // carga da luz na mão, 0..1
    LightMaskParams BuildLighterLightParams(const LightMaskParams& base) const;   // tamanho da luz pela carga
    LightMaskParams BuildLampLightParams(const LightMaskParams& base) const;      // mesma curva do isqueiro
    void TickUsingDurability(float dt);                  // gasta carga com a luz acesa (lamparina dura 2x)

    // ── Recarga ([R]) ────────────────────────────────────────────────────────
    int   GetFuelUnits() const;                          // unidades de combustível na bolsa
    int   FindReloadTarget() const;                      // luz na mão se couber; senão a mais vazia; -1 nenhuma
    bool  CanReload() const { return GetFuelUnits() > 0 && FindReloadTarget() >= 0; }
    bool  BeginReload();                                 // apaga a luz e começa a contar; false se não dá
    bool  TickReload(float dt);                          // true no frame em que termina (já despejou e acendeu)
    bool  IsReloading() const { return reloadTimer > 0.0f; }
    float GetReloadProgress() const;                     // 0..1 da recarga atual
    void  CancelReload();                                // troca de andar/morte: nada é gasto

    // ── Save ─────────────────────────────────────────────────────────────────
    void WriteToSave(SaveGameState& state) const;
    void ReadFromSave(const SaveGameState& state, const std::vector<ItemDef>& itemCatalog);   // aceita os formatos antigos

private:
    std::vector<ItemStack> stacks;
    int   activeIndex = -1;
    float usingDrainAccum = 0.0f;
    float reloadTimer = 0.0f;                            // > 0 = recarregando (conta para baixo)
    std::string reloadTargetName;                        // luz que vai receber (pelo nome: índices podem mudar)

    bool  PourFuelInto(int targetIdx);                   // gasta a unidade da frente; a luz fica na mão
    void DedupeLightSources();                           // cura saves com isqueiro/lamparina duplicados
    void SelectAfterLegacyLoad(int preferred);           // escolha do item na mão ao ler save antigo
    int  FindStackWithName(const std::string& name) const;
    int  FindBestLight(bool lighter) const;              // isqueiro (true) ou lamparina com mais carga
};

#endif

