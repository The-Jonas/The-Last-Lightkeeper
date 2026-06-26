#ifndef INVENTORY_GRID_H
#define INVENTORY_GRID_H

#define INCLUDE_SDL
#include "SDL_include.h"
#include "engine/Component.h"
#include "gameplay/Inventory.h"

#include <functional>
#include <vector>

class Character;
class GameObject;
class ItemPickup;

class InventoryGrid : public Component {
public:
    InventoryGrid(GameObject& associated, Inventory& inventory,
                  Character* bigChar = nullptr,
                  std::vector<ItemPickup*>* itemPickups = nullptr,
                  std::function<void(GameObject*)> addObjFn = {});

    void Start() override;
    void Update(float dt) override;
    void Render() override;

    bool IsOpen() const { return isOpen; }
    void Toggle();
    void Open();
    void Close();

private:
    Inventory& inventory;
    Character* bigCharacter = nullptr;
    std::vector<ItemPickup*>* itemPickups = nullptr;
    std::function<void(GameObject*)> addObjectToState;
    bool isOpen = false;
    int dragSourceSlot = -1;
    bool isDragging = false;

    void HandleMouseDown();
    void HandleMouseUp();
    void CancelDrag();
    void PerformDrop(int targetSlot);

    void DrawBackdrop(SDL_Renderer* renderer);
    void DrawSlot(SDL_Renderer* renderer, int slotIndex, float x, float y, bool dimmed);
    void DrawDraggedItem(SDL_Renderer* renderer);
    int GetSlotAtPosition(int mouseX, int mouseY) const;

    float GetGridLeft() const;
    float GetGridTop() const;

    static constexpr float kSlotSize = 70.0f;
    static constexpr float kSlotGap = 6.0f;
    static constexpr float kGridBorder = 4.0f;
    static constexpr float kTitleHeight = 42.0f;
    static constexpr float kIconSize = 48.0f;
    static constexpr float kGaugeHeight = 4.0f;

    static constexpr int kGridCols = Inventory::kGridWidth;
    static constexpr int kGridRows = Inventory::kGridHeight;
};

#endif
