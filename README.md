# A-Luz-do-Farol---Jogo
Um jogo desenvolvido em grupo, para a disciplina de Introdução a Desenvolvimento de Jogos da Universidade de Brasília

## Build de desenvolvimento (Windows)

```bat
mingw32-make debug
```

## Pasta `dist/` — build de release para compartilhar

Gera **`dist\JOGO.exe`**, as DLLs do SDL em **`dist\`** e a cópia de **`dist\Recursos\`** com os mesmos caminhos do jogo (`Recursos\...`). O jogador deve **executar o `.exe` a partir dentro da pasta `dist`** (ou zipar **`dist`** inteira e enviar).

**Comando principal (clean + release + cópia):**

```bat
mingw32-make dist
```

(Na pasta raiz do projeto, onde está o `makefile`.)

**Alternativas:**

| Comando | Efeito |
|--------|--------|
| `mingw32-make package` ou `mingw32-make ship` | Só empacota; não apaga objetos intermediários primeiro |
| `mingw32-make dist DIST_DIR=pasta_qualquer` | Coloca o bundle em `pasta_qualquer\` |

**Via script:**

```bat
scripts\build-dist.bat
```

**Depois:** compacte **`dist`** em um `.zip` e publique em itch.io, Drive, etc. Inclua na descrição: “descompactar e rodar **`JOGO.exe`** dentro da pasta”.
