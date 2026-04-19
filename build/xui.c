/* Shared UI View Layer for the X3D Toggle Project
 *
 * `xui.c`
 *
 * AUTO-GENERATED FILE. DO NOT EDIT DIRECTLY.
 * EDIT FILE: `xui.sh`
 */

#include "xui.h"

typedef struct {
    const char *name;
    const char *val;
} XUI_Symbol;

static const XUI_Symbol xui_symbols[] = {
    {"CACHED", CACHED}, {"FREQU", FREQU}, {"CACHELIZ", CACHELIZ}, {"CACHEBEAR", CACHEBEAR},
    {"QUERY", QUERY}, {"TOPSWAP", TOPSWAP}, {"DUALIZE", DUALIZE}, {"PINNED", PINNED},
    {"HUT", HUT}, {"CHICKEN", CHICKEN}, {"MANUAL", MANUAL}, {"SLEEPY", SLEEPY},
    {"STOPSIGN", STOPSIGN}, {"SCHED", SCHED}, {"BOOST", BOOST}, {"CCD0", CCD0},
    {"CCD1", CCD1}, {"DRIVER", DRIVER}, {"EPP", EPP}, {"GOV", GOV}, {"SMT", SMT},
    {"PLAT", PLAT}, {"ALRIGHT", ALRIGHT}, {"XOUT", XOUT}, {"WARN", WARN},
    {"NOTICE", NOTICE}, {"WIPE", WIPE}, {"ROCKET", ROCKET}, {"SPARKLE", SPARKLE},
    {"RELOAD", RELOAD}, {"SCREEN", SCREEN}, {"TOOLS", TOOLS}, {"SEARCH", SEARCH},
    {"PACKAGE", PACKAGE}, {"HAMMER", HAMMER}, {"PADLOCK", PADLOCK}, {"SHIELD", SHIELD},
    {"GLOBE", GLOBE}, {"PAUSE", PAUSE}, {"TRASHCAN", TRASHCAN}, {"UNLOCK", UNLOCK},
    {"GEAR", GEAR}, {"ALERT", ALERT}, {"WIZARD", WIZARD}, {"WAND", WAND}, {"LLAP", LLAP},
    {"COLOR_RESET", COLOR_RESET}, {"COLOR_RED", COLOR_RED}, {"COLOR_GREEN", COLOR_GREEN},
    {"COLOR_YELLOW", COLOR_YELLOW}, {"COLOR_BLUE", COLOR_BLUE}, {"COLOR_CYAN", COLOR_CYAN},
    {"COLOR_BOLD", COLOR_BOLD}, {"COLOR_DIM", COLOR_DIM},
    {NULL, NULL}
};

static void _xui_rev(char *s, int len) {
    for (int i = 0, j = len - 1; i < j; i++, j--) {
        char t = s[i]; s[i] = s[j]; s[j] = t;
    }
}

static int _xui_itoa(long long n, char *s, int base) {
    int i = 0;
    int neg = 0;
    if (n == 0) { s[i++] = '0'; s[i] = '\0'; return 1; }
    if (n < 0 && base == 10) { neg = 1; n = -n; }
    while (n != 0) {
        int r = n % base;
        s[i++] = (r > 9) ? (r - 10) + 'a' : r + '0';
        n /= base;
    }
    if (neg) s[i++] = '-';
    s[i] = '\0';
    _xui_rev(s, i);
    return i;
}

static int _xui_utoa(unsigned long long n, char *s, int base) {
    int i = 0;
    if (n == 0) { s[i++] = '0'; s[i] = '\0'; return 1; }
    while (n != 0) {
        unsigned long long r = n % base;
        s[i++] = (r > 9) ? (r - 10) + 'a' : r + '0';
        n /= base;
    }
    s[i] = '\0';
    _xui_rev(s, i);
    return i;
}

int printf_vsn(char *buf, size_t size, const char *fmt, __builtin_va_list args) {
    size_t i = 0;
    const char *p = fmt;
    while (*p && i < size - 1) {
        if (*p == '$' && *(p+1) == '{') {
            p += 2;
            const char *start = p;
            while (*p && *p != '}') p++;
            if (*p == '}') {
                char sym_name[64];
                size_t n = p - start;
                if (n < 64) {
                    for (size_t k = 0; k < n; k++) sym_name[k] = start[k];
                    sym_name[n] = '\0';
                    const char *val = NULL;
                    for (int k = 0; xui_symbols[k].name; k++) {
                        int match = 1;
                        for (size_t j = 0; j <= n; j++) {
                            if (sym_name[j] != xui_symbols[k].name[j]) { match = 0; break; }
                        }
                        if (match) { val = xui_symbols[k].val; break; }
                    }
                    if (val) {
                        while (*val && i < size - 1) buf[i++] = *val++;
                        p++;
                        continue;
                    }
                }
            }
            p = start - 2; // Rollback
        }

        if (*p != '%') { buf[i++] = *p++; continue; }
        p++;
        int left = 0;
        if (*p == '-') { left = 1; p++; }
        int width = 0;
        if (*p == '*') { width = __builtin_va_arg(args, int); p++; }
        while (*p >= '0' && *p <= '9') { width = width * 10 + (*p - '0'); p++; }
        
        if (*p == 's') {
            const char *arg_s = __builtin_va_arg(args, const char *);
            if (!arg_s) arg_s = "(null)";
            int slen = 0;
            while (arg_s[slen]) slen++;
            if (!left) { while (width > slen && i < size - 1) { buf[i++] = ' '; width--; } }
            for (int k = 0; k < slen && i < size - 1; k++) buf[i++] = arg_s[k];
            if (left) { while (width > slen && i < size - 1) { buf[i++] = ' '; width--; } }
        } else if (*p == 'd') {
            char t[32];
            int n = __builtin_va_arg(args, int);
            int tlen = _xui_itoa(n, t, 10);
            if (!left) { while (width > tlen && i < size - 1) { buf[i++] = ' '; width--; } }
            for (int k = 0; t[k] && i < size - 1; k++) buf[i++] = t[k];
            if (left) { while (width > tlen && i < size - 1) { buf[i++] = ' '; width--; } }
        } else if (*p == 'u' || (*p == 'z' && *(p+1) == 'u')) {
            if (*p == 'z') p++;
            char t[32];
            unsigned long long n;
            if (*(p-1) == 'z') n = (unsigned long long)__builtin_va_arg(args, size_t);
            else n = __builtin_va_arg(args, unsigned int);
            int tlen = _xui_utoa(n, t, 10);
            if (!left) { while (width > tlen && i < size - 1) { buf[i++] = ' '; width--; } }
            for (int k = 0; t[k] && i < size - 1; k++) buf[i++] = t[k];
            if (left) { while (width > tlen && i < size - 1) { buf[i++] = ' '; width--; } }
        } else if (*p == 'x') {
            char t[32];
            unsigned int n = __builtin_va_arg(args, unsigned int);
            int tlen = _xui_utoa(n, t, 16);
            if (!left) { while (width > tlen && i < size - 1) { buf[i++] = ' '; width--; } }
            for (int k = 0; t[k] && i < size - 1; k++) buf[i++] = t[k];
            if (left) { while (width > tlen && i < size - 1) { buf[i++] = ' '; width--; } }
        } else if (*p == '.') {
            // Very simple fixed point for %.1f
            p++; if (*p == '1' && *(p+1) == 'f') {
                p += 1;
                double f = __builtin_va_arg(args, double);
                char t[32];
                long long whole = (long long)f;
                int frac = (int)((f - whole) * 10);
                if (frac < 0) frac = -frac;
                _xui_itoa(whole, t, 10);
                for (int k = 0; t[k] && i < size - 1; k++) buf[i++] = t[k];
                if (i < size - 1) buf[i++] = '.';
                if (i < size - 1) buf[i++] = (char)(frac + '0');
            }
        } else { buf[i++] = *p; }
        p++;
    }
    buf[i] = '\0';
    return (int)i;
}

int printf_sn(char *buf, size_t size, const char *fmt, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    int res = printf_vsn(buf, size, fmt, args);
    __builtin_va_end(args);
    return res;
}

void printf_br(void) {
    write(1, "\n", 1);
}

void printf_string(const char *fmt, ...) {
    char buf[8192];
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    printf_vsn(buf, sizeof(buf), fmt, args);
    __builtin_va_end(args);

    int level = 1;
    char *content = buf;
    if (buf[0] >= '0' && buf[0] <= '9') {
        char *comma = strchr(buf, ',');
        if (comma && (comma - buf) < 5) {
            level = atoi(buf);
            content = comma + 1;
        }
    } else if (buf[0] == ',') {
        content = buf + 1;
    }

    int spaces = level * 4;
    char prefix[64] = {0};
    for (int i = 0; i < spaces && i < 63; i++) prefix[i] = ' ';

    int len = 0;
    while (content[len] != '\0') len++;

    write(1, prefix, spaces);
    write(1, content, len);
    write(1, "\n", 1);
    write(1, COLOR_RESET, 4);
}

void printf_step(const char *fmt, ...) {
    char buf[8192];
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    printf_vsn(buf, sizeof(buf), fmt, args);
    __builtin_va_end(args);

    int level = 1;
    char *content = buf;
    if (buf[0] >= '0' && buf[0] <= '9') {
        char *comma = strchr(buf, ',');
        if (comma && (comma - buf) < 5) {
            level = atoi(buf);
            content = comma + 1;
        }
    } else if (buf[0] == ',') {
        content = buf + 1;
    }

    int spaces = level * 4;
    if (content[0] == '?' || !strncmp(content, "❓", 3)) {
        spaces += 4;
    }
    char prefix[64] = {0};
    for (int i = 0; i < spaces && i < 63; i++) prefix[i] = ' ';

    char *line = content;
    char *next;
    while ((next = strchr(line, '\n'))) {
        *next = '\0';
        write(1, prefix, spaces);
        write(1, line, strlen(line));
        write(1, "\n", 1);
        line = next + 1;
    }
    if (*line) {
        write(1, prefix, spaces);
        write(1, line, strlen(line));
        write(1, "\n", 1);
    }
    write(1, COLOR_RESET, 4);
}

void printf_step_no_nl(const char *fmt, ...) {
    char buf[8192];
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    printf_vsn(buf, sizeof(buf), fmt, args);
    __builtin_va_end(args);

    int level = 1;
    char *content = buf;
    if (buf[0] >= '0' && buf[0] <= '9') {
        char *comma = strchr(buf, ',');
        if (comma && (comma - buf) < 5) {
            level = atoi(buf);
            content = comma + 1;
        }
    } else if (buf[0] == ',') {
        content = buf + 1;
    }

    int spaces = level * 4;
    if (content[0] == '?' || !strncmp(content, "❓", 3)) {
        spaces += 4;
    }
    char prefix[64] = {0};
    for (int i = 0; i < spaces && i < 63; i++) prefix[i] = ' ';
    write(1, prefix, spaces);
    write(1, content, strlen(content));
}

void printf_upcoming(const char *feature) {
    printf_br();
    printf_step("ℹ️   Feature '%s' will be available in an upcoming feature update.", feature);
}

void printf_divider(void) {
    printf_br();
    int line_width = TERM_WIDTH - 4;
    for (int i = 0; i < line_width; i++) {
        write(1, "=", 1);
    }
    printf_br();
    printf_br();
}

void printf_center(const char *str) {
    int len = 0;
    int in_escape = 0;
    for (int i = 0; str[i] != '\0' && str[i] != '\n'; i++) {
        if (str[i] == '\x1b') in_escape = 1;
        else if (in_escape) { if (str[i] == 'm') in_escape = 0; }
        else {
            unsigned char c = (unsigned char)str[i];
            if ((c & 0xC0) != 0x80) { 
                if (c >= 0xF0) { len += 2; i += 3; }
                else if (c >= 0xE0) { len += 2; i += 2; }
                else if (c >= 0xC2) { len += 1; i += 1; }
                else { len++; }
            }
        }
    }

    int active_width = TERM_WIDTH - 4;
    int pad = (len < active_width) ? (active_width - len) / 2 : 0;
    printf_step("1,%*s%s", pad, "", str);
}

void printf_signature(void) {
    printf_center ("Please consider leaving feedback or\n"
                   "contributing to the project if you\n"
                   "are not satisfied with the utility. 🚀");
    printf_br();
    printf_center ("Live Long and Prosper 🖖 \n"
                   "=/\\= Pyrotiger =/\\=");
    printf_divider();
}

/* end of XUI.C */
