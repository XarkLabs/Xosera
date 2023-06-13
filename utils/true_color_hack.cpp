// Quick & Dirty PNG to Xosera 12-bit 8+4 hack (8-bit R+G, 4-bit B image)
// Xark - 2021
// See top-level LICENSE file for license information. (Hint: MIT)
#include <SDL.h>
#include <SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

bool   noise_mode      = false;
bool   create_pal      = false;
bool   batch_mode      = false;
bool   interleave_mode = false;
char * in_file         = nullptr;
char * out_file        = nullptr;
char   out_file8[4096];
char   out_file4[4096];

#define NOISE_MOD 13        // r = rand % NOISE_MOD
#define NOISE_SUB 6         // n = r - NOISE_SUB

Uint32 getpixel(SDL_Surface * surface, int x, int y);

int main(int argc, char ** argv)
{
    printf("true_color_hack: PNG to Xosera raw 12-bit (8-bit RG + 4-bit B) - Xark\n\n");

    for (int a = 1; a < argc; a++)
    {
        if (argv[a][0] == '-')
        {
            if (strcmp("-n", argv[a]) == 0)
            {
                noise_mode = true;
            }
            else if (strcmp("-b", argv[a]) == 0)
            {
                batch_mode = true;
            }
            else if (strcmp("-p", argv[a]) == 0)
            {
                create_pal = true;
            }
            else if (strcmp("-i", argv[a]) == 0)
            {
                interleave_mode = true;
            }
            else
            {
                printf("Unexpected option: '%s'\n", argv[a]);
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            if (!in_file)
            {
                in_file = argv[a];
            }
            else if (!out_file)
            {
                out_file = argv[a];
            }
            else
            {
                printf("Unexpected extra argument: '%s'\n", argv[a]);
                exit(EXIT_FAILURE);
            }
        }
    }

    if (!in_file || !out_file)
    {
        printf("true_color_hack: PNG to Xosera raw 12-bit (8-bit RG + 4-bit B) by Xark\n\n");
        printf("Usage:  true_color_hack <input PNG filepath> <output file basename> [-i]\n");
        printf("        (will create \"<basname>_RG8.raw\" and \"<basname>_B4.raw\"\n");
        printf("         or \"<basname>_RG8B4.raw\" with interleave option \"-i\")\n");

        printf("   -b   Batch mode, don't draw image\n");
        printf("   -n   Add some random noise to output to reduce 12-bit banding\n");
        printf("   -i   Interlave RG and B lines (each line has RG bytes, followed by B)\n");
        printf("   -p   Write raw COLORMEM data 256 RG + 16 B words (with ADD set in alpha)\n");

        exit(EXIT_FAILURE);
    }

    if (noise_mode)
    {
        printf("Noise will be added to reduce banding\n");
    }
    if (interleave_mode)
    {
        printf("Noise will be added to reduce banding\n");
    }
    if (create_pal)
    {
        printf("A COLORMEM 12-bit identity palette will be saved (256 RG then 16 B)\n");
    }
    if (batch_mode)
    {
        printf("Batch mode, image will not be shown\n");
    }

    printf("Input image file     : \"%s\"\n", in_file);

    if (!interleave_mode)
    {
        snprintf(out_file8, sizeof(out_file8), "%s_RG8.raw", out_file);
        snprintf(out_file4, sizeof(out_file4), "%s_B4.raw", out_file);

        printf("Output 8-bpp R+G raw image: \"%s\"\n", out_file8);
        printf("Output 4-bpp B   raw image: \"%s\"\n", out_file4);
    }
    else
    {
        snprintf(out_file8, sizeof(out_file8), "%s_RG8B4.raw", out_file);
        printf("Output interleaved RG 8-bpp & B 4-bpp scanlines: \"%s\"\n", out_file8);
    }

    bool quit = false;

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
        if (!interleave_mode)
        {
            printf("Output 8-bpp RG raw size: %6d bytes (%6.1f KB)\n", w * h, (w * h) / 1024.0f);
            printf("Output 4-bpp B  raw size: %6d bytes (%6.1f KB)\n", (w / 2) * h, ((w / 2) * h) / 1024.0f);
            printf("   12-bpp RGB total size: %6d bytes (%6.1f KB)\n",
                   ((w * h) + ((w / 2) * h)),
                   ((w * h) + ((w / 2) * h)) / 1024.0f);
        }
        else
        {
            printf("Output 12-bpp RG+B raw size: %6d bytes (%6.1f KB)\n",
                   ((w * h) + ((w / 2) * h)),
                   ((w * h) + ((w / 2) * h)) / 1024.0f);
        }

        if (((w * h) + ((w / 2) * h)) > (128 * 1024))
        {
            printf("\nWARNING: Will not fit in Xosera 128KB VRAM\n");
        }
    }

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

    IMG_Quit();
    SDL_Quit();

    return 0;
}

Uint32 getpixel(SDL_Surface * surface, int x, int y)
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
            if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
                return p[0] << 16 | p[1] << 8 | p[2];
            else
                return p[0] | p[1] << 8 | p[2] << 16;
            break;

        case 4:
            return *(Uint32 *)p;
            break;

        default:
            return 0; /* shouldn't happen, but avoids warnings */
    }
}
