// Quick & Dirty PNG to Verilog memb file converter
// Xark - 2021
// See top-level LICENSE file for license information. (Hint: MIT)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char * in_file  = nullptr;
char * out_file = nullptr;


uint8_t out_buffer[128 * 1024];
uint8_t in_buffer[128 * 1024];

bool pal = false;

int main(int argc, char ** argv)
{
    printf("Extracts low nibble form each byte - Xark\n\n");

    for (int a = 1; a < argc; a++)
    {
        if (argv[a][0] == '-')
        {
            if (strcmp("-p", argv[a]) == 0)
            {
                pal = true;
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
        printf("raw256to16color: Extract low nibble from file\n");
        printf("Usage:  raw256to16color <input file> <output file>\n");
        printf(" -p   treat 3 bytes as 16-bit palette entry\n");
        exit(EXIT_FAILURE);
    }

    printf("Input image file     : \"%s\"\n", in_file);
    printf("Output mem font file : \"%s\"\n", out_file);
    if (pal)
    {
        printf("Padding for 16-bit palette\n");
    }

    size_t in_length = 0;
    {
        FILE * fp = fopen(in_file, "rb");
        if (fp != nullptr)
        {
            printf("Reading input...\n");

            in_length = fread(in_buffer, 1, 128 * 1024, fp);

            fclose(fp);
            printf("Success, %zd bytes.\n", in_length);
        }
        else
        {
            printf("*** Unable to open input file\n");
            exit(EXIT_FAILURE);
        }
    }

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

    return EXIT_SUCCESS;
}
