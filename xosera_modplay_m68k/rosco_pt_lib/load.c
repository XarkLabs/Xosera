/*
 * Copyright (c) 2022 Ross Bamford
 */

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "rosco_m68k_support.h"

#include "pt_mod.h"

extern int print_mod(PtMod * mod);
extern int xosera_play(PtMod * mod, int n, uint16_t rate);

static uint8_t buffer[524288];

static bool load_mod(const char * filename, uint8_t * buf)
{
    FILE * f = fopen(filename, "r");

    if (!f)
    {
        printf("Unable to open MOD '%s'\n", filename);
        return false;
    }

    if (fseek(f, 0, SEEK_END))
    {
        fclose(f);
        printf("Seek failed; bailing\n");
        return false;
    }

    const long fsize = ftell(f);
    if (fsize == -1L)
    {
        fclose(f);
        printf("ftell failed; bailing\n");
        return false;
    }

    if (fseek(f, 0, SEEK_SET))
    {
        fclose(f);
        printf("Second seek failed; bailing\n");
        return false;
    }

    if (fread(buf, fsize, 1, f) != (size_t)fsize)
    {
        fclose(f);
        printf("Read failed; bailing\n");
        return false;
    }

    fclose(f);
    return true;
}

int main()
{
    const char * filename = "/sd/xenon2.mod";

    debug_printf("Loading mod: %s\n", filename);
    if (load_mod(filename, buffer))
    {

        PtMod * mod = (PtMod *)buffer;

        debug_printf("MOD is %20s\n", mod->song_name);

#ifdef PRINT_INFO
        print_mod(mod);
#endif

#ifdef PLAY_SAMPLE
        debug_printf("Playing; This will mess up your screen\n");
        xosera_play(mod, 0x17, 22100);
#endif

        debug_printf("All done, bye!\n");
    }
    else
    {
        debug_printf("Failed to open MOD\n");
    }
}
