#include "integer.h"
#include "SDL_MAINLOOP.h"
#include "ppu.h"
#include "gba.h"
#include "memory.h"
#include "dma.h"

#include "ppu_utils.h"

typedef enum {NO_BG = 0, TILED, AFFINE, BITMAP_3, BITMAP_4, BITMAP_5} bgType;
typedef enum {IN_WIN0 = 0, IN_WIN1 = 1, IN_WINOUT = 2, IN_OBJWIN = 3, WIN_DISABLED = 0xFF} winType; 

void renderLineMode(ppu_t* ppu, bgType* bg_type);
void renderLine(ppu_t* ppu);

void renderLineBitmap3(ppu_t* ppu, u8* blend_info);
void renderLineBitmap4(ppu_t* ppu, u8* blend_info);
void renderLineBitmap5(ppu_t* ppu, u8* blend_info);

void renderLineRegBg(ppu_t* ppu, int bg_idx, u8* blend_info, bool* obj_win_mask);
void renderLineAffBg(ppu_t* ppu, int bg_idx, u8* blend_info, bool* obj_win_mask);
void renderLineSprite(ppu_t* ppu, u16* obj_attr_ptr, u8* blend_info, bool* obj_win_mask);
void getObjMask(ppu_t* ppu, u16* obj_attr_ptr, bool* obj_win_mask);

bool windowShouldDraw(ppu_t* ppu, u8 x, u8 y, int renderType, bool obj_win_mask, winType* winType_out);
u16 applyColorEffect(ppu_t* ppu, u16 inCol, u8 topType, u8 botType, u16 backCol, winType winType);

void updatePPU(gba_t* gba, int cycles){
    ppu_t* ppu = &gba->ppu;
    int inter_cycles = cycles % SCANLINE_CYCLES;
    bool new_isHBlank = inter_cycles >= DRAW_CYCLES;
    bool new_isVBlank = cycles >= VBLANK;
    bool new_isVCount = (ppu->VCOUNT == ((ppu->DISPSTAT >> 8) % 228));
    ppu->VCOUNT = cycles / SCANLINE_CYCLES;

    if(!ppu->isHBlank && new_isHBlank){
        if(ppu->DISPSTAT & (1 << 4)){
            gba->IF |= 0b10;
            checkInterrupts(gba);
        }
        if(ppu->VCOUNT < SCREEN_HEIGHT){
            renderLine(ppu);
            updateHblankDma(gba);
        }
    }

    if((ppu->DISPSTAT & (1 << 5)) && new_isVCount && !ppu->isVCount){
        gba->IF |= 0b100;
        checkInterrupts(gba);
    }

    if(!ppu->isVBlank && new_isVBlank){
        if(ppu->DISPSTAT & (1 << 3)){
            gba->IF |= 0b1;
            checkInterrupts(gba);
        }
        renderPixels();
        updateVblankDma(gba);
        for(int i = 0; i < 2; i++){
            ppu->INTERNAL_BGX[i] = (i32)ppu->BGX[i];
            ppu->INTERNAL_BGY[i] = (i32)ppu->BGY[i];
        }
    }

    ppu->isHBlank = new_isHBlank;
    ppu->isVBlank = new_isVBlank;     
    ppu->isVCount = new_isVCount;
}
  
void renderLine(ppu_t* ppu){
    int y = ppu->VCOUNT;
    bgType bg_type[4];
    switch(ppu->DISPCNT & 0b111){
        case 0:
        bg_type[0] = TILED;
        bg_type[1] = TILED;
        bg_type[2] = TILED;
        bg_type[3] = TILED;
        break;

        case 1:
        bg_type[0] = TILED;
        bg_type[1] = TILED;
        bg_type[2] = AFFINE;
        bg_type[3] = NO_BG;
        break;

        case 2:
        bg_type[0] = NO_BG;
        bg_type[1] = NO_BG;
        bg_type[2] = AFFINE;
        bg_type[3] = AFFINE;
        break;

        case 3:
        bg_type[0] = NO_BG;
        bg_type[1] = NO_BG;
        bg_type[2] = BITMAP_3;
        bg_type[3] = NO_BG;
        break;

        case 4:
        bg_type[0] = NO_BG;
        bg_type[1] = NO_BG;
        bg_type[2] = BITMAP_4;
        bg_type[3] = NO_BG;
        break;

        case 5:
        bg_type[0] = NO_BG;
        bg_type[1] = NO_BG;
        bg_type[2] = BITMAP_5;
        bg_type[3] = NO_BG;
        break;

        default:
        printf("UNSUPPORTED RENDERING %d!\n", ppu->DISPCNT & 0b111);
    }

    renderLineMode(ppu, bg_type);

    for(int i = 0; i < SCREEN_WIDTH; i++){
        int col = rgb555torgb888(pixels[i + y*SCREEN_WIDTH]);
        pixels[i + y * SCREEN_WIDTH] = col;
    }
}

void renderLineRegBg(ppu_t* ppu, int bg_idx, u8* blend_info, bool* obj_win_mask){
    int y = ppu->VCOUNT;
    u8* VRAM = ppu->VRAM;
    u16 bgcnt = ppu->BGCNT[bg_idx];
    u8 char_base_block = (bgcnt >> 2) & 0b11;
    bool mosaic = (bgcnt >> 6) & 1;
    bool color_bpp = (bgcnt >> 7) & 1;
    u8* screen_base_block = &VRAM[((bgcnt >> 8) & 0b11111) << 11];
    u8 bg_size = (bgcnt >> 0xE);
    int bg_width = 256 * (1 + ((bg_size & 1)));
    int bg_height = 256 * (1 + ((bg_size >> 1)));
    winType win_type;

    for(int x = 0; x < SCREEN_WIDTH; x++){
        if(!windowShouldDraw(ppu, x, y, bg_idx, obj_win_mask[x], &win_type))
            continue;
        int offX = (x + (ppu->BGHOFS[bg_idx] & 0x1FF)) % bg_width;
        int offY = (y + (ppu->BGVOFS[bg_idx] & 0x1FF)) % bg_height;
        int sbx = offX / 256;
        int sby = offY / 256;

        int internal_offX = (offX >> 3) % 32;
        int internal_offY = (offY >> 3) % 32;
        u16* sb_map = (u16*)&screen_base_block[sbx * 0x800 + sby * (bg_width << 3)];

        u16 tileData = sb_map[internal_offX + internal_offY * 32];
        int tileIdx = tileData & 0x3FF;
        bool h_flip = (tileData >> 0xA) & 1;
        bool v_flip = (tileData >> 0xB) & 1;
        u8 paletteIdx = tileData >> 12;
        u8 stride = color_bpp ? 64 : 32;
        u8* tilePtr = &VRAM[(char_base_block << 14) + tileIdx*stride];
        u8 px = offX % 8;
        u8 py = offY % 8;

        bool transparent = false;
        int bg_col = getTilePixel(tilePtr, h_flip ? 7 - px : px, v_flip ? 7 - py : py, ppu->PALETTE_RAM, paletteIdx*32, color_bpp, &transparent);
        if(!transparent){
            int out_col = applyColorEffect(ppu, bg_col, bg_idx, blend_info[x], pixels[x + y * SCREEN_WIDTH], win_type);
            pixels[x + y * SCREEN_WIDTH] = out_col;
            blend_info[x] = bg_idx;
        }
    }
}

void renderLineAffBg(ppu_t* ppu, int bg_idx, u8* blend_info, bool* obj_win_mask){
    int y = ppu->VCOUNT;
    int aff_idx = bg_idx-2;
    u8* VRAM = ppu->VRAM;
    u16 bgcnt = ppu->BGCNT[bg_idx];
    u8 char_base_block = (bgcnt >> 2) & 0b11;
    bool mosaic = (bgcnt >> 6) & 1;
    u8* screen_base_block = &VRAM[((bgcnt >> 8) & 0b11111) << 11];
    bool wrapping = (bgcnt >> 0xD) & 1;
    u8 bg_size = (bgcnt >> 0xE);
    int bg_width = 128 << bg_size;
    int bg_height = 128 << bg_size;
    u16* bgp = ppu->BGP;
    i16 dx = bgp[aff_idx*4 + 0];
    i16 dy = bgp[aff_idx*4 + 2];
    i16 dmx = bgp[aff_idx*4 + 1];
    i16 dmy = bgp[aff_idx*4 + 3];
    winType win_type;

    i32 startX = geti28(ppu->INTERNAL_BGX[aff_idx]);
    i32 startY = geti28(ppu->INTERNAL_BGY[aff_idx]);

    for(int x = 0; x < SCREEN_WIDTH; x++){
        i32 offX = startX;
        i32 offY = startY;
        startX = geti28(startX + dx);
        startY = geti28(startY + dy);
        if(!windowShouldDraw(ppu, x, y, bg_idx, obj_win_mask[x], &win_type))
            continue;
        offX >>= 8;
        offY >>= 8;
        if(!wrapping){
            if(offX < 0 || offX >= bg_width)
                continue;
            if(offY < 0 || offY >= bg_height)
                continue;
        } else {
            offX = wrapValue(offX, bg_width);
            offY = wrapValue(offY, bg_height);
        }
        u8 tileIdx = screen_base_block[(offX >> 3) + (offY >> 3) * (bg_width >> 3)];
        u8* tilePtr = &VRAM[(char_base_block << 14) + tileIdx*64];
        u8 px = offX % 8;
        u8 py = offY % 8;

        bool transparent = false;
        int bg_col = getTilePixel(tilePtr, px, py, ppu->PALETTE_RAM, 0, true, &transparent);
        if(!transparent){
            int out_col = applyColorEffect(ppu, bg_col, bg_idx, blend_info[x], pixels[x + y * SCREEN_WIDTH], win_type);
            pixels[x + y * SCREEN_WIDTH] = out_col;
            blend_info[x] = bg_idx;
        }
    }

    ppu->INTERNAL_BGX[aff_idx] = geti28(ppu->INTERNAL_BGX[aff_idx] + dmx);
    ppu->INTERNAL_BGY[aff_idx] = geti28(ppu->INTERNAL_BGY[aff_idx] + dmy);
}

void renderLineMode(ppu_t* ppu, bgType* bg_type){
    int y = ppu->VCOUNT;
    int backdrop_col = getRgb555FromMemory(ppu->PALETTE_RAM, 0);
    for(int x = 0; x < SCREEN_WIDTH; x++)
        pixels[x + y * SCREEN_WIDTH] = backdrop_col;

    u8 blend_info[SCREEN_WIDTH] = { [0 ... 239] = 5 };
    bool obj_win_mask[SCREEN_WIDTH] = {[0 ... 239] = false};

    if(ppu->DISPCNT & 0x8000){
        for(int i = 0; i < 128; i++){
            u16* obj_attr_ptr = (u16*)(&ppu->OAM[i*8]);
            getObjMask(ppu, obj_attr_ptr, obj_win_mask);
        }
    }

    for(int prio = 3; prio >= 0; prio--){
        for(int i = 3; i >= 0; i--){
            bool bg_on = ppu->DISPCNT & (1 << (8 + i));
            u8 bg_prio = ppu->BGCNT[i] & 0b11;
            if(bg_on && bg_prio == prio)
                switch(bg_type[i]){
                    case NO_BG:
                    break;

                    case TILED:
                    renderLineRegBg(ppu, i, blend_info, obj_win_mask);
                    break;

                    case AFFINE:
                    renderLineAffBg(ppu, i, blend_info, obj_win_mask);
                    break;

                    case BITMAP_3:
                    renderLineBitmap3(ppu, blend_info);
                    break;

                    case BITMAP_4:
                    renderLineBitmap4(ppu, blend_info);
                    break;

                    case BITMAP_5:
                    renderLineBitmap5(ppu, blend_info);
                    break;
                }
        }

        if(!(ppu->DISPCNT & (1 << 12)))
            continue;

        for(int i = 127; i >= 0; i--){
            u16* obj_attr_ptr = (u16*)(&ppu->OAM[i*8]);
            u8 obj_prio = (obj_attr_ptr[2] >> 0xA) & 0b11;
            u16 tid = obj_attr_ptr[2] & 0x3FF;
            if(tid < 512 && bg_type[2] >= BITMAP_3)
                continue;
            if(obj_prio == prio)
                renderLineSprite(ppu, obj_attr_ptr, blend_info, obj_win_mask);
        }
    }
}

void renderLineBitmap3(ppu_t* ppu, u8* blend_info){
    int y = ppu->VCOUNT;
    for(int x = 0; x < SCREEN_WIDTH; x++){
        int idx = x + y * SCREEN_WIDTH;
        u16 col555 = getRgb555FromMemory(ppu->VRAM, idx);
        pixels[idx] = col555;
        blend_info[x] = 2;
    }
}

void renderLineBitmap4(ppu_t* ppu, u8* blend_info){
    int y = ppu->VCOUNT;
    int bg_offset = ((bool)(ppu->DISPCNT & (1 << 4))) * 0xA000;
    for(int x = 0; x < SCREEN_WIDTH; x++){
        int idx = x + y * SCREEN_WIDTH;
        u8 palIdx = ppu->VRAM[bg_offset + idx];
        if(!palIdx)
            continue;
        u16 col555 = getRgb555FromMemory(ppu->PALETTE_RAM, palIdx);
        pixels[idx] = col555;
        blend_info[x] = 2;
    }
}

void renderLineBitmap5(ppu_t* ppu, u8* blend_info){
    int y = ppu->VCOUNT;
    if(y >= 128)
        return;
    int bg_offset = ((bool)(ppu->DISPCNT & (1 << 4))) * 0xA000;
    for(int x = 0; x < 160; x++){
        int vram_idx = x + y * 160;
        int screen_idx = x + y * SCREEN_WIDTH;
        u16 col555;
        col555 = getRgb555FromMemory(ppu->VRAM + bg_offset, vram_idx);
        pixels[screen_idx] = col555;
        blend_info[x] = 2;
    }
}

bool windowShouldDraw(ppu_t* ppu, u8 x, u8 y, int renderType, bool obj_win_mask, winType* winType_out){
    bool win_active[2];
    bool obj_win_active = (ppu->DISPCNT >> 15) & 1;
    win_active[0] = (ppu->DISPCNT >> 13) & 1;
    win_active[1] = (ppu->DISPCNT >> 14) & 1;

    if(!win_active[0] && !win_active[1] && !obj_win_active){
        *winType_out = WIN_DISABLED;
        return true;
    }

    for(int i = 0; i < 2; i++){
        if(win_active[i] && isInsideWindow(x, y, ppu->WINH[i], ppu->WINV[i])){
            *winType_out = i; 
            return ppu->WININ & (1 << (renderType + 0x8 * i));
        }
    }

    if(obj_win_active && obj_win_mask){
        *winType_out = IN_OBJWIN;
        return ppu->WINOUT & (1 << (renderType + 0x8));
    }

    // outside case
    *winType_out = IN_WINOUT;
    return ppu->WINOUT & (1 << renderType);
}

void composeDispstat(ppu_t* ppu){
    ppu->DISPSTAT &= 0xFFFFFFF8;
    if(ppu->VCOUNT >= SCREEN_HEIGHT)
        ppu->DISPSTAT |= 0b1;
    if(ppu->isHBlank)
        ppu->DISPSTAT |= 0b10;
    if(ppu->VCOUNT == (ppu->DISPSTAT >> 8))
        ppu->DISPSTAT |= 0b100;
}

u16 applyColorEffect(ppu_t* ppu, u16 inCol, u8 topType, u8 botType, u16 backCol, winType winType){
    u16 bldcnt = ppu->BLDCNT;
    if(winType != WIN_DISABLED){
        if(winType <= IN_WIN1)
            if(!(ppu->WININ & (1 << (5 + 8 * winType))))
                return inCol;
        if(winType == IN_OBJWIN && !(ppu->WINOUT & (1 << 0xD)))
            return inCol;
        if(winType == IN_WINOUT && !(ppu->WINOUT & (1 << 5)))
            return inCol;
    }

    u8 blend_mode = (bldcnt >> 6) & 0b11;
    switch(blend_mode){ 
        case 0b00:
        return inCol;
        break;

        case 0b01:
        return isBldTop(bldcnt, topType) && isBldBot(bldcnt, botType) ? colorBlend(ppu->BLDALPHA, inCol, backCol) : inCol;
        break;

        case 0b10:
        return isBldTop(bldcnt, topType) ? brightnessBlend(ppu->BLDY, inCol, 1) : inCol;
        break;

        case 0b11:
        return isBldTop(bldcnt, topType) ? brightnessBlend(ppu->BLDY, inCol, -1) : inCol;
    }
}

void renderLineSprite(ppu_t* ppu, u16* obj_attr_ptr, u8* blend_info, bool* obj_win_mask){
    int y = ppu->VCOUNT;
    bool mapping_1d = ppu->DISPCNT & (1 << 6);
    u8* VRAM = ppu->VRAM;
    u8* PALETTE_RAM = ppu->PALETTE_RAM;
    u8* OAM = ppu->OAM;

    u8 mode = (obj_attr_ptr[0] >> 8) & 0b11;
    if(mode == 0b10)
        return;

    u8 gfx_mode = (obj_attr_ptr[0] >> 0xA) & 0b11;
    if(gfx_mode == 0b10)
        return;

    bool is_8bpp = (obj_attr_ptr[0] >> 0xD) & 1;
    bool h_flip;
    bool v_flip;
    int sprite_width;
    int sprite_height;
    int sprite_x;
    int sprite_y;
    int signed_sprite_x;
    int signed_sprite_y;
    int render_limit_width;
    int render_limit_height;
    u8 stride = is_8bpp ? 64 : 32;
    bool alpha_blending = ((obj_attr_ptr[0] >> 0xA) & 0b11) == 0b01;
    i16 aff_mat[4];
    winType win_type;
    getSpriteDim(obj_attr_ptr, &sprite_width, &sprite_height);
    getSpritePos(obj_attr_ptr, &sprite_x, &sprite_y);
    signed_sprite_x = sprite_x & 0x100 ? 0xFFFFFE00 | sprite_x : sprite_x;
    render_limit_width = sprite_width;
    render_limit_height = sprite_height;

    if(mode == 0b00){
        h_flip = (obj_attr_ptr[1] >> 0xC) & 1;
        v_flip = (obj_attr_ptr[1] >> 0xD) & 1;
    } else {
        u8 mat_idx = (obj_attr_ptr[1] >> 0x9) & 0x1F;
        aff_mat[0] = *(i16*)(&OAM[mat_idx*0x20 + 0x06]);
        aff_mat[1] = *(i16*)(&OAM[mat_idx*0x20 + 0x0E]);
        aff_mat[2] = *(i16*)(&OAM[mat_idx*0x20 + 0x16]);
        aff_mat[3] = *(i16*)(&OAM[mat_idx*0x20 + 0x1E]);
        if(mode == 0b11){
            render_limit_width <<= 1;
            render_limit_height <<= 1;
        }
    }

    int y_min = sprite_y & 255;
    int y_max = (y_min + render_limit_height) & 255;
    if(y_max < y_min)
        y_min -= 256;

    if(y < y_min || y >= y_max)
        return;

    u16 tid = obj_attr_ptr[2] & 0x3FF;
    u8 palBank = obj_attr_ptr[2] >> 0xC;

    for(int x = 0; x < render_limit_width; x++){
        int actual_sprite_x = signed_sprite_x + x;
        u8* blend_info_ptr = &blend_info[actual_sprite_x];
        if(actual_sprite_x < 0 || actual_sprite_x >= SCREEN_WIDTH)
            continue;
        if(!windowShouldDraw(ppu, actual_sprite_x, y, 4, obj_win_mask[actual_sprite_x], &win_type))
            continue;
        bool transparent;
        u8 px;
        u8 py;
        u8 offX;
        u8 offY;
        if(mode == 0b00){
            px = h_flip ? (sprite_width - 1 - x) % 8 : x % 8;
            py = v_flip ? (sprite_height - 1 - (y - y_min)) % 8 : (y - y_min) % 8;
            offX = h_flip ? (sprite_width - 1 - x) >> 3 : x >> 3;
            offY = v_flip ? (sprite_height - 1 - (y - y_min)) >> 3 : (y - y_min) >> 3;
        } else {
            i32 center_x = (sprite_width << 7);
            i32 center_y = (sprite_height << 7);
            i32 aff_x = (x << 8) - center_x;
            i32 aff_y = ((y - y_min) << 8) - center_y;
            if(mode == 0b11){
                aff_x -= sprite_width << 7;
                aff_y -= sprite_height << 7;
            }
            applyRotScaleMat(aff_mat, &aff_x, &aff_y);
            aff_x += center_x;
            aff_y += center_y;
            aff_x >>= 8;
            aff_y >>= 8;
            if(aff_x < 0 || aff_x >= sprite_width)
                continue;
            if(aff_y < 0 || aff_y >= sprite_height)
                continue;
            offX = aff_x >> 3;
            offY = aff_y >> 3;
            px = aff_x % 8;
            py = aff_y % 8;
        }
        u8* tilePtr = &VRAM[0x10000 +  tid*32];
        if(mapping_1d)
            tilePtr += (offX + offY * (sprite_width >> 3)) * stride;
        else
            tilePtr += (offX * stride + offY * 32 * 32);
        int sprite_col = getTilePixel(tilePtr, px, py, &PALETTE_RAM[512], palBank*32, is_8bpp, &transparent);
        if(!transparent) {
            int out_col;
            if(gfx_mode == 0b01 && isBldBot(ppu->BLDCNT, blend_info[actual_sprite_x]))
                out_col = colorBlend(ppu->BLDALPHA, sprite_col, pixels[actual_sprite_x + y * SCREEN_WIDTH]);
            else 
                out_col = applyColorEffect(ppu, sprite_col, 4, blend_info[actual_sprite_x], pixels[actual_sprite_x + y * SCREEN_WIDTH], win_type);
            pixels[actual_sprite_x + y * SCREEN_WIDTH] = out_col;
            blend_info[actual_sprite_x] = 4;
        }
    }
}

void getObjMask(ppu_t* ppu, u16* obj_attr_ptr, bool* obj_win_mask){
    int y = ppu->VCOUNT;
    bool mapping_1d = ppu->DISPCNT & (1 << 6);
    u8* VRAM = ppu->VRAM;
    u8* PALETTE_RAM = ppu->PALETTE_RAM;
    u8* OAM = ppu->OAM;

    u8 mode = (obj_attr_ptr[0] >> 8) & 0b11;
    if(mode == 0b10)
        return;

    u8 gfx_mode = (obj_attr_ptr[0] >> 0xA) & 0b11;
    if(gfx_mode != 0b10)
        return;

    bool is_8bpp = (obj_attr_ptr[0] >> 0xD) & 1;
    bool h_flip;
    bool v_flip;
    int sprite_width;
    int sprite_height;
    int sprite_x;
    int sprite_y;
    int signed_sprite_x;
    int signed_sprite_y;
    int render_limit_width;
    int render_limit_height;
    u8 stride = is_8bpp ? 64 : 32;
    bool alpha_blending = ((obj_attr_ptr[0] >> 0xA) & 0b11) == 0b01;
    i16 aff_mat[4];
    getSpriteDim(obj_attr_ptr, &sprite_width, &sprite_height);
    getSpritePos(obj_attr_ptr, &sprite_x, &sprite_y);
    signed_sprite_x = sprite_x & 0x100 ? 0xFFFFFE00 | sprite_x : sprite_x;
    render_limit_width = sprite_width;
    render_limit_height = sprite_height;

    if(mode == 0b00){
        h_flip = (obj_attr_ptr[1] >> 0xC) & 1;
        v_flip = (obj_attr_ptr[1] >> 0xD) & 1;
    } else {
        u8 mat_idx = (obj_attr_ptr[1] >> 0x9) & 0x1F;
        aff_mat[0] = *(i16*)(&OAM[mat_idx*0x20 + 0x06]);
        aff_mat[1] = *(i16*)(&OAM[mat_idx*0x20 + 0x0E]);
        aff_mat[2] = *(i16*)(&OAM[mat_idx*0x20 + 0x16]);
        aff_mat[3] = *(i16*)(&OAM[mat_idx*0x20 + 0x1E]);
        if(mode == 0b11){
            render_limit_width <<= 1;
            render_limit_height <<= 1;
        }
    }

    int y_min = sprite_y & 255;
    int y_max = (y_min + render_limit_height) & 255;
    if(y_max < y_min)
        y_min -= 256;

    if(y < y_min || y >= y_max)
        return;

    u16 tid = obj_attr_ptr[2] & 0x3FF;
    u8 palBank = obj_attr_ptr[2] >> 0xC;

    for(int x = 0; x < render_limit_width; x++){
        int actual_sprite_x = signed_sprite_x + x;
        if(actual_sprite_x < 0 || actual_sprite_x >= SCREEN_WIDTH)
            continue;
        bool transparent;
        u8 px;
        u8 py;
        u8 offX;
        u8 offY;
        if(mode == 0b00){
            px = h_flip ? (sprite_width - 1 - x) % 8 : x % 8;
            py = v_flip ? (sprite_height - 1 - (y - y_min)) % 8 : (y - y_min) % 8;
            offX = h_flip ? (sprite_width - 1 - x) >> 3 : x >> 3;
            offY = v_flip ? (sprite_height - 1 - (y - y_min)) >> 3 : (y - y_min) >> 3;
        } else {
            i32 center_x = (sprite_width << 7);
            i32 center_y = (sprite_height << 7);
            i32 aff_x = (x << 8) - center_x;
            i32 aff_y = ((y - y_min) << 8) - center_y;
            if(mode == 0b11){
                aff_x -= sprite_width << 7;
                aff_y -= sprite_height << 7;
            }
            applyRotScaleMat(aff_mat, &aff_x, &aff_y);
            aff_x += center_x;
            aff_y += center_y;
            aff_x >>= 8;
            aff_y >>= 8;
            if(aff_x < 0 || aff_x >= sprite_width)
                continue;
            if(aff_y < 0 || aff_y >= sprite_height)
                continue;
            offX = aff_x >> 3;
            offY = aff_y >> 3;
            px = aff_x % 8;
            py = aff_y % 8;
        }
        u8* tilePtr = &VRAM[0x10000 +  tid*32];
        if(mapping_1d)
            tilePtr += (offX + offY * (sprite_width >> 3)) * stride;
        else
            tilePtr += (offX * stride + offY * 32 * 32);
        getTilePixel(tilePtr, px, py, &PALETTE_RAM[512], palBank*32, is_8bpp, &transparent);
        if(!transparent)
            obj_win_mask[actual_sprite_x] = true;
    }
}

void drawPaletteRam(SDL_Window* win, u8* ptr){
    SDL_Surface* surface = SDL_GetWindowSurface(win);

    for(int y = 0; y < 16; y++){
        for(int x = 0; x < 16; x++){
            int idx = x + y * 16;
            u16 col555 = getRgb555FromMemory(ptr, idx);
            u32 col888 = rgb555torgb888(col555);
            SDL_Rect rect = {.x = x*16, .y = y*16, .w = 16, .h = 16};
            SDL_FillRect(surface, &rect, col888);
        }
    }
}

void drawTileMap(SDL_Window* win, ppu_t* ppu){
    SDL_Surface* surface = SDL_GetWindowSurface(win);
    int* pixels = (int*)surface->pixels;

    for(int tileY = 0; tileY < 64; tileY++){
        for(int tileX = 0; tileX < 32; tileX++){
            int tileIdx = tileX + tileY * 32;
            for(int y = 0; y < 8; y++){
                for(int x = 0; x < 8; x++){
                    u8* tilePtr = &ppu->VRAM[tileIdx*32];
                    bool transparent;
                    int col = rgb555torgb888(getTilePixel(tilePtr, x, y, ppu->PALETTE_RAM, 0, false, &transparent));
                    int pixIdx = (x + tileX*8) + (y + tileY*8) * 256;
                    pixels[pixIdx] = col;
                }
            }
        }
    }
}