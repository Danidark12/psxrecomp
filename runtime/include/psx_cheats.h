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
} PsxMenuOption;
void psx_cheats_menu_set_options(const PsxMenuOption *opts, int count);
/* 0 = menu de cheats, 1 = menu de opções (mesma tela, listas separadas). */
void psx_cheats_menu_set_mode(int options);
/* Idioma da interface (o do launcher): 0 = inglês, 1 = português, 2 = espanhol. */
int psx_ui_lang(void);

void psx_cheats_menu_set_state(int open, int selected);
int  psx_cheats_menu_selected(void);
int  psx_cheats_menu_needs_present(void);
int  psx_cheats_menu_overlay_image(const uint32_t **pixels, int *w, int *h);

#ifdef __cplusplus
}
#endif

#endif /* PSX_CHEATS_H */
