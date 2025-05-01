#include "gba.h"

#include "sram.h"
#include "flash.h"
#include "eeprom.h"

#include "memory_utils.h"

#include <stdio.h>
#include <stdlib.h>

#include "tinyfiledialogs.h"

DEFINE_READ_FUNC(Byte, 8, 0)
DEFINE_READ_FUNC(HalfWord, 16, 0b1)
DEFINE_READ_FUNC(Word, 32, 0b11)

DEFINE_WRITE_FUNC(Byte, 8, 0)
DEFINE_WRITE_FUNC(HalfWord, 16, 0b1)
DEFINE_WRITE_FUNC(Word, 32, 0b11)

void loadBios(const char* filename, u8** bios){
    FILE* fptr = fopen(filename, "rb");
    if(!fptr){
        tinyfd_messageBox(
            "LOADING BIOS ERROR",
            "Cannot found data/gba_bios.bin\n"
            "Please load your own bios copy on that folder\n"
            "Quitting...",
            "ok",
            "error",
            0
        );
        exit(EXIT_FAILURE);
        return;
    }

    *bios = (u8*)malloc(BIOS_SIZE);

    fread(*bios, 1, BIOS_SIZE, fptr);

    fclose(fptr);
}