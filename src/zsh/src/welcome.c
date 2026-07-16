#include <stdio.h>
#include <unistd.h>
#include "welcome.h"
#include "tuk_shell.h"

void print_welcome_screen(void)
{
    /* 가상 OS 부팅 시퀀스 느낌을 위한 딜레이와 메시지 */
    printf(TU_SKY_BLUE "Booting TUK-OS (x86_64-apple-darwin) ...\n" COLOR_RESET);
    usleep(200000);
    printf(TU_SKY_BLUE "Loading kernel modules" COLOR_RESET " ... [OK]\n");
    usleep(150000);
    printf(TU_SKY_BLUE "Mounting virtual file systems" COLOR_RESET " ... [OK]\n");
    usleep(150000);
    printf(TU_SKY_BLUE "Starting background services" COLOR_RESET " ... [OK]\n");
    usleep(100000);
    printf("\n");

    /* 한국공학대학교 결정체(육각형) 로고 확장 및 OS 타이틀 */
    printf(TU_BLUE);
    printf("           /\\           \n");
    printf("        /\\/  \\/\\        \n");
    printf("       / / \\/ \\ \\       \n");
    printf("       \\ \\ /\\ / /       \n");
    printf("        \\/\\  /\\/        \n");
    printf("           \\/           \n");
    printf(COLOR_RESET "\n");

    printf(TU_MINT "    W E L C O M E   T O   T U K   O S    \n" COLOR_RESET);
    printf(TU_SKY_BLUE "  =======================================  \n" COLOR_RESET);
    printf("    System : TUK-OS v1.0.0 (ZSH Edition) \n");
    printf("    Kernel : Darwin 23.0.0 / Custom      \n");
    printf(TU_SKY_BLUE "  =======================================  \n\n" COLOR_RESET);
}