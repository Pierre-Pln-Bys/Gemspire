CXX ?= g++
CXXFLAGS += -std=c++17 -Wall -Wextra -Werror -pedantic
# Crée les dépendances pendant la compilation
CXXFLAGS += -MMD

# Nom de l'exécutable
TARGET = splendor
# Liste des fichiers nécessaires à sa réalisation
OBJS = src/main.o src/data.o src/rules.o src/game.o src/display.o
# Tout sauf main.o, pour les tests
LOGIC_OBJS = src/data.o src/rules.o src/game.o

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

tests/test_rules: tests/test_rules.o $(LOGIC_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test: tests/test_rules
	./tests/test_rules

clean:
	$(RM) $(TARGET) tests/test_rules src/*.o tests/*.o
clean-deps: clean
	$(RM) src/*.d tests/*.d

# Injecte les dépendances si présentes
-include src/*.d tests/*.d

.PHONY: clean clean-deps test
