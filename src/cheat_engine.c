#include "cheat_engine.h"
#include "gba.h"

#include <stdio.h>
#include <string.h>
#include "vector.h"

#include "SDL_MAINLOOP.h"

#define CHEAT_ENGINE_RESULT_LIMIT 32
#define CHEAT_ENGINE_CHAR_WIDTH 32
#define CHEAT_ENGINE_RENDER_MARGIN 5

SDL_Window* cheatEngineWindow;

typedef struct address_t {
    u32 address;
    u8 type;
} address_t;

static vector_t possible_addresses;

#define IF_EQUAL_THEN_ADD(ram, x, n_bits) \
if(*((u ## n_bits *)(&ram[x])) == value_to_find){ \
        vector_add(&possible_addresses, &addr_struct, sizeof(address_t)); \
}

#define SCAN_RAM(ram, ram_size, start_addr) \
for(int i = 0; i < ram_size / sizeof(u32); i++){ \
    address_t addr_struct; \
    addr_struct.address = i * 4 + start_addr; \
    addr_struct.type = 4; \
    IF_EQUAL_THEN_ADD(ram, addr_struct.address - start_addr, 32) \
    else { \
        addr_struct.type = 2; \
        IF_EQUAL_THEN_ADD(ram, addr_struct.address - start_addr, 16) \
        else { \
            addr_struct.type = 1; \
            IF_EQUAL_THEN_ADD(ram, addr_struct.address - start_addr, 8) \
            addr_struct.address += 1; \
            IF_EQUAL_THEN_ADD(ram, addr_struct.address - start_addr, 8) \
        } \
        addr_struct.address = i * 4 + 2 + start_addr; \
        addr_struct.type = 2; \
        IF_EQUAL_THEN_ADD(ram, addr_struct.address - start_addr, 16) \
        else { \
            addr_struct.type = 1; \
            IF_EQUAL_THEN_ADD(ram, addr_struct.address - start_addr, 8) \
            addr_struct.address += 1; \
            IF_EQUAL_THEN_ADD(ram, addr_struct.address - start_addr, 8) \
        } \
    } \
}

void cheatEnginePrintAddresses(){
    Uint32 id = SDL_GetWindowID(cheatEngineWindow);
    if(!id)
        cheatEngineWindow = SDL_CreateWindow(
            "found addresses",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            CHEAT_ENGINE_CHAR_WIDTH*8+CHEAT_ENGINE_RENDER_MARGIN*2, CHEAT_ENGINE_RESULT_LIMIT*8*2+CHEAT_ENGINE_RENDER_MARGIN*2, 
            0
        );

    SDL_Surface* surface = SDL_GetWindowSurface(cheatEngineWindow);
    SDL_FillRect(surface, NULL, 0);

    int written = 0;
    int n_addresses = possible_addresses.size / sizeof(address_t);
    char text[CHEAT_ENGINE_CHAR_WIDTH];
    for(int i = 0; i < n_addresses && written != CHEAT_ENGINE_RESULT_LIMIT; i++){
        address_t* addr_struct = &((address_t*)possible_addresses.buffer)[i];
        snprintf(text, CHEAT_ENGINE_CHAR_WIDTH - 1, "0x%X n. bytes: %d", addr_struct->address, addr_struct->type);
        text[CHEAT_ENGINE_CHAR_WIDTH-1] = 0;
        drawText(surface, CHEAT_ENGINE_RENDER_MARGIN, CHEAT_ENGINE_RENDER_MARGIN+i*8*2, text);
        written += 1;
    }


    if(written == n_addresses)
        strcpy(text, "END");
    else    
        strcpy(text, "...");

    drawText(surface, (CHEAT_ENGINE_CHAR_WIDTH-3)*8, CHEAT_ENGINE_RENDER_MARGIN+(CHEAT_ENGINE_RESULT_LIMIT-1)*8*2+8, text);

    SDL_UpdateWindowSurface(cheatEngineWindow);
}

void cheatEngineNewSearch(gba_t* gba, u32 value_to_find){
    if(!possible_addresses.buffer)
        vector_init(&possible_addresses, 64);
    else
        possible_addresses.size = 0;

    SCAN_RAM(gba->IWRAM, IWRAM_SIZE, 0x03000000);
    SCAN_RAM(gba->EWRAM, EWRAM_SIZE, 0x02000000);
}

void cheatEngineContinueSearch(gba_t* gba, u32 value_to_find){
    if(!possible_addresses.buffer)
        return;

    vector_t new_vec;
    vector_init(&new_vec, possible_addresses.size);

    for(int i = 0; i < possible_addresses.size / sizeof(address_t); i++){
        address_t* addr_ptr = &((address_t*)possible_addresses.buffer)[i];
        u32 address = addr_ptr->address;
        u8* ram = (address >> 24) == 0x3 ? gba->IWRAM : gba->EWRAM; 
        u32 new_value = 0;
        for(int j = 0; j < addr_ptr->type; j++)
            new_value |= (u32)(ram[(address+j) & ((1 << 24) - 1)]) << (j * 8);
        if(new_value == value_to_find)
            vector_add(&new_vec, addr_ptr, sizeof(address_t));
    }

    vector_free(&possible_addresses);
    memcpy(&possible_addresses, &new_vec, sizeof(vector_t));
}

void cheatEngineWriteToFoundAddresses(gba_t* gba, u32 value_to_write){
    for(int i = 0; i < possible_addresses.size / sizeof(address_t); i++){
        address_t* addr_ptr = &((address_t*)possible_addresses.buffer)[i];
        cheatEngineWriteToAddress(gba, addr_ptr->address, value_to_write, addr_ptr->type);
    }
}

void cheatEngineWriteToAddress(gba_t* gba, u32 address, u32 value_to_write, u8 type){
    u32 old_cycles = gba->cpu.cycles;
    prefetcher_t old_prefetch;
    memcpy(&old_prefetch, &gba->prefetcher, sizeof(prefetcher_t));
    for(int j = 0; j < type; j++){
        gba->cpu.writeByte(&gba->cpu, address+j, value_to_write & 0xFF, true);
        value_to_write >>= 8;
    }
    gba->cpu.cycles = old_cycles;
    memcpy(&gba->prefetcher, &old_prefetch, sizeof(prefetcher_t));
}