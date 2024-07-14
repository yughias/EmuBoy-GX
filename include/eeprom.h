#ifndef __EEPROM_H__
#define __EEPROM_H__

#include "gamepak.h"
#include "integer.h"

#define EEPROM_512B_SIZE (1 << 9)
#define EEPROM_8K_SIZE   (1 << 13) 

typedef enum {
    EEPROM_IDLE = 0,
    EEPROM_SET_ADDRESS, EEPROM_WRITE_REQUEST, EEPROM_WRITE_DATA, EEPROM_WRITE_END,
    EEPROM_READ_DUMMY, EEPROM_READ_DATA
} EEPROM_STATE;

typedef struct eeprom_t
{
    EEPROM_STATE state;
    u8 n;
    u8 bit_counter;
    u16 address;
    uint64_t buffer;
} eeprom_t;

void setupEepromMemory(gamepak_t* gampepak, size_t size);
u8 readEeprom(gamepak_t* gamepak);
void writeEeprom(gamepak_t* gamepak, u8 val);

#endif