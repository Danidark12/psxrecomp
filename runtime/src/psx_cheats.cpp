/* psx_cheats.cpp - cheats no formato GameShark (cheats.txt) + menu no jogo.
 *
 * Os códigos são aplicados a cada VBlank do jogo, como um GameShark de
 * verdade faz (ele se pendura no VSync e regrava os valores). A lista é a
 * mesma da aba Cheats do launcher (recomp-ui/src/recomp_cheats.h); ligar ou
 * desligar no menu do jogo regrava cheats.txt na hora.
 *
 * Tipos suportados:
 *   80aaaaaa vvvv  escreve 16 bits          30aaaaaa 00vv  escreve 8 bits
 *   D0/D1/D2/D3    se 16 bits ==/!=/</>      E0/E1/E2/E3    idem, 8 bits
 *                  (a condição vale só para a linha seguinte)
 *   5000nnss iiii  repete a linha seguinte nn vezes, endereço += ss,
 *                  valor += iiii
 *
 * Desligado em netplay (os dois lados precisam rodar o mesmo jogo) e antes
 * de o jogo começar (BIOS / tela de boot).
 */

#include "psx_cheats.h"

#if !defined(RECOMP_LAUNCHER)
/* Alvo montado sem a recomp-ui (sem launcher): não existe leitor de
 * cheats.txt nem aba de cheats, então o recurso fica indisponível — nenhum
 * código é aplicado e o menu mostra só que não há lista. */
extern "C" void psx_cheats_on_vblank(void) {}
extern "C" void psx_cheats_reload(void) {}
extern "C" int  psx_cheats_count(void) { return 0; }
extern "C" int  psx_cheats_file_found(void) { return 0; }
extern "C" int  psx_cheats_toggle(int, char* msg, int msg_cap) {
    if (msg && msg_cap > 0) msg[0] = 0;
    return -1;
}
extern "C" void psx_cheats_menu_set_state(int, int) {}
extern "C" int  psx_cheats_menu_selected(void) { return 0; }
extern "C" int  psx_cheats_menu_needs_present(void) { return 0; }
extern "C" int  psx_cheats_menu_overlay_image(const uint32_t**, int*, int*) { return 0; }
extern "C" void psx_cheats_menu_set_options(const PsxMenuOption*, int) {}
extern "C" void psx_cheats_menu_set_mode(int) {}
extern "C" int  psx_ui_lang(void) { return 0; }
extern "C" int  psx_screen_filter_load(void) { return 0; }
extern "C" void psx_screen_filter_save(int) {}
extern "C" const char* psx_screen_filter_label(int, int) { return "NONE"; }
#else

#include "recomp_cheats.h"
#include "launcher_i18n.h"

#include <stdio.h>
#include <string.h>

extern "C" {
uint16_t psx_read_half(uint32_t addr);
uint8_t  psx_read_byte(uint32_t addr);
void     psx_write_half(uint32_t addr, uint16_t val);
void     psx_write_byte(uint32_t addr, uint8_t val);
int      fntrace_is_game_started(void);
int      psx_netplay_active(void);
}

static RecompCheatList s_list;
static int s_loaded = 0;
static int s_any_enabled = 0;

static void recount(void) {
    s_any_enabled = 0;
    for (int i = 0; i < s_list.count; ++i)
        if (s_list.cheats[i].enabled && s_list.cheats[i].supported)
            s_any_enabled = 1;
}

static void ensure_loaded(void) {
    if (s_loaded) return;
    s_loaded = 1;
    recomp_cheats_load(&s_list);
    recount();
}

extern "C" void psx_cheats_reload(void) {
    s_loaded = 0;
    ensure_loaded();
}

static inline uint32_t ram_addr(uint32_t word) {
    return 0x80000000u | (word & 0x1FFFFFu);
}

static void put16(uint32_t word, uint16_t v) {
    const uint32_t a = ram_addr(word) & ~1u;
    if (psx_read_half(a) != v) psx_write_half(a, v);
}

static void put8(uint32_t word, uint8_t v) {
    const uint32_t a = ram_addr(word);
    if (psx_read_byte(a) != v) psx_write_byte(a, v);
}

static int cond_true(uint32_t word, uint16_t v) {
    const uint32_t type = word >> 24;
    if (type >= 0xD0 && type <= 0xD3) {
        const uint16_t cur = psx_read_half(ram_addr(word) & ~1u);
        switch (type) {
        case 0xD0: return cur == v;
        case 0xD1: return cur != v;
        case 0xD2: return cur < v;
        default:   return cur > v;
        }
    }
    const uint8_t cur = psx_read_byte(ram_addr(word));
    const uint8_t b = (uint8_t)v;
    switch (type) {
    case 0xE0: return cur == b;
    case 0xE1: return cur != b;
    case 0xE2: return cur < b;
    default:   return cur > b;
    }
}

static void run_cheat(const RecompCheat* c) {
    int skip_next = 0;
    for (int i = 0; i < c->code_count; ++i) {
        const uint32_t w = c->addr[i];
        const uint16_t v = c->value[i];
        const uint32_t type = w >> 24;
        if (skip_next) {
            skip_next = 0;
            if (type == 0x50) ++i;   /* a repetição inteira é a "linha" */
            continue;
        }
        switch (type) {
        case 0x80: put16(w, v); break;
        case 0x30: put8(w, (uint8_t)v); break;
        case 0xD0: case 0xD1: case 0xD2: case 0xD3:
        case 0xE0: case 0xE1: case 0xE2: case 0xE3:
            if (!cond_true(w, v)) skip_next = 1;
            break;
        case 0x50: {
            if (i + 1 >= c->code_count) return;
            const uint32_t count = (w >> 8) & 0xFFu;
            const uint32_t step = w & 0xFFu;
            uint32_t tw = c->addr[i + 1];
            uint16_t tv = c->value[i + 1];
            const uint32_t ttype = tw >> 24;
            for (uint32_t k = 0; k < count; ++k) {
                if (ttype == 0x30) put8(tw, (uint8_t)tv);
                else put16(tw, tv);
                tw = (tw & 0xFF000000u) | ((tw + step) & 0x00FFFFFFu);
                tv = (uint16_t)(tv + v);
            }
            ++i;
            break;
        }
        default:
            return;   /* tipo desconhecido: o cheat já vem marcado como não suportado */
        }
    }
}

extern "C" void psx_cheats_on_vblank(void) {
    ensure_loaded();
    if (!s_any_enabled) return;
    if (psx_netplay_active()) return;
    if (!fntrace_is_game_started()) return;
    for (int i = 0; i < s_list.count; ++i) {
        const RecompCheat* c = &s_list.cheats[i];
        if (c->enabled && c->supported) run_cheat(c);
    }
}

/* Opções do jogo (vídeo etc.) mostradas antes dos cheats no menu. Quem as
 * fornece é o main.cpp; aqui só desenhamos e repassamos o "ativar". */
static const PsxMenuOption* s_opts = nullptr;
static int s_nopt_all = 0;
static int s_mode = 0;          /* 0 = menu de cheats, 1 = menu de opções */
static int s_mode_dirty = 0;
#define s_nopt   (s_mode ? s_nopt_all : 0)
#define s_ncheat (s_mode ? 0 : s_list.count)

extern "C" void psx_cheats_menu_set_mode(int options) {
    options = options ? 1 : 0;
    if (options != s_mode) s_mode_dirty = 1;
    s_mode = options;
}

extern "C" void psx_cheats_menu_set_options(const PsxMenuOption* opts, int n) {
    s_opts = opts;
    s_nopt_all = (opts && n > 0) ? n : 0;
}

/* Total de linhas do menu: opções + cheats. */
extern "C" int psx_cheats_count(void) {
    ensure_loaded();
    return s_nopt + s_ncheat;
}

extern "C" int psx_cheats_file_found(void) {
    ensure_loaded();
    return s_list.loaded;
}

/* Idioma do menu: 0 = inglês, 1 = português, 2 = espanhol (o do launcher). */
static int ui_lang(void) {
    return recomp_cheat_lang_from_code(launcher_i18n_language());
}
extern "C" int psx_ui_lang(void) { return ui_lang(); }
extern "C" int psx_screen_filter_load(void) { return recomp_screen_filter_load(); }
extern "C" void psx_screen_filter_save(int mode) { recomp_screen_filter_save(mode); }
extern "C" const char* psx_screen_filter_label(int mode, int lang) {
    return recomp_screen_filter_name_upper(mode, lang);
}
static const char* tr(int lang, const char* en, const char* pt, const char* es) {
    return lang == 1 ? pt : lang == 2 ? es : en;
}

/* ------------------------------------------------------------------ menu */
#define CM_W 640
#define CM_H 480
#define CM_ROW_H 26
#define CM_ROWS_Y 64
#define CM_VISIBLE 11

/* font8x8_basic (domínio público), ASCII 32..90 — o mesmo do menu de saves */
static const uint8_t FONT8[59][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00},
    {0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00}, {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00},
    {0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00}, {0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00},
    {0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00}, {0x06,0x06,0x03,0x00,0x00,0x00,0x00,0x00},
    {0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0x00}, {0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0x00},
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, {0x00,0x0C,0x0C,0x3F,0x0C,0x0C,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x06}, {0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00}, {0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00},
    {0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00}, {0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0x00},
    {0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0x00}, {0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0x00},
    {0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0x00}, {0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0x00},
    {0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0x00}, {0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x00},
    {0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0x00}, {0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0x00},
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x00}, {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x06},
    {0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0x00}, {0x00,0x00,0x3F,0x00,0x00,0x3F,0x00,0x00},
    {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00}, {0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0x00},
    {0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0x00}, {0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00},
    {0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0x00}, {0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0x00},
    {0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0x00}, {0x7F,0x06,0x06,0x3E,0x06,0x06,0x7F,0x00},
    {0x7F,0x06,0x06,0x3E,0x06,0x06,0x06,0x00}, {0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0x00},
    {0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0x00}, {0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00},
    {0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0x00}, {0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0x00},
    {0x06,0x06,0x06,0x06,0x06,0x06,0x7F,0x00}, {0x63,0x77,0x7F,0x7F,0x6B,0x63,0x63,0x00},
    {0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00}, {0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00},
    {0x3F,0x66,0x66,0x3E,0x06,0x06,0x06,0x00}, {0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0x00},
    {0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0x00}, {0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0x00},
    {0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, {0x33,0x33,0x33,0x33,0x33,0x33,0x3F,0x00},
    {0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0x00}, {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00},
    {0x63,0x63,0x36,0x1C,0x1C,0x36,0x63,0x00}, {0x33,0x33,0x33,0x1E,0x0C,0x0C,0x1E,0x00},
    {0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0x00},
};

static int s_open = 0;
static int s_sel = 0;
static int s_dirty = 1;
static uint32_t s_panel[CM_W * CM_H];

static void fill_rect(int x0, int y0, int w, int h, uint32_t col) {
    if (x0 < 0) { w += x0; x0 = 0; }
    if (y0 < 0) { h += y0; y0 = 0; }
    if (x0 + w > CM_W) w = CM_W - x0;
    if (y0 + h > CM_H) h = CM_H - y0;
    if (w <= 0 || h <= 0) return;
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++)
            s_panel[y * CM_W + x] = col;
}

static void stroke_rect(int x, int y, int w, int h, uint32_t col) {
    fill_rect(x, y, w, 1, col);
    fill_rect(x, y + h - 1, w, 1, col);
    fill_rect(x, y, 1, h, col);
    fill_rect(x + w - 1, y, 1, h, col);
}

static void draw_char(int x0, int y0, char c, uint32_t col, int scale) {
    if (c >= 'a' && c <= 'z') c = (char)(c - 32);
    if (c < 32 || c > 90) c = '?';
    const uint8_t* g = FONT8[(int)c - 32];
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
            if ((g[y] & (1u << x)) == 0) continue;
            fill_rect(x0 + x * scale, y0 + y * scale, scale, scale, col);
        }
}

/* UTF-8 -> ASCII maiúsculo (a fonte do menu não tem acentos). */
static void to_ascii(const char* s, char* out, size_t cap) {
    size_t n = 0;
    const unsigned char* p = (const unsigned char*)s;
    while (*p && n + 1 < cap) {
        unsigned char c = *p;
        if (c < 0x80) {
            out[n++] = (char)c;
            p++;
            continue;
        }
        if (c == 0xC3 && p[1]) {
            static const char map[] =   /* U+00C0..U+00FF */
                "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";
            unsigned char d = p[1];
            if (d >= 0x80 && d <= 0xBF) out[n++] = map[d - 0x80];
            p += 2;
            continue;
        }
        /* outros caracteres multibyte: pula a sequência inteira */
        p++;
        while ((*p & 0xC0) == 0x80) p++;
        out[n++] = '?';
    }
    out[n] = 0;
}

static int draw_text(int x, int y, const char* s, uint32_t col, int scale, int max_chars) {
    char buf[512];
    to_ascii(s ? s : "", buf, sizeof(buf));
    int i = 0;
    for (; buf[i] && (max_chars <= 0 || i < max_chars); ++i)
        draw_char(x + i * 8 * scale, y, buf[i], col, scale);
    return i;
}

/* quebra em linhas de até `width` caracteres; devolve quantas desenhou */
static int draw_wrapped(int x, int y, const char* s, uint32_t col, int width, int max_lines) {
    char buf[1024];
    to_ascii(s ? s : "", buf, sizeof(buf));
    int lines = 0;
    const char* p = buf;
    while (*p && lines < max_lines) {
        int len = (int)strlen(p);
        int take = len;
        if (take > width) {
            take = width;
            while (take > 0 && p[take] != ' ') take--;
            if (take == 0) take = width;
        }
        for (int i = 0; i < take; ++i)
            draw_char(x + i * 8, y + lines * 12, p[i], col, 1);
        p += take;
        while (*p == ' ') p++;
        lines++;
    }
    return lines;
}

static void rasterize(void) {
    const int lang = ui_lang();
    char buf[160];
    const int total = s_nopt + s_ncheat;
    s_mode_dirty = 0;
    for (int i = 0; i < CM_W * CM_H; i++) s_panel[i] = 0xFF0F1118u;
    fill_rect(0, 0, CM_W, 46, 0xFF171B25u);
    draw_text(24, 14, s_mode ? tr(lang, "OPTIONS", "OPCOES", "OPCIONES")
                             : tr(lang, "CHEATS", "CHEATS", "TRUCOS"), 0xFFFFD24Du, 2, 0);
    draw_text(s_mode ? 440 : 480, 18,
              s_mode ? tr(lang, "F4 / SELECT+SQUARE", "F4 / SELECT+QUADRADO", "F4 / SELECT+CUADRADO")
                     : "F5 / SELECT+L3",
              0xFFB8BDC8u, 1, 0);

    if (total == 0) {
        draw_wrapped(28, 80, s_list.loaded
                     ? tr(lang, "CHEATS.TXT HAS NO CHEATS.", "O ARQUIVO CHEATS.TXT NAO TEM NENHUM CHEAT.",
                          "CHEATS.TXT NO TIENE TRUCOS.")
                     : tr(lang, "CHEATS.TXT NOT FOUND NEXT TO THE GAME.", "CHEATS.TXT NAO ENCONTRADO NA PASTA DO JOGO.",
                          "NO SE ENCONTRO CHEATS.TXT JUNTO AL JUEGO."),
                     0xFFE2E5EBu, 70, 3);
    } else {
        int first = s_sel - CM_VISIBLE / 2;
        if (first > total - CM_VISIBLE) first = total - CM_VISIBLE;
        if (first < 0) first = 0;
        snprintf(buf, sizeof(buf), "%d / %d", s_sel + 1, total);
        draw_text(24, 50, buf, 0xFF7F8796u, 1, 0);
        for (int i = first; i < total && i < first + CM_VISIBLE; ++i) {
            const int y = CM_ROWS_Y + (i - first) * CM_ROW_H;
            const int sel = (i == s_sel);
            fill_rect(20, y, 600, CM_ROW_H - 4, sel ? 0xFF2B2830u : 0xFF191D27u);
            stroke_rect(20, y, 600, CM_ROW_H - 4, sel ? 0xFFFFD24Du : 0xFF303746u);
            if (i < s_nopt) {
                const PsxMenuOption* o = &s_opts[i];
                char val[48];
                val[0] = 0;
                if (o->value) o->value(val, (int)sizeof(val), lang);
                draw_text(32, y + 7, tr(lang, o->name_en, o->name_pt, o->name_es),
                          sel ? 0xFFFFD24Du : 0xFFE2E5EBu, 1, 44);
                char a[48];
                to_ascii(val, a, sizeof(a));
                int len = (int)strlen(a);
                draw_text(608 - len * 8, y + 7, a, 0xFF7FD4FFu, 1, 0);
                continue;
            }
            const RecompCheat* c = &s_list.cheats[i - s_nopt];
            /* caixinha */
            stroke_rect(30, y + 5, 12, 12, 0xFFB8BDC8u);
            if (c->enabled) fill_rect(33, y + 8, 6, 6, 0xFF6BE38Bu);
            uint32_t fg = !c->supported ? 0xFF6A7080u : sel ? 0xFFFFD24Du : 0xFFE2E5EBu;
            draw_text(52, y + 7, recomp_cheat_name(c, lang), fg, 1, 68);
        }
        /* descrição do selecionado */
        fill_rect(0, 352, CM_W, 58, 0xFF141822u);
        if (s_sel < s_nopt) {
            const PsxMenuOption* o = &s_opts[s_sel];
            draw_wrapped(24, 358, tr(lang, o->note_en, o->note_pt, o->note_es), 0xFFB8BDC8u, 74, 4);
        } else {
            const RecompCheat* c = &s_list.cheats[s_sel - s_nopt];
            int used = 0;
            if (!c->supported)
                used = draw_wrapped(24, 358, tr(lang, "UNSUPPORTED CODE TYPE - WILL NOT BE APPLIED.",
                                                "TIPO DE CODIGO NAO SUPORTADO - NAO SERA APLICADO.",
                                                "TIPO DE CODIGO NO COMPATIBLE - NO SE APLICARA."),
                                    0xFFFF8A65u, 74, 1);
            draw_wrapped(24, 358 + used * 12, recomp_cheat_note(c, lang), 0xFFB8BDC8u, 74, 4 - used);
        }
    }

    fill_rect(0, 410, CM_W, 70, 0xFF171B25u);
    draw_text(32, 426, tr(lang, "UP/DOWN: SELECT   X/ENTER: CHANGE   O/ESC: CLOSE",
                              "CIMA/BAIXO: ESCOLHER   X/ENTER: MUDAR   O/ESC: SAIR",
                              "ARRIBA/ABAJO: ELEGIR   X/ENTER: CAMBIAR   O/ESC: SALIR"),
              0xFFE2E5EBu, 1, 0);
    draw_text(32, 450, s_mode ? tr(lang, "CHANGES APPLY IMMEDIATELY AND ARE SAVED.",
                                       "AS MUDANCAS VALEM NA HORA E FICAM SALVAS.",
                                       "LOS CAMBIOS SE APLICAN AL INSTANTE Y SE GUARDAN.")
                              : tr(lang, "ENABLED CHEATS APPLY EVERY FRAME. CHOICES ARE SAVED.",
                                       "OS CHEATS LIGADOS VALEM A CADA QUADRO. A ESCOLHA FICA SALVA.",
                                       "LOS TRUCOS ACTIVOS SE APLICAN EN CADA FOTOGRAMA. LA ELECCION SE GUARDA."),
              0xFF7F8796u, 1, 0);
    s_dirty = 0;
}

extern "C" void psx_cheats_menu_set_state(int open, int selected) {
    ensure_loaded();
    if (s_nopt + s_ncheat > 0) {
        while (selected < 0) selected += s_nopt + s_ncheat;
        selected %= s_nopt + s_ncheat;
    } else {
        selected = 0;
    }
    if (s_open != (open ? 1 : 0) || s_sel != selected || s_mode_dirty) s_dirty = 1;
    if (open && !s_open) {   /* recarrega ao abrir: pega edições do arquivo */
        int keep = selected;
        psx_cheats_reload();
        if (keep >= s_nopt + s_ncheat) keep = 0;
        selected = keep;
        s_dirty = 1;
    }
    s_open = open ? 1 : 0;
    s_sel = selected;
}

extern "C" int psx_cheats_menu_selected(void) { return s_sel; }

/* Liga/desliga o cheat selecionado. Retorna o novo estado (ou -1). Grava em
 * cheats.txt e escreve em msg uma mensagem curta para a tela. */
extern "C" int psx_cheats_toggle(int index, char* msg, int msg_cap) {
    ensure_loaded();
    if (index >= 0 && index < s_nopt) {
        if (msg && msg_cap > 0) msg[0] = 0;
        if (s_opts[index].activate) s_opts[index].activate(msg, msg_cap);
        s_dirty = 1;
        return 1;
    }
    index -= s_nopt;
    if (index < 0 || index >= s_ncheat) return -1;
    RecompCheat* c = &s_list.cheats[index];
    const int lang = ui_lang();
    if (!c->supported) {
        if (msg) snprintf(msg, msg_cap, "%s",
                          tr(lang, "Cheat not supported", "Cheat nao suportado", "Truco no compatible"));
        return -1;
    }
    c->enabled = !c->enabled;
    recount();
    recomp_cheats_save(&s_list);
    s_dirty = 1;
    if (msg) {
        char name[96];
        to_ascii(recomp_cheat_name(c, lang), name, sizeof(name));
        snprintf(msg, msg_cap, "%s: %s", name,
                 c->enabled ? tr(lang, "on", "ligado", "activado")
                            : tr(lang, "off", "desligado", "desactivado"));
    }
    return c->enabled;
}

extern "C" int psx_cheats_menu_needs_present(void) { return s_open; }

extern "C" int psx_cheats_menu_overlay_image(const uint32_t** pixels, int* w, int* h) {
    if (!s_open) return 0;
    if (s_dirty) rasterize();
    if (pixels) *pixels = s_panel;
    if (w) *w = CM_W;
    if (h) *h = CM_H;
    return 1;
}

#endif /* RECOMP_LAUNCHER */

/* Abre o launcher num processo novo (o chamador encerra o jogo em seguida).
 * Fica fora do #if RECOMP_LAUNCHER: sem launcher o processo novo só inicia o
 * jogo de novo. */
#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <wchar.h>
extern "C" int psx_relaunch_launcher(void) {
    wchar_t exe[2048];
    DWORD n = GetModuleFileNameW(NULL, exe, 2048);
    if (n == 0 || n >= 2048) return 0;
    wchar_t cmd[2200];
    _snwprintf(cmd, 2200, L"\"%ls\" --launcher", exe);
    cmd[2199] = 0;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    if (!CreateProcessW(exe, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        return 0;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 1;
}
#else
#  include <unistd.h>
extern "C" int psx_relaunch_launcher(void) {
    char exe[4096];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (n <= 0) return 0;
    exe[n] = 0;
    pid_t pid = fork();
    if (pid < 0) return 0;
    if (pid == 0) {
        execl(exe, exe, "--launcher", (char*)NULL);
        _exit(127);
    }
    return 1;
}
#endif
