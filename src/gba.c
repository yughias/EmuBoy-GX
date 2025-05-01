#include "gba.h"
#include "gamepak.h"

#include <SDL2/SDL.h>

#include <string.h>
#include <stdlib.h>

void checkInterrupts(gba_t* gba){
    if(!(gba->IME & 1) || gba->cpu.irq_disable)
        return;
    bool irq_mask = gba->IE & gba->IF & 0x3FFF;
    if(irq_mask == 0)
        return;

    arm7tdmi_trigger_exception(&gba->cpu, 0x18, 0x12);
}

// TO-DO
// i should check for interrupt everytime I change SPSR because i could pass from interrupt disabled to interrupt enabled

void emulateGba(gba_t* gba){
    gba->KEYINPUT = update_keypad();

    u32 prev_cycles = gba->cpu.cycles;
    while(gba->cpu.cycles < CYCLES_PER_FRAME){
        u32 elapsed = 0;

        while(elapsed < gba->scheduler_head->remaining){
            if(gba->HALTCNT && (gba->HALTCNT = !(gba->IF & gba->IE & 0x3FFF))){
                gba->cpu.cycles += gba->scheduler_head->remaining - elapsed;
            } else {
                arm7tdmi_step(&gba->cpu);
                checkInterrupts(gba);
            }
            elapsed = gba->cpu.cycles - prev_cycles;
        }

        stepScheduler(gba, &gba->scheduler_head, elapsed);
        prev_cycles = gba->cpu.cycles;
    }

    gba->cpu.cycles -= CYCLES_PER_FRAME;
    gba->clock_before_scheduling -= CYCLES_PER_FRAME;
    gba->prefetcher.cycle_counter -= CYCLES_PER_FRAME;
    gba->frame_clock += CYCLES_PER_FRAME;
}

void initGba(gba_t* gba, const char* biosFilename, const char* romFilename, SDL_AudioSpec audioSpec){
    memset(gba, 0, sizeof(gba_t));

    loadBios(biosFilename, &gba->bios.data);
    loadGamePak(&gba->gamepak, romFilename);

    apu_t* apu = &gba->apu;
    gba->apu.audioSpec = audioSpec;

    resetGba(gba);
}

void resetGba(gba_t* gba){
    bios_t tmp_bios;
    gamepak_t tmp_gamepak;
    SDL_AudioSpec tmp_audioSpec;

    memcpy(&tmp_bios, &gba->bios, sizeof(bios_t));
    memcpy(&tmp_audioSpec, &gba->apu.audioSpec, sizeof(SDL_AudioSpec));
    memcpy(&tmp_gamepak, &gba->gamepak, sizeof(gamepak_t));
    memset(gba, 0, sizeof(gba_t));
    memcpy(&gba->bios, &tmp_bios, sizeof(bios_t));
    memcpy(&gba->gamepak, &tmp_gamepak, sizeof(gamepak_t));
    memcpy(&gba->apu.audioSpec, &tmp_audioSpec, sizeof(SDL_AudioSpec));

    for(int i = 0; i < 2; i++){
        gba->ppu.BGP[0 + 4*i] = 0x100;
        gba->ppu.BGP[1 + 4*i] = 0;
        gba->ppu.BGP[2 + 4*i] = 0;
        gba->ppu.BGP[3 + 4*i] = 0x100;
    }

    #ifdef EMSCRIPTEN
    gba->cpu.r[13] = 0x3007F00;
    gba->cpu.r[15] = 0x8000000;
    gba->cpu.irq_r[1] = 0x0;
    gba->cpu.irq_r[0] = 0x03007FA0;
    gba->cpu.usr_r[13] = 0x03007F00;
    gba->cpu.svc_r[0] = 0x03007FE0;
    gba->cpu.mode_bits = 0x1F;
    #else
    gba->cpu.mode_bits = 0x13;
    gba->cpu.fiq_disable = true;
    gba->cpu.irq_disable = true;
    #endif
    gba->cpu.readByte = readByte;
    gba->cpu.readHalfWord = readHalfWord;
    gba->cpu.readWord = readWord;
    gba->cpu.writeByte = writeByte;
    gba->cpu.writeHalfWord = writeHalfWord;
    gba->cpu.writeWord = writeWord;
    gba->cpu.master = gba;

    gba->active_dma = DMA_OFF;

    gba->KEYINPUT = 0xFFFF;
    gba->RCNT = 0x8000;
    gba->apu.SOUNDBIAS = 0x200;
    updateWaitStates(&gba->gamepak, &gba->cpu, gba->WAITCNT);

    arm7tdmi_pipeline_refill(&gba->cpu);

    scheduler_t* block = occupySchedulerBlock(gba->scheduler_pool, GBA_SCHEDULER_POOL_SIZE);
    block->remaining = DRAW_CYCLES;
    block->event = event_startHBlank;
    addEventToScheduler(&gba->scheduler_head, block);

    apu_t* apu = &gba->apu;
    apu->sound_channels_amplifier_left = 1;
    apu->sound_channels_amplifier_right = 1;
    if(apu->audioSpec.freq){
        apu->samplePushRate = CYCLES_PER_FRAME * REFRESH_RATE / apu->audioSpec.freq;
        createAndAddEventWith0Args(&gba->scheduler_head, gba->scheduler_pool, GBA_SCHEDULER_POOL_SIZE, event_pushSampleToAudioDevice, gba->apu.samplePushRate);
    }
}

void freeGba(gba_t* gba){
    if(gba->bios.data)
        free(gba->bios.data);

    freeGamePak(&gba->gamepak);
}