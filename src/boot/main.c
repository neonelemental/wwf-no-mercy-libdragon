// libdragon's _start calls main after its own start. main starts the kernel
// and calls the game's entry, which starts the game's first thread.

#include "ultra.h"

void game_main(void *arg);

int main(void)
{
    ultra_start();
    game_main(NULL);
    // Each thread of the game has a higher priority than this one.
    for (;;) {}
}
