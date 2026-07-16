#include "platform.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32

#include <direct.h>
#include <io.h>
#include <process.h>
#include <windows.h>

static int append_text(char *buffer, size_t capacity, size_t *length,
                       const char *text)
{
    size_t text_length = strlen(text);
    if (*length > capacity - 1 || text_length > capacity - 1 - *length) {
        return -1;
    }
    memcpy(buffer + *length, text, text_length);
    *length += text_length;
    buffer[*length] = '\0';
    return 0;
}

static int append_power_shell_command(char *buffer, size_t capacity,
                                      size_t *length, char **argv)
{
    if (append_text(buffer, capacity, length,
                    "powershell.exe -NoLogo -NoProfile -NonInteractive -Command \"") != 0) {
        return -1;
    }

    for (int i = 0; argv[i] != NULL; i++) {
        if (i > 0 && append_text(buffer, capacity, length, " ") != 0) {
            return -1;
        }
        for (const char *p = argv[i]; *p != '\0'; p++) {
            if (*p == '"' &&
                append_text(buffer, capacity, length, "\\\"") != 0) {
                return -1;
            }
            char one[2] = { *p, '\0' };
            if (*p != '"' && append_text(buffer, capacity, length, one) != 0) {
                return -1;
            }
        }
    }
    return append_text(buffer, capacity, length, "\"");
}

int tuk_spawn(char **argv, tuk_pid_t *pid, tuk_process_handle_t *handle)
{
    if (argv == NULL || argv[0] == NULL || pid == NULL || handle == NULL) {
        errno = EINVAL;
        return -1;
    }

    /* MAX_INPUT_LENGTH is 1024; this leaves room for PowerShell quoting. */
    char command_line[4096];
    size_t length = 0;
    if (append_power_shell_command(command_line, sizeof(command_line), &length,
                                   argv) != 0) {
        fprintf(stderr, "tuk-shell: command line is too long\n");
        errno = E2BIG;
        return -1;
    }

    STARTUPINFOA startup = {0};
    PROCESS_INFORMATION process = {0};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    if (!CreateProcessA(NULL, command_line, NULL, NULL, TRUE, 0, NULL, NULL,
                        &startup, &process)) {
        fprintf(stderr, "tuk-shell: CreateProcess failed (error %lu)\n",
                (unsigned long)GetLastError());
        errno = EIO;
        return -1;
    }

    CloseHandle(process.hThread);
    *pid = process.dwProcessId;
    *handle = process.hProcess;
    return 0;
}

int tuk_wait_process(tuk_pid_t pid, tuk_process_handle_t handle, int nohang,
                     TukWaitState *state, int *exit_code)
{
    (void)pid;
    if (handle == NULL || state == NULL || exit_code == NULL) {
        errno = EINVAL;
        return -1;
    }

    DWORD wait_result = WaitForSingleObject(handle,
                                            nohang ? 0 : INFINITE);
    if (wait_result == WAIT_TIMEOUT) {
        *state = TUK_WAIT_RUNNING;
        return 0;
    }
    if (wait_result != WAIT_OBJECT_0) {
        fprintf(stderr, "tuk-shell: WaitForSingleObject failed (error %lu)\n",
                (unsigned long)GetLastError());
        errno = EIO;
        return -1;
    }

    DWORD code = 0;
    if (!GetExitCodeProcess(handle, &code)) {
        fprintf(stderr, "tuk-shell: GetExitCodeProcess failed (error %lu)\n",
                (unsigned long)GetLastError());
        errno = EIO;
        return -1;
    }
    *state = TUK_WAIT_EXITED;
    *exit_code = (int)code;
    return 0;
}

void tuk_close_process_handle(tuk_process_handle_t handle)
{
    if (handle != NULL) {
        CloseHandle(handle);
    }
}

int tuk_set_environment(const char *name, const char *value, int overwrite)
{
    if (!overwrite && getenv(name) != NULL) {
        return 0;
    }
    return _putenv_s(name, value);
}

int tuk_change_directory(const char *path)
{
    return _chdir(path);
}

char *tuk_get_current_directory(char *buffer, size_t buffer_size)
{
    return _getcwd(buffer, (int)buffer_size);
}

int tuk_file_readable(const char *path)
{
    return _access(path, 4) == 0;
}

int tuk_resolve_path(const char *path, char *out, size_t out_size)
{
    return _fullpath(out, path, out_size) == NULL ? -1 : 0;
}

void tuk_flush_stdin(void)
{
    /* There is no termios input queue on the Windows console. */
}

void tuk_sleep_ms(unsigned long milliseconds)
{
    Sleep(milliseconds);
}

void tuk_init_console(void)
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (output != INVALID_HANDLE_VALUE && GetConsoleMode(output, &mode)) {
        SetConsoleMode(output, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
}

char *tuk_strdup(const char *text)
{
    return _strdup(text);
}

#else

#include <limits.h>
#include <termios.h>
#include <time.h>
#include <sys/wait.h>
#include <unistd.h>

int tuk_spawn(char **argv, tuk_pid_t *pid, tuk_process_handle_t *handle)
{
    if (argv == NULL || argv[0] == NULL || pid == NULL || handle == NULL) {
        errno = EINVAL;
        return -1;
    }
    pid_t child = fork();
    if (child < 0) {
        return -1;
    }
    if (child == 0) {
        execvp(argv[0], argv);
        perror(argv[0]);
        _exit(127);
    }
    *pid = child;
    *handle = child;
    return 0;
}

int tuk_wait_process(tuk_pid_t pid, tuk_process_handle_t handle, int nohang,
                     TukWaitState *state, int *exit_code)
{
    (void)handle;
    if (state == NULL || exit_code == NULL) {
        errno = EINVAL;
        return -1;
    }

    int status = 0;
    pid_t result = waitpid(pid, &status,
                           nohang ? (WNOHANG | WUNTRACED | WCONTINUED) : 0);
    if (result < 0) {
        return -1;
    }
    if (result == 0) {
        *state = TUK_WAIT_RUNNING;
        return 0;
    }
    if (WIFEXITED(status)) {
        *state = TUK_WAIT_EXITED;
        *exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        *state = TUK_WAIT_EXITED;
        *exit_code = 128 + WTERMSIG(status);
    } else if (WIFSTOPPED(status)) {
        *state = TUK_WAIT_STOPPED;
    } else if (WIFCONTINUED(status)) {
        *state = TUK_WAIT_CONTINUED;
    }
    return 0;
}

void tuk_close_process_handle(tuk_process_handle_t handle)
{
    (void)handle;
}

int tuk_set_environment(const char *name, const char *value, int overwrite)
{
    if (!overwrite && getenv(name) != NULL) {
        return 0;
    }
    return setenv(name, value, 1);
}

int tuk_change_directory(const char *path)
{
    return chdir(path);
}

char *tuk_get_current_directory(char *buffer, size_t buffer_size)
{
    return getcwd(buffer, buffer_size);
}

int tuk_file_readable(const char *path)
{
    return access(path, R_OK) == 0;
}

int tuk_resolve_path(const char *path, char *out, size_t out_size)
{
    (void)out_size;
    return realpath(path, out) == NULL ? -1 : 0;
}

void tuk_flush_stdin(void)
{
    if (tcflush(STDIN_FILENO, TCIFLUSH) != 0) {
        /* stdin may be a pipe in automated tests; no flush is needed there. */
    }
}

void tuk_sleep_ms(unsigned long milliseconds)
{
    struct timespec delay;
    delay.tv_sec = (time_t)(milliseconds / 1000);
    delay.tv_nsec = (long)(milliseconds % 1000) * 1000000L;
    while (nanosleep(&delay, &delay) != 0 && errno == EINTR) {
    }
}

void tuk_init_console(void)
{
}

char *tuk_strdup(const char *text)
{
    return strdup(text);
}

#endif
