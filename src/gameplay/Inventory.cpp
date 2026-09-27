#include "gameplay/Inventory.h"
#include "core/SaveData.h"

#include <algorithm>
#include <cmath>

namespace {

// Carga da frente de uma pilha (0 se não houver nenhuma).
int FrontCharge(const Inventory::ItemStack& s) {
    return s.durabilities.empty() ? 0 : s.durabilities.front();
}

// "Flashlight" e "Broken Flashlight" são o MESMO isqueiro (aceso/apagado).
bool IsLighterName(const std::string& name) {
    return name == "Flashlight" || name == "Broken Flashlight";
}

// Carga → tamanho da luz, igual para isqueiro e lamparina: 0% → 48%, 100% → 120%.
float FuelToSizeScale(float fuelRatio) {
    constexpr float kMinLightSizeFrac = 0.48f;
    constexpr float kMaxLightSizeFrac = 1.20f;
    return kMinLightSizeFrac + (kMaxLightSizeFrac - kMinLightSizeFrac) * std::clamp(fuelRatio, 0.0f, 1.0f);
}

// Chave de unicidade de uma fonte de luz: "lighter", "lamp" ou o próprio nome.
// "" para itens que não são luz (esses podem repetir).
std::string LightSourceKey(const ItemDef& def) {
    if (!def.HasProperty(ItemProperty::LIGHT_SOURCE)) return "";
    if (IsLighterName(def.name)) return "lighter";
    if (def.name == "Lamp") return "lamp";
    return def.name;
}

// Fonte de luz que ainda cabe combustível.
bool CanTakeFuel(const Inventory::ItemStack& s) {
    if (!s.def.HasProperty(ItemProperty::LIGHT_SOURCE) || s.durabilities.empty()) return false;
    return s.def.maxDurability <= 0 || s.durabilities.front() < s.def.maxDurability;
}

// Definição do item pelo nome no catálogo; nomes antigos de combustível viram "Fuel".
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

}  // namespace

// ═════════════════════════════════════════════════════════════════════════════
//  Itens
// ═════════════════════════════════════════════════════════════════════════════

void Inventory::ClearAll() {
    stacks.clear();
    activeIndex = -1;
    oilApplyMode = false;
    oilApplySourceIndex = -1;
    oilApplyReturnActiveIndex = 0;
    oilTargetSelection = 0;
    usingDrainAccum = 0.0f;
    isLightToggledOn = false;
}

// Adiciona como pilha nova (se CanAcceptItem deixar). O 1º item vai para a mão.
bool Inventory::AddItem(const ItemDef& def, int durability) {
    if (!CanAcceptItem(def)) return false;
    const bool wasEmpty = stacks.empty();
    stacks.push_back(ItemStack{def, 1, {durability}});
    if (wasEmpty) activeIndex = 0;
    return true;
}

// Recusa com a bolsa cheia, com combustível no limite ou com uma luz do mesmo
// tipo já na bolsa. É o mesmo critério da fala "não consigo" e do contorno vermelho.
bool Inventory::CanAcceptItem(const ItemDef& def) const {
    if (IsFull()) return false;
    if (def.HasProperty(ItemProperty::FUEL) && IsFuelAtMax()) return false;
    const std::string key = LightSourceKey(def);
    if (key.empty()) return true;
    return std::none_of(stacks.begin(), stacks.end(),
                        [&](const ItemStack& s) { return LightSourceKey(s.def) == key; });
}

bool Inventory::IsFuelAtMax() const {
    int units = 0;
    for (const ItemStack& s : stacks) {
        if (s.def.HasProperty(ItemProperty::FUEL)) units += s.count;
    }
    return units >= kMaxFuelUnits;
}

bool Inventory::HasItem(const std::string& name) const {
    return FindStackWithName(name) >= 0;
}

// Gasta uma unidade do item; se a pilha acabar, some e a roda mantém o item que estava na mão.
bool Inventory::TryConsumeItem(const std::string& name) {
    const int idx = FindStackWithName(name);
    if (idx < 0) return false;
    ItemStack& stack = stacks[static_cast<size_t>(idx)];
    if (stack.count <= 0) return false;

    stack.count--;
    if (!stack.durabilities.empty()) stack.durabilities.erase(stack.durabilities.begin());
    if (stack.count > 0) return true;

    int sel = GetSelectedStackIndex();
    stacks.erase(stacks.begin() + idx);
    if (stacks.empty()) {
        activeIndex = 0;
        return true;
    }
    if (sel > idx)     sel--;    // o item da mão desceu uma posição
    else if (sel < 0)  sel = 0;
    SetSelectedStackIndex(sel);
    return true;
}

const Inventory::ItemStack* Inventory::GetStack(int index) const {
    if (index < 0 || index >= GetStackCount()) return nullptr;
    return &stacks[static_cast<size_t>(index)];
}

int Inventory::FindStackWithName(const std::string& name) const {
    for (size_t i = 0; i < stacks.size(); ++i) {
        if (stacks[i].def.name == name) return static_cast<int>(i);
    }
    return -1;
}

// Isqueiro (lighter = true) ou lamparina com mais carga; -1 se não houver.
int Inventory::FindBestLight(bool lighter) const {
    int best = -1, bestCharge = -1;
    for (size_t i = 0; i < stacks.size(); ++i) {
        const ItemStack& s = stacks[i];
        if (!s.def.HasProperty(ItemProperty::LIGHT_SOURCE)) continue;
        if (lighter ? !IsLighterName(s.def.name) : s.def.name != "Lamp") continue;
        if (FrontCharge(s) > bestCharge) {
            bestCharge = FrontCharge(s);
            best = static_cast<int>(i);
        }
    }
    return best;
}

// Mantém uma fonte de luz de cada tipo (a 1ª), com a maior carga entre as cópias.
void Inventory::DedupeLightSources() {
    for (size_t i = 0; i < stacks.size();) {
        const std::string key = LightSourceKey(stacks[i].def);
        auto first = std::find_if(stacks.begin(), stacks.begin() + static_cast<long>(i),
                                  [&](const ItemStack& s) { return !key.empty() && LightSourceKey(s.def) == key; });
        if (key.empty() || first == stacks.begin() + static_cast<long>(i)) {
            ++i;
            continue;
        }
        const int dupCharge = FrontCharge(stacks[i]);
        if (dupCharge > FrontCharge(*first)) {
            if (first->durabilities.empty()) first->durabilities.push_back(dupCharge);
            else first->durabilities.front() = dupCharge;
        }
        stacks.erase(stacks.begin() + static_cast<long>(i));   // o próximo desliza para i
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Roda
// ═════════════════════════════════════════════════════════════════════════════

// Girar a roda apaga a luz: o item precisa ser ligado de novo ([F]) no centro.
void Inventory::CycleLeft() {
    activeIndex--;
    isLightToggledOn = false;
}

void Inventory::CycleRight() {
    activeIndex++;
    isLightToggledOn = false;
}

// Sempre 3. Com 5, os ajustes manuais de posição da roda (medidos para 3)
// fazem os slots vizinhos colapsarem sobre o centro. A roda e o gameplay leem
// daqui, para o item desenhado no centro ser sempre o que o [F] usa.
int Inventory::GetVisibleSlotCount() const {
    return 3;
}

int Inventory::GetRingSize() const {
    return std::max(GetStackCount(), GetVisibleSlotCount());
}

// O centro da roda é ((-activeIndex) mod ring); posições além dos itens são vazias.
int Inventory::GetSelectedStackIndex() const {
    const int count = GetStackCount();
    if (count <= 0) return -1;
    const int ring = GetRingSize();
    const int center = ((-activeIndex) % ring + ring) % ring;
    return (center < count) ? center : -1;
}

// Resolve ((-activeIndex) mod ring) == stackIndex escolhendo o activeIndex mais
// perto do atual, para a roda girar pelo caminho curto.
void Inventory::SetSelectedStackIndex(int stackIndex) {
    const int count = GetStackCount();
    if (count <= 0) {
        activeIndex = 0;
        return;
    }
    stackIndex = std::clamp(stackIndex, 0, count - 1);
    const int ring = GetRingSize();
    const int base = ((-stackIndex) % ring + ring) % ring;
    const int k = static_cast<int>(std::lround(static_cast<double>(activeIndex - base) / ring));
    activeIndex = base + k * ring;
}

const Inventory::ItemStack* Inventory::GetActiveStack() const {
    const int idx = GetSelectedStackIndex();
    return (idx < 0) ? nullptr : &stacks[static_cast<size_t>(idx)];
}

// ═════════════════════════════════════════════════════════════════════════════
//  Luz
// ═════════════════════════════════════════════════════════════════════════════

bool Inventory::TryTurnLightOn() {
    const ItemStack* active = GetActiveStack();
    if (!active || !active->def.HasProperty(ItemProperty::LIGHT_SOURCE) || FrontCharge(*active) <= 0) return false;
    isLightToggledOn = true;
    return true;
}

// Se nada estiver aceso, põe na mão o isqueiro com carga (ou a lamparina) e acende.
bool Inventory::TryActivateBestLight() {
    if (IsUsableLightActive()) return true;
    auto hasCharge = [&](int idx) { return idx >= 0 && FrontCharge(stacks[static_cast<size_t>(idx)]) > 0; };
    int idx = FindBestLight(true);
    if (!hasCharge(idx)) idx = FindBestLight(false);
    if (!hasCharge(idx)) return false;
    SetSelectedStackIndex(idx);
    return TryTurnLightOn();
}

bool Inventory::IsUsableLightActive() const {
    const ItemStack* active = GetActiveStack();
    return active && isLightToggledOn && FrontCharge(*active) > 0 &&
           active->def.HasProperty(ItemProperty::LIGHT_SOURCE);
}

bool Inventory::IsActiveLightLighter() const {
    return IsUsableLightActive() && IsLighterName(GetActiveStack()->def.name);
}

bool Inventory::IsActiveLightLamp() const {
    return IsUsableLightActive() && GetActiveStack()->def.name == "Lamp";
}

bool Inventory::IsActiveItemLighter() const {
    const ItemStack* active = GetActiveStack();
    return active && IsLighterName(active->def.name);
}

bool Inventory::IsActiveItemFuel() const {
    const ItemStack* active = GetActiveStack();
    return active && active->def.HasProperty(ItemProperty::FUEL);
}

bool Inventory::HasDepletedLighter() const {
    return std::any_of(stacks.begin(), stacks.end(), [](const ItemStack& s) {
        return IsLighterName(s.def.name) && s.def.maxDurability > 0 && FrontCharge(s) <= 0;
    });
}

// Isqueiro ou lamparina só aparecem na mão ACESOS — assim o jogador vê pelo
// personagem se a luz está ligada.
HeldPropVisual Inventory::GetHeldPropVisual() const {
    const ItemStack* active = GetActiveStack();
    if (!active || !isLightToggledOn || !active->def.HasProperty(ItemProperty::LIGHT_SOURCE)) {
        return HeldPropVisual::None;
    }
    if (IsLighterName(active->def.name)) return HeldPropVisual::Lighter;
    if (active->def.name == "Lamp")      return HeldPropVisual::Lamp;
    return HeldPropVisual::None;
}

float Inventory::GetSelectedLightFuelRatio() const {
    const ItemStack* active = GetActiveStack();
    if (!active || !active->def.HasProperty(ItemProperty::LIGHT_SOURCE)) return 0.0f;
    if (active->def.maxDurability <= 0) return 1.0f;
    if (active->durabilities.empty()) return 0.0f;
    return std::clamp(static_cast<float>(active->durabilities.front()) / active->def.maxDurability, 0.0f, 1.0f);
}

// Raio, sombra e cone da luz escalados pela carga (FuelToSizeScale).
LightMaskParams Inventory::BuildLighterLightParams(const LightMaskParams& base) const {
    LightMaskParams params = base;
    const float scale = FuelToSizeScale(GetSelectedLightFuelRatio());
    params.falloffRadiusPx   *= scale;
    params.shadowMaxLengthPx *= scale;
    params.coneLengthPx      *= scale;
    return params;
}

LightMaskParams Inventory::BuildLampLightParams(const LightMaskParams& base) const {
    return BuildLighterLightParams(base);
}

// Gasta 1 de carga por segundo (lamparina: a cada 2 s). Ao zerar, apaga — a
// luz fica na bolsa com carga 0, esperando combustível.
void Inventory::TickUsingDurability(float dt) {
    if (!IsUsableLightActive()) return;
    ItemStack& active = stacks[static_cast<size_t>(GetSelectedStackIndex())];
    if (active.def.maxDurability <= 0) return;

    const float drainInterval = (active.def.name == "Lamp") ? 2.0f : 1.0f;
    int& charge = active.durabilities.front();
    usingDrainAccum += dt;
    while (usingDrainAccum >= drainInterval && charge > 0) {
        charge -= 1;
        usingDrainAccum -= drainInterval;
        if (charge <= 0) {
            charge = 0;
            isLightToggledOn = false;
            usingDrainAccum = 0.0f;
        }
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Reabastecimento
// ═════════════════════════════════════════════════════════════════════════════

std::vector<int> Inventory::GetRefuelTargetIndices() const {
    std::vector<int> out;
    for (int i = 0; i < GetStackCount(); ++i) {
        if (CanTakeFuel(stacks[static_cast<size_t>(i)])) out.push_back(i);
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
    if (n > 0) oilTargetSelection = (oilTargetSelection - 1 + n) % n;
}

void Inventory::RefuelSelectionNext() {
    const int n = GetRefuelTargetCount();
    if (n > 0) oilTargetSelection = (oilTargetSelection + 1) % n;
}

// Combustível com carga na mão e alguma luz que caiba óleo: abre o modal,
// lembrando onde a roda estava para voltar lá.
bool Inventory::TryPrimeOil() {
    if (oilApplyMode) return false;
    const int sel = GetSelectedStackIndex();
    if (sel < 0) return false;
    const ItemStack& active = stacks[static_cast<size_t>(sel)];
    if (!active.def.HasProperty(ItemProperty::FUEL) || FrontCharge(active) <= 0) return false;
    if (GetRefuelTargetIndices().empty()) return false;

    oilApplyMode = true;
    oilApplySourceIndex = sel;
    oilApplyReturnActiveIndex = activeIndex;
    oilTargetSelection = 0;
    return true;
}

// Despeja a unidade de combustível INTEIRA no alvo escolhido (o que não couber
// se perde), remove-a da bolsa e deixa a luz recarregada na mão.
bool Inventory::TryCombineOil() {
    if (!oilApplyMode) return false;
    if (oilApplySourceIndex < 0 || oilApplySourceIndex >= GetStackCount()) {
        ExitOilApplyMode();
        return false;
    }
    const std::vector<int> targets = GetRefuelTargetIndices();
    if (targets.empty()) {
        ExitOilApplyMode();
        return false;
    }
    if (oilTargetSelection < 0 || oilTargetSelection >= static_cast<int>(targets.size())) oilTargetSelection = 0;
    const int targetIdx = targets[static_cast<size_t>(oilTargetSelection)];
    if (targetIdx == oilApplySourceIndex) return false;

    ItemStack& oil = stacks[static_cast<size_t>(oilApplySourceIndex)];
    if (FrontCharge(oil) <= 0) {
        ExitOilApplyMode();
        return false;
    }

    ItemStack& target = stacks[static_cast<size_t>(targetIdx)];
    const int maxDur = target.def.maxDurability;
    const int oilAmount = oil.durabilities.front();
    const int room = (maxDur > 0) ? (maxDur - target.durabilities.front()) : oilAmount;
    target.durabilities.front() += std::min(room, oilAmount);   // antes do erase (que invalida referências)

    oil.durabilities.erase(oil.durabilities.begin());
    oil.count--;
    int finalTargetIdx = targetIdx;
    if (oil.count <= 0) {
        stacks.erase(stacks.begin() + oilApplySourceIndex);
        if (oilApplySourceIndex < targetIdx) finalTargetIdx--;
    }

    oilApplyMode = false;
    oilApplySourceIndex = -1;
    SetSelectedStackIndex(finalTargetIdx);
    return true;
}

void Inventory::CancelOil() {
    if (oilApplyMode) ExitOilApplyMode();
}

// Fecha o modal e devolve a roda à posição de antes.
void Inventory::ExitOilApplyMode() {
    oilApplyMode = false;
    oilApplySourceIndex = -1;
    oilTargetSelection = 0;
    activeIndex = oilApplyReturnActiveIndex;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Save
// ═════════════════════════════════════════════════════════════════════════════

// Grava as pilhas e o item na mão no formato atual (os campos antigos vão vazios).
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
    const int selected = GetSelectedStackIndex();
    state.activeStackIndex = selected;
    state.selectedSlot = selected;
    state.primedOilDurability = 0;   // o modal de combustível não é salvo

    state.inventorySlots.clear();
    state.selectedBackpackGroup = -1;
    state.backpackGroups.clear();
    state.usingItem.reset();
    state.slots.clear();
}

// Lê o formato atual (pilhas) ou, em saves antigos, um dos três formatos
// anteriores (slots do inventário, grupos da mochila, slots + item em uso).
void Inventory::ReadFromSave(const SaveGameState& state, const std::vector<ItemDef>& itemCatalog) {
    ClearAll();
    isLightToggledOn = state.lightOn;
    auto add = [&](const std::string& name, int durability) {
        if (const ItemDef* def = FindItemDefByName(name, itemCatalog)) AddItem(*def, durability);
    };

    if (!state.inventoryStacks.empty()) {
        for (const SavedInventoryStack& saved : state.inventoryStacks) {
            const ItemDef* def = FindItemDefByName(saved.name, itemCatalog);
            if (def) stacks.push_back(ItemStack{*def, saved.count, saved.durabilities});
        }
        DedupeLightSources();   // aqui não passa pelo AddItem
        if (stacks.empty()) activeIndex = -1;
        else SetSelectedStackIndex(state.activeStackIndex);
        return;
    }

    if (!state.inventorySlots.empty()) {
        for (size_t i = 0; i < state.inventorySlots.size() && i < 25; ++i) {
            if (state.inventorySlots[i].has_value()) add(state.inventorySlots[i]->name, state.inventorySlots[i]->durability);
        }
        if (stacks.empty()) activeIndex = -1;
        else SetSelectedStackIndex(state.selectedSlot);
        return;
    }

    if (!state.backpackGroups.empty()) {
        for (const SavedBackpackGroup& group : state.backpackGroups) {
            for (const SavedItemSlot& saved : group.items) add(saved.name, saved.durability);
        }
        int preferred = -1;
        const int g = state.selectedBackpackGroup;
        if (g >= 0 && g < static_cast<int>(state.backpackGroups.size()) && !state.backpackGroups[static_cast<size_t>(g)].items.empty()) {
            preferred = FindStackWithName(state.backpackGroups[static_cast<size_t>(g)].items.front().name);
        }
        SelectAfterLegacyLoad(preferred);
        return;
    }

    if (!state.slots.empty()) {
        for (size_t i = 0; i < state.slots.size() && i < 25; ++i) {
            if (state.slots[i].has_value()) add(state.slots[i]->name, state.slots[i]->durability);
        }
        SelectAfterLegacyLoad(state.usingItem.has_value() ? FindStackWithName(state.usingItem->name) : -1);
        return;
    }

    SelectAfterLegacyLoad(-1);
}

// Item na mão depois de um save antigo: o preferido, senão o isqueiro, senão o 1º.
void Inventory::SelectAfterLegacyLoad(int preferred) {
    int sel = preferred;
    if (sel < 0) sel = FindBestLight(true);
    if (sel < 0 && !stacks.empty()) sel = 0;
    if (sel >= 0) SetSelectedStackIndex(sel);
}