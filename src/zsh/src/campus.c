#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "campus.h"
#include "tuk_shell.h"

/*
 * 이번 단계(7단계) 구현 범위:
 *  - 캠퍼스 명령어 7종(schedule/bus/bob/notice/map/weather/contact) 진입점 식별
 *  - 옵션 파싱 및 사용법/잘못된 옵션 처리 (01문서 9-3: argv 순회 + strcmp)
 *  - data/campus/ 아래 로컬 텍스트 파일 파싱 후 TU MINT로 결과 출력
 *
 * plan.md 3-[6]의 "추후 API 연동 확장이 가능하도록 함수 인터페이스를 분리"에 따라
 * 파일 접근은 이 파일의 static 함수(open_campus_data 등)로 캡슐화했다. API 연동으로
 * 교체할 때 campus.h의 handle_*_command 시그니처는 그대로 두고 내부만 바꾸면 된다.
 *
 * [메모리] 이 파일은 동적 메모리를 사용하지 않는다. 고정 크기 스택 버퍼에 한 줄씩
 * 읽어 처리하므로 별도의 free() 지점이 없다(05문서 2-1 대상 아님).
 */

#define CAMPUS_LINE_MAX 512
#define CAMPUS_PATH_MAX 512
#define CAMPUS_MAX_FIELDS 8

/* 데이터 파일 스키마상 최대 필드 수보다 넉넉히 잡은 주간 예보 버퍼 (weather -w/-d) */
#define WEATHER_MAX_ROWS 400
#define WEATHER_RECENT_DAYS 7

/* ============================================================
 * 데이터 파일 접근 (02문서 6-2 campus data .txt)
 * ============================================================ */

/*
 * resolve_campus_data_dir
 *  - TUK_CAMPUS_DATA 환경변수가 있으면 그대로 사용한다(폴백 없음 - 잘못 지정하면
 *    open_campus_data()가 그대로 실패해 원인 파악이 쉽다).
 *  - 없으면 실행 위치 기준 후보를 순서대로 탐색한다. src/zsh에서 실행하는 경우
 *    (../../data/campus)와 저장소 루트에서 실행하는 경우(data/campus)를 모두 포괄한다.
 */
static const char *resolve_campus_data_dir(void)
{
    static const char *candidates[] = {
        "data/campus",
        "../../data/campus",
        "../../../data/campus"
    };
    static char resolved[CAMPUS_PATH_MAX];
    char probe[CAMPUS_PATH_MAX];
    FILE *probe_fp;
    const char *env;
    size_t i;

    env = getenv("TUK_CAMPUS_DATA");
    if (env != NULL && env[0] != '\0') {
        return env;
    }

    for (i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        snprintf(probe, sizeof(probe), "%s/schedule.txt", candidates[i]);
        probe_fp = fopen(probe, "r");
        if (probe_fp != NULL) {
            fclose(probe_fp);
            snprintf(resolved, sizeof(resolved), "%s", candidates[i]);
            return resolved;
        }
    }

    /* 후보를 못 찾으면 첫 후보를 돌려줘 open_campus_data()가 파일명을 포함한
     * 일관된 오류 메시지로 실패하게 한다. */
    snprintf(resolved, sizeof(resolved), "%s", candidates[0]);
    return resolved;
}

/*
 * open_campus_data
 *  - 캠퍼스 데이터 파일을 읽기 모드로 연다.
 *  - 실패 시 오류 출력 후 NULL 반환. 호출부는 -1을 반환해 해당 명령만 실패시키고
 *    쉘은 계속 유지한다 (02문서 6-3, 05문서 3-2 캠퍼스 데이터 장애 격리).
 */
static FILE *open_campus_data(const char *filename)
{
    char path[CAMPUS_PATH_MAX];
    FILE *fp;

    snprintf(path, sizeof(path), "%s/%s", resolve_campus_data_dir(), filename);

    fp = fopen(path, "r");
    if (fp == NULL) {
        fprintf(stderr, "campus: 데이터 파일을 찾을 수 없습니다 (%s)\n", filename);
    }
    return fp;
}

/*
 * read_data_line
 *  - 주석(#)과 빈 줄을 건너뛰며 유효한 한 줄을 읽는다(개행 제거 완료).
 *  - 반환값: 1 정상 읽음, 0 EOF
 */
static int read_data_line(FILE *fp, char *buffer, size_t size)
{
    while (fgets(buffer, (int)size, fp) != NULL) {
        buffer[strcspn(buffer, "\n")] = '\0';
        if (buffer[0] == '\0' || buffer[0] == '#') {
            continue;
        }
        return 1;
    }
    return 0;
}

/*
 * split_pipe
 *  - '|' 구분 필드를 line 버퍼 내부에서 분리한다(parse_command의 strtok과 동일하게
 *    원본 버퍼를 파괴하며, 동적 메모리를 쓰지 않는다).
 *  - 반환값: 분리된 필드 개수
 */
static int split_pipe(char *line, char **fields, int max_fields)
{
    int count = 0;
    char *p = line;

    fields[count++] = p;
    while (*p != '\0' && count < max_fields) {
        if (*p == '|') {
            *p = '\0';
            fields[count++] = p + 1;
        }
        p++;
    }
    return count;
}

/* 쉼표 구분 목록을 sep로 바꿔 출력 (임시 버퍼 없이 문자 단위 처리) */
static void print_csv_as(const char *csv, const char *sep)
{
    const char *p;

    for (p = csv; *p != '\0'; p++) {
        if (*p == ',') {
            fputs(sep, stdout);
        } else {
            fputc(*p, stdout);
        }
    }
}

/* 쉼표 구분 목록의 항목 수 (bus 운행 횟수 표기용) */
static int count_csv_items(const char *csv)
{
    const char *p;
    int count;

    if (csv == NULL || csv[0] == '\0') {
        return 0;
    }
    count = 1;
    for (p = csv; *p != '\0'; p++) {
        if (*p == ',') {
            count++;
        }
    }
    return count;
}

/*
 * display_width
 *  - UTF-8 문자열이 터미널에서 차지하는 칸 수를 계산한다.
 *  - printf("%-16s")는 바이트 수로 폭을 맞추는데 한글은 UTF-8에서 한 글자가 3바이트라
 *    표가 어긋난다. 그래서 표 출력에는 %-Ns 대신 print_padded()를 쓴다.
 *  - 3바이트 문자(한글/CJK)를 2칸으로 계산한다. 본 프로젝트 데이터는 한글+ASCII만
 *    쓰므로 이 근사로 충분하다(엄밀한 폭 계산은 wcwidth 필요).
 */
static int display_width(const char *s)
{
    const unsigned char *p = (const unsigned char *)s;
    int width = 0;

    while (*p != '\0') {
        if (*p < 0x80) {                 /* ASCII */
            width += 1;
            p += 1;
        } else if ((*p & 0xE0) == 0xC0) { /* 2바이트 */
            width += 1;
            p += 2;
        } else if ((*p & 0xF0) == 0xE0) { /* 3바이트: 한글/CJK */
            width += 2;
            p += 3;
        } else {                          /* 4바이트: 이모지 등 */
            width += 2;
            p += 4;
        }
    }
    return width;
}

/* 문자열을 출력하고 표시 폭 기준으로 width 칸까지 공백을 채운다 */
static void print_padded(const char *s, int width)
{
    int w = display_width(s);

    fputs(s, stdout);
    while (w < width) {
        fputc(' ', stdout);
        w++;
    }
}

/* argv[i]가 순수 숫자인지 검사 - "notice -n abc" 방어 (01문서 9-3) */
static int is_all_digits(const char *s)
{
    if (s == NULL || *s == '\0') {
        return 0;
    }
    for (; *s != '\0'; s++) {
        if (*s < '0' || *s > '9') {
            return 0;
        }
    }
    return 1;
}

/* ============================================================
 * 명령어 식별 및 옵션 검증 (01문서 9-1, 9-3)
 * ============================================================ */

static const char *CAMPUS_NAMES[] = {
    "schedule", "bus", "bob", "notice", "map", "weather", "contact"
};
static const int CAMPUS_COUNT = 7;

int is_campus_command(const char *cmd)
{
    int i;

    if (cmd == NULL) {
        return 0;
    }
    for (i = 0; i < CAMPUS_COUNT; i++) {
        if (strcmp(cmd, CAMPUS_NAMES[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

/*
 * validate_single_option
 *  - "옵션 1개 필수"(02문서 3-3)인 bus/bob/weather 공용 검증.
 *  - 반환값: 0 통과, 1 사용법 오류
 */
static int validate_single_option(int argc, char **argv, const char **options,
                                  int option_count, const char *usage)
{
    int i;

    if (argc != 2) {
        fprintf(stderr, "usage: %s\n", usage);
        return 1;
    }
    for (i = 0; i < option_count; i++) {
        if (strcmp(argv[1], options[i]) == 0) {
            return 0;
        }
    }
    fprintf(stderr, "usage: %s\n", usage); /* 01문서 9-3: 미정의 옵션 -> 사용법 */
    return 1;
}

/* notice [-g|-a|-s] [-n N]: 분류 옵션 최대 1개, -n은 선택이며 값 필수 (02문서 3-3) */
static int validate_notice(int argc, char **argv)
{
    static const char *usage = "notice [-g | -a | -s] [-n N]";
    int category_count = 0;
    int count_option = 0;
    int i = 1;

    while (i < argc) {
        if (strcmp(argv[i], "-g") == 0 || strcmp(argv[i], "-a") == 0 ||
            strcmp(argv[i], "-s") == 0) {
            category_count++;
            if (category_count > 1) { /* 허용된 조합만 지원 (01문서 9-3) */
                fprintf(stderr, "usage: %s\n", usage);
                return 1;
            }
            i++;
        } else if (strcmp(argv[i], "-n") == 0) {
            count_option++;
            /* 값 누락 / 비숫자 / -n 중복 방어 */
            if (count_option > 1 || i + 1 >= argc || !is_all_digits(argv[i + 1]) ||
                atol(argv[i + 1]) <= 0) {
                fprintf(stderr, "usage: %s\n", usage);
                return 1;
            }
            i += 2;
        } else {
            fprintf(stderr, "usage: %s\n", usage);
            return 1;
        }
    }
    return 0;
}

/* map -A..-G | -f | -find ROOM: -find는 값 필수 (02문서 3-3) */
static int validate_map(int argc, char **argv)
{
    static const char *usage = "map -A | -B | -C | -D | -E | -F | -G | -f | -find [ROOM]";
    static const char *floor_options[] = {
        "-A", "-B", "-C", "-D", "-E", "-F", "-G", "-f"
    };
    int i;

    if (argc == 3 && strcmp(argv[1], "-find") == 0) {
        return 0;
    }
    if (argc == 2) {
        for (i = 0; i < 8; i++) {
            if (strcmp(argv[1], floor_options[i]) == 0) {
                return 0;
            }
        }
    }
    fprintf(stderr, "usage: %s\n", usage); /* -find 값 누락/개수 오류/미정의 옵션 */
    return 1;
}

/* contact -p NAME|-l | -d DEPT|-l | -e */
static int validate_contact(int argc, char **argv)
{
    static const char *usage = "contact -p [NAME | -l] | -d [DEPT | -l] | -e";

    if (argc == 2 && strcmp(argv[1], "-e") == 0) {
        return 0;
    }
    if (argc == 3 && (strcmp(argv[1], "-p") == 0 || strcmp(argv[1], "-d") == 0)) {
        return 0;
    }
    fprintf(stderr, "usage: %s\n", usage);
    return 1;
}

int dispatch_campus_command(int argc, char **argv)
{
    static const char *bus_options[] = { "-1", "-2" };
    static const char *bob_options[] = { "-t", "-e", "-d" };
    static const char *weather_options[] = { "-c", "-w", "-d" };

    if (strcmp(argv[0], "schedule") == 0) {
        if (argc != 1) { /* 02문서 3-3: schedule은 인자 없음 */
            fprintf(stderr, "usage: schedule\n");
            return 1;
        }
        return handle_schedule_command(argc, argv);
    }

    if (strcmp(argv[0], "bus") == 0) {
        if (validate_single_option(argc, argv, bus_options, 2, "bus -1 | -2") != 0) {
            return 1;
        }
        return handle_bus_command(argc, argv);
    }

    if (strcmp(argv[0], "bob") == 0) {
        if (validate_single_option(argc, argv, bob_options, 3, "bob -t | -e | -d") != 0) {
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
        if (validate_single_option(argc, argv, weather_options, 3,
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

    /* is_campus_command() 확인 없이 호출된 방어적 상황 */
    fprintf(stderr, "error: unknown campus command '%s'\n", argv[0]);
    return -1;
}

/* ============================================================
 * schedule: AI소프트웨어학과 시간표 (schedule.txt)
 * 스키마: day|period|time|course|professor|room
 * ============================================================ */

int handle_schedule_command(int argc, char **argv)
{
    FILE *fp;
    char line[CAMPUS_LINE_MAX];
    char *f[CAMPUS_MAX_FIELDS];
    int printed = 0;

    (void)argc;
    (void)argv;

    fp = open_campus_data("schedule.txt");
    if (fp == NULL) {
        return -1;
    }

    printf("\n");
    printf(TU_MINT "[AI소프트웨어학과 시간표]\n" COLOR_RESET);
    printf(TU_SKY_BLUE " ════════════════════════════════════════════════════════════════════════════\n" COLOR_RESET);
    fputs(TU_BLUE, stdout);
    printf("  "); print_padded("요일", 6);
    print_padded("교시", 6);
    print_padded("시간", 16);
    print_padded("과목", 24);
    print_padded("교수", 10);
    fputs("강의실" COLOR_RESET "\n", stdout);
    printf(TU_SKY_BLUE " ════════════════════════════════════════════════════════════════════════════\n" COLOR_RESET);

    while (read_data_line(fp, line, sizeof(line))) {
        if (split_pipe(line, f, CAMPUS_MAX_FIELDS) < 6) {
            continue; /* 스키마 불일치 줄은 건너음 */
        }
        fputs(TU_MINT, stdout);
        printf("  "); print_padded(f[0], 6);  /* 요일 */
        print_padded(f[1], 6);  /* 교시 */
        fputs(COLOR_RESET, stdout);
        print_padded(f[2], 16); /* 시간 */
        fputs(TU_MINT, stdout);
        print_padded(f[3], 24); /* 과목 */
        fputs(COLOR_RESET, stdout);
        print_padded(f[4], 10); /* 교수 */
        fputs(f[5], stdout);    /* 강의실 */
        fputs("\n", stdout);
        printed = 1;
    }
    fclose(fp);
    printf(TU_SKY_BLUE " ════════════════════════════════════════════════════════════════════════════\n\n" COLOR_RESET);

    if (!printed) {
        printf("등록된 시간표가 없습니다.\n");
    }
    return 0;
}

/* ============================================================
 * bus -1 | -2: 셔틀버스 시간표 (bus.txt)
 * 스키마: route_code|name|stops(,)|weekday_times(,)|weekend_times(,)
 * ============================================================ */

int handle_bus_command(int argc, char **argv)
{
    FILE *fp;
    char line[CAMPUS_LINE_MAX];
    char *f[CAMPUS_MAX_FIELDS];
    int found = 0;

    (void)argc;

    fp = open_campus_data("bus.txt");
    if (fp == NULL) {
        return -1;
    }

    while (read_data_line(fp, line, sizeof(line))) {
        if (split_pipe(line, f, CAMPUS_MAX_FIELDS) < 5) {
            continue;
        }
        if (strcmp(f[0], argv[1]) != 0) {
            continue;
        }
        found = 1;
        printf("\n");
        printf(TU_SKY_BLUE " ▶▶ " TU_MINT "노선 정보 : %s\n" COLOR_RESET, f[1]);
        printf(TU_SKY_BLUE " ════════════════════════════════════════════════════════════\n" COLOR_RESET);
        printf(TU_BLUE "  경 유 지  : " COLOR_RESET); print_csv_as(f[2], " ➔  "); printf("\n");
        printf(TU_BLUE "  평일운행  : " TU_MINT "[%d회] " COLOR_RESET, count_csv_items(f[3])); print_csv_as(f[3], "  "); printf("\n");
        printf(TU_BLUE "  주말운행  : " TU_MINT "[%d회] " COLOR_RESET, count_csv_items(f[4])); print_csv_as(f[4], "  "); printf("\n");
        printf(TU_SKY_BLUE " ════════════════════════════════════════════════════════════\n\n" COLOR_RESET);
        break; /* route_code는 유일 */
    }
    fclose(fp);

    if (!found) {
        printf("해당 노선 정보를 찾을 수 없습니다.\n");
    }
    return 0;
}
/* ============================================================
 * bob -t | -e | -d: 학식 메뉴 (bob.txt) - 가장 최근 날짜의 메뉴 출력
 * 스키마: date|weekday|restaurant_code|restaurant_name|price|menu(,)
 * ============================================================ */

int handle_bob_command(int argc, char **argv)
{
    FILE *fp;
    char line[CAMPUS_LINE_MAX];
    char *f[CAMPUS_MAX_FIELDS];
    char best_date[16];
    char best_weekday[8];
    char best_name[64];
    char best_price[16];
    char best_menu[256];
    int found = 0;

    (void)argc;

    fp = open_campus_data("bob.txt");
    if (fp == NULL) {
        return -1;
    }

    /* bob.txt는 날짜 오름차순이므로 마지막 매칭 줄이 최신 메뉴다. */
    while (read_data_line(fp, line, sizeof(line))) {
        if (split_pipe(line, f, CAMPUS_MAX_FIELDS) < 6) {
            continue;
        }
        if (strcmp(f[2], argv[1]) != 0) {
            continue;
        }
        found = 1;
        snprintf(best_date, sizeof(best_date), "%s", f[0]);
        snprintf(best_weekday, sizeof(best_weekday), "%s", f[1]);
        snprintf(best_name, sizeof(best_name), "%s", f[3]);
        snprintf(best_price, sizeof(best_price), "%s", f[4]);
        snprintf(best_menu, sizeof(best_menu), "%s", f[5]);
    }
    fclose(fp);

    if (!found) {
        printf("해당 식당의 메뉴 정보를 찾을 수 없습니다.\n");
        return 0;
    }

    printf(TU_MINT "[%s] %s (%s)  %s원\n" COLOR_RESET,
           best_name, best_date, best_weekday, best_price);
    printf(TU_MINT "메뉴: " COLOR_RESET);
    print_csv_as(best_menu, ", ");
    printf("\n");
    return 0;
}

/* ============================================================
 * notice [-g|-a|-s] [-n N]: 공지사항 (notice.txt, 최신순 정렬됨)
 * 스키마: id|date|category_code|title
 * ============================================================ */

static const char *notice_category_label(const char *code)
{
    if (code == NULL) {
        return "전체";
    }
    if (strcmp(code, "-g") == 0) {
        return "일반";
    }
    if (strcmp(code, "-a") == 0) {
        return "학사";
    }
    if (strcmp(code, "-s") == 0) {
        return "장학";
    }
    return "전체";
}

int handle_notice_command(int argc, char **argv)
{
    FILE *fp;
    char line[CAMPUS_LINE_MAX];
    char *f[CAMPUS_MAX_FIELDS];
    const char *category = NULL; /* NULL이면 전체 (분류 필터 없음) */
    long limit = 0;              /* 0이면 -n 미지정 (전체 출력) */
    long total = 0;
    long shown;
    long index = 0;
    int i;

    /* 옵션 해석 - 값/중복 검증은 dispatch 이전 validate_notice()에서 이미 끝났다 */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-g") == 0 || strcmp(argv[i], "-a") == 0 ||
            strcmp(argv[i], "-s") == 0) {
            category = argv[i];
        } else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            limit = atol(argv[i + 1]);
            i++;
        }
    }

    /* 1차 순회: 분류 조건에 맞는 총 건수 계산 (헤더 표기용) */
    fp = open_campus_data("notice.txt");
    if (fp == NULL) {
        return -1;
    }
    while (read_data_line(fp, line, sizeof(line))) {
        if (split_pipe(line, f, CAMPUS_MAX_FIELDS) < 4) {
            continue;
        }
        if (category != NULL && strcmp(f[2], category) != 0) {
            continue;
        }
        total++;
    }
    fclose(fp);

    shown = (limit > 0 && limit < total) ? limit : total;

    if (limit > 0) {
        printf(TU_MINT "[%s 공지] 최근 %ld건\n" COLOR_RESET,
               notice_category_label(category), shown);
    } else {
        printf(TU_MINT "[%s 공지] 전체 %ld건\n" COLOR_RESET,
               notice_category_label(category), shown);
    }

    if (shown == 0) {
        printf("등록된 공지가 없습니다.\n");
        return 0;
    }

    /* 2차 순회: notice.txt가 최신순이므로 상위 shown건이 곧 "최근 N건"이다 */
    fp = open_campus_data("notice.txt");
    if (fp == NULL) {
        return -1;
    }
    while (index < shown && read_data_line(fp, line, sizeof(line))) {
        if (split_pipe(line, f, CAMPUS_MAX_FIELDS) < 4) {
            continue;
        }
        if (category != NULL && strcmp(f[2], category) != 0) {
            continue;
        }
        index++;
        printf(TU_MINT "%2ld. [%s] %s  (%s)\n" COLOR_RESET,
               index, notice_category_label(f[2]), f[3], f[1]);
    }
    fclose(fp);

    return 0;
}

/* ============================================================
 * map -A..-G | -f | -find ROOM: 캠퍼스 안내 (map.txt)
 * 스키마: BUILDING|code|name|floor|floor_desc
 *        FAC|name|location
 *        ROOM|room_no|building|floor|desc
 * ============================================================ */

/* map -find [ROOM]: 강의실 위치 검색 */
static int handle_map_find(char **argv)
{
    FILE *fp;
    char line[CAMPUS_LINE_MAX];
    char *f[CAMPUS_MAX_FIELDS];
    int found = 0;

    fp = open_campus_data("map.txt");
    if (fp == NULL) {
        return -1;
    }

    while (read_data_line(fp, line, sizeof(line))) {
        if (split_pipe(line, f, CAMPUS_MAX_FIELDS) < 5) {
            continue;
        }
        if (strcmp(f[0], "ROOM") != 0 || strcmp(f[1], argv[2]) != 0) {
            continue;
        }
        found = 1;
        printf(TU_MINT "[%s] %s %s층 - %s\n" COLOR_RESET, f[1], f[2], f[3], f[4]);
        break; /* room_no는 유일 */
    }
    fclose(fp);

    if (!found) {
        printf("%s: 해당 강의실 정보를 찾을 수 없습니다.\n", argv[2]);
    }
    return 0;
}

/* map -f: 교내 편의시설 */
static int handle_map_facility(void)
{
    FILE *fp;
    char line[CAMPUS_LINE_MAX];
    char *f[CAMPUS_MAX_FIELDS];
    int printed = 0;

    fp = open_campus_data("map.txt");
    if (fp == NULL) {
        return -1;
    }

    printf(TU_MINT "[교내 편의시설]\n" COLOR_RESET);
    while (read_data_line(fp, line, sizeof(line))) {
        if (split_pipe(line, f, CAMPUS_MAX_FIELDS) < 3) {
            continue;
        }
        if (strcmp(f[0], "FAC") != 0) {
            continue;
        }
        fputs(TU_MINT, stdout);
        print_padded(f[1], 20); /* 시설명 */
        fputs(f[2], stdout);    /* 위치 */
        fputs(COLOR_RESET "\n", stdout);
        printed = 1;
    }
    fclose(fp);

    if (!printed) {
        printf("등록된 편의시설이 없습니다.\n");
    }
    return 0;
}

/* map -A ~ -G: 건물별 층 안내 (BUILDING 레코드는 건물당 여러 줄) */
static int handle_map_building(char **argv)
{
    FILE *fp;
    char line[CAMPUS_LINE_MAX];
    char *f[CAMPUS_MAX_FIELDS];
    int printed = 0;

    fp = open_campus_data("map.txt");
    if (fp == NULL) {
        return -1;
    }

    while (read_data_line(fp, line, sizeof(line))) {
        if (split_pipe(line, f, CAMPUS_MAX_FIELDS) < 5) {
            continue;
        }
        if (strcmp(f[0], "BUILDING") != 0 || strcmp(f[1], argv[1]) != 0) {
            continue;
        }
        if (!printed) {
            printf(TU_MINT "[%s]\n" COLOR_RESET, f[2]);
        }
        printf(TU_MINT "%3s층: %s\n" COLOR_RESET, f[3], f[4]);
        printed = 1;
    }
    fclose(fp);

    if (!printed) {
        printf("해당 건물 정보를 찾을 수 없습니다.\n");
    }
    return 0;
}

int handle_map_command(int argc, char **argv)
{
    (void)argc;

    if (strcmp(argv[1], "-find") == 0) {
        return handle_map_find(argv);
    }
    if (strcmp(argv[1], "-f") == 0) {
        return handle_map_facility();
    }
    return handle_map_building(argv);
}

/* ============================================================
 * weather -c | -w | -d: 시흥 캠퍼스 날씨 (weather.txt)
 * 스키마: date|location|condition|temp_high|temp_low|dust_level
 * weather.txt 주석 규약: 마지막 줄이 현재 날씨(-c), 최근 7줄이 주간 예보(-w)
 * ============================================================ */

typedef struct {
    char date[16];
    char location[32];
    char condition[16];
    char temp_high[8];
    char temp_low[8];
    char dust[16];
} WeatherRow;

/*
 * load_weather_rows
 *  - weather.txt 전체를 고정 크기 배열에 적재한다(동적 할당 없음).
 *  - 반환값: 읽은 행 수, 파일 열기 실패 시 -1
 */
static long load_weather_rows(WeatherRow *rows, long max_rows)
{
    FILE *fp;
    char line[CAMPUS_LINE_MAX];
    char *f[CAMPUS_MAX_FIELDS];
    long count = 0;

    fp = open_campus_data("weather.txt");
    if (fp == NULL) {
        return -1;
    }

    while (count < max_rows && read_data_line(fp, line, sizeof(line))) {
        if (split_pipe(line, f, CAMPUS_MAX_FIELDS) < 6) {
            continue;
        }
        snprintf(rows[count].date, sizeof(rows[count].date), "%s", f[0]);
        snprintf(rows[count].location, sizeof(rows[count].location), "%s", f[1]);
        snprintf(rows[count].condition, sizeof(rows[count].condition), "%s", f[2]);
        snprintf(rows[count].temp_high, sizeof(rows[count].temp_high), "%s", f[3]);
        snprintf(rows[count].temp_low, sizeof(rows[count].temp_low), "%s", f[4]);
        snprintf(rows[count].dust, sizeof(rows[count].dust), "%s", f[5]);
        count++;
    }
    fclose(fp);

    return count;
}

int handle_weather_command(int argc, char **argv)
{
    WeatherRow rows[WEATHER_MAX_ROWS];
    const WeatherRow *latest;
    long count;
    long start;
    long i;

    (void)argc;

    count = load_weather_rows(rows, WEATHER_MAX_ROWS);
    if (count < 0) {
        return -1;
    }
    if (count == 0) {
        printf("날씨 정보를 찾을 수 없습니다.\n");
        return 0;
    }

    latest = &rows[count - 1];
    start = (count > WEATHER_RECENT_DAYS) ? count - WEATHER_RECENT_DAYS : 0;

    if (strcmp(argv[1], "-c") == 0) {
        printf(TU_MINT "[현재 날씨] %s (%s)\n" COLOR_RESET,
               latest->location, latest->date);
        printf(TU_MINT "%s  최고 %s'C / 최저 %s'C  미세먼지 %s\n" COLOR_RESET,
               latest->condition, latest->temp_high, latest->temp_low, latest->dust);
        return 0;
    }

    if (strcmp(argv[1], "-w") == 0) {
        printf(TU_MINT "[주간 예보] 최근 %ld일\n" COLOR_RESET, count - start);
        fputs(TU_MINT, stdout);
        print_padded("날짜", 13);
        print_padded("날씨", 11);
        print_padded("최고/최저", 15);
        fputs("미세먼지" COLOR_RESET "\n", stdout);
        for (i = start; i < count; i++) {
            char temp_buf[24];
            snprintf(temp_buf, sizeof(temp_buf), "%s/%s'C",
                     rows[i].temp_high, rows[i].temp_low);
            fputs(TU_MINT, stdout);
            print_padded(rows[i].date, 13);
            print_padded(rows[i].condition, 11);
            print_padded(temp_buf, 15);
            fputs(rows[i].dust, stdout);
            fputs(COLOR_RESET "\n", stdout);
        }
        return 0;
    }

    /* -d: 미세먼지 */
    printf(TU_MINT "[미세먼지] %s 기준\n" COLOR_RESET, latest->location);
    printf(TU_MINT "현재 (%s): %s\n" COLOR_RESET, latest->date, latest->dust);
    printf(TU_MINT "최근 %ld일: " COLOR_RESET, count - start);
    for (i = start; i < count; i++) {
        printf("%s%s", rows[i].dust, (i + 1 < count) ? ", " : "");
    }
    printf("\n");
    return 0;
}

/* ============================================================
 * contact -p NAME|-l | -d DEPT|-l | -e: 연락처 검색 (contact.txt)
 * 스키마: PROF|name|dept|office|phone|email
 *        DEPT|name|phone|location
 *        EMERG|name|phone
 * ============================================================ */

int handle_contact_command(int argc, char **argv)
{
    FILE *fp;
    char line[CAMPUS_LINE_MAX];
    char *f[CAMPUS_MAX_FIELDS];
    int matched = 0;
    int is_list_mode = 0;

    (void)argc;

    fp = open_campus_data("contact.txt");
    if (fp == NULL) {
        return -1;
    }

    /* argv[2]가 "-l" 이면 전체 목록 모드로 동작 */
    if (argc == 3 && strcmp(argv[2], "-l") == 0) {
        is_list_mode = 1;
    }

    printf("\n");

    if (strcmp(argv[1], "-p") == 0) {
        if (is_list_mode) {
            printf(TU_MINT " [교수 연락처 전체 목록]\n" COLOR_RESET);
        } else {
            printf(TU_MINT " [교수 연락처] \"%s\" 검색 결과\n" COLOR_RESET, argv[2]);
        }
        printf(TU_SKY_BLUE " ════════════════════════════════════════════════════════════════════════════\n" COLOR_RESET);
        fputs(TU_BLUE, stdout);
        printf("  "); print_padded("이름", 10);
        print_padded("소속학과", 22);
        print_padded("연구실", 12);
        print_padded("전화번호", 16);
        fputs("이메일" COLOR_RESET "\n", stdout);
        printf(TU_SKY_BLUE " ════════════════════════════════════════════════════════════════════════════\n" COLOR_RESET);
        
        while (read_data_line(fp, line, sizeof(line))) {
            if (split_pipe(line, f, CAMPUS_MAX_FIELDS) < 6) continue;
            if (strcmp(f[0], "PROF") != 0) continue;
            
            /* 리스트 모드가 아닐 때만 검색어 매칭 확인 */
            if (!is_list_mode && strstr(f[1], argv[2]) == NULL) continue;
            
            fputs(TU_MINT, stdout);
            printf("  "); print_padded(f[1], 10); /* 이름 */
            fputs(COLOR_RESET, stdout);
            print_padded(f[2], 22);             /* 학과 */
            print_padded(f[3], 12);             /* 연구실 */
            print_padded(f[4], 16);             /* 전화 */
            fputs(f[5], stdout);                /* 이메일 */
            fputs("\n", stdout);
            matched = 1;
        }
        printf(TU_SKY_BLUE " ════════════════════════════════════════════════════════════════════════════\n\n" COLOR_RESET);
        
    } else if (strcmp(argv[1], "-d") == 0) {
        if (is_list_mode) {
            printf(TU_MINT "[부서 연락처 전체 목록]\n" COLOR_RESET);
        } else {
            printf(TU_MINT "[부서 연락처] \"%s\" 검색 결과\n" COLOR_RESET, argv[2]);
        }
        printf(TU_SKY_BLUE " ════════════════════════════════════════════════════════════════════════════\n" COLOR_RESET);
        fputs(TU_BLUE, stdout);
        printf("  "); print_padded("부서명", 20);
        print_padded("전화번호", 18);
        fputs("위치" COLOR_RESET "\n", stdout);
        printf(TU_SKY_BLUE " ════════════════════════════════════════════════════════════════════════════\n" COLOR_RESET);

        while (read_data_line(fp, line, sizeof(line))) {
            if (split_pipe(line, f, CAMPUS_MAX_FIELDS) < 4) continue;
            if (strcmp(f[0], "DEPT") != 0) continue;
            
            /* 리스트 모드가 아닐 때만 검색어 매칭 확인 */
            if (!is_list_mode && strstr(f[1], argv[2]) == NULL) continue;
            
            fputs(TU_MINT, stdout);
            printf("  "); print_padded(f[1], 20); /* 부서명 */
            fputs(COLOR_RESET, stdout);
            print_padded(f[2], 18);             /* 전화 */
            fputs(f[3], stdout);                /* 위치 */
            fputs("\n", stdout);
            matched = 1;
        }
        printf(TU_SKY_BLUE " ════════════════════════════════════════════════════════════════════════════\n\n" COLOR_RESET);
        
    } else { /* -e: 긴급 연락처 전체 */
        printf(TU_MINT "[긴급 연락처]\n" COLOR_RESET);
        printf(TU_SKY_BLUE " ════════════════════════════════════════════════════════════════════════════\n" COLOR_RESET);
        fputs(TU_BLUE, stdout);
        printf("  "); print_padded("시설/기관명", 26);
        fputs("전화번호" COLOR_RESET "\n", stdout);
        printf(TU_SKY_BLUE " ════════════════════════════════════════════════════════════════════════════\n" COLOR_RESET);

        while (read_data_line(fp, line, sizeof(line))) {
            if (split_pipe(line, f, CAMPUS_MAX_FIELDS) < 3) continue;
            if (strcmp(f[0], "EMERG") != 0) continue;
            
            fputs(TU_MINT, stdout);
            printf("  "); print_padded(f[1], 26); /* 기관명 */
            fputs(COLOR_RESET, stdout);
            fputs(f[2], stdout);                /* 전화 */
            fputs("\n", stdout);
            matched = 1;
        }
        printf(TU_SKY_BLUE " ════════════════════════════════════════════════════════════════════════════\n\n" COLOR_RESET);
    }
    
    fclose(fp);

    if (!matched) {
        printf("검색 결과가 없습니다.\n\n");
    }
    return 0;
}