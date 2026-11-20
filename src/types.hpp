#ifndef SPLENDOR_TYPES_HPP
#define SPLENDOR_TYPES_HPP

#include <array>
#include <string>

// Les 5 couleurs de ressources. Cet ordre est celui des données de data.cpp.
enum class Color { White, Blue, Green, Red, Black };

const int COLOR_COUNT = 5;

// Une quantité de chaque ressource, indicée par la couleur (0 = Blanc ... 4 = Noir)
using Resources = std::array<int, COLOR_COUNT>;

// Une carte de développement
struct Card {
    int points;        // points de prestige
    Color provides;    // couleur du bonus fourni
    Resources cost;    // coût en ressources
};

// Un noble : il coûte des bonus (cartes possédées), pas des jetons
struct Noble {
    int points;
    Resources cost;
    std::string name;
};

// Conversion couleur <-> indice dans un tableau Resources
inline int index_of(Color color)
{
    return static_cast<int>(color);
}

inline Color color_at(int index)
{
    return static_cast<Color>(index);
}

#endif
