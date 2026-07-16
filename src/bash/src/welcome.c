#include "welcome.h"

#include <stdio.h>
#include <time.h> /* usleep 대신 nanosleep 사용을 위해 추가 */

#include "tuk_shell.h"

/* 밀리초(ms) 단위 지연 함수 (usleep 대체) */
static void sleep_ms(long ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

void print_welcome_screen(void)
{
    /* 가상 OS 부팅 시퀀스 느낌을 위한 딜레이와 메시지 */
    printf(TUK_COLOR_SKY "Booting TUK-OS (x86_64-apple-darwin) ...\n" TUK_COLOR_RESET);
    sleep_ms(200); /* 200,000us = 200ms */
    
    printf(TUK_COLOR_SKY "Loading kernel modules" TUK_COLOR_RESET " ... [OK]\n");
    sleep_ms(150);
    
    printf(TUK_COLOR_SKY "Mounting virtual file systems" TUK_COLOR_RESET " ... [OK]\n");
    sleep_ms(150);
    
    printf(TUK_COLOR_SKY "Starting background services" TUK_COLOR_RESET " ... [OK]\n");
    sleep_ms(100);
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
    printf("    System : TUK-OS v1.0.0 (BASH Edition) \n");
    printf("    Kernel : Darwin 23.0.0 / Custom      \n");
    printf(TUK_COLOR_SKY "  =======================================  \n\n" TUK_COLOR_RESET);
}
