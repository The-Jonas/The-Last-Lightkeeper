#include "gameplay/Inventory.h"
#include "core/SaveData.h"

#include <algorithm>
#include <cmath>

namespace {
// Mapeia a carga de combustível -> tamanho da luz. Vale igual para o isqueiro e
// a lamparina. Piso de 40% (nunca encolhe abaixo disso, mesmo quase sem carga)
// e teto de 120% (max 20% maior que o tamanho base). 0% de carga => 40%;
// 100% => 120%.
float FuelToSizeScale(float fuelRatio) {
    constexpr float kMinLightSizeFrac = 0.48f;   // +20% no mínimo (era 0.40) ao esgotar
    constexpr float kMaxLightSizeFrac = 1.20f;
    fuelRatio = std::max(0.0f, std::min(1.0f, fuelRatio));
    return kMinLightSizeFrac + (kMaxLightSizeFrac - kMinLightSizeFrac) * fuelRatio;
}

// Classe canônica de uma fonte de luz ÚNICA. O jogador só pode ter UM isqueiro e
// UMA lamparina — ambos coexistem, mas nunca há dois do mesmo tipo. "Flashlight"
// e "Broken Flashlight" são o MESMO isqueiro (aceso/apagado), então mapeiam para
// a mesma chave. Retorna "" para itens que não são fonte de luz (não são únicos).
std::string LightSourceKey(const ItemDef& def) {
    if (!def.HasProperty(ItemProperty::LIGHT_SOURCE)) return "";
    if (def.name == "Flashlight" || def.name == "Broken Flashlight") return "lighter";
    if (def.name == "Lamp") return "lamp";
    return def.name;   // qualquer outra fonte de luz: única pelo próprio nome
}
} // namespace

void Inventory::ClearAll() {
    stacks.clear();
    activeIndex = -1;
    oilApplyMode = false;
    oilApplySourceIndex = -1;
    oilApplyReturnActiveIndex = 0;
    oilTargetSelection = 0;
    primedOilSpritePath.clear();
    usingDrainAccum = 0.0f;
    isLightToggledOn = false;
}

bool Inventory::AddItem(const ItemDef& def, int durability) {
    // Fonte ÚNICA de verdade de "posso pegar?" — ver CanAcceptItem. Garante que a
    // fala "não consigo" e o contorno VERMELHO de item bloqueado usem exatamente o
    // mesmo critério (antes divergiam: fala vinha do AddItem, contorno de um
    // CanAcceptItem que sempre retornava true → nunca ficava vermelho).
    if (!CanAcceptItem(def)) {
        return false;   // bloqueado (bolsa cheia, 2 combustíveis, ou luz duplicada)
    }
    const bool wasEmpty = stacks.empty();
    ItemStack stack;
    stack.def = def;
    stack.count = 1;
    stack.durabilities.push_back(durability);
    stacks.push_back(stack);
    if (wasEmpty) {
        activeIndex = 0;
    }
    return true;
}

void Inventory::SetActiveIndex(int index) {
    activeIndex = index;
}

void Inventory::CycleLeft() {
    activeIndex--;
    // Moving the wheel deselects whatever was lit; the item must be re-activated
    // (press F) when it returns to the center.
    isLightToggledOn = false;
}

void Inventory::CycleRight() {
    activeIndex++;
    isLightToggledOn = false;
}

int Inventory::GetVisibleStackIndex(int visibleOffset, int visibleCount) const {
    if (stacks.empty()) return -1;
    int half = visibleCount / 2;
    int idx = activeIndex - half + visibleOffset;
    int n = static_cast<int>(stacks.size());
    while (idx < 0) idx += n;
    idx = idx % n;
    return idx;
}

int Inventory::GetVisibleSlotCount() const {
    return (GetStackCount() >= 4) ? 5 : 3;
}

int Inventory::GetRingSize() const {
    return std::max(GetStackCount(), GetVisibleSlotCount());
}

int Inventory::GetSelectedStackIndex() const {
    const int count = GetStackCount();
    if (count <= 0) return -1;
    const int ring = GetRingSize();
    // The wheel renders its center slot as ((-activeIndex) mod ring); gameplay
    // must select that exact stack. Positions beyond the item count are empty.
    const int center = ((-activeIndex) % ring + ring) % ring;
    return (center < count) ? center : -1;
}

void Inventory::SetSelectedStackIndex(int stackIndex) {
    const int count = GetStackCount();
    if (count <= 0) {
        activeIndex = 0;
        return;
    }
    stackIndex = std::max(0, std::min(stackIndex, count - 1));
    const int ring = GetRingSize();
    // Solve ((-activeIndex) mod ring) == stackIndex, picking the representative
    // nearest the current activeIndex so the wheel takes the short way around.
    const int base = ((-stackIndex) % ring + ring) % ring;
    const int k = static_cast<int>(std::lround(static_cast<double>(activeIndex - base) / ring));
    activeIndex = base + k * ring;
}

const Inventory::ItemStack* Inventory::GetActiveStack() const {
    const int idx = GetSelectedStackIndex();
    if (idx < 0) return nullptr;
    return &stacks[static_cast<size_t>(idx)];
}

Inventory::ItemStack* Inventory::GetActiveStackMutable() {
    const int idx = GetSelectedStackIndex();
    if (idx < 0) return nullptr;
    return &stacks[static_cast<size_t>(idx)];
}

const Inventory::ItemStack* Inventory::GetStack(int index) const {
    if (index < 0 || index >= static_cast<int>(stacks.size())) return nullptr;
    return &stacks[static_cast<size_t>(index)];
}

int Inventory::GetPrimedOilDurability() const {
    if (!oilApplyMode) return 0;
    if (oilApplySourceIndex < 0 || oilApplySourceIndex >= static_cast<int>(stacks.size())) return 0;
    const std::vector<int>& dur = stacks[static_cast<size_t>(oilApplySourceIndex)].durabilities;
    return dur.empty() ? 0 : dur.front();
}

bool Inventory::CanCombineOilWithActive() const {
    if (!oilApplyMode) return false;
    const int targetIdx = GetSelectedStackIndex();
    if (targetIdx < 0 || targetIdx == oilApplySourceIndex) return false;  // empty slot or the oil itself
    const ItemStack& target = stacks[static_cast<size_t>(targetIdx)];
    if (!target.def.HasProperty(ItemProperty::LIGHT_SOURCE)) return false;
    if (target.durabilities.empty()) return false;
    const int maxDur = target.def.maxDurability;
    if (maxDur > 0 && target.durabilities.front() >= maxDur) return false;  // already full
    return true;
}

void Inventory::ExitOilApplyMode() {
    oilApplyMode = false;
    oilApplySourceIndex = -1;
    oilTargetSelection = 0;
    primedOilSpritePath.clear();
    // Return the wheel to the slot the oil was in when the player started.
    activeIndex = oilApplyReturnActiveIndex;
}

// Fontes de luz (isqueiro/lamparina) que AINDA cabem óleo — os únicos alvos
// válidos para o combustível. O próprio combustível (FUEL) fica de fora.
std::vector<int> Inventory::GetRefuelTargetIndices() const {
    std::vector<int> out;
    for (int i = 0; i < static_cast<int>(stacks.size()); ++i) {
        const ItemStack& s = stacks[static_cast<size_t>(i)];
        if (!s.def.HasProperty(ItemProperty::LIGHT_SOURCE)) continue;
        if (s.durabilities.empty()) continue;
        const int maxDur = s.def.maxDurability;
        if (maxDur > 0 && s.durabilities.front() >= maxDur) continue;   // já cheio
        out.push_back(i);
    }
    return out;
}

int Inventory::GetRefuelTargetCount() const {
    return static_cast<int>(GetRefuelTargetIndices().size());
}

const Inventory::ItemStack* Inventory::GetRefuelTargetStack(int selectionIdx) const {
    const std::vector<int> targets = GetRefuelTargetIndices();
    if (selectionIdx < 0 || selectionIdx >= static_cast<int>(targets.size())) return nullptr;
    return &stacks[static_cast<size_t>(targets[static_cast<size_t>(selectionIdx)])];
}

void Inventory::RefuelSelectionPrev() {
    const int n = GetRefuelTargetCount();
    if (n <= 0) return;
    oilTargetSelection = (oilTargetSelection - 1 + n) % n;
}

void Inventory::RefuelSelectionNext() {
    const int n = GetRefuelTargetCount();
    if (n <= 0) return;
    oilTargetSelection = (oilTargetSelection + 1) % n;
}

bool Inventory::TryPrimeOil() {
    if (oilApplyMode) return false;
    const int sel = GetSelectedStackIndex();
    if (sel < 0) return false;
    const ItemStack& active = stacks[static_cast<size_t>(sel)];
    if (!active.def.HasProperty(ItemProperty::FUEL)) return false;
    if (active.durabilities.empty() || active.durabilities.front() <= 0) return false;
    // Sem nenhuma fonte de luz que caiba óleo, não há o que abastecer.
    if (GetRefuelTargetIndices().empty()) return false;

    // Enter apply mode. The oil stays where it is; remember its slot so we can
    // return there (showing it, or an empty slot if it gets used up).
    oilApplyMode = true;
    oilApplySourceIndex = sel;
    oilApplyReturnActiveIndex = activeIndex;
    oilTargetSelection = 0;
    primedOilSpritePath = active.def.spritePath;
    return true;
}

bool Inventory::TryCombineOil() {
    if (!oilApplyMode) return false;
    if (oilApplySourceIndex < 0 || oilApplySourceIndex >= static_cast<int>(stacks.size())) {
        ExitOilApplyMode();
        return false;
    }

    // Alvo = item escolhido no modal central (lista de fontes de luz com espaço).
    std::vector<int> targets = GetRefuelTargetIndices();
    if (targets.empty()) { ExitOilApplyMode(); return false; }
    if (oilTargetSelection < 0 || oilTargetSelection >= static_cast<int>(targets.size())) {
        oilTargetSelection = 0;
    }
    const int targetIdx = targets[static_cast<size_t>(oilTargetSelection)];
    if (targetIdx == oilApplySourceIndex) return false;  // segurança (nunca o próprio óleo)

    ItemStack& target = stacks[static_cast<size_t>(targetIdx)];
    if (!target.def.HasProperty(ItemProperty::LIGHT_SOURCE)) return false;
    if (target.durabilities.empty()) return false;

    const int maxDur = target.def.maxDurability;
    int& targetDurability = target.durabilities.front();
    if (maxDur > 0 && targetDurability >= maxDur) return false;  // already full

    ItemStack& oil = stacks[static_cast<size_t>(oilApplySourceIndex)];
    if (oil.durabilities.empty() || oil.durabilities.front() <= 0) {
        ExitOilApplyMode();
        return false;
    }

    const int oilAmount = oil.durabilities.front();
    const int room = (maxDur > 0) ? (maxDur - targetDurability) : oilAmount;
    // O combustível é usado POR COMPLETO: enche o alvo até onde der e o excedente
    // se perde. Uma unidade de combustível é sempre gasta inteira, nunca em parte.
    targetDurability += std::min(room, oilAmount);   // muta o alvo antes de qualquer erase

    // Remove a unidade de combustível inteira da bolsa.
    oil.durabilities.erase(oil.durabilities.begin());
    oil.count--;
    bool oilStackRemoved = false;
    if (oil.count <= 0) {
        stacks.erase(stacks.begin() + oilApplySourceIndex);
        oilStackRemoved = true;
    }
    // A partir daqui NÃO usar mais as referências `oil`/`target`/`targetDurability`
    // (o erase acima pode tê-las invalidado).

    // Mantém selecionado o item RECARREGADO (isqueiro/lamparina), não o combustível.
    // Ajusta o índice caso a remoção do combustível o tenha deslocado.
    int finalTargetIdx = targetIdx;
    if (oilStackRemoved && oilApplySourceIndex < targetIdx) {
        finalTargetIdx--;
    }
    oilApplyMode = false;
    oilApplySourceIndex = -1;
    primedOilSpritePath.clear();
    SetSelectedStackIndex(finalTargetIdx);
    return true;
}

void Inventory::CancelOil() {
    if (!oilApplyMode) return;
    ExitOilApplyMode();
}

bool Inventory::TryTurnLightOn() {
    const ItemStack* active = GetActiveStack();
    if (!active || !active->def.HasProperty(ItemProperty::LIGHT_SOURCE)) return false;
    if (!active->durabilities.empty() && active->durabilities.front() > 0) {
        isLightToggledOn = true;
        return true;
    }
    return false;
}

bool Inventory::TryActivateBestLight() {
    if (IsUsableLightActive()) {
        return true;   // already lit — nothing to do
    }

    auto hasFuel = [&](int idx) {
        if (idx < 0 || idx >= static_cast<int>(stacks.size())) return false;
        const std::vector<int>& d = stacks[static_cast<size_t>(idx)].durabilities;
        return !d.empty() && d.front() > 0;
    };

    // Prefer the lighter ("Flashlight"); fall back to the lamp.
    int idx = FindBestFlashlight();
    if (!hasFuel(idx)) {
        idx = FindBestLamp();
    }
    if (!hasFuel(idx)) {
        return false;
    }

    SetSelectedStackIndex(idx);
    return TryTurnLightOn();
}

HeldPropVisual Inventory::GetHeldPropVisual() const {
    const ItemStack* active = GetActiveStack();
    if (!active || !active->def.HasProperty(ItemProperty::LIGHT_SOURCE)) {
        return HeldPropVisual::None;
    }
    const std::string& name = active->def.name;
    if (name == "Flashlight" || name == "Broken Flashlight") {
        // #16 Isqueiro APAGADO → mãos vazias (animação idle/walk normal).
        // Só aparece na mão quando aceso. (Aceso = luz ligada de verdade.)
        return isLightToggledOn ? HeldPropVisual::Lighter : HeldPropVisual::None;
    }
    if (name == "Lamp") {
        return HeldPropVisual::Lamp;
    }
    return HeldPropVisual::None;
}

bool Inventory::IsUsableLightActive() const {
    const ItemStack* active = GetActiveStack();
    if (!active || !isLightToggledOn) return false;
    if (active->durabilities.empty() || active->durabilities.front() <= 0) return false;
    return active->def.HasProperty(ItemProperty::LIGHT_SOURCE);
}

bool Inventory::IsActiveLightLamp() const {
    if (!IsUsableLightActive()) return false;
    const ItemStack* active = GetActiveStack();
    return active && active->def.name == "Lamp";
}

bool Inventory::IsActiveLightLighter() const {
    if (!IsUsableLightActive()) return false;
    const ItemStack* active = GetActiveStack();
    return active && (active->def.name == "Flashlight" || active->def.name == "Broken Flashlight");
}

bool Inventory::IsActiveItemLighter() const {
    const ItemStack* active = GetActiveStack();
    return active && (active->def.name == "Flashlight" || active->def.name == "Broken Flashlight");
}

bool Inventory::IsActiveItemFuel() const {
    const ItemStack* active = GetActiveStack();
    return active && active->def.HasProperty(ItemProperty::FUEL);
}

bool Inventory::HasDepletedLightSource() const {
    for (const auto& s : stacks) {
        if (s.def.HasProperty(ItemProperty::LIGHT_SOURCE) && s.def.maxDurability > 0) {
            if (s.durabilities.empty() || s.durabilities.front() <= 0) return true;
        }
    }
    return false;
}

// Só o ISQUEIRO ("Flashlight"/"Broken Flashlight") esgotado — a lamparina não
// dispara o aviso de "sua luz apagou".
bool Inventory::HasDepletedLighter() const {
    for (const auto& s : stacks) {
        const bool isLighter = (s.def.name == "Flashlight" || s.def.name == "Broken Flashlight");
        if (isLighter && s.def.maxDurability > 0) {
            if (s.durabilities.empty() || s.durabilities.front() <= 0) return true;
        }
    }
    return false;
}

float Inventory::GetSelectedLightFuelRatio() const {
    const ItemStack* active = GetActiveStack();
    if (!active || !active->def.HasProperty(ItemProperty::LIGHT_SOURCE)) return 0.0f;
    if (active->def.maxDurability <= 0) return 1.0f;
    if (active->durabilities.empty()) return 0.0f;
    float ratio = static_cast<float>(active->durabilities.front()) / static_cast<float>(active->def.maxDurability);
    return std::max(0.0f, std::min(1.0f, ratio));
}

LightMaskParams Inventory::BuildLighterLightParams(const LightMaskParams& base) const {
    LightMaskParams params = base;
    const float sizeScale = FuelToSizeScale(GetSelectedLightFuelRatio());
    params.falloffRadiusPx *= sizeScale;
    params.shadowMaxLengthPx *= sizeScale;
    params.coneLengthPx *= sizeScale;
    return params;
}

LightMaskParams Inventory::BuildLampLightParams(const LightMaskParams& base) const {
    LightMaskParams params = base;
    // Mesma curva do isqueiro: 100% de carga => 100% do tamanho; 0% => 30%.
    // (Antes a lamparina era maior — brilho quebrado que deixava o monstro sem
    // chance. Agora segue o mesmo piso/teto do isqueiro.)
    const float sizeScale = FuelToSizeScale(GetSelectedLightFuelRatio());
    params.falloffRadiusPx *= sizeScale;
    params.shadowMaxLengthPx *= sizeScale;
    params.coneLengthPx *= sizeScale;
    return params;
}

void Inventory::TickUsingDurability(float dt) {
    if (!IsUsableLightActive()) return;
    const int sel = GetSelectedStackIndex();
    if (sel < 0) return;
    ItemStack* active = &stacks[static_cast<size_t>(sel)];
    if (active->durabilities.empty()) return;
    if (active->def.maxDurability <= 0 || active->durabilities.front() <= 0) return;

    const float drainInterval = (active->def.name == "Lamp") ? 2.0f : 1.0f;
    usingDrainAccum += dt;
    while (usingDrainAccum >= drainInterval && !active->durabilities.empty() && active->durabilities.front() > 0) {
        active->durabilities.front() -= 1;
        usingDrainAccum -= drainInterval;
        if (active->durabilities.front() <= 0) {
            // A fonte de luz (isqueiro/lamparina) é um item único e RECARREGÁVEL:
            // ao esgotar o combustível ela NÃO some da bolsa — fica com carga 0 e
            // apenas apaga. Recarregar com combustível volta a acender.
            active->durabilities.front() = 0;
            isLightToggledOn = false;
            usingDrainAccum = 0.0f;
            break;
        }
    }
}

bool Inventory::HasItem(const std::string& name) const {
    return FindStackWithName(name) >= 0;
}

bool Inventory::HasDepletedLightAndFuel() const {
    bool depletedLight = false;
    bool hasFuel = false;
    for (const ItemStack& s : stacks) {
        const int dur = s.durabilities.empty() ? 0 : s.durabilities.front();
        if (s.def.HasProperty(ItemProperty::LIGHT_SOURCE) && s.def.maxDurability > 0 && dur <= 0) {
            depletedLight = true;
        }
        if (s.def.HasProperty(ItemProperty::FUEL) && dur > 0) {
            hasFuel = true;
        }
    }
    return depletedLight && hasFuel;
}

bool Inventory::CanAcceptItem(const ItemDef& def) const {
    // Critério único de "posso pegar este item?", compartilhado pelo AddItem
    // (bloqueia a coleta + dispara a fala "não consigo") e pelo IsPickupBlocked
    // (contorno vermelho + esconde o prompt "Pegar"). Assim os dois SEMPRE batem.
    if (IsFull()) {
        return false;   // bolsa cheia
    }
    // Combustível é limitado a no máximo kMaxFuelUnits (2) na bolsa.
    if (def.HasProperty(ItemProperty::FUEL)) {
        int fuelUnits = 0;
        for (const ItemStack& s : stacks) {
            if (s.def.HasProperty(ItemProperty::FUEL)) fuelUnits += s.count;
        }
        if (fuelUnits >= kMaxFuelUnits) {
            return false;   // já tem 2 combustíveis
        }
    }
    // Fonte de luz é ÚNICA por tipo: nunca aceita um segundo isqueiro/lamparina
    // (a luz é um item único e RECARREGÁVEL, não empilhável).
    const std::string lightKey = LightSourceKey(def);
    if (!lightKey.empty()) {
        for (const ItemStack& s : stacks) {
            if (LightSourceKey(s.def) == lightKey) {
                return false;   // já possui essa fonte de luz
            }
        }
    }
    return true;
}

bool Inventory::TryConsumeItem(const std::string& name) {
    int idx = FindStackWithName(name);
    if (idx < 0) return false;
    ItemStack& stack = stacks[static_cast<size_t>(idx)];
    if (stack.count <= 0) return false;

    stack.count--;
    if (!stack.durabilities.empty()) {
        stack.durabilities.erase(stack.durabilities.begin());
    }

    if (stack.count <= 0) {
        int sel = GetSelectedStackIndex();
        stacks.erase(stacks.begin() + idx);
        if (stacks.empty()) {
            activeIndex = 0;
        } else {
            if (sel > idx) {
                sel--;  // the selected item shifted down a slot
            } else if (sel < 0) {
                sel = 0;
            }
            SetSelectedStackIndex(sel);
        }
    }

    return true;
}

void Inventory::DedupeLightSources() {
    // Mantém a 1ª ocorrência de cada tipo de fonte de luz, fundindo a maior carga
    // das cópias nela, e remove as duplicatas. n <= kMaxCapacity, então O(n^2) ok.
    for (size_t i = 0; i < stacks.size();) {
        const std::string key = LightSourceKey(stacks[i].def);
        if (key.empty()) { ++i; continue; }
        int firstIdx = -1;
        for (size_t j = 0; j < i; ++j) {
            if (LightSourceKey(stacks[j].def) == key) { firstIdx = static_cast<int>(j); break; }
        }
        if (firstIdx < 0) { ++i; continue; }   // 1ª ocorrência — preserva
        // Duplicata: fica com a maior carga na 1ª cópia e remove esta.
        const int curDur = stacks[i].durabilities.empty() ? 0 : stacks[i].durabilities.front();
        ItemStack& keep = stacks[static_cast<size_t>(firstIdx)];
        const int keepDur = keep.durabilities.empty() ? 0 : keep.durabilities.front();
        if (curDur > keepDur) {
            if (keep.durabilities.empty()) keep.durabilities.push_back(curDur);
            else keep.durabilities.front() = curDur;
        }
        stacks.erase(stacks.begin() + static_cast<long>(i));
        // não incrementa i — o próximo item deslizou para esta posição
    }
}

int Inventory::FindStackWithName(const std::string& name) const {
    for (size_t i = 0; i < stacks.size(); ++i) {
        if (stacks[i].def.name == name) return static_cast<int>(i);
    }
    return -1;
}

int Inventory::FindStackWithProperty(ItemProperty prop) const {
    for (size_t i = 0; i < stacks.size(); ++i) {
        if (stacks[i].def.HasProperty(prop)) return static_cast<int>(i);
    }
    return -1;
}

int Inventory::FindBestFlashlight() const {
    int best = -1;
    int bestDur = -1;
    for (size_t i = 0; i < stacks.size(); ++i) {
        if (!stacks[i].def.HasProperty(ItemProperty::LIGHT_SOURCE)) continue;
        const std::string& name = stacks[i].def.name;
        if (name != "Flashlight" && name != "Broken Flashlight") continue;
        int dur = stacks[i].durabilities.empty() ? 0 : stacks[i].durabilities.front();
        if (dur > bestDur) { bestDur = dur; best = static_cast<int>(i); }
    }
    return best;
}

int Inventory::FindBestLamp() const {
    int best = -1;
    int bestDur = -1;
    for (size_t i = 0; i < stacks.size(); ++i) {
        if (!stacks[i].def.HasProperty(ItemProperty::LIGHT_SOURCE)) continue;
        if (stacks[i].def.name != "Lamp") continue;
        int dur = stacks[i].durabilities.empty() ? 0 : stacks[i].durabilities.front();
        if (dur > bestDur) { bestDur = dur; best = static_cast<int>(i); }
    }
    return best;
}

namespace {

const ItemDef* FindItemDefByName(const std::string& name, const std::vector<ItemDef>& catalog) {
    for (const ItemDef& def : catalog) {
        if (def.name == name) return &def;
    }
    if (name == "Lamp Fuel" || name == "Lighter Fuel" || name == "Light Fuel" || name == "Oil Gallon") {
        for (const ItemDef& def : catalog) {
            if (def.name == "Fuel") return &def;
        }
    }
    return nullptr;
}

} // namespace

void Inventory::WriteToSave(SaveGameState& state) const {
    state.lightOn = isLightToggledOn;

    state.inventoryStacks.clear();
    for (const ItemStack& stack : stacks) {
        SavedInventoryStack saved;
        saved.name = stack.def.name;
        saved.count = stack.count;
        saved.durabilities = stack.durabilities;
        state.inventoryStacks.push_back(saved);
    }
    const int selectedIndex = GetSelectedStackIndex();
    state.activeStackIndex = selectedIndex;
    // Oil is now a normal fuel stack saved with the rest of the inventory; the
    // transient apply-mode state is not persisted.
    state.primedOilDurability = 0;

    state.selectedSlot = selectedIndex;
    state.inventorySlots.clear();
    state.selectedBackpackGroup = -1;
    state.backpackGroups.clear();
    state.usingItem.reset();
    state.slots.clear();
}

void Inventory::ReadFromSave(const SaveGameState& state, const std::vector<ItemDef>& itemCatalog) {
    ClearAll();

    isLightToggledOn = state.lightOn;

    if (!state.inventoryStacks.empty()) {
        for (const SavedInventoryStack& saved : state.inventoryStacks) {
            const ItemDef* def = FindItemDefByName(saved.name, itemCatalog);
            if (!def) continue;
            ItemStack stack;
            stack.def = *def;
            stack.count = saved.count;
            stack.durabilities = saved.durabilities;
            stacks.push_back(stack);
        }
        // Este caminho restaura os stacks direto (sem passar pelo AddItem), então
        // cura aqui qualquer fonte de luz duplicada gravada por saves antigos.
        DedupeLightSources();
        if (stacks.empty()) {
            activeIndex = -1;
        } else {
            SetSelectedStackIndex(state.activeStackIndex);
        }
        return;
    }

    if (!state.inventorySlots.empty()) {
        for (size_t i = 0; i < state.inventorySlots.size() && i < 25; ++i) {
            if (!state.inventorySlots[i].has_value()) continue;
            const ItemDef* def = FindItemDefByName(state.inventorySlots[i]->name, itemCatalog);
            if (!def) continue;
            AddItem(*def, state.inventorySlots[i]->durability);
        }
        if (stacks.empty()) {
            activeIndex = -1;
        } else {
            SetSelectedStackIndex(state.selectedSlot);
        }
        return;
    }

    if (!state.backpackGroups.empty()) {
        for (const SavedBackpackGroup& group : state.backpackGroups) {
            for (const SavedItemSlot& saved : group.items) {
                const ItemDef* def = FindItemDefByName(saved.name, itemCatalog);
                if (def) AddItem(*def, saved.durability);
            }
        }
        int sel = -1;
        if (state.selectedBackpackGroup >= 0 && state.selectedBackpackGroup < static_cast<int>(state.backpackGroups.size())) {
            const auto& oldGroup = state.backpackGroups[static_cast<size_t>(state.selectedBackpackGroup)];
            if (!oldGroup.items.empty()) {
                sel = FindStackWithName(oldGroup.items.front().name);
            }
        }
        if (sel < 0) {
            sel = FindBestFlashlight();
        }
        if (sel < 0 && !stacks.empty()) {
            sel = 0;
        }
        if (sel >= 0) {
            SetSelectedStackIndex(sel);
        }
        return;
    }

    if (!state.slots.empty()) {
        for (size_t i = 0; i < state.slots.size() && i < 25; ++i) {
            if (!state.slots[i].has_value()) continue;
            const ItemDef* def = FindItemDefByName(state.slots[i]->name, itemCatalog);
            if (!def) continue;
            AddItem(*def, state.slots[i]->durability);
        }
        int sel = -1;
        if (state.usingItem.has_value()) {
            sel = FindStackWithName(state.usingItem->name);
        }
        if (sel < 0) {
            sel = FindBestFlashlight();
        }
        if (sel < 0 && !stacks.empty()) {
            sel = 0;
        }
        if (sel >= 0) {
            SetSelectedStackIndex(sel);
        }
        return;
    }

    if (!stacks.empty()) {
        int sel = FindBestFlashlight();
        if (sel < 0) sel = 0;
        SetSelectedStackIndex(sel);
    }
}
