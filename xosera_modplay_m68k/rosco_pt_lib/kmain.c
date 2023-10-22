/*
 * Copyright (c) 2022 Ross Bamford
 */

#include "dprintf.h"
#include "pt_mod.h"
#include <sdfat.h>
#include <stdbool.h>
#include <stdio.h>

extern int print_mod(PtMod * mod);
extern int xosera_play(PtMod * mod, int n, uint16_t rate);

static uint8_t buffer[524288];

static bool load_mod(const char * filename, uint8_t * buf)
{
    FILE * f = fl_fopen(filename, "r");

    if (!f)
    {
        printf("Unable to open MOD '%s'\n", filename);
        return false;
    }

    if (fl_fseek(f, 0, SEEK_END))
    {
        fl_fclose(f);
        printf("Seek failed; bailing\n");
        return false;
    }

    const long fsize = fl_ftell(f);
    if (fsize == -1L)
    {
        fl_fclose(f);
        printf("ftell failed; bailing\n");
        return false;
    }

    if (fl_fseek(f, 0, SEEK_SET))
    {
        fl_fclose(f);
        printf("Second seek failed; bailing\n");
        return false;
    }

    if (fl_fread(buf, fsize, 1, f) != fsize)
    {
        fl_fclose(f);
        printf("Read failed; bailing\n");
        return false;
    }

    fl_fclose(f);
    return true;
}

void kmain()
{
    if (!SD_FAT_initialize())
    {
        printf("no SD card, bailing\n");
        return;
    }

    const char * filename = "/xenon2.mod";

    dprintf("Loading mod: %s\n", filename);
    if (load_mod(filename, buffer))
    {

        PtMod * mod = (PtMod *)buffer;

        dprintf("MOD is %20s\n", mod->song_name);

#ifdef PRINT_INFO
        print_mod(mod);
#endif

#ifdef PLAY_SAMPLE
        dprintf("Playing; This will mess up your screen\n");
        xosera_play(mod, 0x17, 22100);
#endif

        dprintf("All done, bye!\n");
    }
    else
    {
        dprintf("Failed to open MOD\n");
    }
}
