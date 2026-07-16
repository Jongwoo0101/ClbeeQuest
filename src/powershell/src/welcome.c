#include "welcome.h"

#include <stdio.h>
#include <unistd.h>

#include "tuk_shell.h"

void print_welcome_screen(void)
{
    /* 가상 OS 부팅 시퀀스 느낌을 위한 딜레이와 메시지 */
    printf(TUK_COLOR_SKY "Booting TUK-OS (x86_64-apple-darwin) ...\n" TUK_COLOR_RESET);
    usleep(200000);
    printf(TUK_COLOR_SKY "Loading kernel modules" TUK_COLOR_RESET " ... [OK]\n");
    usleep(150000);
    printf(TUK_COLOR_SKY "Mounting virtual file systems" TUK_COLOR_RESET " ... [OK]\n");
    usleep(150000);
    printf(TUK_COLOR_SKY "Starting background services" TUK_COLOR_RESET " ... [OK]\n");
    usleep(100000);
    printf("\n");

    /* 한국공학대학교 결정체(육각형) 로고 확장 및 OS 타이틀 */
    printf(TUK_COLOR_BLUE);
    printf("           /\\           \n");
    printf("        /\\/  \\/\\        \n");
    printf("       / / \\/ \\ \\       \n");
    printf("       \\ \\ /\\ / /       \n");
    printf("        \\/\\  /\\/        \n");
    printf("           \\/           \n");
    printf(TUK_COLOR_RESET "\n");

    printf(TUK_COLOR_MINT "    W E L C O M E   T O   T U K   O S    \n" TUK_COLOR_RESET);
    printf(TUK_COLOR_SKY "  =======================================  \n" TUK_COLOR_RESET);
    printf("    System : TUK-OS v1.0.0 (PowerShell Edition) \n");
    printf("    Kernel : Darwin 23.0.0 / Custom      \n");
    printf(TUK_COLOR_SKY "  =======================================  \n\n" TUK_COLOR_RESET);
}
