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
#include "gameplay/Window.h"
#include "gameplay/RadioAsset.h"
#include "gameplay/Curtain.h"
#include "gameplay/CurtainTrigger.h"
#include "gameplay/Monster.h"
#include "ui/MonsterSilhouette.h"
#include "gameplay/DialogueTrigger.h"
#include "ui/DialogueBox.h"
#include <iostream>
#include <cstdlib> 

namespace {
// Propaga o flip do tile (decodificado do gid no Tiled) para o objeto. Inócuo em
// objetos sem sprite (só define flags que o SpriteRenderer usa se houver sprite).
void ApplyTiledFlip(GameObject* obj, const EntitySpawn& spawn) {
    if (obj) {
        obj->flipH = spawn.flipH;
        obj->flipV = spawn.flipV;
    }
}

// Ancora e dimensiona o sprite conforme o objeto no Tiled: estica para a
// largura/altura do objeto (objetos-tile ancoram no canto inferior-esquerdo) e
// aplica a rotação. Assim o jogo reproduz o que aparece no Tiled.
void ApplyTiledBox(GameObject* obj, const EntitySpawn& spawn) {
    if (!obj) return;
    if (spawn.w > 0.0f) obj->box.w = spawn.w;
    if (spawn.h > 0.0f) obj->box.h = spawn.h;
    obj->box.x = spawn.x;
    obj->box.y = spawn.y - obj->box.h;
    obj->angleDeg = spawn.rotation;
    obj->rotateAroundBottomLeft = true;   // objetos-tile do Tiled giram pelo rodapé-esquerdo

    // ── Ancora de profundidade, vinda do Tiled ──────────────────────────────
    // A ordem de desenho e pela BASE da caixa (box.y + box.h). Isso funciona
    // para um barril, cuja base e onde ele toca o chao — e mente para arte
    // ALTA como uma escada inteira, cuja base fica muitos pixeis abaixo do
    // sitio onde as coisas se encostam a ela. Dai os barris que "atravessam" a
    // escada do 2o andar: a escada ganha a comparacao e passa-lhes a frente.
    //
    // `depthOffset` corrige isso por objecto, sem recompilar: um valor
    // NEGATIVO puxa a ancora para cima (a escada passa a desenhar-se ATRAS de
    // quem esta mais abaixo no ecra), um positivo empurra-a para baixo. Antes
    // so alguns tipos liam a propriedade; agora TODOS os que passam por aqui
    // leem, que sao praticamente todos os objectos do mapa.
    if (spawn.properties.count("depthOffset")) {
        obj->depthOffset = spawn.properties.at("depthOffset").get<float>();
    }
}

std::vector<DialogueBox::Line> ParseDialogueLinesProperty(const EntitySpawn& spawn, const char* key) {
    std::vector<DialogueBox::Line> lines;

    auto parseSpeaker = [](const std::string& s) {
        return (s == "Martin") ? DialogueBox::Speaker::BigBrother
             : (s == "Luke")   ? DialogueBox::Speaker::LittleBrother
                                : DialogueBox::Speaker::None;
    };
    auto parseEmotion = [](const std::string& s) {
        return (s == "Fear")  ? DialogueBox::Emotion::Fear
             : (s == "Doubt") ? DialogueBox::Emotion::Doubt
                               : DialogueBox::Emotion::Normal;
    };

    // Formato 1: propriedade "dialogue" com um JSON de várias falas.
    if (spawn.properties.count(key)) {
        try {
            json arr = json::parse(spawn.properties.at(key).get<std::string>());
            for (const auto& item : arr) {
                DialogueBox::Line line;
                line.speaker         = parseSpeaker(item.value("speaker", "Martin"));
                line.listener        = parseSpeaker(item.value("listener", ""));
                line.emotion         = parseEmotion(item.value("emotion", "Normal"));
                line.listenerEmotion = parseEmotion(item.value("listener_emotion", "Normal"));
                line.text            = item.value("text", "");
                lines.push_back(line);
            }
            return lines;
        } catch (const std::exception& ex) {
            std::cerr << "Dialogo invalido em '" << key << "' (tiledId "
                      << spawn.tiledId << "): " << ex.what() << std::endl;
            return lines;
        }
    }

    // Formato 2 : campos soltos direto no objeto —
    // só dá pra descrever UMA fala assim, mas é bem mais rápido de preencher.
    if (spawn.properties.count("speaker") && spawn.properties.count("text")) {
        DialogueBox::Line line;
        line.speaker  = parseSpeaker(spawn.properties.at("speaker").get<std::string>());
        line.listener = spawn.properties.count("listener")
                       ? parseSpeaker(spawn.properties.at("listener").get<std::string>())
                       : DialogueBox::Speaker::None;
        line.emotion  = spawn.properties.count("emotion")
                       ? parseEmotion(spawn.properties.at("emotion").get<std::string>())
                       : DialogueBox::Emotion::Normal;
        line.listenerEmotion = spawn.properties.count("listener_emotion")
                       ? parseEmotion(spawn.properties.at("listener_emotion").get<std::string>())
                       : DialogueBox::Emotion::Normal;
        line.text     = spawn.properties.at("text").get<std::string>();
        lines.push_back(line);
    }

    return lines;
}

} // namespace

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
 
        ApplyTiledFlip(monsterObj, spawn);
 
        stage.AddObject(monsterObj);

        // ── Objeto filho só para a silhueta (z=100, renderiza ACIMA da escuridão) ──
        GameObject* silhouetteObj = new GameObject();
        silhouetteObj->z = 100;
        silhouetteObj->owner = monsterObj;   
        silhouetteObj->AddComponent(new MonsterSilhouette(*silhouetteObj, monster));
        silhouetteObj->box = monsterObj->box;
        ApplyTiledFlip(silhouetteObj, spawn);
        stage.AddObject(silhouetteObj);
    }
    else if (spawn.type == "Caixa") {
        GameObject* boxObj = new GameObject();
        boxObj->tiledId = spawn.tiledId;
        boxObj->z = spawn.z;
        if (spawn.properties.count("depthOffset")) {
            boxObj->depthOffset = spawn.properties.at("depthOffset").get<float>();
        }
        boxObj->AddComponent(new Box(*boxObj, spawn.isStatic));
        ApplyTiledBox(boxObj, spawn);
        ApplyTiledFlip(boxObj, spawn);
        stage.AddObject(boxObj);
        stage.RegisterTestShadowObject(boxObj);
    }
    else if (spawn.type == "Pilar") {
        std::string pillarName = "pilares";
        if (spawn.properties.count("pillarName"))
            pillarName = spawn.properties.at("pillarName").get<std::string>();

        GameObject* pilarObj = new GameObject();
        pilarObj->tiledId = spawn.tiledId;
        pilarObj->z = spawn.z;

        std::string path = "Recursos/img/cenario/" + pillarName + ".png";
        SpriteRenderer* sprite = new SpriteRenderer(*pilarObj, path);
        pilarObj->AddComponent(sprite);
        pilarObj->AddComponent(new FadeEffect(*pilarObj));

        ApplyTiledBox(pilarObj, spawn);
        ApplyTiledFlip(pilarObj, spawn);

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
            "Recursos/audio/Hit0.wav"
        ));

        ApplyTiledBox(ladderObj, spawn);

        ApplyTiledFlip(ladderObj, spawn);

        stage.AddObject(ladderObj);
    }
    else if (spawn.type == "Escada") {
        GameObject* ladderObj = new GameObject();
        ladderObj->tiledId = spawn.tiledId;
        ladderObj->z = spawn.z;
        ladderObj->isStairs = true;

        ladderObj->AddComponent(new SpriteRenderer(*ladderObj, "Recursos/img/cenario/escada_inteira.png"));
        ladderObj->AddComponent(new FadeEffect(*ladderObj, true));
        
        ApplyTiledBox(ladderObj, spawn);

        ApplyTiledFlip(ladderObj, spawn);

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
        
        ApplyTiledFlip(triggerObj, spawn);
        
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

        ApplyTiledFlip(transitionObj, spawn);

        stage.AddObject(transitionObj);
    }
    else if (spawn.type == "ItemSpawn") {
        if (stage.ShouldSkipPickupSpawn(spawn.tiledId)) {
            return;
        }

        std:: string itemName = "";
        if (spawn.properties.count("itemName")) itemName = spawn.properties.at("itemName").get<std::string>();
        if (itemName == "Lamp Fuel" || itemName == "Lighter Fuel" || itemName == "Light Fuel" ||
            itemName == "Oil Gallon") {
            itemName = "Fuel";
        }

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



            // Cria o Pickup
            ItemPickup* pickup = ItemPickup::Spawn(spawn.x, spawn.y, *foundDef, spawnDurability, stage.itemPickups);
            
            if (pickup) {
            GameObject& itemObj = pickup->GetAssociated();
            itemObj.tiledId = spawn.tiledId;

            // #17: renderiza no CHÃO exatamente a arte que o Tiled mostra para este
            // gid (a imagem do tileset), em vez do ícone de inventário (def.spritePath).
            // Assim o item no mundo fica WYSIWYG com o Tiled. O ícone da mochila
            // continua vindo do def (inalterado).
            if (const std::string* tileImg = stage.level.GetTileImagePath(spawn.gid)) {
                if (SpriteRenderer* sr = itemObj.GetComponent<SpriteRenderer>()) {
                    sr->Open(*tileImg);
                }
            }

            // EXCEÇÃO da regra (pedido do design): itens "Fuel" SEMPRE desenham o
            // galão lighter_fuel.png no chão, independentemente do tile do Tiled.
            if (itemName == "Fuel") {
                if (SpriteRenderer* sr = itemObj.GetComponent<SpriteRenderer>()) {
                    sr->Open("Recursos/img/items/lighter_fuel.png");
                }
            }

            // #17 FIX: o ItemSpawn era o ÚNICO prop que NÃO chamava ApplyTiledBox —
            // usava o tamanho NATIVO do sprite e ignorava w/h e rotação do Tiled, então
            // óleo e tábua saíam com tamanho/posição/ângulo errados. Agora segue o objeto
            // do Tiled (largura, altura, rotação, pivô inferior-esquerdo) como os demais.
            ApplyTiledBox(&itemObj, spawn);

            // A MECÂNICA (Passamos o 0, 1 ou 2 para ditar qual o comportamento da altura do item)
            pickup->SetHeightLevel(itemHeightLevel);

            // O VISUAL (Aplicamos o pixel exato para o Y-Sort não bugar)
            itemObj.z = spawn.z; // Fica no mesmo andar
            itemObj.depthOffset = itemDepthOffset; 

            ApplyTiledFlip(&itemObj, spawn);

            stage.AddObject(&itemObj);
            } 
        }
        else {
            std::cerr << "[ItemSpawn] Item nao encontrado no pickupCycle: '" 
                      << itemName << "'. Verifique a propriedade itemName no Tiled." << std::endl;
        }
    }
    else if (spawn.type == "JornalSpawn") {
    
        // ── SPRITE DECORATIVO (asset físico no chão) ──────────────────────────
        std::string spriteName = "old_letter";
        if (spawn.properties.count("spriteName"))
            spriteName = spawn.properties.at("spriteName").get<std::string>();
        std::string spritePath = "Recursos/img/items/jornais/" + spriteName + ".png";
    
        // ── DOCUMENTO (imagem que abre ao interagir) ───────────────────────────
        std::string imageName = "old_letter";
        if (spawn.properties.count("imageName"))
            imageName = spawn.properties.at("imageName").get<std::string>();
        std::string imagePath = "Recursos/img/documentos/" + imageName + ".png";

        // ── SOM OPCIONAL ao interagir ──────────────────────────────────────────
        std::string soundPath = "";
        if (spawn.properties.count("soundName")) {
            std::string soundName = spawn.properties.at("soundName").get<std::string>();
            soundPath = "Recursos/audio/SFX/INTERACTABLES/" + soundName + ".mp3";
        }
    
        // ── ZOOM ao abrir ──────────────────────────────────────────────────────
        float zoomFactor = 1.0f;
        if (spawn.properties.count("zoomFactor"))
            zoomFactor = spawn.properties.at("zoomFactor").get<float>();

        int heightLevel = 0;
        if (spawn.properties.count("heightLevel"))
            heightLevel = spawn.properties.at("heightLevel").get<int>();

        float depthOffset = 0.0f;
        if (spawn.properties.count("depthOffset"))
            depthOffset = spawn.properties.at("depthOffset").get<float>();

        Jornal* jornal = Jornal::Spawn(spawn.x, spawn.y, spritePath, imagePath,
                                       heightLevel, stage.jornals,
                                       soundPath, zoomFactor);
        if (jornal) {
            GameObject& jornalObj = jornal->GetAssociated();
            jornalObj.tiledId     = spawn.tiledId;
            jornalObj.z           = spawn.z;
            jornalObj.depthOffset = depthOffset;
            ApplyTiledBox(&jornalObj, spawn);
            ApplyTiledFlip(&jornalObj, spawn);
            stage.AddObject(&jornalObj);

            // Zoom com F no visualizador (só documentos de texto; marque no Tiled).
            if (spawn.properties.count("zoomable")) {
                jornal->SetZoomable(spawn.properties.at("zoomable").get<bool>());
            }

            // Documento colecionável: ao interagir, some do cenário e vai para a pasta (Tab).
            if (spawn.properties.count("collectible") &&
                spawn.properties.at("collectible").get<bool>()) {
                std::string title = spawn.properties.count("doc_title")
                                  ? spawn.properties.at("doc_title").get<std::string>()
                                  : imageName;   
                int order = spawn.properties.count("doc_order")
                          ? spawn.properties.at("doc_order").get<int>()
                          : 1000 + spawn.tiledId;
                jornal->SetCollectible(true, title, order);
            }

            // Diálogo específico deste papel (opcional) — mesmo formato do DialogueTrigger.
            std::vector<DialogueBox::Line> lines = ParseDialogueLinesProperty(spawn, "dialogue");
            if (!lines.empty()) {
                bool once = spawn.properties.count("dialogue_once")
                          ? spawn.properties.at("dialogue_once").get<bool>() : true;
                jornal->SetDialogueLines(lines, once);
            }
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
        
        candleObj->AddComponent(new FadeEffect(*candleObj));

        ApplyTiledBox(candleObj, spawn);
        ApplyTiledFlip(candleObj, spawn);
        stage.AddObject(candleObj);
    }
    else if (spawn.type == "Armario") {
        float depthOff = 0.0f;
        if (spawn.properties.count("depthOffset"))
            depthOff = spawn.properties.at("depthOffset").get<float>();

        GameObject* closetObj = new GameObject();
        closetObj->tiledId     = spawn.tiledId;
        closetObj->z           = spawn.z;
        closetObj->depthOffset = depthOff;

        closetObj->AddComponent(new Closet(*closetObj)); 
        ApplyTiledBox(closetObj, spawn);
        ApplyTiledFlip(closetObj, spawn);

        stage.AddObject(closetObj);
        stage.RegisterTestShadowObject(closetObj);
    }
    else if (spawn.type == "Recipiente_Decoracao") {
        // Para as caixas amontoadas e baú 
        float depthOff = 0.0f;
        std::string name = "Amontoado_caixas";
        if (spawn.properties.count("depthOffset")) depthOff = spawn.properties.at("depthOffset").get<float>();
        if (spawn.properties.count("nameObj")) name = spawn.properties.at("nameObj").get<std::string>();
 
        GameObject* caixasObj = new GameObject();
        caixasObj->tiledId = spawn.tiledId;
        caixasObj->z = spawn.z;
        caixasObj->depthOffset = depthOff;

        std::string caminho = "Recursos/img/objetos/mobiliario/" + name + ".png";
        caixasObj->AddComponent(new SpriteRenderer(*caixasObj, caminho));
        ApplyTiledBox(caixasObj, spawn);
 
        // Colisão agora vem da camada "Collision_Obj" desenhada no Tiled.
        // Não há mais injeção manual de SDL_Rect aqui.
 
        ApplyTiledFlip(caixasObj, spawn);
 
        stage.AddObject(caixasObj);
        if (name != "Sangue"){ 
            stage.RegisterTestShadowObject(caixasObj);
        }
    }
    else if (spawn.type == "Mesa") {
        float depthOff = 0.0f;
        std::string variation = "normal";
        if (spawn.properties.count("depthOffset")) depthOff = spawn.properties.at("depthOffset").get<float>();
        if (spawn.properties.count("variation")) variation = spawn.properties.at("variation").get<std::string>();
 
        GameObject* tableObj = new GameObject();
        tableObj->tiledId = spawn.tiledId;
        tableObj->z = spawn.z;
        tableObj->depthOffset = depthOff;

        std::string caminho = "Recursos/img/objetos/mesa/Mesa_" + variation + ".png";
        tableObj->AddComponent(new SpriteRenderer(*tableObj, caminho));
        ApplyTiledBox(tableObj, spawn);

        // Colisão feita no tiled (objeto na diagonal)

        ApplyTiledFlip(tableObj, spawn);

        stage.AddObject(tableObj);
        stage.RegisterTestShadowObject(tableObj);
    }
    else if (spawn.type == "Cadeira_Caida") {
        float depthOff = 0.0f;
        if (spawn.properties.count("depthOffset")) depthOff = spawn.properties.at("depthOffset").get<float>();

        GameObject* fallenChairObj = new GameObject();
        fallenChairObj->tiledId = spawn.tiledId;
        fallenChairObj->z = spawn.z;
        fallenChairObj->depthOffset = depthOff;

        fallenChairObj->AddComponent(new SpriteRenderer(*fallenChairObj, "Recursos/img/objetos/Cadeira_caida.png"));
        ApplyTiledBox(fallenChairObj, spawn);

        // Colisão feita no tiled (objeto na diagonal)

        ApplyTiledFlip(fallenChairObj, spawn);

        stage.AddObject(fallenChairObj);
        stage.RegisterTestShadowObject(fallenChairObj);
    }
        else if (spawn.type == "Mesa_radio") {
        float depthOff = 0.0f;
        if (spawn.properties.count("depthOffset"))
            depthOff = spawn.properties.at("depthOffset").get<float>();
 
        // Duração customizável pelo Tiled (padrão 8s)
        float duration = 8.0f;
        if (spawn.properties.count("playDuration"))
            duration = spawn.properties.at("playDuration").get<float>();
 
        // Som customizável pelo Tiled (padrão: ruído de rádio)
        std::string soundPath = "Recursos/audio/SFX/RADIO/radio_ruido.mp3";
        if (spawn.properties.count("soundPath"))
            soundPath = spawn.properties.at("soundPath").get<std::string>();
 
        GameObject* radioObj = new GameObject();
        radioObj->tiledId    = spawn.tiledId;
        radioObj->z          = spawn.z;
        radioObj->depthOffset = depthOff;
 
        radioObj->AddComponent(new SpriteRenderer(*radioObj, "Recursos/img/objetos/mesa/Mesa_radio.png"));
        radioObj->AddComponent(new FadeEffect(*radioObj));
 
        RadioAsset* radio = new RadioAsset(*radioObj, soundPath);
        radio->playDuration = duration;  
        radioObj->AddComponent(radio);
 
        ApplyTiledBox(radioObj, spawn);
        ApplyTiledFlip(radioObj, spawn);
 
        stage.AddObject(radioObj);
        stage.RegisterTestShadowObject(radioObj);
    }
    else if (spawn.type == "Barril") {
        float depthOff = 0.0f;
        int typeB = 0;
        bool interactive = false;    
        float empty = 0.0f;         

        if (spawn.properties.count("depthOffset")) depthOff = spawn.properties.at("depthOffset").get<float>();
        if (spawn.properties.count("type"))        typeB    = spawn.properties.at("type").get<int>();
        if (spawn.properties.count("empty"))       empty    = spawn.properties.at("empty").get<float>();
        if (spawn.properties.count("interactive")) interactive = spawn.properties.at("interactive").get<bool>();

        GameObject* barrelObj = new GameObject();
        barrelObj->tiledId    = spawn.tiledId;
        barrelObj->z          = spawn.z;
        barrelObj->depthOffset = depthOff;

        std::string caminho = "Recursos/img/objetos/barril/barril_" + std::to_string(typeB) + ".png";

        if (interactive) {
            // #10 Barris mais pesados: reduz o multiplicador de empurrão em relacao ao valor
            // "empty" do Tiled (empurrar fica mais lento). O piso de 0.1 vem de SetSpeedMultiplier.
            constexpr float kBarrelWeightScale = 0.55f;
            barrelObj->AddComponent(new Box(*barrelObj, false, caminho, empty * kBarrelWeightScale));
        } else {
            barrelObj->AddComponent(new SpriteRenderer(*barrelObj, caminho));
        }

        ApplyTiledBox(barrelObj, spawn);
        ApplyTiledFlip(barrelObj, spawn);

        stage.AddObject(barrelObj);
        stage.RegisterTestShadowObject(barrelObj);
    }
    else if (spawn.type == "Janela") {
        float depthOff = -500.0f;
        bool startsOpen = false;
        float windRadius = 300.0f;
        std::string windowType = "1"; 
        
        bool interactive = false; 
        if (spawn.properties.count("depthOffset")) depthOff = spawn.properties.at("depthOffset").get<float>();
        if (spawn.properties.count("startsOpen")) startsOpen = spawn.properties.at("startsOpen").get<bool>();
        if (spawn.properties.count("windRadius")) windRadius = spawn.properties.at("windRadius").get<float>();
        if (spawn.properties.count("type")) windowType = spawn.properties.at("type").get<std::string>();
        if (spawn.properties.count("interactive")) interactive = spawn.properties.at("interactive").get<bool>();

        GameObject* winObj = new GameObject();
        winObj->tiledId = spawn.tiledId;
        winObj->z = spawn.z;
        winObj->depthOffset = depthOff;

        if (interactive) {
            // Janela completa do puzzle! Ganha lógica, estados e vento.
            winObj->AddComponent(new Window(*winObj, windowType, startsOpen, windRadius));
        } else {
            // Janela de enfeite! Só carrega o PNG dela fechada direto da pasta.
            std::string caminho = "Recursos/img/cenario/janelas/janela_" + windowType + "_fechada.png";
            winObj->AddComponent(new SpriteRenderer(*winObj, caminho));
        }

        ApplyTiledBox(winObj, spawn); 

        ApplyTiledFlip(winObj, spawn);

        stage.AddObject(winObj);
    }
    else if (spawn.type == "Pesca_Asset") {
        float depthOff = 0.0f;
        std::string object = "boia";

        if (spawn.properties.count("depthOffset")) depthOff = spawn.properties.at("depthOffset").get<float>();
        if (spawn.properties.count("nameObject")) object = spawn.properties.at("nameObject").get<std::string>();

        GameObject* fishingObj = new GameObject();
        fishingObj->tiledId = spawn.tiledId;
        fishingObj->z = spawn.z;
        fishingObj->depthOffset = depthOff;

        std::string caminho = "Recursos/img/objetos/pesca/" + object + ".png";
        fishingObj->AddComponent(new SpriteRenderer(*fishingObj, caminho));
        if (object == "rede_pendurada") {
            fishingObj->AddComponent(new FadeEffect(*fishingObj));
        }
        ApplyTiledBox(fishingObj, spawn);
        ApplyTiledFlip(fishingObj, spawn);

        stage.AddObject(fishingObj);
        stage.RegisterTestShadowObject(fishingObj);
    }
    else if (spawn.type == "Cortina") {
        int   curtainId   = 0;
        bool  startsOpen  = false;
        float animDuration = 2.0f;
        std::string spriteName = "Lona";
    
        if (spawn.properties.count("curtainId"))    curtainId    = spawn.properties.at("curtainId").get<int>();
        if (spawn.properties.count("startsOpen"))   startsOpen   = spawn.properties.at("startsOpen").get<bool>();
        if (spawn.properties.count("animDuration")) animDuration = spawn.properties.at("animDuration").get<float>();
        if (spawn.properties.count("spriteName"))   spriteName   = spawn.properties.at("spriteName").get<std::string>();
    
        std::string path = "Recursos/img/cenario/" + spriteName + ".png";
    
        GameObject* obj = new GameObject();
        obj->tiledId    = spawn.tiledId;
        obj->z          = spawn.z;
        obj->AddComponent(new SpriteRenderer(*obj, path));
        obj->AddComponent(new Curtain(*obj, curtainId, startsOpen, animDuration));
        ApplyTiledBox(obj, spawn);
        ApplyTiledFlip(obj, spawn);
        stage.AddObject(obj);
    }
    
    else if (spawn.type == "CortinaTriggerAbrir") {
        // Qualquer irmão entra -> abre a cortina
        int curtainId = 0;
        if (spawn.properties.count("curtainId"))
            curtainId = spawn.properties.at("curtainId").get<int>();
    
        GameObject* obj = new GameObject();
        obj->tiledId = spawn.tiledId;
        obj->z       = spawn.z;
        obj->box.x   = spawn.x;      
        obj->box.y   = spawn.y;
        obj->box.w   = spawn.w;
        obj->box.h   = spawn.h;
        obj->AddComponent(new CurtainTrigger(*obj, curtainId, CurtainTriggerAction::OPEN));
        stage.AddObject(obj);
    }
 
    else if (spawn.type == "CortinaTriggerFechar") {
        // AMBOS os irmãos entram -> fecha a cortina
        int curtainId = 0;
        if (spawn.properties.count("curtainId"))
            curtainId = spawn.properties.at("curtainId").get<int>();
    
        GameObject* obj = new GameObject();
        obj->tiledId = spawn.tiledId;
        obj->z       = spawn.z;
        obj->box.x   = spawn.x;      
        obj->box.y   = spawn.y;
        obj->box.w   = spawn.w;
        obj->box.h   = spawn.h;
        obj->AddComponent(new CurtainTrigger(*obj, curtainId, CurtainTriggerAction::CLOSE));
        stage.AddObject(obj);
    }

    else if (spawn.type == "DialogueTrigger") {
        GameObject* triggerObj = new GameObject();
        triggerObj->tiledId = spawn.tiledId;
    
        std::vector<DialogueBox::Line> lines = ParseDialogueLinesProperty(spawn, "dialogue");
        bool once = spawn.properties.count("once") ? spawn.properties.at("once").get<bool>() : true;
    
        triggerObj->AddComponent(new DialogueTrigger(*triggerObj, lines, once));
        triggerObj->box.x = spawn.x;
        triggerObj->box.y = spawn.y;
        triggerObj->box.w = spawn.w;
        triggerObj->box.h = spawn.h;
        stage.AddObject(triggerObj);
    }
}