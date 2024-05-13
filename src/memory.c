#include "hardware.h"
#include "arm7tdmi/arm7tdmi.h"

#include <stdio.h>
#include <stdlib.h>

u8 WRAM_BOARD[1 << 18];
u8 WRAM_CHIP[1 << 15];

u8 PALETTE_RAM[1 << 10];
u8 VRAM[0x18000];
u8 OAM[1 << 10];

u8 SRAM[1 << 16];

u8* BIOS;
u8* ROM;
size_t ROM_SIZE;

u16 DISPCNT;
u16 DISPSTAT;
u16 VCOUNT;

u16 BGCNT[4];
u16 BGHOFS[4];
u16 BGVOFS[4];

u32 BGX[2];
u32 BGY[2];
u32 INTERNAL_BGX[2];
u32 INTERNAL_BGY[2];
u16 BGP[2*4];

u16 KEYINPUT = 0xFFFF;
u16 KEYCNT = 0x0;

u32 DMASAD[4];
u32 DMADAD[4];
u32 DMACNT[4];

u16 WINH[2];
u16 WINV[2];
u16 WININ;
u16 WINOUT;

u16 IE;
u16 IF;
u16 IME;

u16 BLDCNT;
u16 BLDALPHA;
u16 BLDY; 

u16 RCNT = 0x8000;
u8 POSTFLG;
u16 SOUNDBIAS;
u16 WAITCNT;

bool HALTCNT;

void writeByte(u32 addr, u8 val){
    if(addr >= 0x2000000 && addr < 0x3000000){
        addr &= 0x003FFFF;
        WRAM_BOARD[addr & 0x3FFFF] = val;
        return;
    }
    
    if(addr >= 0x3000000 & addr < 0x4000000){
        addr &= 0x7FFF;
        WRAM_CHIP[addr] = val;
        return;
    }

    if(addr >= 0x4000000 && addr < 0x4000002){
        addr -= 0x4000000;
        ((u8*)&DISPCNT)[addr] = val;
        return;
    }

    if(addr >= 0x4000002 && addr < 0x4000004){
        return;
    }

    for(int i = 0; i < 4; i++){
        if(addr >= 0x4000008 + 0x2*i && addr < 0x4000008 + 0x2*(i+1)){
            addr -= 0x4000008 + 0x2*i;
            ((u8*)&BGCNT[i])[addr] = val;
            return;
        }
    }

    if(addr >= 0x4000004 && addr < 0x4000006){
        addr -= 0x4000004;
        ((u8*)&DISPSTAT)[addr] = val;
        return;
    }

    for(int i = 0; i < 4; i++){
        if(addr >= 0x4000010 + 0x4*i && addr < 0x4000010 + 0x4*i + 2){
            addr -= 0x4000010 + 0x4*i;
            ((u8*)&BGHOFS[i])[addr] = val;
            return;    
        }
    }


    for(int i = 0; i < 4; i++){
        if(addr >= 0x4000012 + 0x4*i && addr < 0x4000012 + 0x4*i + 2){
            addr -= 0x4000012 + 0x4*i;
            ((u8*)&BGVOFS[i])[addr] = val;
            return;    
        }
    }

    for(int i = 0; i < 2; i++){
        if(addr >= 0x4000028 + 0x10*i && addr < 0x4000028 + 0x10*i + 4){
            addr -= 0x4000028 + 0x10*i;
            ((u8*)&BGX[i])[addr] = val;
            INTERNAL_BGX[i] = BGX[i];
            return; 
        }
    }

    for(int i = 0; i < 2; i++){
        if(addr >= 0x400002C + 0x10*i && addr < 0x400002C + 0x10*i + 4){
            addr -= 0x400002C + 0x10*i;
            ((u8*)&BGY[i])[addr] = val;
            INTERNAL_BGY[i] = BGY[i];
            return; 
        }
    }

    for(int i = 0; i < 2; i++){
        for(int j = 0; j < 4; j++){
            if(addr >= 0x4000020 + 0x10*i + 0x02*j && addr < 0x4000020 + 0x10*i + 0x02*j + 2){
                addr -= 0x4000020 + 0x10*i + 0x02*j;
                ((u8*)&BGP[i*4 + j])[addr] = val;
                return;
            }
        }
    }


    for(int i = 0; i < 2; i++){
        if(addr >= 0x4000040 + 0x2*i && addr < 0x4000040 + 0x2*(i+1)){
            addr -= 0x4000040 + 0x2*i;
            ((u8*)&WINH[i])[addr] = val;
            return;
        }
    }

    for(int i = 0; i < 2; i++){
        if(addr >= 0x4000044 + 0x2*i && addr < 0x4000044 + 0x2*(i+1)){
            addr -= 0x4000044 + 0x2*i;
            ((u8*)&WINV[i])[addr] = val;
            return;
        }
    }

    if(addr >= 0x4000048 && addr < 0x400004A){
        addr -= 0x4000048;
        ((u8*)&WININ)[addr] = val;
        return;
    }

    if(addr >= 0x400004A && addr < 0x400004C){
        addr -= 0x400004A;
        ((u8*)&WINOUT)[addr] = val;
        return;
    }

    if(addr >= 0x4000050 && addr < 0x4000052){
        addr -= 0x4000050;
        ((u8*)&BLDCNT)[addr] = val;
        return;
    }

    if(addr >= 0x4000052 && addr < 0x4000054){
        addr -= 0x4000052;
        ((u8*)&BLDALPHA)[addr] = val;
        return;
    }

    if(addr >= 0x4000054 && addr < 0x4000056){
        addr -= 0x4000054;
        ((u8*)&BLDY)[addr] = val;
        return;
    }

    if(addr >= 0x4000088 && addr < 0x400008A){
        addr -= 0x4000088;
        ((u8*)&SOUNDBIAS)[addr] = val;
        return;
    }

    for(int i = 0; i < 4; i++){
        if(addr >= 0x40000B0 + 0xC*i && addr < 0x40000B0 + 0xC*(i+1)){
            addr -= 0x40000B0 + 0xC*i;
           
            if(addr < 0x4){
                ((u8*)&DMASAD[i])[addr] = val;
                return;
            }

            if(addr < 0x8){
                ((u8*)&DMADAD[i])[addr - 0x4] = val;
                return;
            }

            if(addr < 0xC){
                ((u8*)&DMACNT[i])[addr - 0x8] = val;
                if(DMACNT[i] >> 31)
                    triggerDma(i);
                return;
            }          
        }
    }

    for(int i = 0; i < 4; i++){
        if(addr >= 0x4000100 + i*4 && addr < 0x4000100 + (i+1)*4){
            addr -=  0x4000100 + i*4;
            u32 old_TMCNT = timers[i].TMCNT;
            ((u8*)&timers[i].TMCNT)[addr] = val;
            if(!((old_TMCNT >> 16) & 0x80) && ((timers[i].TMCNT >> 16) & 0x80)){
                triggerTimer(&timers[i]);
            }
            return;
        }
    }

    if(addr >= 0x4000134 && addr < 0x4000136){
        addr -= 0x4000134;
        ((u8*)&RCNT)[addr] = val;
        return;
    }

    if(addr >= 0x4000200 && addr < 0x4000202){
        addr -= 0x4000200;
        ((u8*)&IE)[addr] = val;
        return;
    }

    
    if(addr >= 0x4000202 && addr < 0x4000204){
        addr -= 0x4000202;
        ((u8*)&IF)[addr] &= ~(val);
        return;
    }

    if(addr >= 0x4000204 && addr < 0x4000206){
        addr -= 0x4000204;
        ((u8*)&WAITCNT)[addr] = val;
        return;
    }

    if(addr >= 0x4000208 && addr < 0x400020A){
        addr -= 0x4000208;
        ((u8*)&IME)[addr] = val;
        return;
    }

    if(addr == 0x4000300){
        POSTFLG = val;
        return;
    }

    if(addr == 0x4000301){
        HALTCNT = true;
        return;
    }

    if(addr >= 0x5000000 && addr < 0x6000000){
        addr &= 0x3FF;
        PALETTE_RAM[addr] = val;
        return;
    }

    if(addr >= 0x6000000 && addr < 0x6018000){
        addr -= 0x6000000;
        VRAM[addr] = val;
        VRAM[(addr & ~(0b1)) + 1] = val;
        return;
    }

    if(addr >= 0x7000000 && addr < 0x8000000){
        addr -= 0x7000000;
        addr &= 0x3FF;
        OAM[addr] = val;
        return;
    }

    if(addr >= 0xE000000 && addr < 0xE010000){
        addr -= 0xE000000;
        SRAM[addr] = val;
        return;
    }

    //printf("WRONG WRITE %X %X\n", addr, cpu.r[15]);
    //arm7tdmi_print(&cpu);
    //exit(0);
}

u8 readByte(u32 addr){
    if(addr < 0x02000000)
        return BIOS[addr & 0x3FFF];

    if(addr >= 0x2000000 && addr < 0x3000000){
        addr -= 0x2000000;
        return WRAM_BOARD[addr & 0x3FFFF];
    }

    if(addr >= 0x3000000 && addr < 0x4000000){
        addr &= 0x7FFF;
        return WRAM_CHIP[addr];
    }

    for(int i = 0; i < 4; i++){
        if(addr >= 0x4000008 + 0x2*i && addr < 0x4000008 + 0x2*(i+1)){
            addr -= 0x4000008 + 0x2*i;
            return ((u8*)&BGCNT[i])[addr];
        }
    }

    if(addr >= 0x4000048 && addr < 0x400004A){
        addr -= 0x4000048;
        return ((u8*)&WININ)[addr];
    }

    if(addr >= 0x4000088 && addr < 0x400008A){
        addr -= 0x4000088;
        return ((u8*)&SOUNDBIAS)[addr];
    }

    if(addr >= 0x400004A && addr < 0x400004C){
        addr -= 0x400004A;
        return ((u8*)&WINOUT)[addr];
    }

    for(int i = 0; i < 4; i++){
        if(addr >= 0x40000BA + 0xC*i && addr < 0x40000BA + 0xC*i + 2){    
            addr -= 0x40000BA + 0xC*i;
            return ((u8*)&DMACNT[i])[addr];
        }
    }

    for(int i = 0; i < 4; i++){
        if(addr >= 0x4000100 + i*4 && addr < 0x4000100 + (i+1)*4){
            addr -=  0x4000100 + i*4;
            if(addr < 2)
                return ((u8*)&timers[i].counter)[addr];
            else
                return ((u8*)&timers[i].TMCNT)[addr];
        }
    }

    if(addr >= 0x4000130 && addr < 0x4000132){
        addr -= 0x4000130;
        return ((u8*)&KEYINPUT)[addr];
    }

    if(addr >= 0x4000132 && addr < 0x4000134){
        addr -= 0x4000132;
        return ((u8*)&KEYCNT)[addr];
    }

    if(addr >= 0x4000000 && addr < 0x4000002){
        addr -= 0x4000000;
        return ((u8*)&DISPCNT)[addr];
    }

    if(addr >= 0x4000004 && addr < 0x4000006){
        addr -= 0x4000004;
        composeDispstat();
        return ((u8*)&DISPSTAT)[addr];
    }

    if(addr >= 0x4000006 && addr < 0x4000008){
        addr -= 0x4000006;
        return ((u8*)&VCOUNT)[addr];
    }

    
    if(addr >= 0x4000050 && addr < 0x4000052){
        addr -= 0x4000050;
        return ((u8*)&BLDCNT)[addr];
    }

    if(addr >= 0x4000052 && addr < 0x4000054){
        addr -= 0x4000052;
        return ((u8*)&BLDALPHA)[addr];
    }

    if(addr >= 0x4000134 && addr < 0x4000136){
        addr -= 0x4000134;
        return ((u8*)&RCNT)[addr];
    }

    if(addr >= 0x4000200 && addr < 0x4000202){
        addr -= 0x4000200;
        return ((u8*)&IE)[addr];
    }

    
    if(addr >= 0x4000202 && addr < 0x4000204){
        addr -= 0x4000202;
        return ((u8*)&IF)[addr];
    }

    if(addr >= 0x4000204 && addr < 0x4000206){
        addr -= 0x4000204;
        return ((u8*)&WAITCNT)[addr];
    }

    if(addr >= 0x4000208 && addr < 0x400020A){
        addr -= 0x4000208;
        return ((u8*)&IME)[addr];
    }

    if(addr == 0x4000300){
        return POSTFLG;
    }

    if(addr >= 0x5000000 && addr < 0x6000000){
        addr &= 0x3FF;
        return PALETTE_RAM[addr];
    }

    if(addr >= 0x6000000 && addr < 0x6018000){
        addr -= 0x6000000;
        return VRAM[addr];
    }

    if(addr >= 0x7000000 && addr < 0x8000000){
        addr -= 0x7000000;
        addr &= 0x3FF;
        return OAM[addr];
    }


    if(addr >= 0x8000000 && addr < 0x0E000000){
        addr -= 0x8000000;
        if(addr >= ROM_SIZE){
            return 0x00;
        }
        return ROM[addr];
    }

    if(addr == 0x0E000000)
        return 0x62;
    
    if(addr == 0x0E000001)
        return 0x13;

    if(addr >= 0xE000000 && addr < 0xE010000){
        addr -= 0xE000000;
        addr &= 0xFFFF;
        return SRAM[addr];
    }

    //printf("WRONG READ %X %X\n", addr, cpu.r[15]);
    //arm7tdmi_print(&cpu);
    //exit(0);
    return 0x00;
}

void loadBios(const char* filename){
    FILE* fptr = fopen(filename, "rb");
    if(!fptr){
        printf("can't open bios\n");
        return;
    }

    BIOS = (u8*)malloc(BIOS_SIZE);

    fread(BIOS, 1, BIOS_SIZE, fptr);

    fclose(fptr);
}

void loadRom(const char* filename){
    FILE* fptr = fopen(filename, "rb");
    if(!fptr){
        printf("can't open rom\n");
        return;
    }

    fseek(fptr, 0, SEEK_END);
    ROM_SIZE = ftell(fptr);
    rewind(fptr);

    ROM = (u8*)malloc(ROM_SIZE);

    fread(ROM, 1, ROM_SIZE, fptr);

    fclose(fptr);
}