#include "world/SpawnFactory.h"
// Incluímos o StageState completo para ter acesso aos seus métodos e atributos
#include "states/stage/StageState.h" 
// Incluindo os componentes e engines necessários para os spawns
#include "engine/GameObject.h"
#include "engine/SpriteRenderer.h"
#include "gameplay/Box.h"
#include "ui/FadeEffect.h"
#include "gameplay/Repairable.h"
#include "gameplay/StairTrigger.h"
#include "gameplay/ItemPickup.h"
#include <iostream>
#include <cstdlib> 

void SpawnFactory::SpawnEntity(const EntitySpawn& spawn, StageState& stage, const StageFirstLoadData& cfg) {
    
    if (spawn.type == "Caixa") {
        GameObject* boxObj = new GameObject();
        boxObj->z = spawn.z; 
        boxObj->AddComponent(new Box(*boxObj, spawn.isStatic));
        boxObj->box.x = spawn.x;
        boxObj->box.y = spawn.y - (boxObj->box.h);
        stage.AddObject(boxObj);
    }
    else if (spawn.type == "Pilar") {
        GameObject* pilarObj = new GameObject();
        pilarObj->z = spawn.z;

        SpriteRenderer* sprite = new SpriteRenderer(*pilarObj, "Recursos/img/cenario/pilares.png");
        pilarObj->AddComponent(sprite);
        pilarObj->AddComponent(new FadeEffect(*pilarObj));

        pilarObj->box.x = spawn.x;
        pilarObj->box.y = spawn.y - pilarObj->box.h;

        stage.AddObject(pilarObj);
    }
    else if (spawn.type == "Escada_Quebrada") {
        GameObject* ladderObj = new GameObject();
        ladderObj->z = spawn.z;
        ladderObj->isStairs= true;
        ladderObj->AddComponent(new SpriteRenderer(*ladderObj, "Recursos/img/cenario/escada_quebrada.png"));
        
        ladderObj->AddComponent(new FadeEffect(*ladderObj, true));
        ladderObj->AddComponent(new Repairable(*ladderObj,
            "Recursos/img/cenario/escada_inteira.png",
            "Tabua de Madeira",
            "Recursos/audio/Hit0.wav",
            120.0f,
            Vec2(1780, 1050)
        ));

        ladderObj->box.x = spawn.x;
        ladderObj->box.y = spawn.y - ladderObj->box.h;

        stage.AddObject(ladderObj);
    }
    else if (spawn.type == "StairTrigger") {
        GameObject* triggerObj = new GameObject();
        triggerObj->box.x = spawn.x;
        triggerObj->box.y = spawn.y;
        triggerObj->box.w = spawn.w; 
        triggerObj->box.h = spawn.h;

        triggerObj->AddComponent(new StairTrigger(*triggerObj));
        
        stage.AddObject(triggerObj);
    }
    else if (spawn.type == "ItemSpawn") {
        const ItemDef* foundDef = nullptr;
        // Acessando o 'cfg' de dentro da instância do stage passada por referência
        for (const auto& def : cfg.pickupCycle) {
            if (def.name == spawn.customString) {
                foundDef = &def;
                break;
            }   
        }

        if (!foundDef && cfg.startingFlashlight.name == spawn.customString) {
            foundDef = &cfg.startingFlashlight;
        }

        if (foundDef) {
            int spawnDurability = foundDef->maxDurability;
            if (foundDef->HasProperty(ItemProperty::LIGHT_SOURCE)) {
                spawnDurability = 1 + (rand() % 100);
            }
            const float itemSize = 48.0f;

            // Posição base (como se estivesse no chão)
            Vec2 tl(spawn.x - itemSize * 0.5f, spawn.y - itemSize);
            tl = stage.ClampPickupTopLeft(tl, itemSize, itemSize);

            // Cria o Pickup
            ItemPickup* pickup = ItemPickup::Spawn(tl.x, tl.y, *foundDef, spawnDurability, stage.itemPickups);
            
            if (pickup) {
            GameObject& itemObj = pickup->GetAssociated();

            // A MECÂNICA (Passamos o 0, 1 ou 2 para ditar qual animação rodar depois) 
            int itemHeightLevel = spawn.customInt;
            pickup->SetHeightLevel(itemHeightLevel);

            // O VISUAL (Aplicamos o pixel exato para o Y-Sort não bugar)
            itemObj.z = spawn.z; // Fica no mesmo andar
            itemObj.depthOffset = spawn.customFloat; 

            stage.AddObject(&pickup->GetAssociated());
            } 
        }
        else {
            std::cerr << "[ItemSpawn] Item nao encontrado no pickupCycle: '" 
                      << spawn.customString << "'. Verifique a propriedade itemName no Tiled." << std::endl;
        }
    }    
}