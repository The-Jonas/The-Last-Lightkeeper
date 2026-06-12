#include "world/SpawnFactory.h"
// Incluímos o StageState completo para ter acesso aos seus métodos e atributos
#include "states/stage/StageState.h" 
// Incluindo os componentes e engines necessários para os spawns
#include "engine/GameObject.h"
#include "world/Collider.h"
#include "engine/SpriteRenderer.h"
#include "gameplay/Box.h"
#include "gameplay/CandleStick.h"
#include "ui/FadeEffect.h"
#include "gameplay/Repairable.h"
#include "gameplay/StairTrigger.h"
#include "gameplay/LevelTransition.h"
#include "gameplay/ItemPickup.h"
#include "gameplay/Jornal.h"
#include "gameplay/Closet.h"
#include "gameplay/Monster.h"
#include "ui/MonsterSilhouette.h"
#include <iostream>
#include <cstdlib> 

void SpawnFactory::SpawnEntity(const EntitySpawn& spawn, StageState& stage, const StageFirstLoadData& cfg) {
    
    if (spawn.type == "Monstro") {
        GameObject* monsterObj = new GameObject();
        monsterObj->tiledId = spawn.tiledId;
        monsterObj->z = spawn.z;
 
        Monster* monster = new Monster(*monsterObj);
 
        // Lê waypoints da propriedade "waypoints" do objeto no Tiled
        // Formato esperado: "x1,y1;x2,y2;x3,y3" (pontos separados por ;)
        if (spawn.properties.count("waypoints")) {
            std::string raw = spawn.properties.at("waypoints").get<std::string>();
            // Parser simples de "x,y;x,y;..."
            std::stringstream ss(raw);
            std::string token;
            while (std::getline(ss, token, ';')) {
                float wx = 0.0f, wy = 0.0f;
                if (sscanf(token.c_str(), "%f,%f", &wx, &wy) == 2)
                    monster->AddPatrolPoint(Vec2(wx, wy));
            }
        }
 
        // Se não tiver waypoints definidos, usa a própria posição como âncora  
        if (spawn.properties.count("waypoints") == 0) {
            monster->AddPatrolPoint(Vec2(spawn.x, spawn.y));
        }
 
        monsterObj->AddComponent(monster);
        monsterObj->AddComponent(new Collider(*monsterObj, Vec2(0.3f, 0.15f), Vec2(60.0f, monsterObj->box.h * 0.4f)));
 
        monsterObj->box.x = spawn.x;
        monsterObj->box.y = spawn.y;
 
        stage.AddObject(monsterObj);

        // ── Objeto filho só para a silhueta (z=100, renderiza ACIMA da escuridão) ──
        GameObject* silhouetteObj = new GameObject();
        silhouetteObj->z = 100;
        silhouetteObj->owner = monsterObj;   
        silhouetteObj->AddComponent(new MonsterSilhouette(*silhouetteObj, monster));
        silhouetteObj->box = monsterObj->box;
        stage.AddObject(silhouetteObj);
    }
    else if (spawn.type == "Caixa") {
        GameObject* boxObj = new GameObject();
        boxObj->tiledId = spawn.tiledId;
        boxObj->z = spawn.z; 
        boxObj->AddComponent(new Box(*boxObj, spawn.isStatic));
        boxObj->box.x = spawn.x;
        boxObj->box.y = spawn.y - (boxObj->box.h);
        stage.AddObject(boxObj);
    }
    else if (spawn.type == "Pilar") {
        GameObject* pilarObj = new GameObject();
        pilarObj->tiledId = spawn.tiledId;
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
        ladderObj->tiledId = spawn.tiledId;
        ladderObj->z = spawn.z;
        ladderObj->isStairs= true;
        ladderObj->AddComponent(new SpriteRenderer(*ladderObj, "Recursos/img/cenario/escada_quebrada.png"));
        
        ladderObj->AddComponent(new FadeEffect(*ladderObj, true));
        ladderObj->AddComponent(new Repairable(*ladderObj,
            "Recursos/img/cenario/escada_tabua.png",
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
        triggerObj->tiledId = spawn.tiledId;

        // Define um valor padrão de segurança caso se esqueça de colocar no Tiled
        float myAnchor = 886.18f;

        // Lê o que tá no Tiled
        if (spawn.properties.count("anchorY")) {
            myAnchor = spawn.properties.at("anchorY").get<float>();
        }

        triggerObj->AddComponent(new StairTrigger(*triggerObj, myAnchor));

        triggerObj->box.x = spawn.x;
        triggerObj->box.y = spawn.y;
        triggerObj->box.w = spawn.w; 
        triggerObj->box.h = spawn.h;
        
        stage.AddObject(triggerObj);
    }
    else if (spawn.type == "LevelTransition") {
        int targetLevel = 1;
        if (spawn.properties.count("targetLevelIndex")) {
            targetLevel = spawn.properties.at("targetLevelIndex").get<int>();
        }

        GameObject* transitionObj = new GameObject();
        transitionObj->tiledId = spawn.tiledId;
        transitionObj->AddComponent(new LevelTransition(*transitionObj, targetLevel));
        transitionObj->box.x = spawn.x;
        transitionObj->box.y = spawn.y;
        transitionObj->box.w = spawn.w;
        transitionObj->box.h = spawn.h;

        stage.AddObject(transitionObj);
    }
    else if (spawn.type == "ItemSpawn") {
        if (stage.ShouldSkipPickupSpawn(spawn.tiledId)) {
            return;
        }

        std:: string itemName = "";
        if (spawn.properties.count("itemName")) itemName = spawn.properties.at("itemName").get<std::string>();

        int itemHeightLevel = 0;
        if (spawn.properties.count("heightLevel")) itemHeightLevel = spawn.properties.at("heightLevel").get<int>();

        float itemDepthOffset = 0.0f;
        if (spawn.properties.count("depthOffset")) itemDepthOffset = spawn.properties.at("depthOffset").get<float>();

        const ItemDef* foundDef = nullptr;
        // Acessando o 'cfg' de dentro da instância do stage passada por referência
        for (const auto& def : cfg.pickupCycle) {
            if (def.name == itemName) {
                foundDef = &def;
                break;
            }   
        }

        if (!foundDef && cfg.startingFlashlight.name == itemName) {
            foundDef = &cfg.startingFlashlight;
        }

        if (foundDef) {
            int spawnDurability = foundDef->maxDurability;
            if (foundDef->name == "Lamp") {
                spawnDurability = 0;
            } else if (foundDef->HasProperty(ItemProperty::LIGHT_SOURCE)) {
                spawnDurability = 1 + (rand() % 100);
            }
            const float itemSize = 48.0f;

            // Posição base (como se estivesse no chão)
            Vec2 tl(spawn.x, spawn.y - itemSize);;
            tl = stage.ClampPickupTopLeft(tl, itemSize, itemSize);

            // Cria o Pickup
            ItemPickup* pickup = ItemPickup::Spawn(tl.x, tl.y, *foundDef, spawnDurability, stage.itemPickups);
            
            if (pickup) {
            GameObject& itemObj = pickup->GetAssociated();
            itemObj.tiledId = spawn.tiledId;

            // A MECÂNICA (Passamos o 0, 1 ou 2 para ditar qual o comportamento da altura do item) 
            pickup->SetHeightLevel(itemHeightLevel);

            // O VISUAL (Aplicamos o pixel exato para o Y-Sort não bugar)
            itemObj.z = spawn.z; // Fica no mesmo andar
            itemObj.depthOffset = itemDepthOffset; 

            stage.AddObject(&pickup->GetAssociated());
            } 
        }
        else {
            std::cerr << "[ItemSpawn] Item nao encontrado no pickupCycle: '" 
                      << itemName << "'. Verifique a propriedade itemName no Tiled." << std::endl;
        }
    }
    else if (spawn.type == "JornalSpawn") {
        std::string imagePath = "Recursos/img/items/old_letter.jpg";
        if (spawn.properties.count("imagePath")) {
            imagePath = spawn.properties.at("imagePath").get<std::string>();
        }

        int heightLevel = 0;
        if (spawn.properties.count("heightLevel")) {
            heightLevel = spawn.properties.at("heightLevel").get<int>();
        }

        float depthOffset = 0.0f;
        if (spawn.properties.count("depthOffset")) {
            depthOffset = spawn.properties.at("depthOffset").get<float>();
        }

        const float thumbMax = 48.0f;
        Vec2 tl(spawn.x, spawn.y - thumbMax);
        tl = stage.ClampPickupTopLeft(tl, thumbMax, thumbMax);

        Jornal* jornal = Jornal::Spawn(tl.x, tl.y, imagePath, heightLevel, stage.jornals);
        if (jornal) {
            GameObject& jornalObj = jornal->GetAssociated();
            jornalObj.tiledId = spawn.tiledId;
            jornalObj.z = spawn.z;
            jornalObj.depthOffset = depthOffset;
            stage.AddObject(&jornalObj);
        }
    }
    else if (spawn.type == "Castical") {
        std::string dir = "frente";
        if (spawn.properties.count("direction")) dir = spawn.properties.at("direction").get<std::string>();
        
        bool startsLit = false;
        if (spawn.properties.count("startsLit")) startsLit = spawn.properties.at("startsLit").get<bool>();

        float depthOff = 0.0f;
        if (spawn.properties.count("depthOffset")) depthOff = spawn.properties.at("depthOffset").get<float>();

        GameObject* candleObj = new GameObject();
        candleObj->tiledId = spawn.tiledId;
        candleObj->z = spawn.z;
        candleObj->depthOffset = depthOff;
        candleObj->AddComponent(new Candlestick(*candleObj, startsLit, dir));
        
        // ADICIONADO O FADE EFFECT
        candleObj->AddComponent(new FadeEffect(*candleObj));

        candleObj->box.x = spawn.x;
        candleObj->box.y = spawn.y - candleObj->box.h;
        stage.AddObject(candleObj);
    }
    else if (spawn.type == "Armario") {
        std::string dir = "frente";
        if (spawn.properties.count("direction")) dir = spawn.properties.at("direction").get<std::string>();
 
        float depthOff = 0.0f;
        if (spawn.properties.count("depthOffset")) depthOff = spawn.properties.at("depthOffset").get<float>();
 
        GameObject* closetObj = new GameObject();
        closetObj->tiledId = spawn.tiledId;
        closetObj->z = spawn.z;
        closetObj->depthOffset = depthOff;
 
        closetObj->AddComponent(new Closet(*closetObj, dir));
 
        closetObj->box.x = spawn.x;
        closetObj->box.y = spawn.y - closetObj->box.h;
 
        // Colisão daqui vem da camada "Collision_Obj" desenhada no Tiled. 
 
        stage.AddObject(closetObj);
    }
    else if (spawn.type == "CaixasAmontoadas") {
        float depthOff = 0.0f;
        if (spawn.properties.count("depthOffset")) depthOff = spawn.properties.at("depthOffset").get<float>();
 
        GameObject* caixasObj = new GameObject();
        caixasObj->tiledId = spawn.tiledId;
        caixasObj->z = spawn.z;
        caixasObj->depthOffset = depthOff;
 
        caixasObj->AddComponent(new SpriteRenderer(*caixasObj, "Recursos/img/objetos/Amontoado_caixas.png"));
 
        caixasObj->box.x = spawn.x;
        caixasObj->box.y = spawn.y - caixasObj->box.h;
 
        // Colisão agora vem da camada "Collision_Obj" desenhada no Tiled.
        // Não há mais injeção manual de SDL_Rect aqui.
 
        stage.AddObject(caixasObj);
    }
    else if (spawn.type == "Mesa") {
        float depthOff = 0.0f;
        if (spawn.properties.count("depthOffset")) depthOff = spawn.properties.at("depthOffset").get<float>();

        GameObject* tableObj = new GameObject();
        tableObj->tiledId = spawn.tiledId;
        tableObj->z = spawn.z;
        tableObj->depthOffset = depthOff;

        tableObj->AddComponent(new SpriteRenderer(*tableObj, "Recursos/img/objetos/Mesa.png"));
        tableObj->box.x = spawn.x;
        tableObj->box.y = spawn.y - tableObj->box.h;

        // Colisão feita no tiled (objeto na diagonal)

        stage.AddObject(tableObj);
    }
    else if (spawn.type == "Cadeira_Caida") {
        float depthOff = 0.0f;
        if (spawn.properties.count("depthOffset")) depthOff = spawn.properties.at("depthOffset").get<float>();

        GameObject* fallenChairObj = new GameObject();
        fallenChairObj->tiledId = spawn.tiledId;
        fallenChairObj->z = spawn.z;
        fallenChairObj->depthOffset = depthOff;

        fallenChairObj->AddComponent(new SpriteRenderer(*fallenChairObj, "Recursos/img/objetos/Cadeira_caida.png"));
        fallenChairObj->box.x = spawn.x;
        fallenChairObj->box.y = spawn.y - fallenChairObj->box.h;

        // Colisão feita no tiled (objeto na diagonal)

        stage.AddObject(fallenChairObj);
    }
}