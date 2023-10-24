// Xosera PNG conversion utility (aka cruncher)
// See top-level LICENSE file for license information. (Hint: MIT)
// vim: set et ts=4 sw=4

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include <SDL.h>
#include <SDL_image.h>

bool display_pic = false;
bool add_noise = false;
bool interleave_RG_B = false;
bool write_palette = false;

static void help()
{
    printf("xosera_convert: PNG to various Xosera image formats\n");
    printf("Usage:  xosera_convert [options ...] <mode> <input_file> <out_basename>\n");
    printf("Options:\n");
    printf(" -c     Number of colors (2, 16, 256 or 4096)\n");
    printf(" -d     Display input and output images\n");
    printf(" -i     Interleave RG and B with 4096 colors\n");
    printf(" -n     Add random noise to reduce 12-bit color banding\n");
    printf(" -p     Also write out colormem palette file\n");
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

int main(int argc, char ** argv)
{
    bool quit = false;
    char *mode = nullptr;
    char *in_file = nullptr;
    char *out_basename = nullptr;

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
            else
            {
                printf("Unexpected option: '%s'\n", argv[a]);
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            if (!mode)
            {
                mode = argv[a];
            }
            if (!in_file)
            {
                in_file = argv[a];
            }
            else if (!out_basename)
            {
                out_basename = argv[a];
            }
            else
            {
                printf("Unexpected extra argument: '%s'\n", argv[a]);
                exit(EXIT_FAILURE);
            }
        }
    }

    if (!mode)
    {
        printf("Error: A conversion <mode> is required.\n");
        help();
    }

    if (!in_file)
    {
        printf("Error: An <input_file> is required.\n");
        help();
    }

    if (!out_basename)
    {
        printf("Error: An <out_basename> is required.\n");
        help();
    }

    printf("Input image file     : \"%s\"\n", in_file);

    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);

    SDL_Surface * image = IMG_Load(in_file);

    int w = 0;
    int h = 0;

    if (!image)
    {
        printf("*** Unable to load \"%s\"\n", in_file);
        quit = true;
    }
    else
    {
        w = image->w;
        h = image->h;
        printf("\nInput image size        : %d x %d\n", w, h);
        // if (!interleave_mode)
        // {
        //     printf("Output 8-bpp RG raw size: %6d bytes (%6.1f KB)\n", w * h, (w * h) / 1024.0f);
        //     printf("Output 4-bpp B  raw size: %6d bytes (%6.1f KB)\n", (w / 2) * h, ((w / 2) * h) / 1024.0f);
        //     printf("   12-bpp RGB total size: %6d bytes (%6.1f KB)\n",
        //            ((w * h) + ((w / 2) * h)),
        //            ((w * h) + ((w / 2) * h)) / 1024.0f);
        // }
        // else
        // {
        //     printf("Output 12-bpp RG+B raw size: %6d bytes (%6.1f KB)\n",
        //            ((w * h) + ((w / 2) * h)),
        //            ((w * h) + ((w / 2) * h)) / 1024.0f);
        // }

        if (((w * h) + ((w / 2) * h)) > (128 * 1024))
        {
            printf("\nWARNING: Will not fit in Xosera 128KB VRAM\n");
        }
    }

    printf("WIP...\n");
    (void)quit;
    exit(EXIT_FAILURE);
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

    IMG_Quit();
    SDL_Quit();

    return 0;
}
