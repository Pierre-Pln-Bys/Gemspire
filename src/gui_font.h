#ifndef SPLENDOR_GUI_FONT_H
#define SPLENDOR_GUI_FONT_H

#include "raylib.h"

#ifdef __cplusplus
extern "C" {
#endif

// Polices DejaVu Sans (normale et grasse), taille de base 48 px, avec les
// accents français. À appeler après InitWindow.
Font load_font_regular(void);
Font load_font_bold(void);

#ifdef __cplusplus
}
#endif

#endif
