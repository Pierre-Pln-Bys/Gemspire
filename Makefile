CXX ?= g++
CXXFLAGS += -std=c++17 -Wall -Wextra -Werror -pedantic
CFLAGS += -std=c99 -Wall -Wextra -Werror -pedantic
# Crée les dépendances pendant la compilation
CXXFLAGS += -MMD
CFLAGS += -MMD

# Version console (aucune dépendance) et version graphique (raylib, incluse)
TARGET = splendor
GUI_TARGET = splendor-gui

# Logique du jeu, commune aux deux versions et aux tests
LOGIC_OBJS = src/data.o src/rules.o src/game.o src/ai.o src/save.o
# Liste des fichiers nécessaires à chaque exécutable
OBJS = src/main.o src/display.o $(LOGIC_OBJS)
GUI_OBJS = src/gui_main.o src/gui_draw.o src/gui_font.o src/display.o $(LOGIC_OBJS)

# raylib est compilée une fois, depuis third_party/, dans build/raylib
RAYLIB_DIR = third_party/raylib/src
RAYLIB_LIB = build/raylib/libraylib.a
GUI_CFLAGS = -isystem $(RAYLIB_DIR)

ifeq ($(OS),Windows_NT)
    # Windows (MinGW) : exécutable autonome, sans fenêtre de console
    GUI_LIBS = $(RAYLIB_LIB) -lopengl32 -lgdi32 -lwinmm -static -mwindows
else
    # Linux : bibliothèques C++ liées statiquement, OpenGL et X11 du système
    GUI_LIBS = $(RAYLIB_LIB) -lGL -lm -lpthread -ldl -lrt -lX11 -static-libstdc++ -static-libgcc
endif

all: $(TARGET) $(GUI_TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(GUI_TARGET): $(GUI_OBJS) $(RAYLIB_LIB)
	$(CXX) $(LDFLAGS) -o $@ $(GUI_OBJS) $(GUI_LIBS)

src/gui_main.o src/gui_draw.o src/gui_font.o: CXXFLAGS += $(GUI_CFLAGS)
src/gui_font.o: CFLAGS += $(GUI_CFLAGS)

$(RAYLIB_LIB):
	mkdir -p build/raylib
	$(MAKE) -C $(RAYLIB_DIR) PLATFORM=PLATFORM_DESKTOP RAYLIB_RELEASE_PATH=$(CURDIR)/build/raylib

# Régénère les polices intégrées (src/font_*.h) à partir des fichiers .ttf
font: $(RAYLIB_LIB)
	$(CC) -std=c99 tools/make_font.c -isystem $(RAYLIB_DIR) $(GUI_LIBS) -o build/make_font
	./build/make_font third_party/fonts/DejaVuSans.ttf third_party/fonts/DejaVuSans-Bold.ttf

tests/test_rules: tests/test_rules.o $(LOGIC_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test: tests/test_rules
	./tests/test_rules

clean:
	$(RM) $(TARGET) $(GUI_TARGET) $(TARGET).exe $(GUI_TARGET).exe tests/test_rules src/*.o tests/*.o
clean-deps: clean
	$(RM) src/*.d tests/*.d
# Supprime aussi la raylib compilée
clean-all: clean-deps
	$(RM) -r build
	$(MAKE) -C $(RAYLIB_DIR) clean

# Injecte les dépendances si présentes
-include src/*.d tests/*.d

.PHONY: all clean clean-deps clean-all test font
