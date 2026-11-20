#include "data.hpp"

// Données du jeu, recopiées depuis les fichiers fournis (ne pas modifier).
// Format d'une carte : {points, couleur fournie, {coût Blanc, Bleu, Vert, Rouge, Noir}}

const std::vector<Card>& cards_of_level(int level)
{
    static const std::vector<Card> level1 = {
        {0, Gem::Black,  {1, 2, 1, 1, 0}},
        {0, Gem::Green,  {1, 1, 0, 1, 1}},
        {0, Gem::Blue,   {1, 0, 1, 2, 1}},
        {0, Gem::Black,  {0, 0, 3, 0, 0}},
        {0, Gem::Blue,   {1, 0, 0, 0, 2}},
        {0, Gem::Black,  {2, 2, 0, 1, 0}},
        {0, Gem::Black,  {2, 0, 2, 0, 0}},
        {0, Gem::White,  {0, 3, 0, 0, 0}},
        {0, Gem::Green,  {2, 1, 0, 0, 0}},
        {0, Gem::Green,  {1, 1, 0, 1, 2}},
        {0, Gem::Green,  {1, 3, 1, 0, 0}},
        {1, Gem::Green,  {0, 0, 0, 0, 4}},
        {0, Gem::Blue,   {1, 0, 2, 2, 0}},
        {0, Gem::Blue,   {1, 0, 1, 1, 1}},
        {0, Gem::Blue,   {0, 0, 2, 0, 2}},
        {0, Gem::Black,  {0, 0, 2, 1, 0}},
        {0, Gem::Red,    {2, 0, 1, 0, 2}},
        {0, Gem::White,  {0, 2, 2, 0, 1}},
        {1, Gem::White,  {0, 0, 4, 0, 0}},
        {0, Gem::Black,  {1, 1, 1, 1, 0}},
        {1, Gem::Black,  {0, 4, 0, 0, 0}},
        {0, Gem::Black,  {0, 0, 1, 3, 1}},
        {1, Gem::Blue,   {0, 0, 0, 4, 0}},
        {0, Gem::Blue,   {0, 1, 3, 1, 0}},
        {0, Gem::Blue,   {0, 0, 0, 0, 3}},
        {0, Gem::Green,  {0, 1, 0, 2, 2}},
        {0, Gem::Green,  {0, 0, 0, 3, 0}},
        {0, Gem::Green,  {0, 2, 0, 2, 0}},
        {0, Gem::Red,    {1, 0, 0, 1, 3}},
        {0, Gem::Red,    {1, 1, 1, 0, 1}},
        {1, Gem::Red,    {4, 0, 0, 0, 0}},
        {0, Gem::White,  {0, 0, 0, 2, 1}},
        {0, Gem::Red,    {2, 0, 0, 2, 0}},
        {0, Gem::Red,    {3, 0, 0, 0, 0}},
        {0, Gem::Red,    {0, 2, 1, 0, 0}},
        {0, Gem::Red,    {2, 1, 1, 0, 1}},
        {0, Gem::White,  {1, 1, 0, 1, 1}},
        {0, Gem::White,  {3, 1, 0, 0, 1}},
        {0, Gem::White,  {1, 2, 0, 1, 1}},
        {0, Gem::White,  {0, 2, 0, 0, 2}},
    };
    static const std::vector<Card> level2 = {
        {2, Gem::Black,  {5, 0, 0, 0, 0}},
        {2, Gem::Red,    {1, 4, 2, 0, 0}},
        {1, Gem::Blue,   {0, 2, 3, 0, 3}},
        {2, Gem::Blue,   {0, 5, 0, 0, 0}},
        {2, Gem::Green,  {4, 2, 0, 0, 1}},
        {2, Gem::Green,  {0, 5, 3, 0, 0}},
        {2, Gem::White,  {0, 0, 0, 5, 3}},
        {2, Gem::Green,  {0, 0, 5, 0, 0}},
        {3, Gem::Green,  {0, 0, 6, 0, 0}},
        {1, Gem::Black,  {3, 2, 2, 0, 0}},
        {2, Gem::White,  {0, 0, 1, 4, 2}},
        {2, Gem::Black,  {0, 1, 4, 2, 0}},
        {1, Gem::Blue,   {0, 2, 2, 3, 0}},
        {3, Gem::Red,    {0, 0, 0, 6, 0}},
        {1, Gem::Black,  {3, 0, 3, 0, 2}},
        {1, Gem::Red,    {0, 3, 0, 2, 3}},
        {3, Gem::Black,  {0, 0, 0, 0, 6}},
        {1, Gem::White,  {2, 3, 0, 3, 0}},
        {2, Gem::Blue,   {2, 0, 0, 1, 4}},
        {1, Gem::Red,    {2, 0, 0, 2, 3}},
        {2, Gem::White,  {0, 0, 0, 5, 0}},
        {2, Gem::Red,    {0, 0, 0, 0, 5}},
        {2, Gem::Black,  {0, 0, 5, 3, 0}},
        {1, Gem::White,  {0, 0, 3, 2, 2}},
        {1, Gem::Green,  {2, 3, 0, 0, 2}},
        {2, Gem::Red,    {3, 0, 0, 0, 5}},
        {1, Gem::Green,  {3, 0, 2, 0, 3}},
        {2, Gem::Blue,   {5, 3, 0, 0, 0}},
        {3, Gem::Blue,   {0, 6, 0, 0, 0}},
        {3, Gem::White,  {6, 0, 0, 0, 0}},
    };
    static const std::vector<Card> level3 = {
        {5, Gem::Green,  {0, 7, 3, 0, 0}},
        {4, Gem::Green,  {3, 6, 3, 0, 0}},
        {3, Gem::Black,  {3, 3, 5, 3, 0}},
        {4, Gem::White,  {3, 0, 0, 3, 6}},
        {4, Gem::Red,    {0, 0, 7, 0, 0}},
        {5, Gem::Red,    {0, 0, 7, 3, 0}},
        {3, Gem::Blue,   {3, 0, 3, 3, 5}},
        {3, Gem::White,  {0, 3, 3, 5, 3}},
        {3, Gem::Green,  {5, 3, 0, 3, 3}},
        {5, Gem::Blue,   {7, 3, 0, 0, 0}},
        {4, Gem::Blue,   {6, 3, 0, 0, 3}},
        {5, Gem::Black,  {0, 0, 0, 7, 3}},
        {5, Gem::White,  {3, 0, 0, 0, 7}},
        {4, Gem::Red,    {0, 3, 6, 3, 0}},
        {4, Gem::Black,  {0, 0, 0, 7, 0}},
        {4, Gem::White,  {0, 0, 0, 0, 7}},
        {3, Gem::Red,    {3, 5, 3, 0, 3}},
        {4, Gem::Green,  {0, 7, 0, 0, 0}},
        {4, Gem::Black,  {0, 0, 3, 6, 3}},
        {4, Gem::Blue,   {7, 0, 0, 0, 0}},
    };

    if (level == 1) {
        return level1;
    }
    if (level == 2) {
        return level2;
    }
    return level3;
}

const std::vector<Noble>& all_nobles()
{
    static const std::vector<Noble> nobles = {
        {3, {0, 0, 4, 4, 0}, "Marie Stuart" },
        {3, {3, 0, 0, 3, 3}, "Charles Quint" },
        {3, {4, 4, 0, 0, 0}, "Machiavel" },
        {3, {4, 0, 0, 0, 4}, "Isabelle de Castille" },
        {3, {0, 4, 4, 0, 0}, "Soliman le Magnifique" },
        {3, {0, 3, 3, 3, 0}, "Catherine de Medicis" },
        {3, {3, 3, 3, 0, 0}, "Anne de Bretagne" },
        {3, {4, 0, 0, 4, 0}, "Henry VIII" },
        {3, {3, 3, 0, 0, 3}, "Elisabeth d'Autriche" },
        {3, {0, 0, 3, 3, 3}, "Francois I" },
    };
    return nobles;
}
