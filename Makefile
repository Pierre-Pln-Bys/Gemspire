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
LOGIC_OBJS = src/data.o src/rules.o src/game.o src/move.o src/ai.o src/save.o src/elo.o
# Liste des fichiers nécessaires à chaque exécutable
OBJS = src/main.o src/display.o $(LOGIC_OBJS)
GUI_OBJS = src/gui_main.o src/gui_draw.o src/gui_font.o src/net.o src/discord_rpc.o src/move_json.o src/display.o \
           $(LOGIC_OBJS)

# raylib est compilée une fois, depuis third_party/, dans build/raylib
RAYLIB_DIR = third_party/raylib/src
RAYLIB_LIB = build/raylib/libraylib.a
GUI_CFLAGS = -isystem $(RAYLIB_DIR) -isystem third_party/json
# Réseau du jeu (HTTPS avec OpenSSL ; lié statiquement sous Windows)
NET_CFLAGS = -isystem third_party/httplib -DCPPHTTPLIB_OPENSSL_SUPPORT

ifeq ($(OS),Windows_NT)
    # Windows (MinGW) : exécutable autonome, sans fenêtre de console
    GUI_LIBS = $(RAYLIB_LIB) -lssl -lcrypto -lws2_32 -lcrypt32 -lbcrypt -lopengl32 -lgdi32 -lwinmm -static -mwindows
else
    # Linux : bibliothèques C++ liées statiquement, OpenGL et X11 du système
    # OpenSSL 3 (libssl.so.3) est présent sur toutes les distributions récentes
    GUI_LIBS = $(RAYLIB_LIB) -lssl -lcrypto -lGL -lm -lpthread -ldl -lrt -lX11 -static-libstdc++ -static-libgcc
endif

# Serveur en ligne (Linux) : cpp-httplib, nlohmann/json, SQLite (inclus), OpenSSL (système)
SERVER_TARGET = splendor-server
SERVER_OBJS = server/main.o server/app.o server/rooms.o server/store.o src/move_json.o src/display.o \
              $(LOGIC_OBJS) build/sqlite3.o
SERVER_CFLAGS = -isystem third_party/httplib -isystem third_party/json -isystem third_party/sqlite \
                -DCPPHTTPLIB_OPENSSL_SUPPORT
SERVER_LIBS = -lssl -lcrypto -lpthread -ldl

all: $(TARGET) $(GUI_TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(GUI_TARGET): $(GUI_OBJS) $(RAYLIB_LIB)
	$(CXX) $(LDFLAGS) -o $@ $(GUI_OBJS) $(GUI_LIBS)

src/gui_main.o src/gui_draw.o src/gui_font.o: CXXFLAGS += $(GUI_CFLAGS)
src/net.o: CXXFLAGS += $(NET_CFLAGS)
src/gui_font.o: CFLAGS += $(GUI_CFLAGS)

$(RAYLIB_LIB):
	mkdir -p build/raylib
	$(MAKE) -C $(RAYLIB_DIR) PLATFORM=PLATFORM_DESKTOP RAYLIB_RELEASE_PATH=$(CURDIR)/build/raylib

$(SERVER_TARGET): $(SERVER_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(SERVER_LIBS)

server/%.o src/move_json.o: CXXFLAGS += $(SERVER_CFLAGS)

# SQLite est du code externe : compilée avec ses propres options, sans -Werror
build/sqlite3.o: third_party/sqlite/sqlite3.c
	mkdir -p build
	$(CC) -O2 -w -DSQLITE_THREADSAFE=1 -DSQLITE_OMIT_LOAD_EXTENSION -c $< -o $@

# Régénère les polices intégrées (src/font_*.h) à partir des fichiers .ttf
font: $(RAYLIB_LIB)
	$(CC) -std=c99 tools/make_font.c -isystem $(RAYLIB_DIR) $(GUI_LIBS) -o build/make_font
	./build/make_font third_party/fonts/DejaVuSans.ttf third_party/fonts/DejaVuSans-Bold.ttf

tests/test_rules: tests/test_rules.o $(LOGIC_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test: tests/test_rules
	./tests/test_rules

tests/test_server: tests/test_server.o server/app.o server/rooms.o server/store.o src/move_json.o src/display.o \
                   $(LOGIC_OBJS) build/sqlite3.o
	$(CXX) $(LDFLAGS) -o $@ $^ $(SERVER_LIBS)

tests/test_server.o: CXXFLAGS += $(SERVER_CFLAGS)

test-server: tests/test_server
	./tests/test_server

tests/test_discord: tests/test_discord.o src/discord_rpc.o
	$(CXX) $(LDFLAGS) -o $@ $^ -lpthread

tests/test_discord.o src/discord_rpc.o: CXXFLAGS += -isystem third_party/json

test-discord: tests/test_discord
	./tests/test_discord

# Bot Discord (Python) : logique, client du serveur, intégration avec splendor-server
test-bot: $(SERVER_TARGET)
	python3 -m unittest bot/test_bot.py bot/test_integration.py

clean:
	$(RM) $(TARGET) $(GUI_TARGET) $(SERVER_TARGET) $(TARGET).exe $(GUI_TARGET).exe tests/test_rules tests/test_server tests/test_discord \
	      src/*.o tests/*.o server/*.o
clean-deps: clean
	$(RM) src/*.d tests/*.d server/*.d
# Supprime aussi la raylib compilée
clean-all: clean-deps
	$(RM) -r build
	$(MAKE) -C $(RAYLIB_DIR) clean

# Injecte les dépendances si présentes
-include src/*.d tests/*.d server/*.d

.PHONY: all clean clean-deps clean-all test test-server test-discord test-bot font
