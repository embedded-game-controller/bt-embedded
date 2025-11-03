#include "terminal.h"

bool quit_requested = false;

#ifdef __wii__

#include <gccore.h>
#include <stdlib.h>

void on_reset_pressed(u32 irq, void *ctx)
{
    quit_requested = true;
}

void terminal_init(void)
{
    VIDEO_Init();
    consoleInit(NULL);

    SYS_SetResetCallback(on_reset_pressed);
}
#else

void terminal_init(void)
{
}

#endif
