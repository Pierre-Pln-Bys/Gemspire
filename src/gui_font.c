// Polices intégrées à l'exécutable (atlas générés par tools/make_font.c).
// Fichier en C car les en-têtes générés par raylib sont du C.
#include "raylib.h"

#include "font_bold.h"
#include "font_regular.h"

#include "gui_font.h"

Font load_font_regular(void)
{
    return LoadFont_FontRegular();
}

Font load_font_bold(void)
{
    return LoadFont_FontBold();
}
