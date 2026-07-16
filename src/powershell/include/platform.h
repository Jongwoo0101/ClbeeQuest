#ifndef TUK_PLATFORM_H
#define TUK_PLATFORM_H

#include <stddef.h>

#ifdef _WIN32
#include <windows.h>
typedef DWORD tuk_pid_t;
typedef HANDLE tuk_process_handle_t;
#else
#include <sys/types.h>
typedef pid_t tuk_pid_t;
typedef pid_t tuk_process_handle_t;
#endif

typedef enum {
    TUK_WAIT_RUNNING = 0,
    TUK_WAIT_EXITED,
    TUK_WAIT_STOPPED,
    TUK_WAIT_CONTINUED
} TukWaitState;

/* Starts a native child process and returns both its id and tracking handle. */
int tuk_spawn(char **argv, tuk_pid_t *pid, tuk_process_handle_t *handle);

/* Polls or waits for a child. nohang=1 is used by jobs/top refreshes. */
int tuk_wait_process(tuk_pid_t pid, tuk_process_handle_t handle, int nohang,
                     TukWaitState *state, int *exit_code);

/* Releases the OS handle without terminating the child process. */
void tuk_close_process_handle(tuk_process_handle_t handle);

int tuk_set_environment(const char *name, const char *value, int overwrite);
int tuk_change_directory(const char *path);
char *tuk_get_current_directory(char *buffer, size_t buffer_size);
int tuk_file_readable(const char *path);
int tuk_resolve_path(const char *path, char *out, size_t out_size);
void tuk_flush_stdin(void);
void tuk_sleep_ms(unsigned long milliseconds);
void tuk_init_console(void);
char *tuk_strdup(const char *text);

#ifdef _WIN32
#define tuk_popen _popen
#define tuk_pclose _pclose
#else
#define tuk_popen popen
#define tuk_pclose pclose
#endif

#endif /* TUK_PLATFORM_H */
