#include "welcome.h"

#include <stdio.h>
#include "platform.h"
#include "tuk_shell.h"

#ifdef _WIN32
#define TUK_HOST_PLATFORM "Windows"
#elif defined(__APPLE__)
#define TUK_HOST_PLATFORM "macOS"
#else
#define TUK_HOST_PLATFORM "Linux"
#endif

void print_welcome_screen(void)
{
    /* 가상 OS 부팅 시퀀스 느낌을 위한 딜레이와 메시지 */
    printf(TUK_COLOR_SKY "Booting TUK-OS (%s) ...\n" TUK_COLOR_RESET,
           TUK_HOST_PLATFORM);
    tuk_sleep_ms(200);
    
    printf(TUK_COLOR_SKY "Loading kernel modules" TUK_COLOR_RESET " ... [OK]\n");
    tuk_sleep_ms(150);
    
    printf(TUK_COLOR_SKY "Mounting virtual file systems" TUK_COLOR_RESET " ... [OK]\n");
    tuk_sleep_ms(150);
    
    printf(TUK_COLOR_SKY "Starting background services" TUK_COLOR_RESET " ... [OK]\n");
    tuk_sleep_ms(100);
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
    printf("    Kernel : %s / Custom      \n", TUK_HOST_PLATFORM);
    printf(TUK_COLOR_SKY "  =======================================  \n\n" TUK_COLOR_RESET);
}
