// Génère src/font_regular.h et src/font_bold.h : atlas de la police DejaVu Sans
// (caractères ASCII, Latin-1 et quelques signes typographiques), intégrés
// dans l'exécutable pour afficher les accents sans fichier externe.
//
// Utilisation (une seule fois, les en-têtes générés sont dans le dépôt) :
//   make font
#include "raylib.h"

static int codepoints[512];

static int build_codepoints(void)
{
    int count = 0;
    for (int c = 32; c <= 126; c++) {
        codepoints[count++] = c;
    }
    for (int c = 160; c <= 255; c++) {
        codepoints[count++] = c;
    }
    int extra[] = {0x152, 0x153, 0x2013, 0x2014, 0x2019, 0x201C, 0x201D, 0x2026, 0x20AC};
    for (int i = 0; i < (int)(sizeof(extra) / sizeof(extra[0])); i++) {
        codepoints[count++] = extra[i];
    }
    return count;
}

int main(int argc, char* argv[])
{
    if (argc != 3) {
        return 1;
    }
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(64, 64, "make_font");
    int count = build_codepoints();
    Font regular = LoadFontEx(argv[1], 48, codepoints, count);
    Font bold = LoadFontEx(argv[2], 48, codepoints, count);
    ExportFontAsCode(regular, "src/font_regular.h");
    ExportFontAsCode(bold, "src/font_bold.h");
    UnloadFont(regular);
    UnloadFont(bold);
    CloseWindow();
    return 0;
}
