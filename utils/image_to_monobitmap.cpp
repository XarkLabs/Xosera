// Quick & Dirty PNG to Verilog memb file converter
// Xark - 2021
// See top-level LICENSE file for license information. (Hint: MIT)
#include <SDL.h>
#include <SDL_image.h>
#include <stdio.h>
#include <stdlib.h>

bool   word_mode = false;
bool   c_mode    = false;
bool   invert    = false;
char * in_file   = nullptr;
char * out_file  = nullptr;

int out_width  = 640;
int out_height = 480;

Uint32 getpixel(SDL_Surface * surface, int x, int y);

int main(int argc, char ** argv)
{
    printf("Xosera image to monochrome bitmap utility - Xark\n\n");

    for (int a = 1; a < argc; a++)
    {
        if (argv[a][0] == '-')
        {
            if (strcmp("-i", argv[a]) == 0)
            {
                invert = true;
            }
            else if (strcmp("-848", argv[a]) == 0)
            {
                out_width = 848;
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
        printf("image_to_mem: Convert image to monochome bitmap file.\n");
        printf("Usage:  image_to_mem <input font image> <output font mem> [-i]\n");
        printf("   -i   Invert pixels\n");
        exit(EXIT_FAILURE);
    }

    printf("Input image file     : \"%s\"\n", in_file);
    printf("Output monochrome bitmap file : \"%s\"\n", out_file);
    if (invert)
    {
        printf("[Inverting pixels] ");
    }
    printf("\n");

    bool quit = false;

    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);

    SDL_Window * window =
        SDL_CreateWindow("SDL2 Displaying Image", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 848, 480, 0);

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
        if ((w & 0x7) != 0)
        {
            printf("*** Unsupported image size (width and height should be multiple of 8)\n");
            quit = true;
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
                break;
            }
        }
    }

    while (!quit)
    {
        int        out_size   = (out_width / 8) * 2 * out_height;
        uint16_t * out_pixels = (uint16_t *)malloc(out_size);

        if (!out_pixels)
        {
            printf("OOM!\n");
            break;
        }

        memset(out_pixels, 0, out_size);
        uint16_t * pptr = out_pixels;

        FILE * fp = fopen(out_file, "w");
        if (fp != nullptr)
        {
            printf("Writing output: \"%s\" %d x %d...\n", out_file, out_width, out_height);

            for (int y = 0; y < out_height; y++)
            {
                for (int x = 0; x < out_width; x += 8)
                {
                    uint16_t val = 0;
                    if (y < h && x < w)
                    {
                        for (int b = 0; b < 8; b++)
                        {
                            SDL_Color rgb;
                            Uint32    data = getpixel(image, x + b, y);
                            SDL_GetRGB(data, image->format, &rgb.r, &rgb.g, &rgb.b);
                            int v = (rgb.r + rgb.g + rgb.b) / 3;

                            bool pixel = (v >= 128);
                            if (invert)
                            {
                                pixel = !pixel;
                            }

                            if (pixel)
                            {
                                val |= (0x80 >> b);
                            }
                        }
                    }

                    *pptr++ = (val << 8) | 0x0F;        // big-endian!
                }
            }

            bool good = (fwrite(out_pixels, out_size, 1, fp) == 1);

            fclose(fp);

            printf(good ? "Success.\n" : "Failed to write");
        }
        else
        {
            printf("*** Unable to open write to output file\n");
        }

        break;
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
