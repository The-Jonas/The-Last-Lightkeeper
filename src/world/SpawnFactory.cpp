#include "world/SpawnFactory.h"
// Incluímos o StageState completo para ter acesso aos seus métodos e atributos
#include "states/stage/StageState.h" 
// Incluindo os componentes e engines necessários para os spawns
#include "engine/GameObject.h"
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
#include <iostream>
#include <cstdlib> 

void SpawnFactory::SpawnEntity(const EntitySpawn& spawn, StageState& stage, const StageFirstLoadData& cfg) {
    
    if (spawn.type == "Caixa") {
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
            if (foundDef->HasProperty(ItemProperty::LIGHT_SOURCE)) {
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
        
        // Adiciona o componente (isso já carrega o SpriteRenderer, então o closetObj->box.w e h já terão o tamanho da imagem correta!)
        closetObj->AddComponent(new Closet(*closetObj, dir));

        closetObj->box.x = spawn.x;
        closetObj->box.y = spawn.y - closetObj->box.h;
        
        // --- COLISÃO ESTÁTICA CUSTOMIZADA POR DIREÇÃO ---
        SDL_Rect colBox;
        
        if (dir == "frente") {
            // Armário de Frente: A colisão é larga e fica só na base de madeira
            colBox.w = closetObj->box.w * 0.75f; 
            colBox.h = closetObj->box.h * 0.30f; 
            colBox.x = closetObj->box.x + (closetObj->box.w - colBox.w) / 2.0f; // Centralizado no X
            colBox.y = closetObj->box.y + (closetObj->box.h - colBox.h) - 70;        // Colado embaixo no Y
        } 
        else if (dir == "esquerda") {
            // Armário virado para a Esquerda: A colisão é mais fina (profundidade) e mais alta
            colBox.w = closetObj->box.w * 0.50f; // 50% da largura da imagem
            colBox.h = closetObj->box.h * 0.85f; // Cobre quase a altura toda
            // Empurra a colisão para a parte de trás do armário (direita da imagem)
            colBox.x = closetObj->box.x + (closetObj->box.w - colBox.w); 
            colBox.y = closetObj->box.y + (closetObj->box.h - colBox.h);
        }
        else if (dir == "direita") {
            // Armário virado para a Direita
            colBox.w = closetObj->box.w * 0.50f; 
            colBox.h = closetObj->box.h * 0.85f; 
            // Cola a colisão na parte de trás do armário (esquerda da imagem)
            colBox.x = closetObj->box.x; 
            colBox.y = closetObj->box.y + (closetObj->box.h - colBox.h);
        }

        // Injeta a colisão calculada direto no LevelManager
        stage.level.GetRectColliders().push_back(colBox);

        stage.AddObject(closetObj);
    }
    else if (spawn.type == "CaixasAmontoadas") {
        GameObject* caixasObj = new GameObject();
        caixasObj->tiledId = spawn.tiledId;
        caixasObj->z = spawn.z;

        // Suporte a depthOffset caso precise ajustar a profundidade delas no mapa
        float depthOff = 0.0f;
        if (spawn.properties.count("depthOffset")) depthOff = spawn.properties.at("depthOffset").get<float>();
        caixasObj->depthOffset = depthOff;

        caixasObj->AddComponent(new SpriteRenderer(*caixasObj, "Recursos/img/objetos/Amontoado_caixas.png"));
        
        // Adiciona o FadeEffect para ficar transparente se o jogador for para trás
        // caixasObj->AddComponent(new FadeEffect(*caixasObj));

        caixasObj->box.x = spawn.x;
        caixasObj->box.y = spawn.y - caixasObj->box.h;
        
        // COLISÃO FÍSICA AUTOMÁTICA
        SDL_Rect colBox;
        
        // 1. LARGURA: Reduz a largura da colisão (ex: 96% do tamanho da imagem)
        // para o jogador não esbarrar no ar transparente das laterais.
        colBox.w = caixasObj->box.w * 0.96f; 

        // 2. ALTURA: Define a grossura da parede invisível (35% da imagem)
        colBox.h = caixasObj->box.h * 0.35f; 

        // 3. EIXO X: Centraliza a colisão encolhida no meio da imagem
        colBox.x = caixasObj->box.x + (caixasObj->box.w - colBox.w) / 2.0f;

        // 4. EIXO Y : Move a colisão "mais pra frente" da imagem.
        // O "+ 50" ali no final empurra a parede invisível 50 pixels para cima
        colBox.y = caixasObj->box.y + (caixasObj->box.h - colBox.h) - 50; 

        // Injeta a parede invisível direto no cérebro do LevelManager
        stage.level.GetRectColliders().push_back(colBox);

        stage.AddObject(caixasObj);
    }    
}