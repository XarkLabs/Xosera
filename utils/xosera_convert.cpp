// Xosera PNG conversion utility (aka cruncher)
// See top-level LICENSE file for license information. (Hint: MIT)
// vim: set et ts=4 sw=4

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <SDL.h>
#include <SDL_image.h>

enum
{
    OUT_RAW     = 0,
    OUT_CH      = 1,
    OUT_ASM     = 2,
    OUT_VERILOG = 3
};

char filename[4096];
char identifier[4096];

bool fail = false;        // error flag

char * convert_mode    = nullptr;        // conversion mode string
char * input_file      = nullptr;        // input image file
char * output_basename = nullptr;        // output file


bool     display_pic     = false;
bool     add_noise       = false;
bool     interleave_RG_B = false;
bool     write_palette   = false;
uint32_t num_colors      = 256;
uint32_t greyscale_bit   = 0xff;

static void help()
{
    printf("xosera_convert: PNG to various Xosera image formats\n");
    printf("Usage:  xosera_convert [options ...] <mode> <input_file> <out_basename>\n");
    printf("Options:\n");
    printf(" -c #   Number of colors (2, 16, 256 or 4096)\n");
    printf(" -d     Display input and output images\n");
    printf(" -i     Interleave RG and B with 4096 colors\n");
    printf(" -n     Add random noise to reduce 12-bit color banding\n");
    printf(" -p     Also write out colormem palette file\n");
    printf(" -g #   Greyscale bit\n");
    printf(" -raw   Output raw headerless binary (*default)\n");
    printf(" -ch    Output C source/header file\n");
    printf(" -as    Output asm source file\n");
    printf(" -memh  Output Verilog hex memory file (16-bit width)\n");
    printf("Conversion mode : <mode>\n");
    printf(" font   Convert PNG to font\n");
    printf(" bitmap Convert PNG to bitmap image\n");
    printf(" cut    Convert PNG with outlined images to blit images\n");
    printf(" pal    Write out palette (use -c to specify colors)\n");
    printf("Input file:   <input_file> (PNG format)\n");
    printf("Output base name: <out_basename>\n");

    exit(EXIT_FAILURE);
}

inline Uint32 getpixel(SDL_Surface * surface, int x, int y)
{
    int bpp = surface->format->BytesPerPixel;
    /* Here p is the address to the pixel we want to retrieve */
    Uint8 * p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;

    switch (bpp)
    {
        case 1:
            return *p;
            break;

        case 2:
            return *(Uint16 *)p;
            break;

        case 3:
            return p[0] | p[1] << 8 | p[2] << 16;
            break;

        case 4:
            return *(Uint32 *)p;
            break;

        default:
            return 0; /* shouldn't happen, but avoids warnings */
    }
}

Uint32 color_array_size;
Uint16 color_array[4096];

Uint32 count_colors(SDL_Surface * surface)
{
    color_array_size = 0;
    memset(color_array, 0, sizeof(color_array));

    for (int y = 0; y < surface->h; y++)
    {
        for (int x = 0; y < surface->w; x++)
        {
            Uint32 pixel = getpixel(surface, x, y);
            (void)pixel;
        }
    }

    return 0;
}

void str_to_identifier(char * str)
{
    char c;
    while (str && (c = toupper(*str)) != '\0')
    {
        if (!isalnum(c))
            c = '_';
        *str++ = c;
    }
}

bool convert_bitmap_1bpp(SDL_Surface * image, char * out_name)
{
    int w  = image->w;
    int ww = (w + 15) >> 4;
    int h  = image->h;
    //    int bpp = image->format->BytesPerPixel;

    snprintf(filename, sizeof(filename) - 1, "%s_image.h", out_name);
    snprintf(identifier, sizeof(identifier) - 1, "%s_IMAGE_H", out_name);
    str_to_identifier(identifier);
    printf("\n\n");

    FILE * out_fp = fopen(filename, "w");
    if (out_fp)
    {
        int count = 0;
        for (int x = 0; x < (ww << 4); x += 16)
        {
            for (int y = 0; y < h; y++)
            {
                uint16_t word = 0;

                int sx = x;
                for (int b = 0x8000; b != 0; b >>= 1, sx++)
                {
                    Uint32 pix = getpixel(image, sx, y);
                    pix        = (pix >> 4) & 0xf;
                    printf("%x", pix);
                    if (greyscale_bit == 888)
                    {
                        if (pix == 0x8 || pix == 0xf)
                        {
                            word |= b;
                        }
                    }
                    else if (greyscale_bit == 8888)
                    {
                        if (pix > 0xa)
                        {
                            word |= b;
                        }
                    }
                    else if (pix & greyscale_bit)
                    {
                        word |= b;
                    }
                }
                if (!count)
                {
                    fprintf(out_fp, "// File: \"%s\"\n", input_file);
                    fprintf(out_fp, "// Image size %d x %d (%d x %d pixels) 1-bpp mono\n", ww, h, w, h);
                    fprintf(out_fp, "#include <stdint.h>\n");
                    fprintf(out_fp, "#if !defined(%s)\n", identifier);
                    fprintf(out_fp, "#define %s\n", identifier);
                    fprintf(
                        out_fp, "static uint16_t %s_w __attribute__ ((unused))  = %3d;    // words\n", out_name, ww);
                    fprintf(out_fp,
                            "static uint16_t %s_pw __attribute__ ((unused)) = %3d;    // pixel width\n",
                            out_name,
                            w);
                    fprintf(
                        out_fp, "static uint16_t %s_h __attribute__ ((unused))  = %3d;    // pixels\n", out_name, h);
                    fprintf(out_fp, "static uint16_t %s[%d * %d] __attribute__ ((unused)) = {\n    ", out_name, ww, h);
                }
                else
                {
                    fprintf(out_fp, ", ");
                }

                fprintf(out_fp, "0x%04x", word);
                count++;
            }
        }
        printf("\n\n");
        fprintf(out_fp, "\n};\n");
        fprintf(out_fp, "#endif // !defined(%s)\n", identifier);

        fclose(out_fp);
        return true;
    }
    return false;
}

bool convert_bitmap_8bpp(SDL_Surface * image, char * out_name)
{
    int w  = image->w;
    int ww = (w + 1) >> 1;
    int h  = image->h;
    //    int bpp = image->format->BytesPerPixel;

    snprintf(filename, sizeof(filename) - 1, "%s_image.h", out_name);
    snprintf(identifier, sizeof(identifier) - 1, "%s_IMAGE_H", out_name);
    str_to_identifier(identifier);

    FILE * out_fp = fopen(filename, "w");
    if (out_fp)
    {
        int count = 0;
        int col   = 0;
        for (int y = 0; y < h; y++)
        {
            for (int x = 0; x < (ww << 1); x++)
            {
                Uint32 pix = 0;
                if (x < w)
                {
                    pix = getpixel(image, x, y);
                }

                if (col & 1)
                {
                    fprintf(out_fp, "%02x", pix & 0xff);
                }
                else
                {
                    if (!count)
                    {
                        fprintf(out_fp, "// File: \"%s\"\n", input_file);
                        fprintf(out_fp, "// Image size %d x %d (%d x %d pixels) 8-bpp\n", ww, h, w, h);
                        fprintf(out_fp, "#include <stdint.h>\n");
                        fprintf(out_fp, "#if !defined(%s)\n", identifier);
                        fprintf(out_fp, "#define %s\n", identifier);
                        fprintf(out_fp,
                                "static uint16_t %s_w __attribute__ ((unused))  = %3d;    // "
                                "words\n",
                                out_name,
                                ww);
                        fprintf(out_fp,
                                "static uint16_t %s_pw __attribute__ ((unused)) = %3d;    // "
                                "pixel width\n",
                                out_name,
                                w);
                        fprintf(out_fp,
                                "static uint16_t %s_h __attribute__ ((unused))  = %3d;    // "
                                "pixels\n",
                                out_name,
                                h);
                        fprintf(
                            out_fp, "static uint16_t %s[%d * %d] __attribute__ ((unused)) = {\n    ", out_name, ww, h);
                    }
                    else if (x == 0)
                    {
                        fprintf(out_fp, ",\n    ");
                    }
                    else
                    {
                        fprintf(out_fp, ", ");
                    }

                    fprintf(out_fp, "0x%02x", pix & 0xff);
                }
                count++;
                col++;
            }
            col = 0;
        }
        fprintf(out_fp, "\n};\n");
        fprintf(out_fp, "#endif // !defined(%s)\n", identifier);

        fclose(out_fp);
        return true;
    }
    return false;
}

int main(int argc, char ** argv)
{

    if (argc == 1)
    {
        help();
    }

    for (int a = 1; a < argc; a++)
    {
        if (argv[a][0] == '-')
        {
            if (strcmp("-n", argv[a]) == 0)
            {
                add_noise = true;
            }
            else if (strcmp("-b", argv[a]) == 0)
            {
                display_pic = true;
            }
            else if (strcmp("-i", argv[a]) == 0)
            {
                interleave_RG_B = true;
            }
            else if (strcmp("-p", argv[a]) == 0)
            {
                write_palette = true;
            }
            else if (strcmp("-c", argv[a]) == 0)
            {
                if (++a >= argc || sscanf(argv[a], "%u", &num_colors) != 1)
                {
                    printf("Number of colors expected after option: '-c'\n");
                }
            }
            else if (strcmp("-g", argv[a]) == 0)
            {
                if (++a >= argc || sscanf(argv[a], "%u", &greyscale_bit) != 1)
                {
                    printf("Power of two expected after option: '-g'\n");
                }
            }
            else
            {
                printf("Unexpected option: '%s'\n", argv[a]);
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            if (!convert_mode)
            {
                convert_mode = argv[a];
            }
            else if (!input_file)
            {
                input_file = argv[a];
            }
            else if (!output_basename)
            {
                output_basename = argv[a];
            }
            else
            {
                printf("error: Extra argument: '%s'\n", argv[a]);
                exit(EXIT_FAILURE);
            }
        }
    }

    if (!convert_mode)
    {
        printf("Error: A conversion <mode> is required.\n");
        help();
    }

    if (!input_file)
    {
        printf("Error: An <input_file> is required.\n");
        help();
    }

    if (!output_basename)
    {
        printf("Error: An <out_basename> is required.\n");
        help();
    }


    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);

    printf("Reading image file: \"%s\"\n", input_file);

    SDL_Surface * image = IMG_Load(input_file);

    int w   = 0;
    int h   = 0;
    int bpp = 0;

    if (!image)
    {
        printf("\n*** Unable to IMG_Load \"%s\"\n", input_file);
        fail = true;
    }
    else
    {
        w   = image->w;
        h   = image->h;
        bpp = image->format->BytesPerPixel;
        printf("\nInput image size        : %d x %d %d bytes per pixel\n", w, h, bpp);
        if (image->format->palette && image->format->palette->ncolors)
        {
            printf("  Palette %d colors\n", image->format->palette->ncolors);
        }
        else
        {
            printf("  No palette\n");
        }


        printf("Conversion mode: %s\n", convert_mode);
        if (strcasecmp("bitmap", convert_mode) == 0)
        {
            if (num_colors == 2)
            {
                convert_bitmap_1bpp(image, output_basename);
            }
            else
            {
                convert_bitmap_8bpp(image, output_basename);
            }
        }
    }

    IMG_Quit();
    SDL_Quit();

    printf("%s\n", fail ? "Failed!" : "Success.");

    return fail ? EXIT_FAILURE : EXIT_SUCCESS;
}

bool process_bitmap()
{
    // if (!interleave_mode)
    // {
    //     printf("Output 8-bpp RG raw size: %6d bytes (%6.1f KB)\n", w * h, (w * h) / 1024.0f);
    //     printf("Output 4-bpp B  raw size: %6d bytes (%6.1f KB)\n", (w / 2) * h, ((w / 2) * h)
    //     / 1024.0f); printf("   12-bpp RGB total size: %6d bytes (%6.1f KB)\n",
    //            ((w * h) + ((w / 2) * h)),
    //            ((w * h) + ((w / 2) * h)) / 1024.0f);
    // }
    // else
    // {
    //     printf("Output 12-bpp RG+B raw size: %6d bytes (%6.1f KB)\n",
    //            ((w * h) + ((w / 2) * h)),
    //            ((w * h) + ((w / 2) * h)) / 1024.0f);
    // }

    // if (((w * h) + ((w / 2) * h)) > (128 * 1024))
    // {
    //     printf("\nWARNING: Will not fit in Xosera 128KB VRAM\n");
    // }
    return true;
}


#if 0

    if (!batch_mode)
    {
        SDL_Window * window = SDL_CreateWindow("SDL2 Displaying Image",
                                               SDL_WINDOWPOS_CENTERED,
                                               SDL_WINDOWPOS_CENTERED,
                                               w < 320 ? 320 + 32 : w + 32,
                                               h < 240 ? 240 + 32 : h + 32,
                                               0);

        if (!window)
        {
            printf("*** Can't open SDL window\n");
            exit(EXIT_FAILURE);
        }

        SDL_Surface * screen = SDL_GetWindowSurface(window);
        if (!screen)
        {
            printf("*** Can't open SDL window\n");
            SDL_DestroyWindow(window);
            SDL_Quit();
        }

        // process the image
        if (!quit)
        {
            // show image for a moment
            int spincount = 120;
            while (!quit)
            {
                SDL_Rect dstrect;
                dstrect.x = 16;
                dstrect.y = 16;
                dstrect.w = w;
                dstrect.w = h;
                SDL_BlitSurface(image, NULL, screen, &dstrect);
                SDL_UpdateWindowSurface(window);

                SDL_Event event;
                SDL_PollEvent(&event);

                switch (event.type)
                {
                    case SDL_QUIT:
                        quit = true;
                        break;

                    case SDL_KEYDOWN:
                        quit = true;
                        break;
                }

                SDL_Delay(17);
                if (--spincount == 0)
                {
                    quit = true;
                    break;
                }
            }

            SDL_DestroyWindow(window);
        }

        srand(time(nullptr));

        {
            FILE * fp8 = fopen(out_file8, "w");
            if (fp8 != nullptr)
            {
                printf("Writing output file: \"%s\"...", out_file8);
                fflush(stdout);

                for (int y = 0; y < h; y++)
                {
                    for (int x = 0; x < w; x++)
                    {
                        SDL_Color rgb;
                        Uint32    data = getpixel(image, x, y);
                        SDL_GetRGB(data, image->format, &rgb.r, &rgb.g, &rgb.b);

                        int tr = 8, tg = 8;
                        if (noise_mode)
                        {
                            tr = (rand() % NOISE_MOD) - NOISE_SUB;
                            tg = (rand() % NOISE_MOD) - NOISE_SUB;
                        }

                        int red   = ((rgb.r + tr) / 16);
                        int green = ((rgb.g + tg) / 16);
                        if (red < 0)
                            red = 0;
                        else if (red > 15)
                            red = 15;
                        if (green < 0)
                            green = 0;
                        else if (green > 15)
                            green = 15;

                        fputc(red << 4 | green, fp8);
                    }
                    if (interleave_mode)
                    {
                        int lastblue = 0;
                        for (int x = 0; x < w; x++)
                        {
                            SDL_Color rgb;
                            Uint32    data = getpixel(image, x, y);
                            SDL_GetRGB(data, image->format, &rgb.r, &rgb.g, &rgb.b);

                            int tb = 8;
                            if (noise_mode)
                            {
                                tb = (rand() % NOISE_MOD) - NOISE_SUB;
                            }

                            int blue = ((rgb.b + tb) / 16);
                            if (blue < 0)
                                blue = 0;
                            if (blue > 15)
                                blue = 15;

                            if (x & 1)
                            {
                                fputc(lastblue << 4 | blue, fp8);
                            }
                            else
                            {
                                lastblue = blue;
                            }
                        }
                    }
                }
                printf("success\n");
            }
            else
            {
                printf("*** Unable to open \"%s\" ", out_file8);
                perror("error");
            }
            fclose(fp8);
        }

        if (!interleave_mode)
        {
            FILE * fp4 = fopen(out_file4, "w");
            if (fp4 != nullptr)
            {
                printf("Writing output file: \"%s\"...", out_file4);
                fflush(stdout);

                for (int y = 0; y < h; y++)
                {
                    int lastblue = 0;
                    for (int x = 0; x < w; x++)
                    {
                        SDL_Color rgb;
                        Uint32    data = getpixel(image, x, y);
                        SDL_GetRGB(data, image->format, &rgb.r, &rgb.g, &rgb.b);

                        int tb = 8;
                        if (noise_mode)
                        {
                            tb = (rand() % NOISE_MOD) - NOISE_SUB;
                        }

                        int blue = ((rgb.b + tb) / 16);
                        if (blue < 0)
                            blue = 0;
                        if (blue > 15)
                            blue = 15;

                        if (x & 1)
                        {
                            fputc(lastblue << 4 | blue, fp4);
                        }
                        else
                        {
                            lastblue = blue;
                        }
                    }
                }
                printf("success\n");
            }
            else
            {
                printf("*** Unable to open \"%s\" ", out_file4);
                perror("error");
            }
            fclose(fp4);
        }

        if (create_pal)
        {
            snprintf(out_file8, sizeof(out_file8), "%s_pal.raw", out_file);

            FILE * fpc = fopen(out_file8, "w");
            if (fpc != nullptr)
            {
                printf("Writing output file: \"%s\"...", out_file8);
                fflush(stdout);

                for (int i = 0; i < 256; i++)
                {
                    // 0x4RG0
                    fputc(0x40 | ((i >> 4) & 0xf), fpc);
                    fputc(0x00 | ((i << 4) & 0xf0), fpc);
                }
                for (int i = 0; i < 16; i++)
                {
                    // 0xF00B
                    fputc(0xF0, fpc);
                    fputc(i, fpc);
                }
                printf("success\n");
            }
            else
            {
                printf("*** Unable to open \"%s\" ", out_file8);
                perror("error");
            }
            fclose(fpc);
        }
    }

    if (image)
    {
        SDL_FreeSurface(image);
    }
#endif
