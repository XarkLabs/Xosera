// Quick & Dirty PNG to Verilog memb file converter
// Xark - 2021
// See top-level LICENSE file for license information. (Hint: MIT)
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char * in_file  = nullptr;
char * out_file = nullptr;


char    line[4096];
uint8_t out_buffer[128 * 1024];

bool round_up = false;

int main(int argc, char ** argv)
{
    printf("Convert Gimp palette into Xosera binary palette - Xark\n\n");

    for (int a = 1; a < argc; a++)
    {
        if (argv[a][0] == '-')
        {
            if (strcmp("-r", argv[a]) == 0)
            {
                round_up = true;
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
        printf("pal_to_raw: Extract low nibble from file\n");
        printf("Usage:  pal_to_raw <input file> <output file>\n");
        printf(" -r   round colors to 4-bit (vs truncate)\n");
        exit(EXIT_FAILURE);
    }

    printf("Input gpl file     : \"%s\"\n", in_file);
    printf("Output raw pal file : \"%s\"\n", out_file);
    if (round_up)
    {
        printf("[Rounding color values to 4-bit]\n");
    }

    size_t in_length = 0;
    {
        FILE * fp = fopen(in_file, "r");
        if (fp != nullptr)
        {
            while (fgets(line, sizeof(line) - 1, fp) != nullptr)
            {
                if (line[0] == '#')
                {
                    break;
                }
            }
            int i = 0;
            while (i < 256 && fgets(line, sizeof(line) - 1, fp) != nullptr)
            {
                int r = 0, g = 0, b = 0;

                if (sscanf(line, "%u %u %u", &r, &g, &b) != 3)
                {
                    printf("error parsing: %s\n", line);
                    exit(EXIT_FAILURE);
                }

                printf("[%02x] R=0x%02x, G=0x%02x, B=0x%02x\n", i, r, g, b);

                //   *out = (r >> 4) & 0xf;

                i++;
            }
            fclose(fp);
            printf("Success, %zd bytes.\n", in_length);
        }
        else
        {
            printf("*** Unable to open input file\n");
            exit(EXIT_FAILURE);
        }
    }
#if 0
    uint8_t * ptr        = out_buffer;
    size_t    out_length = 0;
    if (!pal)
    {
        for (size_t i = 0; i < in_length; i += 2)
        {
            *ptr++ = ((in_buffer[i + 0] & 0xf) << 4) | (in_buffer[i + 1] & 0xf);
            out_length++;
        }
    }
    else
    {
        for (size_t i = 0; i < in_length; i += 3)
        {
            *ptr++ = (in_buffer[i + 0] & 0xf);
            *ptr++ = ((in_buffer[i + 1] & 0xf) << 4) | (in_buffer[i + 2] & 0xf);
            out_length += 2;
        }
    }

    FILE * fp = fopen(out_file, "w");
    if (fp != nullptr)
    {
        printf("Writing output...\n");
        size_t wrote_length = fwrite(out_buffer, 1, out_length, fp);


        fclose(fp);
        printf("Wrote %zd bytes, %s\n", wrote_length, (wrote_length == out_length) ? "Success" : "Fail");
    }
    else
    {
        printf("*** Unable to open to output file\n");
        exit(EXIT_FAILURE);
    }
#endif
    return EXIT_SUCCESS;
}
