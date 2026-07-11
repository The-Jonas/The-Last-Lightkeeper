COMPILER = g++
RMDIR = rm -rdf
RM = rm -f

DEP_FLAGS = -M -MT $@ -MT $(BIN_PATH)/$(*F).o -MP -MF $@
LIBS = -lSDL2 -lSDL2_image -lSDL2_mixer -lSDL2_ttf -lm

INC_PATHS = -I$(INC_PATH) $(addprefix -I,$(SDL_INC_PATH))

FLAGS = -std=c++17 -Wall -pedantic -Wextra -Wno-unused-parameter -Werror=init-self

# Hash do commit atual embutido no binario → aparece no relatorio de crash
# ("Commit: ..."), tornando cada report auto-identificavel (sabemos a fonte
# EXATA para symbolizar). "-dirty" marca build com alteracoes nao commitadas.
# Sem redirecionamento de shell (roda igual em cmd.exe e sh).
GITHASH := $(shell git rev-parse --short HEAD)
GITDIRTY := $(shell git status --porcelain)
ifeq ($(GITHASH),)
GITHASH := desconhecido
endif
ifneq ($(GITDIRTY),)
GITHASH := $(GITHASH)-dirty
endif
FLAGS += -DBUILD_GITHASH=\"$(GITHASH)\"

DFLAGS = -ggdb -O0 -DDEBUG

# -g embute simbolos de depuracao (DWARF) SEM desligar a otimizacao: permite
# resolver as entradas [modulo+0xRVA] do relatorio de crash com addr2line.
# -fno-omit-frame-pointer mantem a cadeia de EBP/RBP: sem isso o StackWalk64 do
# CrashHandler nao consegue subir alem do frame #0 em 32-bit. Custo de perf ~0.
RFLAGS = -O3 -mtune=native -g -fno-omit-frame-pointer

INC_PATH = include
SRC_PATH = src
BIN_PATH = bin
DEP_PATH = dep

CPP_FILES = $(wildcard $(SRC_PATH)/*.cpp) \
            $(wildcard $(SRC_PATH)/*/*.cpp) \
            $(wildcard $(SRC_PATH)/*/*/*.cpp)
OBJ_FILES = $(patsubst $(SRC_PATH)/%.cpp,$(BIN_PATH)/%.o,$(CPP_FILES))
DEP_FILES = $(patsubst $(SRC_PATH)/%.cpp,$(DEP_PATH)/%.d,$(CPP_FILES))

EXEC = JOGO

# SE FOR WINDOWS
ifeq ($(OS),Windows_NT)
RMDIR = rd /s /q
RM = del /q

# SDL_PATHS = C:/SDL2
SDL_PATHS = SDL2

SDL_INC_PATH += $(addsuffix /include,$(SDL_PATHS))
LINK_PATH = $(addprefix -L,$(addsuffix /lib,$(SDL_PATHS)))
FLAGS += -mwindows
DFLAGS += -mconsole
# dbghelp: usado pelo CrashHandler para pilha de chamadas + minidump.
LIBS := -lmingw32 -lSDL2main $(LIBS) -ldbghelp

# Ícone do .exe: compila appicon.rc (que aponta pro appicon.ico) com windres e
# linka o objeto resultante no executável. Só no Windows.
WINDRES = windres
RES_OBJ = $(BIN_PATH)/appicon.res.o

EXEC := $(EXEC).exe

else

UNAME_S := $(shell uname -s)

# SE FOR MAC
ifeq ($(UNAME_S), Darwin)

LIBS = -lm -framework SDL2 -framework SDL2_image -framework SDL2_mixer -framework SDL2_ttf

endif
endif

DIST_DIR := dist

# Static-link the MinGW C/C++ runtime for any DISTRIBUTION build (release or
# debug) so the bundle does not depend on libgcc_s / libstdc++-6 DLLs on the
# player's machine. Note: this does NOT cover libwinpthread-1.dll (POSIX-threads
# MinGW imports it dynamically regardless) — deploy.ps1 bundles that separately.
STATIC_RUNTIME_LINK_FLAGS :=
ifeq ($(OS),Windows_NT)
ifneq ($(filter release package ship dist debug debug-package debug-dist,$(MAKECMDGOALS)),)
STATIC_RUNTIME_LINK_FLAGS := -static-libgcc -static-libstdc++
endif
endif

.PRECIOUS: $(DEP_FILES)
.PHONY: release debug clean folders help package debug-package ship dist debug-dist all

all: $(EXEC)

$(EXEC): $(OBJ_FILES) $(RES_OBJ)
	$(COMPILER) -o $@ $^ $(LINK_PATH) $(LIBS) $(FLAGS) $(STATIC_RUNTIME_LINK_FLAGS)
ifeq ($(OS),Windows_NT)
	@copy /Y "SDL2\bin\*.dll" . > nul
endif

# Objeto de recursos (ícone). Só existe/é construído no Windows (RES_OBJ vazio
# nos demais SOs, então some da lista de pré-requisitos acima).
ifeq ($(OS),Windows_NT)
$(RES_OBJ): appicon.rc appicon.ico | folders
	$(WINDRES) -I. appicon.rc -O coff -o $@
endif

$(BIN_PATH)/%.o: $(SRC_PATH)/%.cpp $(DEP_PATH)/%.d | folders
ifeq ($(OS),Windows_NT)
	@if not exist "$(subst /,\,$(dir $@))" mkdir "$(subst /,\,$(dir $@))"
else
	@mkdir -p $(dir $@)
endif
	$(COMPILER) $(INC_PATHS) -c $(FLAGS) -o $@ $<

$(DEP_PATH)/%.d: $(SRC_PATH)/%.cpp | folders
ifeq ($(OS),Windows_NT)
	@if not exist "$(subst /,\,$(dir $@))" mkdir "$(subst /,\,$(dir $@))"
else
	@mkdir -p $(dir $@)
endif
	$(COMPILER) $(INC_PATHS) $< $(DEP_FLAGS) $(FLAGS)

clean:
	$(RMDIR) $(DEP_PATH)
	$(RMDIR) $(BIN_PATH)
	$(RM) $(EXEC)
ifeq ($(OS),Windows_NT)
	$(RM) *.dll
endif

release: FLAGS += $(RFLAGS)
release: $(EXEC)

debug: FLAGS += $(DFLAGS)
debug: $(EXEC)

## Pasta pronta para distribuir: JOGO.exe + DLLs SDL + Recursos. Uso: mingw32-make dist
package: release
ifeq ($(OS),Windows_NT)
	@if exist "$(DIST_DIR)" $(RMDIR) "$(DIST_DIR)"
	@mkdir "$(DIST_DIR)" 2> nul
	@copy /Y "$(EXEC)" "$(DIST_DIR)\\" > nul
	@copy /Y "SDL2\bin\*.dll" "$(DIST_DIR)\\" > nul
	@xcopy /E /I /Q /Y "Recursos" "$(DIST_DIR)\\Recursos\\" > nul
	@echo.
	@echo Bundle pronto em .\\$(DIST_DIR)\\$(EXEC)  — execute dentro dessa pasta.
else
	mkdir -p "$(DIST_DIR)"
	cp -f "$(EXEC)" "$(DIST_DIR)/"
	cp -r Recursos "$(DIST_DIR)/"
ifneq ($(wildcard SDL2/bin/*.dll),)
	cp -f SDL2/bin/*.dll "$(DIST_DIR)/"
endif
	@echo Bundle pronto em $(DIST_DIR)/$(EXEC)
endif

dist:
	@$(MAKE) clean
	@$(MAKE) package

ship: dist

## Igual ao package, mas com a build de debug (-ggdb -O0 -DDEBUG, console).
## Uso: mingw32-make debug-package
debug-package: debug
ifeq ($(OS),Windows_NT)
	@if exist "$(DIST_DIR)" $(RMDIR) "$(DIST_DIR)"
	@mkdir "$(DIST_DIR)" 2> nul
	@copy /Y "$(EXEC)" "$(DIST_DIR)\\" > nul
	@copy /Y "SDL2\bin\*.dll" "$(DIST_DIR)\\" > nul
	@xcopy /E /I /Q /Y "Recursos" "$(DIST_DIR)\\Recursos\\" > nul
	@echo.
	@echo Bundle DEBUG pronto em .\\$(DIST_DIR)\\$(EXEC)  — execute dentro dessa pasta.
else
	mkdir -p "$(DIST_DIR)"
	cp -f "$(EXEC)" "$(DIST_DIR)/"
	cp -r Recursos "$(DIST_DIR)/"
ifneq ($(wildcard SDL2/bin/*.dll),)
	cp -f SDL2/bin/*.dll "$(DIST_DIR)/"
endif
	@echo Bundle DEBUG pronto em $(DIST_DIR)/$(EXEC)
endif

## clean + debug-package (build de debug pronta para empacotar/enviar).
debug-dist:
	@$(MAKE) clean
	@$(MAKE) debug-package

folders:
ifeq ($(OS),Windows_NT)
	@if NOT exist $(DEP_PATH) mkdir $(DEP_PATH)
	@if NOT exist $(BIN_PATH) mkdir $(BIN_PATH)
	@if NOT exist $(INC_PATH) mkdir $(INC_PATH)
	@if NOT exist $(SRC_PATH) mkdir $(SRC_PATH)
else
	@mkdir -p $(DEP_PATH) $(BIN_PATH) $(INC_PATH) $(SRC_PATH)
endif

print-% : ; echo $* = $($*)

help:
ifeq ($(OS), Windows_NT)
	echo.
endif
	@echo Available targets:
	@echo - release: Builds the release optimized build next to the makefile
	@echo - debug: Builds the debug build
	@echo - package / ship: release + copies $(EXEC), SDL DLLs, Recursos into $(DIST_DIR)
	@echo - dist: clean + package (recommended before sharing the game)
	@echo - debug-package: debug build + copies $(EXEC), SDL DLLs, Recursos into $(DIST_DIR)
	@echo - debug-dist: clean + debug-package (debug build ready to share)
	@echo - clean: Cleans generated files
	@echo - folders: Generates project directories
	@echo - help: Show help
ifeq ($(OS), Windows_NT)
	echo.
endif

.SECONDEXPANSION:
-include $$(DEP_FILES)