<div align="center">

<img src="Gifs%20and%20Images/menu.gif" alt="Menu Principal" width="800"/>

# 🕯️ The Last Lightkeeper
### *A Luz do Farol*

> *Dois irmãos. Um farol. Uma criatura que teme a luz — mas não por muito tempo.*

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue?style=for-the-badge&logo=cplusplus)](https://en.cppreference.com/w/cpp/17)
[![SDL2](https://img.shields.io/badge/SDL2-2.x-orange?style=for-the-badge)](https://www.libsdl.org/)
[![Windows](https://img.shields.io/badge/Windows-10%2F11-0078D6?style=for-the-badge&logo=windows)](https://www.microsoft.com/windows)
[![UnB IDJ](https://img.shields.io/badge/UnB-Introdução_ao_Desenvolvimento_de_Jogos-darkgreen?style=for-the-badge)](https://www.unb.br)

</div>

---

## 📖 Sobre o Jogo

**The Last Lightkeeper** é um jogo de terror e sobrevivência top-down desenvolvido como trabalho final da disciplina de **Introdução ao Desenvolvimento de Jogos** da Universidade de Brasília (UnB).

Dois irmãos estão perdidos nesse universo e sua única fuga é um farol abandonado. Dentro dele, algo os aguarda nas sombras — uma criatura que odeia a luz, mas que aprende a combatê-la. Os irmãos precisam subir até o topo do farol, resolvendo puzzles, gerenciando recursos de luz e fugindo de um inimigo que nunca descansa. Durante o caminho para a luz, encontram documentos que contam tanto da história dos acontecimentos atuais, quanto do passado do farol.

> ⚠️ **Demo:** Esta versão contém **3 dos 10 andares** previstos no design original. O jogo completo expandiria a narrativa até o topo do farol.

---

## 🎮 Gameplay

### Os Andares

<div align="center">

<img src="Gifs%20and%20Images/Andar_1.png" width="700"/>

*Primeiro Andar — Sala de entrada, aprenda os controles e o básico de sobrevivência*

<br/>

<img src="Gifs%20and%20Images/Andar_2.png" width="700"/>

*Segundo Andar — O monstro começa a patrulhar de verdade*

<br/>

<img src="Gifs%20and%20Images/Andar_3.png" width="700"/>

*Terceiro Andar — Cortinas, puzzles de luz e o cerco final*

</div>

---

### 🕯️ Interação e Sobrevivência

<div align="center">
<img src="Gifs%20and%20Images/interacao.gif" alt="Interação com objetos" width="700"/>
</div>

> Colete itens, acenda velas e gerencie o estoque de combustível do seu isqueiro. A luz é sua única proteção — mas ela também dura um tempo.

---

### 🪟 Janelas e Estratégia do Monstro

<div align="center">
<img src="Gifs%20and%20Images/janelas.gif" alt="Sistema de janelas" width="700"/>
</div>

> O monstro não é passivo. Quando os irmãos se escondem, ele começa a **abrir janelas** estrategicamente para apagar as velas do andar e forçá-los a sair das sombras.

---

### 👁️ O Monstro

<div align="center">
<img src="Gifs%20and%20Images/monstro.gif" alt="Monstro em ação" width="700"/>
</div>

> Ser visto é o começo do fim. O monstro possui uma IA com **7 estados de comportamento** — ele patrulha, investiga, persegue, caça, foge da luz, sabota o ambiente e se recupera quando preso. Use o poder do Irmãozinho para criar flashes de luz e repeli-lo quando necessário.

---

### 🏁 Progresso

<div align="center">
<img src="Gifs%20and%20Images/progresso.gif" alt="Subindo de andar" width="700"/>
</div>

> O objetivo é simples: **suba.** Resolva o puzzle de cada andar, repare as escadas e suba para o próximo nível. Mas o monstro fica mais ameaçador conforme você avança.

---

## 🎯 Mecânicas Principais

| Mecânica | Descrição |
|---|---|
| 🔦 **Sistema de Luz** | Isqueiro com durabilidade, velas acendíveis, campo de visão na luz |
| 🧠 **IA Adaptativa** | Monstro com 7 estados — patrulha, perseguição, sabotagem e mais |
| 👬 **Dois Personagens** | Alterne entre Irmãozão e Irmãozinho, cada um com habilidades únicas |
| 📦 **Física de Objetos** | Barris e caixas com peso variável, empurráveis para criar estratégias |
| 🪟 **Sistema de Janelas** | Janelas abertas apagam velas próximas — proteja ou sabote |
| 🧩 **Puzzles por Andar** | Cada andar tem um objetivo único para avançar |
| 💾 **Save System** | Progresso salvo automaticamente entre andares |
| ⚙️ **Controles Remapeáveis** | Configure as teclas pelo menu de configurações |

---

## 🕹️ Controles Padrão

| Ação | Tecla |
|---|---|
| Mover | `W` `A` `S` `D` |
| Interagir | `E` |
| Usar item | `F` |
| Item anterior / próximo | `←` `→` |
| Trocar personagem | `Left Ctrl` |
| Modo duplo | `Q` |
| Pausar | `Esc` |

> Todos os controles podem ser remapeados em **Configurações → Controles**.

---

## 🛠️ Como Compilar e Executar

### Pré-requisitos

- **Windows 10 ou 11**
- **MinGW / TDM-GCC** (32-bit) com suporte a C++17
- As bibliotecas SDL2 já estão incluídas na pasta `SDL2/` do repositório

### Compilação

```bash
# Clone o repositório
git clone https://github.com/The-Jonas/The-Last-Lightkeeper.git
cd The-Last-Lightkeeper

# Compilar versão de release
mingw32-make

# Compilar versão de desenvolvimento (com debug visual e atalhos extras)
mingw32-make debug

# Gerar a versão para distribuição
mingw32-make dist
# OU execute o deploy.bat (na pasta scripts)
```

### Executar

```bash
./JOGO.exe
```

> **Nota:** Execute a partir da raiz do repositório para que os recursos (`Recursos/`) sejam encontrados corretamente.

### Atalhos de Debug (versão dev)

| Atalho | Função |
|---|---|
| `F5` | Pular para o próximo andar |
| `B` | Visualizar caixas de colisão |
| `L` | Alternar luzes |
| `M` | Silenciar música |

---

## 🏗️ Arquitetura Técnica

O jogo foi desenvolvido com uma **engine própria** em C++17 + SDL2, sem uso de engines comerciais.

```text
The-Last-Lightkeeper/
├── src/
│   ├── audio/          # Dublagem, efeitos sonoros, música
│   ├── core/           # Game loop, recursos, input, save
│   ├── engine/         # ECS: GameObject, Component, Camera, Sprite
│   ├── gameplay/       # Monstro, personagens, itens, cortinas, janelas
│   ├── lighting/       # Sistema de iluminação e sombras dinâmicas
│   ├── states/         # TitleState, StageState, EndState e transições
│   ├── ui/             # HUD, inventário, silhueta do monstro
│   └── world/          # TileMap, Collider, SpawnFactory
├── include/            # Headers
├── Recursos/
│   ├── img/            # Sprites, tiles, UI
│   ├── audio/          # Trilhas e efeitos sonoros
│   ├── font/           # Fontes
│   ├── maps/           # Feitos no Tiled, disponíveis em JSON
│   └── data/           # Configurações em JSON (monster.json, etc.)
└── SDL2/               # Bibliotecas SDL2 (já incluídas)
```

**Tecnologias:**

| Biblioteca | Uso |
|---|---|
| SDL2 | Renderização, input, janela |
| SDL2_image | Carregamento de texturas PNG/JPG |
| SDL2_mixer | Áudio multicanal (MP3, WAV) |
| SDL2_ttf | Renderização de texto UTF-8 |
| nlohmann/json | Leitura de mapas e configurações |
| Tiled | Editor de mapas (formato JSON) |

---

## 👥 Equipe

<div align="center">

### 💻 Programação

| | Contribuidor |
|:-:|:-:|
| 👤 | **João Victor Pereira** |
| 👤 | **Matheus Azevedo** |

### 🎨 Design

| | Contribuidor |
|:-:|:-:|
| 👤 | **Flávio de Oliveira Salão** |
| 👤 | **Bryan Gomes Silva** |
| 👤 | **Haru Braga Vasconcelos** |

### 📢 Comunicação

| | Contribuidor |
|:-:|:-:|
| 👤 | **Luana de Moura Duque** |
| 👤 | **Amanda Cavalcante** |

</div>

---

## 📄 Licença

Este projeto foi desenvolvido para fins acadêmicos como trabalho da disciplina de **Introdução ao Desenvolvimento de Jogos** da **Universidade de Brasília (UnB)**.

---

<div align="center">

*🕯️ A luz acabará. E a monstruosidade permanecerá.*

</div>
