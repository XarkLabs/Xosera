// Quick & Dirty PNG to Verilog memb file converter
// Xark - 2021
// See top-level LICENSE file for license information. (Hint: MIT)
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char * in_file1 = nullptr;
char * in_file2 = nullptr;
char * out_file = nullptr;

int chunk_size = 4096;

uint8_t buffer1[32768];
uint8_t buffer2[32768];

// WIP

int main(int argc, char ** argv)
{
    printf("Convert two raw 8-bit audio files into one file interleaved in %d byte chunks\n", chunk_size);

    for (int a = 1; a < argc; a++)
    {
        if (argv[a][0] == '-')
        {
            if (strcmp("-s", argv[a]) == 0)
            {
                chunk_size = 512;
            }
            else
            {
                printf("Unexpected option: '%s'\n", argv[a]);
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            if (!in_file1)
            {
                in_file1 = argv[a];
            }
            else if (!in_file2)
            {
                in_file2 = argv[a];
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

    if (!in_file1 || !in_file2 || !out_file)
    {
        printf("interlave_raw: Interleave two raw audio files with 1K chunks\n");
        printf("Usage:  interlave_raw [options] <input file 1> <input file 2> <output file>\n");
        printf(" -s     Use sector size chunks (512 byte)\n");
        exit(EXIT_FAILURE);
    }

    printf("Input L track file 1      : \"%s\"\n", in_file1);
    printf("Input R track file 2      : \"%s\"\n", in_file2);
    printf("Output interleaved LR file: \"%s\"\n", out_file);

    FILE * fp1 = fopen(in_file1, "r");
    if (fp1 == nullptr)
    {
        printf("*** Unable to open input file \"%s\"\n", in_file1);
        exit(EXIT_FAILURE);
    }
    FILE * fp2 = fopen(in_file1, "r");
    if (fp2 == nullptr)
    {
        printf("*** Unable to open input file \"%s\"\n", in_file2);
        exit(EXIT_FAILURE);
    }

    FILE * ofp = fopen(out_file, "w");
    if (ofp == nullptr)
    {
        printf("*** Unable to open output file \"%s\"\n", out_file);
        exit(EXIT_FAILURE);
    }

    int num1  = 0;
    int num2  = 0;
    int num3  = 0;
    int chunk = 0;
    do
    {
        memset(buffer1, 0, chunk_size);
        num1 = fread(buffer1, chunk_size, 1, fp1);
        memset(buffer2, 0, chunk_size);
        num2 = fread(buffer2, chunk_size, 1, fp2);

        num3 = fwrite(buffer1, chunk_size, 1, ofp);

        num3 &= fwrite(buffer2, chunk_size, 1, ofp);

        printf("Wrote chunk %d   \r", chunk++);

    } while (num1 && num2 && num3);

    fclose(fp1);
    fclose(fp2);
    fclose(ofp);
    printf("Wrote %d chunks of 2 x %d bytes\n", chunk, chunk_size);

    return EXIT_SUCCESS;
}
