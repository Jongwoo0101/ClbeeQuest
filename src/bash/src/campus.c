/*
 * campus.c - 캠퍼스 특화 커스텀 명령어: 파싱/디스패치 [우진] + Mock 핸들러 [원종우]
 *
 * 근거 문서:
 *  - 01_상세기능명세서 9장 (BASH 파트 책임, 옵션 파싱 규칙, 핸들러 인터페이스)
 *  - 02_인터페이스명세서 3-3 (명령어별 사용 형식/인자 규칙)
 *  - 05_개발철칙명세서 5-1 (타 파트 로직은 Mocking으로 결합도 최소화)
 *
 * 파싱 규칙 (01 9-3): 모든 옵션은 argv 순회 + strcmp로 처리하고,
 * 값이 필요한 옵션은 다음 인자의 존재를 검사하며,
 * 정의되지 않은 옵션은 각 명령어별 사용법을 출력한다.
 */
#include "campus.h"

#include <stdio.h>
#include <string.h>

#include "parser.h"

static const char *CAMPUS_COMMANDS[] = {
    "schedule", "bus", "bob", "notice", "map", "weather", "contact",
};

static void print_usage(const char *usage)
{
    fprintf(stderr, "usage: %s\n", usage);
}

int is_campus_command(const char *name)
{
    if (name == NULL) {
        return 0;
    }
    for (size_t i = 0;
         i < sizeof(CAMPUS_COMMANDS) / sizeof(CAMPUS_COMMANDS[0]); i++) {
        if (strcmp(name, CAMPUS_COMMANDS[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

/*
 * 옵션이 정확히 1개이며 허용 목록(options)에 포함되는지 검증.
 * bus, bob, weather처럼 "옵션 1개 필수"(02 3-3)인 명령어 공용.
 * 반환: 통과 0 / 사용법 오류 1
 */
static int validate_single_option(int argc, char **argv,
                                  const char *const *options,
                                  size_t option_count, const char *usage)
{
    if (argc != 2) {
        print_usage(usage);
        return 1;
    }
    for (size_t i = 0; i < option_count; i++) {
        if (strcmp(argv[1], options[i]) == 0) {
            return 0;
        }
    }
    print_usage(usage); /* 01 9-3: 정의되지 않은 옵션 -> 명령어별 사용법 */
    return 1;
}

/* notice [-g|-a|-s] [-n N]: 분류 옵션 최대 1개, -n은 선택이며 값 필수 (02 3-3) */
static int validate_notice(int argc, char **argv)
{
    static const char *USAGE = "notice [-g | -a | -s] [-n N]";
    int category_count = 0;
    int count_option_count = 0;
    int i = 1;

    while (i < argc) {
        if (strcmp(argv[i], "-g") == 0 || strcmp(argv[i], "-a") == 0 ||
            strcmp(argv[i], "-s") == 0) {
            category_count++;
            if (category_count > 1) { /* 허용된 조합만 지원 (01 9-3) */
                print_usage(USAGE);
                return 1;
            }
            i++;
        } else if (strcmp(argv[i], "-n") == 0) {
            long recent_count = 0;
            count_option_count++;
            if (count_option_count > 1 || i + 1 >= argc ||
                parse_positive_long(argv[i + 1], &recent_count) != 0) {
                print_usage(USAGE); /* 값 누락/비숫자/중복 -n */
                return 1;
            }
            i += 2;
        } else {
            print_usage(USAGE);
            return 1;
        }
    }
    return 0;
}

/* map -A..-G | -f | -s | -find ROOM: -find는 값 필수 (02 3-3),
 * -s(스포츠 플라자)는 정보 페이지 링크 안내 옵션 */
static int validate_map(int argc, char **argv)
{
    static const char *USAGE =
        "map -A | -B | -C | -D | -E | -F | -G | -f | -s | -find ROOM";
    static const char *SINGLE_OPTIONS[] = {
        "-A", "-B", "-C", "-D", "-E", "-F", "-G", "-f", "-s",
    };

    if (argc == 3 && strcmp(argv[1], "-find") == 0) {
        return 0;
    }
    if (argc == 2) {
        for (size_t i = 0;
             i < sizeof(SINGLE_OPTIONS) / sizeof(SINGLE_OPTIONS[0]); i++) {
            if (strcmp(argv[1], SINGLE_OPTIONS[i]) == 0) {
                return 0;
            }
        }
    }
    print_usage(USAGE); /* -find 값 누락, 옵션 개수 오류, 미정의 옵션 포함 */
    return 1;
}

/* contact -p NAME | -d DEPT | -e: -p, -d는 값 필수 (02 3-3) */
static int validate_contact(int argc, char **argv)
{
    static const char *USAGE = "contact -p NAME | -d DEPT | -e";

    if (argc == 2 && strcmp(argv[1], "-e") == 0) {
        return 0;
    }
    if (argc == 3 &&
        (strcmp(argv[1], "-p") == 0 || strcmp(argv[1], "-d") == 0)) {
        return 0;
    }
    print_usage(USAGE);
    return 1;
}

int dispatch_campus_command(int argc, char **argv)
{
    static const char *BUS_OPTIONS[] = { "-1", "-2" };
    static const char *BOB_OPTIONS[] = { "-t", "-E", "-d" };
    static const char *WEATHER_OPTIONS[] = { "-c", "-w", "-d" };

    if (strcmp(argv[0], "schedule") == 0) {
        if (argc != 1) { /* 02 3-3: schedule은 인자 없음 */
            print_usage("schedule");
            return 1;
        }
        return handle_schedule_command(argc, argv);
    }
    if (strcmp(argv[0], "bus") == 0) {
        if (validate_single_option(argc, argv, BUS_OPTIONS, 2,
                                   "bus -1 | -2") != 0) {
            return 1;
        }
        return handle_bus_command(argc, argv);
    }
    if (strcmp(argv[0], "bob") == 0) {
        if (validate_single_option(argc, argv, BOB_OPTIONS, 3,
                                   "bob -t | -E | -d") != 0) {
            return 1;
        }
        return handle_bob_command(argc, argv);
    }
    if (strcmp(argv[0], "notice") == 0) {
        if (validate_notice(argc, argv) != 0) {
            return 1;
        }
        return handle_notice_command(argc, argv);
    }
    if (strcmp(argv[0], "map") == 0) {
        if (validate_map(argc, argv) != 0) {
            return 1;
        }
        return handle_map_command(argc, argv);
    }
    if (strcmp(argv[0], "weather") == 0) {
        if (validate_single_option(argc, argv, WEATHER_OPTIONS, 3,
                                   "weather -c | -w | -d") != 0) {
            return 1;
        }
        return handle_weather_command(argc, argv);
    }
    if (strcmp(argv[0], "contact") == 0) {
        if (validate_contact(argc, argv) != 0) {
            return 1;
        }
        return handle_contact_command(argc, argv);
    }
    return 1; /* 도달 불가: is_campus_command() 선행 검사 전제 */
}

/* 캠퍼스 명령 핸들러 실구현은 인터페이스(campus.h)를 고정한 채 별도 모듈에 있다:
 *  - notice           -> notice.c       (학사공지 게시판 라이브 HTML 파싱)
 *  - 그 외 6개 핸들러  -> campus_data.c  (data/campus 디렉터리의 .txt 파싱)
 * 본 파일은 진입점 식별·옵션 검증·디스패치(우진 BASH 책임, 01 9-1)만 담당한다. */
