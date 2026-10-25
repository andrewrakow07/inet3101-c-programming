/* util.c - settings, log file, limits and input helpers */

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include "billing.h"

Config cfg;

/* Build a path inside the data folder. */
void path_in_data(char *out, int size, char *name)
{
    snprintf(out, size, "%s/%s", cfg.data_dir, name);
}

/* Make a folder. It is fine if it already exists. */
static int make_dir(char *path)
{
    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "Cannot create folder '%s'\n", path);
        return -1;
    }
    return 0;
}

/* Read settings from environment variables and make the folders. */
int config_load(void)
{
    char *dir = getenv("BILLING_DATA_DIR");
    char *port = getenv("BILLING_PORT");
    char *log_max = getenv("BILLING_LOG_MAX_BYTES");
    char *end;
    char path[PATH_LEN];

    /* defaults */
    strcpy(cfg.data_dir, "data");
    cfg.port = 5050;
    cfg.log_max_bytes = 1048576;

    if (dir != NULL && dir[0] != '\0') {
        snprintf(cfg.data_dir, PATH_LEN, "%s", dir);
    }

    if (port != NULL && port[0] != '\0') {
        long value = strtol(port, &end, 10);
        if (*end != '\0' || value < 1024 || value > 65535) {
            fprintf(stderr, "Bad BILLING_PORT (use 1024 to 65535)\n");
            return -1;
        }
        cfg.port = (int)value;
    }

    if (log_max != NULL && log_max[0] != '\0') {
        long value = strtol(log_max, &end, 10);
        if (*end != '\0' || value < 1024) {
            fprintf(stderr, "Bad BILLING_LOG_MAX_BYTES (minimum 1024)\n");
            return -1;
        }
        cfg.log_max_bytes = value;
    }

    if (make_dir(cfg.data_dir) != 0) return -1;
    path_in_data(path, PATH_LEN, "invoices");
    if (make_dir(path) != 0) return -1;
    path_in_data(path, PATH_LEN, "exports");
    if (make_dir(path) != 0) return -1;
    return 0;
}

/* Limit open files to 256 and turn off core dumps. */
void limits_apply(void)
{
    struct rlimit rl;

    if (getrlimit(RLIMIT_NOFILE, &rl) == 0) {
        if (rl.rlim_cur > 256) {
            rl.rlim_cur = 256;
        }
        setrlimit(RLIMIT_NOFILE, &rl);
    }

    rl.rlim_cur = 0;
    rl.rlim_max = 0;
    setrlimit(RLIMIT_CORE, &rl);
}

/* Rename billing.log to billing.log.1, .1 to .2, and so on. */
static void rotate_log(char *path)
{
    char from[PATH_LEN + 8];
    char to[PATH_LEN + 8];
    int i;

    snprintf(to, sizeof(to), "%s.%d", path, LOG_KEEP);
    remove(to);

    for (i = LOG_KEEP - 1; i >= 1; i--) {
        snprintf(from, sizeof(from), "%s.%d", path, i);
        snprintf(to, sizeof(to), "%s.%d", path, i + 1);
        rename(from, to);
    }

    snprintf(to, sizeof(to), "%s.1", path);
    rename(path, to);
}

/* Add one line to the log file. Rotate the log if it is too big. */
void log_event(char *fmt, ...)
{
    char path[PATH_LEN];
    char stamp[32];
    struct stat st;
    time_t now;
    FILE *f;
    va_list args;

    path_in_data(path, PATH_LEN, "billing.log");

    if (stat(path, &st) == 0 && st.st_size >= cfg.log_max_bytes) {
        rotate_log(path);
    }

    f = fopen(path, "a");
    if (f == NULL) return;

    now = time(NULL);
    strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(f, "[%s] ", stamp);

    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);

    fprintf(f, "\n");
    fclose(f);
}

/* Show a prompt and read one line. Returns -1 at end of input. */
int read_line(char *prompt, char *buf, int size)
{
    int len;
    int c;

    printf("%s", prompt);
    fflush(stdout);

    if (fgets(buf, size, stdin) == NULL) return -1;

    len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
    } else {
        /* line was too long, throw away the rest */
        while ((c = getchar()) != '\n' && c != EOF) {
        }
    }
    return 0;
}

/* Read a whole number. Asks again until the input is valid. */
int read_int(char *prompt, int *out)
{
    char buf[64];
    char *end;
    long value;

    while (read_line(prompt, buf, sizeof(buf)) == 0) {
        value = strtol(buf, &end, 10);
        if (buf[0] == '\0' || *end != '\0' || value < 0 || value > 1000000000) {
            printf("Enter a whole number, 0 or more.\n");
            continue;
        }
        *out = (int)value;
        return 0;
    }
    return -1;
}

/* Read a number from 0 to MAX_VALUE. Asks again until the input is valid. */
int read_double(char *prompt, double *out)
{
    char buf[64];
    char *end;
    double value;

    while (read_line(prompt, buf, sizeof(buf)) == 0) {
        value = strtod(buf, &end);
        if (buf[0] == '\0' || *end != '\0' || !(value >= 0 && value <= MAX_VALUE)) {
            printf("Enter a number from 0 to %.0f.\n", MAX_VALUE);
            continue;
        }
        *out = value;
        return 0;
    }
    return -1;
}

/* Same as read_double, but Enter keeps the old value. */
int read_double_keep(char *prompt, double *value)
{
    char buf[64];
    char *end;
    double number;

    while (read_line(prompt, buf, sizeof(buf)) == 0) {
        if (buf[0] == '\0') return 0;

        number = strtod(buf, &end);
        if (*end != '\0' || !(number >= 0 && number <= MAX_VALUE)) {
            printf("Enter a number from 0 to %.0f, or press Enter to keep.\n", MAX_VALUE);
            continue;
        }
        *value = number;
        return 0;
    }
    return -1;
}

/* Turn YYYY-MM-DD into a time. Returns -1 if the date is not valid. */
time_t parse_date(char *s)
{
    int year, month, day;
    char extra;
    struct tm tm;
    time_t t;

    if (sscanf(s, "%d-%d-%d%c", &year, &month, &day, &extra) != 3) return -1;
    if (year < 1971 || year > 2100) return -1;

    memset(&tm, 0, sizeof(tm));
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = 12;
    tm.tm_isdst = -1;

    t = mktime(&tm);

    /* mktime fixes bad dates like Feb 30, so check nothing changed */
    if (tm.tm_year != year - 1900 || tm.tm_mon != month - 1 || tm.tm_mday != day) return -1;
    return t;
}

/* Valid period is empty (all time) or YYYY-MM. */
int valid_period(char *s)
{
    int i;
    int month;

    if (s[0] == '\0') return 1;
    if (strlen(s) != 7 || s[4] != '-') return 0;

    for (i = 0; i < 7; i++) {
        if (i == 4) continue;
        if (s[i] < '0' || s[i] > '9') return 0;
    }

    month = (s[5] - '0') * 10 + (s[6] - '0');
    if (month < 1 || month > 12) return 0;
    return 1;
}
