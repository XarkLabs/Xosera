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

int font_height = 0;
int font_chars  = 0;

Uint32 getpixel(SDL_Surface * surface, int x, int y);

int main(int argc, char ** argv)
{
    printf("Xosera image to Verilog mem utility for 8x8 or 8x16 monochrome fonts - Xark\n\n");

    for (int a = 1; a < argc; a++)
    {
        if (argv[a][0] == '-')
        {
            if (strcmp("-i", argv[a]) == 0)
            {
                invert = true;
            }
            else if (strcmp("-c", argv[a]) == 0)
            {
                c_mode = true;
            }
            else if (strcmp("-w", argv[a]) == 0)
            {
                word_mode = true;
            }
            else if (strcmp("-8", argv[a]) == 0)
            {
                font_height = 8;
            }
            else if (strcmp("-16", argv[a]) == 0)
            {
                font_height = 16;
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
        printf("   -i   Invert pixels\n");
        printf("   -c   Output C compatible code, vs Verilog mem\n");
        printf("   -w   16-bit word output\n");
        printf("   -8   Override font size auto-detect and use 8x8\n");
        printf("   -16  Override font size auto-detect and use 8x16\n");
        exit(EXIT_FAILURE);
    }

    printf("Input image file     : \"%s\"\n", in_file);
    printf("Output mem font file : \"%s\"\n", out_file);
    if (invert)
    {
        printf("[Inverting pixels] ");
    }
    if (font_height == 8)
    {
        printf("[Force 8x8 font size] ");
    }
    else if (font_height == 16)
    {
        printf("[Force 8x16 font size] ");
    }
    printf("\n");

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
        if ((w & 0x7) != 0 || (h & 0x7) != 0)
        {
            printf("*** Unsupported image size (width and height should be multiple of 8)\n");
            quit = true;
        }
        else
        {
            int pixelcount = w * h;

            if (pixelcount <= 16384)
            {
                printf("8x8 font detected.\n");
                font_height = 8;
                font_chars  = (w / 8) * (h / 8);
            }
            else if (pixelcount <= 32768)
            {
                printf("256 8x16 font detected.\n");
                font_height = 16;
                font_chars  = (w / 8) * (h / 16);
            }
            else
            {
                printf("*** Can't autodetect 8x8 or 8x16, need to specify\n");
                quit = true;
            }

            printf("Converting %d 8x%d glyphs...\n", font_chars, font_height);
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

        FILE * fp = fopen(out_file, "w");
        if (fp != nullptr)
        {
            printf("Writing output...\n");

            if (c_mode)
            {
                fprintf(fp, "// Generated by: image_to_mem ");
                for (int i = 1; i < argc; i++)
                {
                    fprintf(fp, "%s ", argv[i]);
                }
                fprintf(fp, "\n");
                fprintf(fp, "uint8_t font[256*%d] =\n", font_height);
                fprintf(fp, "{\n");
            }

            uint8_t cn = 0;
            for (int cy = 0; cy < h; cy += font_height)
            {
                for (int cx = 0; cx < w; cx += 8)
                {
                    char hex[8] = {0};
                    char lit[8] = {0};
                    snprintf(hex, sizeof(hex), "\\x%02x", cn);
                    snprintf(lit, sizeof(lit), "%c", cn);
                    fprintf(fp, "// 0x%02x '%s'\n", cn, isprint(cn) ? lit : hex);
                    for (int y = 0; y < font_height; y++)
                    {
                        if (c_mode && (!word_mode || !(y & 1)))
                        {
                            fprintf(fp, "0b");
                        }
                        for (int x = 0; x < 8; x++)
                        {
                            SDL_Color rgb;
                            Uint32    data = getpixel(image, cx + x, cy + y);
                            SDL_GetRGB(data, image->format, &rgb.r, &rgb.g, &rgb.b);
                            int v = (rgb.r + rgb.g + rgb.b) / 3;

                            bool pixel = (v >= 128);
                            if (invert)
                            {
                                pixel = !pixel;
                            }
                            fprintf(fp, "%s", pixel ? "1" : "0");
                        }
                        if (!word_mode)
                        {
                            if (c_mode)
                            {
                                fprintf(fp, ",");
                            }
                            fprintf(fp, "    // ");
                            for (int x = 0; x < 8; x++)
                            {
                                SDL_Color rgb;
                                Uint32    data = getpixel(image, cx + x, cy + y);
                                SDL_GetRGB(data, image->format, &rgb.r, &rgb.g, &rgb.b);
                                int v = (rgb.r + rgb.g + rgb.b) / 3;

                                bool pixel = (v >= 128);
                                if (invert)
                                {
                                    pixel = !pixel;
                                }
                                fprintf(fp, "%s", pixel ? "#" : ".");
                            }
                            fprintf(fp, "\n");
                        }
                        else
                        {
                            if (y & 1)
                            {
                                if (c_mode)
                                {
                                    fprintf(fp, ",");
                                }
                                fprintf(fp, "\n");
                            }
                        }
                    }
                    fprintf(fp, "\n");
                    cn++;
                }
            }
            if (c_mode)
            {
                fprintf(fp, "};\n");
            }
            fclose(fp);
            printf("Success.\n");
        }
        else
        {
            printf("*** Unable to open write to output file\n");
        }
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
