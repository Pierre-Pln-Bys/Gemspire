CXX ?= g++
CXXFLAGS += -std=c++17 -Wall -Wextra -Werror -pedantic
# Crée les dépendances pendant la compilation
CXXFLAGS += -MMD

# Version console (aucune dépendance) et version graphique (GTK 3)
TARGET = splendor
GUI_TARGET = splendor-gui

# Logique du jeu, commune aux deux versions et aux tests
LOGIC_OBJS = src/data.o src/rules.o src/game.o
# Liste des fichiers nécessaires à chaque exécutable
OBJS = src/main.o src/display.o $(LOGIC_OBJS)
GUI_OBJS = src/gui_main.o src/gui_draw.o src/display.o $(LOGIC_OBJS)

# GTK : en-têtes inclus en -isystem pour ne pas appliquer -Werror à GTK lui-même
GTK_CFLAGS = $(shell pkg-config --cflags gtk+-3.0 2>/dev/null | sed 's/-I/-isystem /g')
GTK_LIBS = $(shell pkg-config --libs gtk+-3.0 2>/dev/null)

all: $(TARGET) $(GUI_TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(GUI_TARGET): $(GUI_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(GTK_LIBS)

src/gui_main.o src/gui_draw.o: CXXFLAGS += $(GTK_CFLAGS)

tests/test_rules: tests/test_rules.o $(LOGIC_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test: tests/test_rules
	./tests/test_rules

clean:
	$(RM) $(TARGET) $(GUI_TARGET) $(TARGET).exe tests/test_rules src/*.o tests/*.o
clean-deps: clean
	$(RM) src/*.d tests/*.d

# Injecte les dépendances si présentes
-include src/*.d tests/*.d

.PHONY: all clean clean-deps test
