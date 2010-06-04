#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>

#include "config.h"
#include "libptoc.h"

#define RANDOM_MOD	48

#define DI_MAX		(255+8)
#define DI_MIN		(-8)

enum dither_mode
{ dit_none, dit_random, dit_diffusion };

unsigned char *image_resize(unsigned char *rgb, int ox, int oy, int nx, int ny);

typedef struct
{
    int pause;
    int newx;
    int newy;
    int resize;

    enum ptoc_client_t client;
    enum ptoc_font_t font;
    enum ptoc_term_t term;
    enum dither_mode dither;
} config_t;

void
usage(const char *arg)
{
    putchar('\n');

    /*
     * printf("usage: %s -c client [-t term] [-f font] [-d dither_mode] [filename]\n\n", arg);
     */

    printf
        ("usage: %s -c client [-f font] [-d dither_mode] [-x width] [-y height] [-w sec] [filename]\n\n",
         arg);

    puts("\t-c (silc|irssi|mirc|ansi|safeansi|ansiplus|bitchx)");

    /*
     * puts("\t-t (xterm|putty|console|gnome|apple|mirc)");
     */

    puts("\t-f (courier|terminal|fixedsys|average) [default: courier]");
    puts("\t-d (none|random|diffusion) [default: diffusion]");
    puts("\t-x width");
    puts("\t-y height");
    puts("\t-w sec\tsleep in seconds per line [default: 0]");

    putchar('\n');

    printf("\tif no filename is specified, then %s will take input from stdin\n", arg);

    putchar('\n');
}

int
set_config_opt(int ch, const char *param, config_t * config)
{
    switch (ch) {

    case 'd':

        if (strcasecmp(param, "random") == 0)
            config->dither = dit_random;
        else if (strcasecmp(param, "diffusion") == 0)
            config->dither = dit_diffusion;
        else if (strcasecmp(param, "none") == 0)
            config->dither = dit_none;
        else {
            printf("unknown dithering mode: %s\n", param);
            exit(EXIT_FAILURE);
        }

        break;

    case 'c':

        config->client = ptoc_get_client(param);

        break;

    case 'f':

        config->font = ptoc_get_font(param);

        break;

    case 'x':

        config->resize = 1;
        config->newx = atoi(param);

        break;

    case 'y':

        config->resize = 1;
        config->newy = atoi(param);

        break;

    case 'w':

        config->pause = atoi(param);

        break;

    case '?':

        return (-1);
        break;
    }

    return (0);
}

void
loadconfig(config_t * config, const char *argv0)
{
    const char configfn[] = ".j2crc";

    char *home;
    char *fn;

    FILE *fp;

    int fn_sz;

    home = getenv("HOME");

    if (home == NULL)
        return;

    fn_sz = strlen(home) + strlen(configfn) + 1 + 1;

    fn = malloc(fn_sz);
    if (fn == NULL) {
        perror("malloc()");
        exit(EXIT_FAILURE);
    }

    snprintf(fn, fn_sz, "%s/%s", home, configfn);

    fp = fopen(fn, "r");

    if (fp == NULL) {
        free(fn);
        return;
    }

    for (;;) {

        int n;
        char ch;
        char param[64];

        n = fscanf(fp, " -%c %63s ", &ch, param);

        if (n == EOF)
            break;

        if (n != 2) {

            fprintf(stderr, "invalid config file information at %s\n", fn);

            free(fn);
            fclose(fp);

            exit(EXIT_FAILURE);
        }

        n = set_config_opt(ch, param, config);

        if (n == -1) {
            fprintf(stderr, "invalid option -- %c in config file at %s\n", ch, fn);
            usage(argv0);
            exit(EXIT_FAILURE);
        }
    }

    free(fn);
    fclose(fp);
}

int
main(int argc, char **argv, char **envp)
{

    config_t config;

    const char *block;
    const int *opixel;
    int xw, yw, y, x, n;

    char vbuf[1024];

    unsigned char *rgb, *pixel;
    int qerror[3] = { 0 }, ipixel[3];

    const char *fn = NULL;

    config.client = cli_none;
    config.font = font_courier;
    config.term = term_none;
    config.dither = dit_diffusion;
    config.newx = -1;
    config.newy = -1;
    config.resize = 0;
    config.pause = 0;

    loadconfig(&config, *argv);

    for (;;) {

        int ch;

        ch = getopt(argc, argv, "f:d:c:x:y:w:");

        if (ch == -1)
            break;

        n = set_config_opt(ch, optarg, &config);

        if (n == -1) {
            usage(*argv);
            exit(EXIT_FAILURE);
        }
    }

    if (config.client == cli_none) {
        puts("\nplease select a client!");
        usage(*argv);
        exit(EXIT_FAILURE);
    }

    if (optind < argc)
        fn = argv[optind];

    srand(time(NULL));

    /*
     * initialization of rgb to ansi api
     */

    ptoc_init(config.client, config.font, config.term);
    ptoc_newimage();

    rgb = ptoc_jpegtorgb(fn, &xw, &yw);

    if (rgb == NULL) {
        fputs("jpegtorgb() failed for some reason, fuck off!", stderr);
        exit(EXIT_FAILURE);
    }

    if (config.resize) {

        unsigned char *newrgb;

        int fixx, fixy;

        if (config.newx < 1 && config.newy < 1) {
            fputs("at least 1 dimension (-x/-y) must be > 0", stderr);
        }

        fixx = 0;
        fixy = 0;

        if (config.newx < 1) {

            config.newx = xw * config.newy / yw;
            fixx = 1;

        } else if (config.newy < 1) {

            config.newy = yw * config.newx / xw;
            fixy = 1;

        }

        if (fixx)
            config.newx = (int)ceil((double)config.newx * ptoc_get_aspect(config.font));
        if (fixy)
            config.newy = (int)ceil((double)config.newy / ptoc_get_aspect(config.font));

        newrgb = image_resize(rgb, xw, yw, config.newx, config.newy);

        xw = config.newx;
        yw = config.newy;

        free(rgb);

        rgb = newrgb;

    }

    pixel = rgb;

    /*
     * all the work occurs here
     */

    setvbuf(stdout, vbuf, _IOLBF, sizeof(vbuf));

    for (y = 0; y < yw; y++) {

        /*
         * initialize the quantization error accumulator
         */

        ptoc_newimage();

        if (config.dither == dit_diffusion) {
            qerror[0] = 0;
            qerror[1] = 0;
            qerror[2] = 0;
        }

        for (x = xw; x--;) {

            /*
             * assign the composite pixel = pixel + quant error
             * to diffuse quantization errors
             */

            ipixel[0] = pixel[0];
            ipixel[1] = pixel[1];
            ipixel[2] = pixel[2];

            if (config.dither != dit_none) {

                for (n = 0; n < 3; n++) {

                    switch (config.dither) {

                    case dit_diffusion:

                        ipixel[n] += qerror[n];

                        break;

                    case dit_random:

                        ipixel[n] += (rand() % RANDOM_MOD) - (RANDOM_MOD / 2);

                        break;

                    case dit_none:

                        break;

                    }

                    /*
                     *
                     * clip the color components at [0,255]
                     *
                     */

                    if (ipixel[n] > DI_MAX)
                        ipixel[n] = DI_MAX;
                    if (ipixel[n] < DI_MIN)
                        ipixel[n] = DI_MIN;

                }
            }

            block = ptoc_rgbtochat(ipixel[0], ipixel[1], ipixel[2], &opixel);

            fputs(block, stdout);

            /*
             * adjust the quantization error accumulator
             */

            if (config.dither == dit_diffusion) {
                qerror[0] = (ipixel[0] - opixel[0]);
                qerror[1] = (ipixel[1] - opixel[1]);
                qerror[2] = (ipixel[2] - opixel[2]);
            }

            pixel += 3;
        }

        fputs(ptoc_geteol(), stdout);

        if (config.pause > 0)
            sleep(config.pause);

    }

    free(rgb);

    exit(EXIT_SUCCESS);
}

unsigned char
image_resample(unsigned char *rgb, double x, double y, double dx, double dy, int X, int Y,
               int component)
{

    int i, j;
    int imin, imax;
    int jmin, jmax;

    int factor;
    int n;

    factor = 0;
    n = 0;

    imin = floor(x);
    imax = floor(x + dx);

    jmin = floor(y);
    jmax = floor(y + dy);

    if (imin == imax)
        imax++;

    if (jmin == jmax)
        jmax++;

    for (i = imin; i < imax; i++) {
        for (j = jmin; j < jmax; j++) {

            n += rgb[(j * X + i) * 3 + component];

            factor++;
        }
    }

    n /= factor;

    return ((unsigned char)n);
}

unsigned char *
image_resize(unsigned char *rgb, int ox, int oy, int nx, int ny)
{

    const int components = 3;
    unsigned char *new;

    double ux, uy;
    double dx, dy;

    int i, j, k;

    new = malloc(nx * ny * components);

    if (new == NULL) {
        perror("malloc()");
        exit(EXIT_FAILURE);
    }

    dx = (double)ox / nx;
    dy = (double)oy / ny;

    uy = 0.0;

    for (j = 0; j < ny; j++) {

        ux = 0.0;

        for (i = 0; i < nx; i++) {

            for (k = 0; k < components; k++)

                new[(j * nx + i) * components + k] = image_resample(rgb, ux, uy, dx, dy, ox, oy, k);

            ux += dx;
        }

        uy += dy;
    }

    return (new);
}
