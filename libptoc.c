#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <jpeglib.h>

#include "config.h"
#include "libptoc.h"

#define USE_FAST_RGB_CACHE      0
#define USE_NEAREST_CONVEX      1

#define LEVELS          5
#define MAX_COLORS	16

static unsigned char *map_irssi[] = {
    "\xff\xff\xff", "\x00\x00\x00", "\x00\x00\xbb", "\x00\xbb\x00",
    "\xff\x55\x55", "\xbb\x00\x00", "\xbb\x00\xbb", "\xbb\xbb\x00",
    "\xff\xff\x55", "\x55\xff\x55", "\x00\xbb\xbb", "\x55\xff\xff",
    "\x55\x55\xff", "\xff\x55\xff", "\x55\x55\x55", "\xbb\xbb\xbb"
};

static unsigned char *map_mirc[] = {
    "\xff\xff\xff", "\x00\x00\x00", "\x00\x00\x7f", "\x00\x93\x00",
    "\xff\x00\x00", "\x7f\x00\x00", "\x9c\x00\x9c", "\xfc\x7f\x00",
    "\xff\xff\x00", "\x00\xfc\x00", "\x00\x93\x93", "\x00\xff\xff",
    "\x00\x00\xfc", "\xff\x00\xff", "\x7f\x7f\x7f", "\xd2\xd2\xd2"
};

static unsigned char *map_ansi[] = {
    "\x00\x00\x00", "\xbb\x00\x00", "\x00\xbb\x00", "\xbb\xbb\x00",
    "\x00\x00\xbb", "\xbb\x00\xbb", "\x00\xbb\xbb", "\xbb\xbb\xbb",
    "\x55\x55\x55", "\xff\x55\x55", "\x55\xff\x55", "\xff\xff\x55",
    "\x55\x55\xff", "\xff\x55\xff", "\x55\xff\xff", "\xff\xff\xff"
};

static unsigned char *map_safeansi[] = {
    "\x00\x00\x00", "\xff\x00\x00", "\x00\xff\x00", "\xff\xff\x00",
    "\x00\x00\xff", "\xff\x00\xff", "\x00\xff\xff", "\xff\xff\xff",
};

static unsigned char **map_bitchx = map_ansi;
static unsigned char **map_ansiplus = map_ansi;
static unsigned char **map_silc = map_irssi;

static const int bgcount_ansi = 8;
static const int bgcount_safeansi = 8;
static const int bgcount_ansiplus = 16;
static const int bgcount_mirc = 16;
static const int bgcount_silc = 16;
static const int bgcount_irssi = 16;
static const int bgcount_bitchx = 8;

static const int fgcount_ansi = 16;
static const int fgcount_ansiplus = 16;
static const int fgcount_safeansi = 8;
static const int fgcount_mirc = 16;
static const int fgcount_silc = 16;
static const int fgcount_irssi = 16;
static const int fgcount_bitchx = 16;

static int bgcount = 0;
static int fgcount = 0;

enum ptoc_client_t current_client = cli_none;

typedef const char *format_func_t(int fg, int bg, int grad);

static const char *format_for_irc(int fg, int bg, int grad);
static const char *format_for_ansi(int fg, int bg, int grad);

static format_func_t *format_safeansi = format_for_ansi;
static format_func_t *format_ansiplus = format_for_ansi;
static format_func_t *format_ansi = format_for_ansi;
static format_func_t *format_mirc = format_for_irc;
static format_func_t *format_silc = format_for_irc;
static format_func_t *format_irssi = format_for_irc;
static format_func_t *format_bitchx = format_for_ansi;

static format_func_t *current_format = NULL;

static int format_state_fg = -1;
static int format_state_bg = -1;

static const int fadelvl_fixedsys[] = { 0, 6, 28, 36, 47 };
static const int fadelvl_terminal[] = { 0, 6, 24, 34, 40 };
static const int fadelvl_courier[] = { 0, 4, 16, 18, 26 };
static const int fadelvl_average[] = { 0, 5, 23, 29, 38 };

static const int fademax_courier = 128;
static const int fademax_terminal = 96;
static const int fademax_fixedsys = 120;
static const int fademax_average = 115;

static const double aspect_courier = 2.0;
static const double aspect_terminal = 1.5;
static const double aspect_fixedsys = 1.875;
static const double aspect_average = 1.7197;

static int ansi2rgb[MAX_COLORS][MAX_COLORS][LEVELS][3];

/*
 * USE_NEAREST_CONVEX
 * 
 * if more than one ansi bg/fg+gradient combination makes a certain color then
 * use the one with the bg and fg being the least squares minimum distance
 * from each other in RGB and also avoid using fg+bg combos with too great an
 * RGB square distance from each other
 */

#if USE_NEAREST_CONVEX
static long ansicdelta[MAX_COLORS][MAX_COLORS];
#endif

/*
 * USE_FAST_RGB_CACHE
 * 
 * the rgb fast caching sacrafices 50% color depth reducing it down to 12-bit
 * RGB to ANSI but since the ansi color map is relatively small being 640
 * non-unique colors (some collide) which is barely over 8-bit color a color
 * lookup caching is used to speed up rgb to ansi conversion by a shitload
 */

#if USE_FAST_RGB_CACHE
#define FRROR        4
#define FRROL        (8 - FRROR)
#define FRCOUNT      (1 << FRROL)
#define FRI(a)       ( ( ((a) < 0) ? 0 : MIN(a, 255) ) >> FRROR)
#define FRS(a)       ((a) << FRROR)

enum fastflags
{
    frflag, frbg, frfg, frgrad
};

static int fastrtoa[FRCOUNT][FRCOUNT][FRCOUNT][4];
#endif

unsigned char *
ptoc_jpegtorgb(const char *fn, int *xp, int *yp)
{
    const int target_components = 3;
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    FILE *fp;

    JSAMPARRAY buffer;

    unsigned char *rgb;

    int x, y, components;

    int i, j, k;

    if (fn == NULL || strcmp(fn, "-") == 0) {

        fp = stdin;

    } else {

        fp = fopen(fn, "rb");

        if (fp == NULL) {
            perror("fopen()");
            exit(EXIT_FAILURE);
        }

    }

    cinfo.err = jpeg_std_error(&jerr);

    jpeg_create_decompress(&cinfo);

    jpeg_stdio_src(&cinfo, fp);

    jpeg_read_header(&cinfo, TRUE);

    jpeg_start_decompress(&cinfo);

    /* Compute the new x and y ratios. */
    /* fixprintsize(sugx, sugy, cinfo->output_width, cinfo->output_height); */

    x = cinfo.image_width;
    y = cinfo.image_height;

    components = cinfo.output_components;

    if (components != target_components && components != 1) {
        fprintf(stderr, "the number of components per pixel are %i\n", cinfo.output_components);
        fprintf(stderr,
                "since this isnt %i for RGB or 1 for greyscale im not going to fuck with this shit\n",
                target_components);

        exit(EXIT_FAILURE);
    }

    rgb = malloc(x * y * target_components);
    if (rgb == NULL) {
        perror("malloc()");
        exit(EXIT_FAILURE);
    }

    /*
     *
     * load the actual image as RGB triplets
     *
     */

    /*
     *
     * this alloc seems like a fucking mem leak but wtf is the free function????????
     *
     */

    buffer = (*cinfo.mem->alloc_sarray) ((j_common_ptr) & cinfo, JPOOL_IMAGE, x * components, 1);

    for (j = 0; j < y; j++) {

        unsigned char *p;

        (void)jpeg_read_scanlines(&cinfo, buffer, 1);

        p = rgb + (j * x * target_components);

        for (i = 0; i < x; i++) {
            for (k = 0; k < target_components; k++) {
                if (components == target_components)
                    p[i * target_components + k] = buffer[0][i * target_components + k];
                else if (components == 1)
                    p[i * target_components + k] = buffer[0][i];
            }
        }
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    if (fp != stdin)
        fclose(fp);

    *xp = x;
    *yp = y;

    return (rgb);
}

enum ptoc_client_t
ptoc_get_client(const char *s)
{
#define TEST_CLIENT(a,str)	if(strcasecmp(str,#a) == 0) return(cli_##a)
    TEST_CLIENT(silc, s);
    TEST_CLIENT(mirc, s);
    TEST_CLIENT(ansi, s);
    TEST_CLIENT(ansiplus, s);
    TEST_CLIENT(safeansi, s);
    TEST_CLIENT(irssi, s);
    TEST_CLIENT(bitchx, s);
    return (cli_none);
#undef TEST_CLIENT
}

enum ptoc_font_t
ptoc_get_font(const char *s)
{
#define TEST_FONT(a,str)	if(strcasecmp(str,#a) == 0) return(font_##a)
    TEST_FONT(terminal, s);
    TEST_FONT(fixedsys, s);
    TEST_FONT(courier, s);
    TEST_FONT(average, s);
    return (font_none);
#undef TEST_FONT
}

double
ptoc_get_aspect(enum ptoc_font_t font)
{
#define GET_ASPECT(a)		if(font == font_##a) return(aspect_##a)
    GET_ASPECT(courier);
    GET_ASPECT(terminal);
    GET_ASPECT(fixedsys);
    GET_ASPECT(average);
    return (1.0);
#undef GET_ASPECT
}

/*
static ssize_t
read2(int fd, void *buf, size_t count)
{
    size_t nleft, nread;
    ssize_t n;

    nleft = count;
    nread = 0;

    do {
        n = read(fd, (char *)buf + nread, nleft);
        if (n == 0)
            return (nread);
        else if (n == -1)
            return (-1);

        nread += n;
        nleft -= n;
    } while (nleft);

    return (nread);

}
*/

const char *
ptoc_geteol()
{
    if (current_format == format_for_ansi) {
        return ("\33[0m\n");
    } else if (current_format == format_for_irc) {
        return ("\n");
    } else {
        return ("\n");
    }
}

static const char __gradbyte[] = " .oO@";

#define gradbyte(a) __gradbyte[a]

/*
 * convert ANSI fg,bg + gradient mapping to actual screen representation with
 * a run length type of optimized encoding method so you dont have to do an
 * ESC+[ every character
 */

static const char *
format_for_ansi(int fg, int bg, int grad)
{

    static char ansi[64];

    int bold, plus, fgansi, bgansi;

    int used;

    if ((fg < 0 || fg >= fgcount) || (bg < 0 || bg >= bgcount) || (grad < 0 || grad >= LEVELS)) {

        format_state_bg = -1;
        format_state_fg = -1;

        return (NULL);
    }
#define ISBOLD(a)	((a) >= 8)
#define CMASK(a)	((a) & 7)

    bold = ISBOLD(fg);
    plus = ISBOLD(bg) ? 5 : 0;

    fgansi = 30 + CMASK(fg);
    bgansi = 40 + CMASK(bg);

    strcpy(ansi, "\33[0;");

    if (bold)
        strcat(ansi, "1;");
    if (plus)
        strcat(ansi, "5;");

    used = strlen(ansi);

    snprintf(ansi + used, sizeof(ansi) - used, "%i;%im%c", fgansi, bgansi, gradbyte(grad));

#undef ISBOLD
#undef CMASK

    format_state_bg = bg;
    format_state_fg = fg;

    return (ansi);
}

static const char *
format_for_irc(int fg, int bg, int grad)
{
    static char expr[32];

    if ((fg < 0 || fg >= fgcount) || (bg < 0 || bg >= bgcount) || (grad < 0 || grad >= LEVELS)) {

        format_state_bg = -1;
        format_state_fg = -1;

        return (NULL);
    }

    if (format_state_bg == bg && format_state_fg == fg) {

        snprintf(expr, sizeof(expr), "%c", gradbyte(grad));

    } else {

        snprintf(expr, sizeof(expr), "\3%i,%i%c", fg, bg, gradbyte(grad));

    }

    format_state_bg = bg;
    format_state_fg = fg;

    return (expr);
}

/*
 * initialize the rgb to ansi mapping api
 */

void
ptoc_init(enum ptoc_client_t cli, enum ptoc_font_t font, enum ptoc_term_t term)
{

    const int *fadelvl;
    int fademax;

    unsigned char **color_map;

    int bg, fg, grad, rgrad, *rgb;

#define R(a)	(color_map[(a)][0])
#define G(a)	(color_map[(a)][1])
#define B(a)	(color_map[(a)][2])

#define SET_CLI(a)\
	case cli_##a:\
		color_map = map_##a;\
    		current_format = format_##a;\
    		fgcount = fgcount_##a;\
    		bgcount = bgcount_##a;\
    		break

    current_client = cli;

    switch (cli) {

        SET_CLI(irssi);
        SET_CLI(silc);
        SET_CLI(bitchx);
        SET_CLI(mirc);
        SET_CLI(ansi);
        SET_CLI(safeansi);
        SET_CLI(ansiplus);

    default:

        puts("you fail bigtime, unknown client type");
        exit(EXIT_FAILURE);
        break;

    }

#undef SET_MAP_IF

#define SET_FONT(a)\
    	case font_##a:\
		fadelvl = fadelvl_##a;\
    		fademax = fademax_##a;\
    		break

    switch (font) {

        SET_FONT(courier);
        SET_FONT(terminal);
        SET_FONT(fixedsys);
        SET_FONT(average);

    default:

        puts("you fail bigtime, unknown font type");
        exit(EXIT_FAILURE);
        break;

    }

#undef SET_FONT

#if USE_FAST_RGB_CACHE
    memset(fastrtoa, 0, sizeof(fastrtoa));
#endif

    for (bg = bgcount; bg--;) {
        for (fg = fgcount; fg--;) {

#if USE_NEAREST_CONVEX
            ansicdelta[bg][fg] = (R(bg) - R(fg)) * (R(bg) - R(fg));
            ansicdelta[bg][fg] += (G(bg) - G(fg)) * (G(bg) - G(fg));
            ansicdelta[bg][fg] += (B(bg) - B(fg)) * (B(bg) - B(fg));
#endif

            for (grad = LEVELS; grad--;) {

                rgrad = LEVELS - grad;

                rgb = ansi2rgb[bg][fg][grad];

                /*
                 *
                 * calculate rgb mappings for given fg/bg/grad
                 *
                 */

                rgb[0] = (R(bg) * (fademax - fadelvl[grad]) + R(fg) * fadelvl[grad]) / fademax;
                rgb[1] = (G(bg) * (fademax - fadelvl[grad]) + G(fg) * fadelvl[grad]) / fademax;
                rgb[2] = (B(bg) * (fademax - fadelvl[grad]) + B(fg) * fadelvl[grad]) / fademax;
            }
        }
    }

#undef R
#undef G
#undef B

}

/*
 * resets the rgb to ansi api so if you wanna start a new ansi image
 */

void
ptoc_newimage(void)
{
    (void)(*current_format) (-1, -1, -1);
}

/*
 * this does the actual rgb to ansi mapping and saves the mapped rgb value
 * out to outpixel which should be a pointer to 3 integers and it returns the
 * ansi screen representation of that color
 */

const char *
ptoc_rgbtochat(int r, int g, int b, const int **outpixel)
{

    long bdelta = LONG_MAX;
    long delta;
    int fg, bg, grad;
    int fgb = -1, bgb = -1, gradb = -1;
    int *rgb;

#if USE_NEAREST_CONVEX
    long bcdelta = LONG_MAX, cdelta;
#endif

#if USE_FAST_RGB_CACHE
    int *fr_entry;

    fr_entry = fastrtoa[FRI(r)][FRI(g)][FRI(b)];

    if (fr_entry[frflag]) {

        bg = fr_entry[frbg];
        fg = fr_entry[frfg];

        grad = fr_entry[frgrad];

        *outpixel = ansi2rgb[bg][fg][grad];

        return ((*current_format) (fg, bg, grad));
    }
#endif

    for (fg = fgcount; fg--;) {
        for (bg = bgcount; bg--;) {

            for (grad = LEVELS; grad--;) {

#if USE_NEAREST_CONVEX
                cdelta = ansicdelta[bg][fg];
                if (cdelta >= (1 << 17))
                    continue;
#endif

                rgb = ansi2rgb[bg][fg][grad];

                delta = (rgb[0] - r) * (rgb[0] - r);
                delta += (rgb[1] - g) * (rgb[1] - g);
                delta += (rgb[2] - b) * (rgb[2] - b);

                if (delta > bdelta)
                    continue;

#if USE_NEAREST_CONVEX
                if (delta == bdelta && cdelta >= bcdelta)
                    continue;
#endif

                bdelta = delta;
                fgb = fg;
                bgb = bg;
                gradb = grad;
                *outpixel = rgb;
#if USE_NEAREST_CONVEX
                bcdelta = cdelta;
#endif
            }
        }
    }

#if USE_FAST_RGB_CACHE

#if USE_CLEAN_BG
    if (fgb == bgb)
        gradb = 0;
#endif

    fr_entry[frflag] = 1;
    fr_entry[frbg] = bgb;
    fr_entry[frfg] = fgb;
    fr_entry[frgrad] = gradb;

#endif

    return ((*current_format) (fgb, bgb, gradb));
}
