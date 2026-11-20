#include "data.hpp"

// Données du jeu, recopiées depuis les fichiers fournis (ne pas modifier).
// Format d'une carte : {points, couleur fournie, {coût Blanc, Bleu, Vert, Rouge, Marron}}

const std::vector<Card>& cards_of_level(int level)
{
    static const std::vector<Card> level1 = {
        {0, Color::Maroon, {1, 2, 1, 1, 0}},
        {0, Color::Green,  {1, 1, 0, 1, 1}},
        {0, Color::Blue,   {1, 0, 1, 2, 1}},
        {0, Color::Maroon, {0, 0, 3, 0, 0}},
        {0, Color::Blue,   {1, 0, 0, 0, 2}},
        {0, Color::Maroon, {2, 2, 0, 1, 0}},
        {0, Color::Maroon, {2, 0, 2, 0, 0}},
        {0, Color::White,  {0, 3, 0, 0, 0}},
        {0, Color::Green,  {2, 1, 0, 0, 0}},
        {0, Color::Green,  {1, 1, 0, 1, 2}},
        {0, Color::Green,  {1, 3, 1, 0, 0}},
        {1, Color::Green,  {0, 0, 0, 0, 4}},
        {0, Color::Blue,   {1, 0, 2, 2, 0}},
        {0, Color::Blue,   {1, 0, 1, 1, 1}},
        {0, Color::Blue,   {0, 0, 2, 0, 2}},
        {0, Color::Maroon, {0, 0, 2, 1, 0}},
        {0, Color::Red,    {2, 0, 1, 0, 2}},
        {0, Color::White,  {0, 2, 2, 0, 1}},
        {1, Color::White,  {0, 0, 4, 0, 0}},
        {0, Color::Maroon, {1, 1, 1, 1, 0}},
        {1, Color::Maroon, {0, 4, 0, 0, 0}},
        {0, Color::Maroon, {0, 0, 1, 3, 1}},
        {1, Color::Blue,   {0, 0, 0, 4, 0}},
        {0, Color::Blue,   {0, 1, 3, 1, 0}},
        {0, Color::Blue,   {0, 0, 0, 0, 3}},
        {0, Color::Green,  {0, 1, 0, 2, 2}},
        {0, Color::Green,  {0, 0, 0, 3, 0}},
        {0, Color::Green,  {0, 2, 0, 2, 0}},
        {0, Color::Red,    {1, 0, 0, 1, 3}},
        {0, Color::Red,    {1, 1, 1, 0, 1}},
        {1, Color::Red,    {4, 0, 0, 0, 0}},
        {0, Color::White,  {0, 0, 0, 2, 1}},
        {0, Color::Red,    {2, 0, 0, 2, 0}},
        {0, Color::Red,    {3, 0, 0, 0, 0}},
        {0, Color::Red,    {0, 2, 1, 0, 0}},
        {0, Color::Red,    {2, 1, 1, 0, 1}},
        {0, Color::White,  {1, 1, 0, 1, 1}},
        {0, Color::White,  {3, 1, 0, 0, 1}},
        {0, Color::White,  {1, 2, 0, 1, 1}},
        {0, Color::White,  {0, 2, 0, 0, 2}},
    };
    static const std::vector<Card> level2 = {
        {2, Color::Maroon, {5, 0, 0, 0, 0}},
        {2, Color::Red,    {1, 4, 2, 0, 0}},
        {1, Color::Blue,   {0, 2, 3, 0, 3}},
        {2, Color::Blue,   {0, 5, 0, 0, 0}},
        {2, Color::Green,  {4, 2, 0, 0, 1}},
        {2, Color::Green,  {0, 5, 3, 0, 0}},
        {2, Color::White,  {0, 0, 0, 5, 3}},
        {2, Color::Green,  {0, 0, 5, 0, 0}},
        {3, Color::Green,  {0, 0, 6, 0, 0}},
        {1, Color::Maroon, {3, 2, 2, 0, 0}},
        {2, Color::White,  {0, 0, 1, 4, 2}},
        {2, Color::Maroon, {0, 1, 4, 2, 0}},
        {1, Color::Blue,   {0, 2, 2, 3, 0}},
        {3, Color::Red,    {0, 0, 0, 6, 0}},
        {1, Color::Maroon, {3, 0, 3, 0, 2}},
        {1, Color::Red,    {0, 3, 0, 2, 3}},
        {3, Color::Maroon, {0, 0, 0, 0, 6}},
        {1, Color::White,  {2, 3, 0, 3, 0}},
        {2, Color::Blue,   {2, 0, 0, 1, 4}},
        {1, Color::Red,    {2, 0, 0, 2, 3}},
        {2, Color::White,  {0, 0, 0, 5, 0}},
        {2, Color::Red,    {0, 0, 0, 0, 5}},
        {2, Color::Maroon, {0, 0, 5, 3, 0}},
        {1, Color::White,  {0, 0, 3, 2, 2}},
        {1, Color::Green,  {2, 3, 0, 0, 2}},
        {2, Color::Red,    {3, 0, 0, 0, 5}},
        {1, Color::Green,  {3, 0, 2, 0, 3}},
        {2, Color::Blue,   {5, 3, 0, 0, 0}},
        {3, Color::Blue,   {0, 6, 0, 0, 0}},
        {3, Color::White,  {6, 0, 0, 0, 0}},
    };
    static const std::vector<Card> level3 = {
        {5, Color::Green,  {0, 7, 3, 0, 0}},
        {4, Color::Green,  {3, 6, 3, 0, 0}},
        {3, Color::Maroon, {3, 3, 5, 3, 0}},
        {4, Color::White,  {3, 0, 0, 3, 6}},
        {4, Color::Red,    {0, 0, 7, 0, 0}},
        {5, Color::Red,    {0, 0, 7, 3, 0}},
        {3, Color::Blue,   {3, 0, 3, 3, 5}},
        {3, Color::White,  {0, 3, 3, 5, 3}},
        {3, Color::Green,  {5, 3, 0, 3, 3}},
        {5, Color::Blue,   {7, 3, 0, 0, 0}},
        {4, Color::Blue,   {6, 3, 0, 0, 3}},
        {5, Color::Maroon, {0, 0, 0, 7, 3}},
        {5, Color::White,  {3, 0, 0, 0, 7}},
        {4, Color::Red,    {0, 3, 6, 3, 0}},
        {4, Color::Maroon, {0, 0, 0, 7, 0}},
        {4, Color::White,  {0, 0, 0, 0, 7}},
        {3, Color::Red,    {3, 5, 3, 0, 3}},
        {4, Color::Green,  {0, 7, 0, 0, 0}},
        {4, Color::Maroon, {0, 0, 3, 6, 3}},
        {4, Color::Blue,   {7, 0, 0, 0, 0}},
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
