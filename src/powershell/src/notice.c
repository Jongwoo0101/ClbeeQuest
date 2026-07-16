/*
 * notice.c - notice 명령어 실데이터 핸들러
 *            [우진 구현 - 원종우 Mock 핸들러 인수인계, 종우 승인]
 *
 * 근거 문서:
 *  - docs/10_planning/plan.md (notice: -g 일반 / -a 학사 / -s 장학 / -n N)
 *  - 01_상세기능명세서 9-4 (핸들러 반환 규약: 성공 0 / 사용법 1 / 데이터 로드 실패 -1)
 *  - 02_인터페이스명세서 7장 (내부 명령 출력 형식은 3쉘 동일 - 본 파트가 최초 정의)
 *  - 05_개발철칙명세서 2-1(free 시점 주석), 3-x(시스템콜 반환값 검증+perror)
 *
 * 데이터 출처: 한국공학대학교 학사공지 게시판(k2cms) HTML 목록
 *   https://www.tukorea.ac.kr/bbs/tukorea/1303/artclList.do?bbsOpenWrdSeq=<카테고리>
 *   해당 게시판의 rssList.do는 캐시 고장으로 1건만 반환하므로(2026-07 확인),
 *   RSS 대신 사람이 보는 목록 페이지 HTML을 파싱한다.
 *
 * 카테고리(bbsOpenWrdSeq): 학사=482, 장학=484, 일반=487, 전체=미지정.
 *
 * 오프라인/검증: 환경변수 TUK_NOTICE_FIXTURE에 로컬 HTML 경로를 주면
 *   네트워크 대신 그 파일을 파싱한다(회귀 테스트 T09, 인터넷 없는 데모 폴백).
 */
#include "campus.h" /* handle_notice_command 선언 (인터페이스 고정) */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"    /* parse_positive_long */
#include "platform.h"
#include "tuk_shell.h" /* TUK_COLOR_MINT / TUK_COLOR_RESET */

#define NOTICE_URL_BASE "https://www.tukorea.ac.kr"
#define NOTICE_BOARD_PATH "/bbs/tukorea/1303/artclList.do"
#define NOTICE_ARTICLE_MARK "/artclView.do"
#define NOTICE_HREF_PREFIX "href=\"/bbs/tukorea/1303/"
#define NOTICE_MAX_ITEMS 200 /* 목록 1페이지 분량 상한(폭주 방지) */

/* 공지 1건. 문자열은 모두 malloc, notice_free_items()로 일괄 해제. */
typedef struct {
    char *title;
    char *date;
    char *url;
} NoticeItem;

static void notice_free_items(NoticeItem *items, long count)
{
    if (items == NULL) {
        return;
    }
    for (long i = 0; i < count; i++) {
        /* free 시점: 각 항목 문자열은 append 시 소유권이 배열로 이전됨 */
        free(items[i].title);
        free(items[i].date);
        free(items[i].url);
    }
    free(items);
}

/* [s, e) 구간에 sub가 존재하면 1, 아니면 0 (구간 밖 매칭은 제외) */
static int contains_within(const char *s, const char *e, const char *sub)
{
    const char *hit = strstr(s, sub);
    return hit != NULL && hit < e;
}

/*
 * &amp; &lt; &gt; &quot; &#39; &apos; &nbsp; 를 in-place로 복원한다.
 * 디코드 결과 길이는 항상 원본 이하이므로 같은 버퍼에 다시 쓴다.
 */
static void decode_entities(char *s)
{
    char *w = s;
    for (const char *r = s; *r != '\0';) {
        if (*r != '&') {
            *w++ = *r++;
            continue;
        }
        if (strncmp(r, "&amp;", 5) == 0) {
            *w++ = '&';
            r += 5;
        } else if (strncmp(r, "&lt;", 4) == 0) {
            *w++ = '<';
            r += 4;
        } else if (strncmp(r, "&gt;", 4) == 0) {
            *w++ = '>';
            r += 4;
        } else if (strncmp(r, "&quot;", 6) == 0) {
            *w++ = '"';
            r += 6;
        } else if (strncmp(r, "&#39;", 5) == 0 || strncmp(r, "&apos;", 6) == 0) {
            *w++ = '\'';
            r += (r[1] == '#') ? 5 : 6;
        } else if (strncmp(r, "&nbsp;", 6) == 0) {
            *w++ = ' ';
            r += 6;
        } else {
            *w++ = *r++; /* 미지원 엔티티는 그대로 둔다 */
        }
    }
    *w = '\0';
}

/* [start, end) 앞뒤 공백을 잘라 복제 후 엔티티 디코드. 실패 시 NULL. */
static char *dup_trimmed(const char *start, const char *end)
{
    while (start < end && isspace((unsigned char)*start)) {
        start++;
    }
    while (end > start && isspace((unsigned char)*(end - 1))) {
        end--;
    }
    size_t len = (size_t)(end - start);
    char *out = malloc(len + 1); /* free 시점: notice_free_items() */
    if (out == NULL) {
        perror("malloc");
        return NULL;
    }
    memcpy(out, start, len);
    out[len] = '\0';
    decode_entities(out);
    return out;
}

/*
 * from 이후에서 open 다음부터 close 전까지의 텍스트를 잘라 복제한다.
 * 두 태그 모두 limit 이전에 있어야 유효(다음 항목 침범 방지). 없으면 NULL.
 */
static char *extract_between(const char *from, const char *limit,
                            const char *open, const char *close)
{
    const char *o = strstr(from, open);
    if (o == NULL || o >= limit) {
        return NULL;
    }
    o += strlen(open);
    const char *c = strstr(o, close);
    if (c == NULL || c > limit) {
        return NULL;
    }
    return dup_trimmed(o, c);
}

/*
 * from 이후 첫 '게시글 보기' 링크 앵커(href="/bbs/tukorea/1303/..artclView.do")를
 * 가리키는 포인터를 반환한다. 페이징 등 비-게시글 링크는 건너뛴다. 없으면 NULL.
 */
static const char *find_article_href(const char *from)
{
    const char *p = from;
    while ((p = strstr(p, NOTICE_HREF_PREFIX)) != NULL) {
        const char *v = p + strlen("href=\"");   /* href 값 시작('/bbs..') */
        const char *q = strchr(v, '"');
        if (q == NULL) {
            return NULL;
        }
        if (contains_within(v, q, NOTICE_ARTICLE_MARK)) {
            return p;
        }
        p = q; /* 게시글 링크가 아니면(예: 페이징) 다음 후보로 */
    }
    return NULL;
}

/* href 값 [v, q) 앞에 도메인을 붙인 절대 URL을 복제한다. 실패 시 NULL. */
static char *build_url(const char *v, const char *q)
{
    size_t path_len = (size_t)(q - v);
    size_t total = strlen(NOTICE_URL_BASE) + path_len;
    char *url = malloc(total + 1); /* free 시점: notice_free_items() */
    if (url == NULL) {
        perror("malloc");
        return NULL;
    }
    memcpy(url, NOTICE_URL_BASE, strlen(NOTICE_URL_BASE));
    memcpy(url + strlen(NOTICE_URL_BASE), v, path_len);
    url[total] = '\0';
    return url;
}

/* items 배열에 1건 추가(필요 시 확장). 성공 0 / 메모리 부족 -1 */
static int append_item(NoticeItem **items, long *count, long *cap,
                      char *title, char *date, char *url)
{
    if (*count == *cap) {
        long new_cap = (*cap == 0) ? 16 : (*cap * 2);
        NoticeItem *grown = realloc(*items, (size_t)new_cap * sizeof(**items));
        if (grown == NULL) {
            perror("realloc");
            return -1;
        }
        *items = grown;
        *cap = new_cap;
    }
    (*items)[*count].title = title; /* 소유권 이전: 이후 해제는 배열이 담당 */
    (*items)[*count].date = date;
    (*items)[*count].url = url;
    (*count)++;
    return 0;
}

/*
 * 게시판 목록 HTML에서 공지 항목을 최신순으로 파싱한다.
 * 성공: 파싱 개수(>=0) 반환, *out에 배열 할당(호출자가 notice_free_items로 해제).
 * 메모리 부족: -1 반환, *out은 NULL.
 */
static long notice_parse(const char *html, NoticeItem **out)
{
    NoticeItem *items = NULL;
    long count = 0;
    long cap = 0;
    const char *html_end = html + strlen(html);
    const char *cursor = html;

    while (count < NOTICE_MAX_ITEMS) {
        const char *href = find_article_href(cursor);
        if (href == NULL) {
            break;
        }
        const char *v = href + strlen("href=\"");
        const char *q = strchr(v, '"');
        if (q == NULL) {
            break;
        }
        /* 현재 항목 필드 검색 범위는 다음 게시글 앵커 직전까지로 제한한다. */
        const char *next = find_article_href(q);
        const char *region_end = (next != NULL) ? next : html_end;

        const char *title_div = strstr(q, "<div class=\"title\">");
        char *title = NULL;
        if (title_div != NULL && title_div < region_end) {
            title = extract_between(title_div, region_end, "<strong>",
                                    "</strong>");
        }
        if (title == NULL) {
            /* 제목을 못 얻으면 유효 항목이 아니므로 건너뛴다 */
            cursor = (next != NULL) ? next : html_end;
            if (next == NULL) {
                break;
            }
            continue;
        }
        const char *date_dl = strstr(q, "<dl class=\"date\">");
        char *date = NULL;
        if (date_dl != NULL && date_dl < region_end) {
            date = extract_between(date_dl, region_end, "<dd>", "</dd>");
        }
        char *url = build_url(v, q);

        if (append_item(&items, &count, &cap, title, date, url) != 0) {
            free(title); /* free 시점: 아직 배열에 미등록이므로 직접 해제 */
            free(date);
            free(url);
            notice_free_items(items, count);
            *out = NULL;
            return -1;
        }
        cursor = (next != NULL) ? next : html_end;
        if (next == NULL) {
            break;
        }
    }
    *out = items;
    return count;
}

/* 파일 전체를 malloc 문자열로 읽는다(오프라인 fixture 경로). 실패 시 NULL. */
static char *read_file_to_string(const char *path)
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
                free(buf); /* free 시점: 확장 실패로 기존 버퍼 폐기 */
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
        free(buf); /* free 시점: 읽기 실패로 버퍼 폐기 */
        fclose(fp);
        return NULL;
    }
    fclose(fp);
    if (buf == NULL) { /* 빈 파일 */
        buf = malloc(1);
        if (buf == NULL) {
            perror("malloc");
            return NULL;
        }
    }
    buf[len] = '\0';
    return buf;
}

/* curl 자식 프로세스의 표준출력을 malloc 문자열로 수집한다. 실패 시 NULL. */
static char *fetch_via_curl(int category_seq)
{
    /* category_seq는 내부 상수(0/482/484/487)뿐이라 주입 위험이 없다. */
    char cmd[512];
#ifdef _WIN32
    const char *template_by_category =
        "curl -s -L --max-time 8 -A \"TUK-Shell notice\" \"%s%s?bbsOpenWrdSeq=%d\"";
    const char *template_all =
        "curl -s -L --max-time 8 -A \"TUK-Shell notice\" \"%s%s\"";
#else
    const char *template_by_category =
        "curl -s -L --max-time 8 -A 'TUK-Shell notice' '%s%s?bbsOpenWrdSeq=%d'";
    const char *template_all =
        "curl -s -L --max-time 8 -A 'TUK-Shell notice' '%s%s'";
#endif
    if (category_seq > 0) {
        snprintf(cmd, sizeof(cmd), template_by_category, NOTICE_URL_BASE,
                 NOTICE_BOARD_PATH, category_seq);
    } else {
        snprintf(cmd, sizeof(cmd), template_all, NOTICE_URL_BASE,
                 NOTICE_BOARD_PATH);
    }

    FILE *pp = tuk_popen(cmd, "r");
    if (pp == NULL) {
        perror("popen"); /* 05 3-x: 시스템콜 반환값 검증 */
        return NULL;
    }
    char *buf = NULL;
    size_t len = 0;
    size_t cap = 0;
    char chunk[4096];
    size_t n;
    while ((n = fread(chunk, 1, sizeof(chunk), pp)) > 0) {
        if (len + n + 1 > cap) {
            size_t new_cap = (cap == 0) ? (sizeof(chunk) * 2) : (cap * 2);
            while (new_cap < len + n + 1) {
                new_cap *= 2;
            }
            char *grown = realloc(buf, new_cap);
            if (grown == NULL) {
                perror("realloc");
                free(buf); /* free 시점: 확장 실패로 폐기 */
                tuk_pclose(pp);
                return NULL;
            }
            buf = grown;
            cap = new_cap;
        }
        memcpy(buf + len, chunk, n);
        len += n;
    }
    int status = tuk_pclose(pp);
    if (status != 0) {
        /* curl 미설치/네트워크 오류 등: 데이터 로드 실패로 처리 */
        free(buf); /* free 시점: 실패 응답 폐기 */
        return NULL;
    }
    if (buf == NULL) {
        return NULL; /* 빈 응답 */
    }
    buf[len] = '\0';
    return buf;
}

/* fixture 환경변수가 있으면 로컬 파일, 없으면 curl로 HTML을 얻는다. */
static char *notice_fetch(int category_seq)
{
    const char *fixture = getenv("TUK_NOTICE_FIXTURE");
    if (fixture != NULL && fixture[0] != '\0') {
        return read_file_to_string(fixture);
    }
    return fetch_via_curl(category_seq);
}

/*
 * argv에서 카테고리(-g/-a/-s)와 개수(-n N)를 추출한다.
 * campus.c의 validate_notice()가 이미 형식을 보증하므로 여기서는 값만 뽑는다.
 * category_seq: 전체=0, 일반=487, 학사=482, 장학=484 / *label에 한글 라벨.
 * limit: -1이면 전체 표시.
 */
static void parse_notice_args(int argc, char **argv, int *category_seq,
                             const char **label, long *limit)
{
    *category_seq = 0;
    *label = "전체";
    *limit = -1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-g") == 0) {
            *category_seq = 487;
            *label = "일반";
        } else if (strcmp(argv[i], "-a") == 0) {
            *category_seq = 482;
            *label = "학사";
        } else if (strcmp(argv[i], "-s") == 0) {
            *category_seq = 484;
            *label = "장학";
        } else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            (void)parse_positive_long(argv[i + 1], limit); /* 검증 완료 값 */
            i++;
        }
    }
}

int handle_notice_command(int argc, char **argv)
{
    int category_seq;
    const char *label;
    long limit;
    parse_notice_args(argc, argv, &category_seq, &label, &limit);

    char *html = notice_fetch(category_seq);
    if (html == NULL) {
        /* 01 9-4: 데이터 로드 실패 -> -1 */
        fprintf(stderr,
                "notice: 공지사항을 불러오지 못했습니다 (네트워크/curl 확인)\n");
        return -1;
    }

    NoticeItem *items = NULL;
    long count = notice_parse(html, &items);
    free(html); /* free 시점: 파싱이 문자열을 모두 복제했으므로 원본 폐기 */
    if (count < 0) {
        fprintf(stderr, "notice: 메모리 부족으로 파싱에 실패했습니다\n");
        return -1;
    }

    long shown = count;
    if (limit >= 0 && limit < count) {
        shown = limit;
    }

    fputs(TUK_COLOR_MINT, stdout);
    printf("[%s] 최근 %ld건\n", label, shown);
    if (shown == 0) {
        printf("  등록된 공지가 없습니다.\n");
    }
    for (long i = 0; i < shown; i++) {
        printf(" %2ld. %s  (%s)\n", i + 1, items[i].title,
               (items[i].date != NULL) ? items[i].date : "-");
        if (items[i].url != NULL) {
            printf("     %s\n", items[i].url);
        }
    }
    fputs(TUK_COLOR_RESET, stdout);

    notice_free_items(items, count); /* free 시점: 출력 완료 후 일괄 해제 */
    return 0;
}
