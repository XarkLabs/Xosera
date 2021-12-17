// Quick & Dirty PNG to Xosera 12-bit 8+4 hack (8-bit R+G, 4-bit B image)
// Xark - 2021
// See top-level LICENSE file for license information. (Hint: MIT)
#include <SDL.h>
#include <SDL_image.h>
#include <stdio.h>
#include <stdlib.h>

bool   noise_mode = false;
char * in_file    = nullptr;
char * out_file   = nullptr;
char   out_file8[4096];
char   out_file4[4096];

Uint32 getpixel(SDL_Surface * surface, int x, int y);

int main(int argc, char ** argv)
{
    printf("Xosera image to Verilog mem utility for 8x8 or 8x16 monochrome fonts - Xark\n\n");

    for (int a = 1; a < argc; a++)
    {
        if (argv[a][0] == '-')
        {
            if (strcmp("-n", argv[a]) == 0)
            {
                noise_mode = true;
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
        printf("image_to_mem: Convert image to monochome 8x8 or 8x16 Verilog \"mem\" file.\n");
        printf("Usage:  image_to_mem <input font image> <output font mem> [-i]\n");
        printf("   -c   Output C compatible code (ignored)\n");
        exit(EXIT_FAILURE);
    }

    printf("Input image file     : \"%s\"\n", in_file);

    sprintf(out_file8, "%s_tc_RG8.raw", out_file);
    sprintf(out_file4, "%s_tc_B4.raw", out_file);

    printf("Output 8-bpp R+G raw image: \"%s\"\n", out_file8);
    printf("Output 4-bpp B   raw image: \"%s\"\n", out_file4);
    if (noise_mode)
    {
        printf("A small amount of noise will be added to reduce banding\n");
    }

    bool quit = false;

    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);

    SDL_Window * window =
        SDL_CreateWindow("SDL2 Displaying Image", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, 0);

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
        printf("Input image size: %d x %d\n", w, h);
        printf("Output 8-bpp RG raw size: %d bytes (%6.1f KB)\n", w * h, (w * h) / 1024.0f);
        printf("Output 4-bpp B  raw size: %d bytes (%6.1f KB)\n", (w / 2) * h, ((w / 2) * h) / 1024.0f);
        printf("\n12-bpp RGB total size: %d bytes (%6.1f KB)\n",
               ((w * h) + ((w / 2) * h)),
               ((w * h) + ((w / 2) * h)) / 1024.0f);

        if (((w * h) + ((w / 2) * h)) > (128 * 1024))
        {
            printf("\nWARNING: Will not fit in Xosera 128KB VRAM\n");
        }
    }

    // process the image
    if (!quit)
    {
        // show font for a moment
        int spincount = 100;
        while (!quit)
        {
            SDL_BlitSurface(image, NULL, screen, NULL);
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

            SDL_Delay(10);
            if (--spincount == 0)
            {
                quit = true;
                break;
            }
        }

        FILE * fp8 = fopen(out_file8, "w");
        FILE * fp4 = fopen(out_file4, "w");
        if (fp8 != nullptr && fp4 != nullptr)
        {
            printf("Writing outputs...\n");

            for (int y = 0; y < h; y++)
            {
                int blue2 = 0;
                for (int x = 0; x < w; x++)
                {
                    SDL_Color rgb;
                    Uint32    data = getpixel(image, x, y);
                    SDL_GetRGB(data, image->format, &rgb.r, &rgb.g, &rgb.b);

                    int tr = 8, tg = 8, tb = 8;
                    if (noise_mode)
                    {
                        tr = ((rand() / (RAND_MAX / 16)) & 0x7) - 3;
                        tg = ((rand() / (RAND_MAX / 16)) & 0x7) - 3;
                        tb = ((rand() / (RAND_MAX / 16)) & 0x7) - 3;
                    }

                    int red   = ((rgb.r + tr) / 16);
                    int green = ((rgb.g + tg) / 16);
                    int blue  = ((rgb.b + tb) / 16);
                    if (red > 15)
                        red = 15;
                    if (green > 15)
                        green = 15;
                    if (blue > 15)
                        blue = 15;

                    fputc(red << 4 | green, fp8);
                    if (x & 1)
                    {
                        fputc(blue2 << 4 | blue, fp4);
                    }
                    else
                    {
                        blue2 = blue;
                    }
                }
            }
            printf("Success.\n");
        }
        else
        {
            printf("*** Unable to open both output file for write ");
            perror("error");
        }
        fclose(fp8);
        fclose(fp4);

        FILE * fpc = fopen("true_color.pal", "w");
        if (fpc != nullptr)
        {
            for (int i = 0; i < 256; i++)
            {
                // 0x8RG0
                fputc(0x80 | ((i >> 4) & 0xf), fpc);
                fputc(0x00 | ((i << 4) & 0xf0), fpc);
            }
            for (int i = 0; i < 16; i++)
            {
                // 0x00B
                fputc(0x00, fpc);
                fputc(i, fpc);
            }
        }
        fclose(fpc);
    }

    if (image)
    {
        SDL_FreeSurface(image);
    }
    SDL_DestroyWindow(window);

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
