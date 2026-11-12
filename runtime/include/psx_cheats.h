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

void psx_cheats_menu_set_state(int open, int selected);
int  psx_cheats_menu_selected(void);
int  psx_cheats_menu_needs_present(void);
int  psx_cheats_menu_overlay_image(const uint32_t **pixels, int *w, int *h);

#ifdef __cplusplus
}
#endif

#endif /* PSX_CHEATS_H */
