#ifndef PSX_CHEATS_H
#define PSX_CHEATS_H

/* Cheats GameShark (cheats.txt) aplicados por VBlank + menu no jogo.
 * Ver src/psx_cheats.cpp. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void psx_cheats_on_vblank(void);
void psx_cheats_reload(void);
int  psx_cheats_count(void);
int  psx_cheats_file_found(void);
int  psx_cheats_toggle(int index, char *msg, int msg_cap);

/* Linha de opção do menu em jogo (aparece antes dos cheats). */
typedef struct PsxMenuOption {
    const char *name_pt, *name_en;   /* ASCII, maiúsculas */
    const char *note_pt, *note_en;
    const char *name_es, *note_es;
    void (*value)(char *out, int cap, int lang); /* valor atual; lang 0=en 1=pt 2=es */
    void (*activate)(char *msg, int cap);        /* X/Enter: troca o valor */
    /* Opcional: miniatura RGBA8 mostrada quando a linha está selecionada
     * (ex.: a moldura atual). Retorna 0 quando não há o que mostrar. */
    int (*preview)(const unsigned char **rgba, int *w, int *h);
} PsxMenuOption;
void psx_cheats_menu_set_options(const PsxMenuOption *opts, int count);
/* 0 = menu de cheats, 1 = menu de opções (mesma tela, listas separadas). */
void psx_cheats_menu_set_mode(int options);
/* Idioma da interface (o do launcher): 0 = inglês, 1 = português, 2 = espanhol. */
int psx_ui_lang(void);
/* Filtro de tela / máscara (0 nenhum, 1 LCD Grid, 2 LCD RGB, 3 CRT,
 * 4 Soft CRT, 5 Dot Matrix) e upscaler (0 nenhum, 1 Sharp Bilinear,
 * 2 Scale2x, 3 Scale3x, 4 xBR); combinam entre si e com as scanlines. */
int psx_screen_filter_load(void);
void psx_screen_filter_save(int mode);
const char *psx_screen_filter_label(int mode, int lang);
int psx_upscale_filter_load(void);
void psx_upscale_filter_save(int mode);
const char *psx_upscale_filter_label(int mode, int lang);

void psx_cheats_menu_set_state(int open, int selected);
int  psx_cheats_menu_selected(void);
int  psx_cheats_menu_needs_present(void);
int  psx_cheats_menu_overlay_image(const uint32_t **pixels, int *w, int *h);

#ifdef __cplusplus
}
#endif

#endif /* PSX_CHEATS_H */
