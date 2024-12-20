#ifndef __TIMER_H__
#define __TIMER_H__

#include "integer.h"

typedef struct gba_t gba_t;
typedef struct scheduler_t scheduler_t;

typedef struct gba_tmr_t
{
    u64 started_clock;
    u32 started_value;
    u32 counter;
    u8 speed_shift;
    u32 TMCNT;

    scheduler_t* scheduled_event;
} gba_tmr_t;

void triggerTimer(gba_t* gba, int i);
void updateTimerCounter(gba_t* gba, int i);
void descheduleTimer(gba_t* gba, int i);
void disableCascadeModeTimer(gba_t* gba, int i);

void event_timerOverflow(gba_t* gba, u32 i);

#endif