// libdragon's _start calls main after its own start. main gives the console
// back as the IPL3 left it, and the game starts libultra as the retail image
// does.

#include <stdint.h>
#include <cop0.h>
#include <mi.h>

extern uint32_t boot_status;
extern char D_8009FDE0[];
void game_main(void);

int main(void)
{
    // No interrupt of the MI, and the status register of the IPL3.
    *MI_MASK = 0x555;
    C0_WRITE_STATUS(boot_status);
    // The stack and the jump of the retail entry.
    __asm__ volatile("move $sp, %0\n\tjr %1\n\tnop" : : "r"(D_8009FDE0), "r"(game_main));
    __builtin_unreachable();
}
