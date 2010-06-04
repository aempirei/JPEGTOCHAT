#ifndef PTOC_H
#define PTOC_H

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

enum ptoc_client_t
{
    cli_none,
    cli_irssi,
    cli_mirc,
    cli_bitchx,
    cli_silc,
    cli_ansi,
    cli_safeansi,
    cli_ansiplus
};

enum ptoc_font_t
{
    font_none,
    font_courier,
    font_fixedsys,
    font_terminal,
    font_average
};

enum ptoc_term_t
{
    term_none,
    term_mirc,
    term_xterm,
    term_apple,
    term_gnome,
    term_putty,
    term_console
};

void ptoc_init(enum ptoc_client_t cli, enum ptoc_font_t font, enum ptoc_term_t term);
void ptoc_newimage(void);

const char *ptoc_rgbtochat(int r, int g, int b, const int **outpixel);

enum ptoc_client_t ptoc_get_client(const char *s);

enum ptoc_font_t ptoc_get_font(const char *s);

double ptoc_get_aspect(enum ptoc_font_t font);

const char *ptoc_geteol();

unsigned char *ptoc_jpegtorgb(const char *fn, int *xp, int *yp);

#endif
