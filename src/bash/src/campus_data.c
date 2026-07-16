/*
 * campus_data.c - 로컬 데이터(data/campus 디렉터리의 .txt) 기반 캠퍼스 명령 핸들러 [우진]
 *                 bus / bob / map / weather / contact / schedule
 *
 * 근거 문서:
 *  - docs/10_planning/plan.md 6단계 ("로컬 텍스트/JSON 파일을 파싱해서 출력하고,
 *    추후 API 연동 확장이 가능하도록 함수 인터페이스를 분리해서 설계")
 *  - 01_상세기능명세서 9-2/9-4 (지원 명령어·옵션, 반환 규약)
 *  - 02_인터페이스명세서 3-3 (명령어별 사용 형식)
 *  - 05_개발철칙명세서 2-1(free 시점 주석), 3-x(시스템콜 반환값 검증)
 *
 * 데이터 파일 위치: 환경변수 TUK_CAMPUS_DATA 우선, 없으면 data/campus →
 *   ../../data/campus 순으로 탐색(빌드 위치 src/bash 및 리포 루트 실행 모두 지원).
 *
 * 반환 규약(01 9-4): 성공 0 / 데이터 로드 실패 -1. (옵션 사용법 오류는
 *   campus.c의 validate_*가 선행 처리하므로 여기서는 값만 사용한다.)
 * 출력 색상: TU MINT (05 4-1). 크로스쉘 출력 형식은 본 파트가 최초 정의.
 */
#include "campus.h" /* handle_*_command 선언 (인터페이스 고정) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> /* access, R_OK */

#include "tuk_shell.h" /* TUK_COLOR_MINT / TUK_COLOR_RESET */

/* 파싱된 데이터 파일: 한 덩어리 버퍼 + 주석/빈 줄 제외한 라인 포인터 배열 */
typedef struct {
    char *buf;    /* 파일 전체(라인은 이 버퍼를 가리킴) */
    char **lines; /* 유효 데이터 라인 포인터 배열 */
    int count;
} DataFile;

static void begin_mint(void) { fputs(TUK_COLOR_MINT, stdout); }
static void end_mint(void) { fputs(TUK_COLOR_RESET, stdout); }

/*
 * 데이터 파일의 읽기 가능한 경로를 찾는다.
 * TUK_CAMPUS_DATA가 지정되면 그 디렉터리만 사용한다(명시적 재정의 우선 —
 * 잘못 지정 시 조용히 폴백하지 않고 실패). 미지정이면 data/campus →
 * ../../data/campus 순으로 탐색(리포 루트/‘src/bash’ 실행 모두 지원).
 */
static int find_data_file(const char *filename, char *out, size_t outsz)
{
    const char *env = getenv("TUK_CAMPUS_DATA");
    const char *bases[4];
    size_t nb = 0;
    if (env != NULL && env[0] != '\0') {
        bases[nb++] = env; /* 명시적 지정: 이 경로만 시도 */
    } else {
        bases[nb++] = "data/campus";
        bases[nb++] = "../data/campus";
        bases[nb++] = "../../data/campus";
        bases[nb++] = "../../../data/campus";
    }

    for (size_t i = 0; i < nb; i++) {
        int w = snprintf(out, outsz, "%s/%s", bases[i], filename);
        if (w > 0 && (size_t)w < outsz && access(out, R_OK) == 0) {
            return 0;
        }
    }
    return -1;
}

/* 파일 전체를 malloc 문자열로 읽는다. 실패 시 NULL. */
static char *read_whole_file(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        perror(path);
        return NULL;
    }
    char *buf = NULL;
    size_t len = 0;
    size_t cap = 0;
    char chunk[4096];
    size_t n;
    while ((n = fread(chunk, 1, sizeof(chunk), fp)) > 0) {
        if (len + n + 1 > cap) {
            size_t new_cap = (cap == 0) ? (sizeof(chunk) * 2) : (cap * 2);
            while (new_cap < len + n + 1) {
                new_cap *= 2;
            }
            char *grown = realloc(buf, new_cap);
            if (grown == NULL) {
                perror("realloc");
                free(buf); /* free 시점: 확장 실패로 폐기 */
                fclose(fp);
                return NULL;
            }
            buf = grown;
            cap = new_cap;
        }
        memcpy(buf + len, chunk, n);
        len += n;
    }
    if (ferror(fp)) {
        perror(path);
        free(buf); /* free 시점: 읽기 실패로 폐기 */
        fclose(fp);
        return NULL;
    }
    fclose(fp);
    if (buf == NULL) {
        buf = malloc(1); /* 빈 파일 */
        if (buf == NULL) {
            perror("malloc");
            return NULL;
        }
    }
    buf[len] = '\0';
    return buf;
}

/*
 * 데이터 파일을 열어 주석(#)/빈 줄을 제외한 라인 배열로 만든다. 성공 0 / 실패 -1.
 * quiet=1이면 파일 부재 시 오류 메시지를 내지 않는다(bob/map의 보조 링크 조회처럼
 * 파일이 없어도 명령 자체는 계속 진행해야 하는 경우에 사용).
 */
static int data_open_ex(const char *filename, DataFile *df, int quiet)
{
    char path[512];
    if (find_data_file(filename, path, sizeof(path)) != 0) {
        if (!quiet) {
            fprintf(stderr, "campus: 데이터 파일을 찾을 수 없습니다 (%s)\n",
                    filename);
        }
        return -1;
    }
    char *buf = read_whole_file(path);
    if (buf == NULL) {
        return -1;
    }

    char **lines = NULL;
    int count = 0;
    int cap = 0;
    char *p = buf;
    while (*p != '\0') {
        char *nl = strchr(p, '\n');
        if (nl != NULL) {
            *nl = '\0';
        }
        char *end = (nl != NULL) ? nl : (p + strlen(p));
        if (end > p && *(end - 1) == '\r') {
            *(end - 1) = '\0'; /* CRLF 대응 */
        }
        if (*p != '\0' && *p != '#') { /* 데이터 라인만 수집 */
            if (count == cap) {
                int new_cap = (cap == 0) ? 32 : (cap * 2);
                char **grown = realloc(lines, (size_t)new_cap * sizeof(*grown));
                if (grown == NULL) {
                    perror("realloc");
                    free(lines); /* free 시점: 확장 실패로 폐기 */
                    free(buf);
                    return -1;
                }
                lines = grown;
                cap = new_cap;
            }
            lines[count++] = p;
        }
        if (nl == NULL) {
            break;
        }
        p = nl + 1;
    }
    df->buf = buf;
    df->lines = lines;
    df->count = count;
    return 0;
}

static int data_open(const char *filename, DataFile *df)
{
    return data_open_ex(filename, df, 0);
}

static void data_close(DataFile *df)
{
    /* free 시점: 핸들러 출력 완료 후. lines는 buf 내부를 가리키므로 buf는 마지막에 */
    free(df->lines);
    free(df->buf);
    df->lines = NULL;
    df->buf = NULL;
    df->count = 0;
}

/* line을 '|' 기준으로 in-place 분할(최대 max개). 반환: 실제 필드 수 */
static int split_fields(char *line, char **fields, int max)
{
    int n = 0;
    fields[n++] = line;
    for (char *p = line; *p != '\0' && n < max; p++) {
        if (*p == '|') {
            *p = '\0';
            fields[n++] = p + 1;
        }
    }
    return n;
}

/* CSV(콤마 구분) 토큰 개수 */
static int csv_count(const char *csv)
{
    if (*csv == '\0') {
        return 0;
    }
    int c = 1;
    for (const char *p = csv; *p != '\0'; p++) {
        if (*p == ',') {
            c++;
        }
    }
    return c;
}

/* CSV 토큰을 sep로 이어서 출력 */
static void print_csv(const char *csv, const char *sep)
{
    const char *p = csv;
    int first = 1;
    while (*p != '\0') {
        const char *comma = strchr(p, ',');
        if (!first) {
            fputs(sep, stdout);
        }
        first = 0;
        if (comma != NULL) {
            fwrite(p, 1, (size_t)(comma - p), stdout);
            p = comma + 1;
        } else {
            fputs(p, stdout);
            break;
        }
    }
}

/*
 * links.txt에서 key에 해당하는 정보 페이지 링크를 `🔗 <라벨> <suffix>: <url>`로
 * 출력한다(자주 바뀌는 페이지는 로컬 저장 대신 공식 링크만 안내). bob/map의 보조
 * 출력이므로 파일이 없어도 조용히 넘어간다.
 * 반환: 출력함 0 / 키 없음 1 / 파일 없음 -1.
 */
static int print_link_line(const char *key, const char *suffix)
{
    DataFile df;
    if (data_open_ex("links.txt", &df, 1) != 0) {
        return -1;
    }
    int rc = 1;
    for (int i = 0; i < df.count; i++) {
        char *f[3]; /* key|label|url */
        if (split_fields(df.lines[i], f, 3) < 3) {
            continue;
        }
        if (strcmp(f[0], key) == 0) {
            printf("🔗 %s %s: %s\n", f[1], suffix, f[2]);
            rc = 0;
            break;
        }
    }
    data_close(&df);
    return rc;
}

/* ============================================================
 * 명령 핸들러 (반환 규약 01 9-4). 옵션 유효성은 campus.c에서 선검증됨.
 * ============================================================ */

int handle_schedule_command(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    DataFile df;
    if (data_open("schedule.txt", &df) != 0) {
        return -1;
    }
    begin_mint();
    printf("[AI소프트웨어학과 시간표]\n");
    for (int i = 0; i < df.count; i++) {
        char *f[6];
        if (split_fields(df.lines[i], f, 6) < 6) {
            continue;
        }
        /* 요일 교시 시간 과목 교수 강의실 */
        printf(" %s  %s  %s  %s (%s, %s)\n", f[0], f[1], f[2], f[3], f[4], f[5]);
    }
    end_mint();
    data_close(&df);
    return 0;
}

int handle_bus_command(int argc, char **argv)
{
    (void)argc;
    const char *code = argv[1]; /* -1 | -2 */
    DataFile df;
    if (data_open("bus.txt", &df) != 0) {
        return -1;
    }
    begin_mint();
    int found = 0;
    for (int i = 0; i < df.count; i++) {
        char *f[5]; /* code|name|stops|weekday|weekend */
        if (split_fields(df.lines[i], f, 5) < 5) {
            continue;
        }
        if (strcmp(f[0], code) != 0) {
            continue;
        }
        found = 1;
        printf("[%s]\n", f[1]);
        printf("경유: ");
        print_csv(f[2], " → ");
        printf("\n");
        printf("평일 %d회: ", csv_count(f[3]));
        print_csv(f[3], " ");
        printf("\n");
        printf("주말 %d회: ", csv_count(f[4]));
        print_csv(f[4], " ");
        printf("\n");
        break;
    }
    if (!found) {
        printf("%s: 해당 노선 정보가 없습니다.\n", code);
    }
    end_mint();
    data_close(&df);
    return 0;
}

int handle_bob_command(int argc, char **argv)
{
    (void)argc;
    const char *code = argv[1]; /* -t | -e | -d */
    DataFile df;
    if (data_open("bob.txt", &df) != 0) {
        return -1;
    }
    /* 파일은 날짜 오름차순 → 해당 식당의 마지막 매칭이 가장 최근 메뉴 */
    char *best[6];
    int have = 0;
    for (int i = 0; i < df.count; i++) {
        char *f[6]; /* date|weekday|code|name|price|menu */
        if (split_fields(df.lines[i], f, 6) < 6) {
            continue;
        }
        if (strcmp(f[2], code) == 0) {
            for (int k = 0; k < 6; k++) {
                best[k] = f[k];
            }
            have = 1;
        }
    }
    begin_mint();
    if (have) {
        printf("[%s] %s (%s)  %s원\n", best[3], best[0], best[1], best[4]);
        printf("메뉴: ");
        print_csv(best[5], ", ");
        printf("\n");
    } else {
        printf("%s: 해당 식당 메뉴를 찾을 수 없습니다.\n", code);
    }
    /* 식당별 최신 메뉴 링크(있으면). -d(대신식당)는 미확보라 항목이 없어 생략된다. */
    char link_key[16];
    snprintf(link_key, sizeof(link_key), "meal%s", code); /* meal-t / meal-E / meal-d */
    (void)print_link_line(link_key, "메뉴");
    end_mint();
    data_close(&df);
    return 0;
}

int handle_map_command(int argc, char **argv)
{
    DataFile df;
    if (data_open("map.txt", &df) != 0) {
        return -1;
    }
    begin_mint();
    if (argc == 3 && strcmp(argv[1], "-find") == 0) {
        const char *room = argv[2];
        int found = 0;
        for (int i = 0; i < df.count; i++) {
            char *f[5]; /* ROOM|room_no|building|floor|desc */
            if (split_fields(df.lines[i], f, 5) < 5) {
                continue;
            }
            if (strcmp(f[0], "ROOM") == 0 && strcmp(f[1], room) == 0) {
                printf("[%s] %s %s층 - %s\n", f[1], f[2], f[3], f[4]);
                found = 1;
                break;
            }
        }
        if (!found) {
            printf("%s: 해당 강의실 정보를 찾을 수 없습니다.\n", room);
        }
    } else if (strcmp(argv[1], "-f") == 0) {
        printf("[교내 편의시설]\n");
        for (int i = 0; i < df.count; i++) {
            char *f[3]; /* FAC|name|location */
            if (split_fields(df.lines[i], f, 3) < 3) {
                continue;
            }
            if (strcmp(f[0], "FAC") == 0) {
                printf(" %s - %s\n", f[1], f[2]);
            }
        }
        (void)print_link_line("facility", "안내"); /* 편의시설 공식 링크(있으면) */
    } else if (strcmp(argv[1], "-s") == 0) { /* 스포츠 플라자: 링크 안내 전용 */
        if (print_link_line("sports", "안내") != 0) {
            printf("스포츠 플라자 링크 정보를 찾을 수 없습니다.\n");
        }
    } else { /* -A ~ -G 건물 층별 안내 */
        const char *code = argv[1];
        int header = 0;
        for (int i = 0; i < df.count; i++) {
            char *f[5]; /* BUILDING|code|name|floor|floor_desc */
            if (split_fields(df.lines[i], f, 5) < 5) {
                continue;
            }
            if (strcmp(f[0], "BUILDING") == 0 && strcmp(f[1], code) == 0) {
                if (!header) {
                    printf("[%s]\n", f[2]);
                    header = 1;
                }
                printf(" %s층: %s\n", f[3], f[4]);
            }
        }
        if (!header) {
            printf("%s: 해당 건물 정보가 없습니다.\n", code);
        }
    }
    end_mint();
    data_close(&df);
    return 0;
}

int handle_weather_command(int argc, char **argv)
{
    (void)argc;
    const char *opt = argv[1]; /* -c | -w | -d */
    DataFile df;
    if (data_open("weather.txt", &df) != 0) {
        return -1;
    }
    begin_mint();
    if (df.count == 0) {
        printf("날씨 데이터가 없습니다.\n");
        end_mint();
        data_close(&df);
        return 0;
    }
    /* 파일 형식: date|location|condition|temp_high|temp_low|dust */
    if (strcmp(opt, "-c") == 0) { /* 현재 = 마지막 행 */
        char *f[6];
        if (split_fields(df.lines[df.count - 1], f, 6) >= 6) {
            printf("[현재 날씨] %s · %s\n", f[1], f[0]);
            printf("%s  최고 %s℃ / 최저 %s℃  미세먼지 %s\n", f[2], f[3], f[4],
                   f[5]);
        }
    } else if (strcmp(opt, "-w") == 0) { /* 주간 = 최근 7행 */
        printf("[주간 예보] 최근 7일\n");
        int start = (df.count >= 7) ? (df.count - 7) : 0;
        for (int i = start; i < df.count; i++) {
            char *f[6];
            if (split_fields(df.lines[i], f, 6) < 6) {
                continue;
            }
            printf(" %s  %s  %s/%s℃  미세먼지 %s\n", f[0], f[2], f[3], f[4],
                   f[5]);
        }
    } else { /* -d 미세먼지: 각 라인 1회만 분할해 dust 수집 */
        int start = (df.count >= 7) ? (df.count - 7) : 0;
        char *dusts[7];
        char *date = NULL;
        char *loc = NULL;
        int dn = 0;
        for (int i = start; i < df.count; i++) {
            char *f[6];
            if (split_fields(df.lines[i], f, 6) < 6) {
                continue;
            }
            dusts[dn++] = f[5];
            date = f[0];
            loc = f[1];
        }
        printf("[미세먼지] %s · %s 기준\n", (loc != NULL) ? loc : "",
               (date != NULL) ? date : "");
        printf(" 현재: %s\n", (dn > 0) ? dusts[dn - 1] : "-");
        printf(" 최근 7일: ");
        for (int i = 0; i < dn; i++) {
            if (i > 0) {
                putchar(' ');
            }
            fputs(dusts[i], stdout);
        }
        printf("\n");
    }
    end_mint();
    data_close(&df);
    return 0;
}

int handle_contact_command(int argc, char **argv)
{
    DataFile df;
    if (data_open("contact.txt", &df) != 0) {
        return -1;
    }
    begin_mint();
    if (argc == 2 && strcmp(argv[1], "-e") == 0) { /* 긴급 연락처 모아보기 */
        printf("[긴급 연락처]\n");
        for (int i = 0; i < df.count; i++) {
            char *f[3]; /* EMERG|name|phone */
            if (split_fields(df.lines[i], f, 3) < 3) {
                continue;
            }
            if (strcmp(f[0], "EMERG") == 0) {
                printf(" %s · %s\n", f[1], f[2]);
            }
        }
    } else if (strcmp(argv[1], "-p") == 0) { /* 교수 이름 부분 검색 */
        const char *q = argv[2];
        int found = 0;
        printf("[교수 연락처] \"%s\"\n", q);
        for (int i = 0; i < df.count; i++) {
            char *f[6]; /* PROF|name|dept|office|phone|email */
            if (split_fields(df.lines[i], f, 6) < 6) {
                continue;
            }
            if (strcmp(f[0], "PROF") == 0 && strstr(f[1], q) != NULL) {
                printf(" %s · %s · %s · %s · %s\n", f[1], f[2], f[3], f[4],
                       f[5]);
                found = 1;
            }
        }
        if (!found) {
            printf(" 검색 결과가 없습니다.\n");
        }
    } else { /* -d 부서명 부분 검색 */
        const char *q = argv[2];
        int found = 0;
        printf("[부서 연락처] \"%s\"\n", q);
        for (int i = 0; i < df.count; i++) {
            char *f[4]; /* DEPT|name|phone|location */
            if (split_fields(df.lines[i], f, 4) < 4) {
                continue;
            }
            if (strcmp(f[0], "DEPT") == 0 && strstr(f[1], q) != NULL) {
                printf(" %s · %s · %s\n", f[1], f[2], f[3]);
                found = 1;
            }
        }
        if (!found) {
            printf(" 검색 결과가 없습니다.\n");
        }
    }
    end_mint();
    data_close(&df);
    return 0;
}
