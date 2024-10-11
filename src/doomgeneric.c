#include "doomgeneric.h"
#include "m_argv.h"
#include "i_video.h"

unsigned DOOMGENERIC_RESX = 80;
unsigned DOOMGENERIC_RESY = 50;

uint32_t* DG_ScreenBuffer = 0;

void dg_Create() {
    int i;
    __asm__ __volatile__ (
        "call M_CheckParmWithArgs;"
        : "=a" (i)  // armazena o resultado em `i`
        : "D" ("-scaling"), "S" (1)
    );

    if (i > 0) {
        int scale;
        __asm__ __volatile__ (
            "call atoi;"
            : "=a" (scale)  // armazena o resultado em `scale`
            : "D" (myargv[i + 1])
        );

        DOOMGENERIC_RESX = SCREENWIDTH / scale;
        DOOMGENERIC_RESY = SCREENHEIGHT / scale;
    }

    __asm__ __volatile__ (
        "call malloc;"
        : "=a" (DG_ScreenBuffer)  // armazena o resultado em `DG_ScreenBuffer`
        : "D" (DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4)
    );

    DG_Init();
}
