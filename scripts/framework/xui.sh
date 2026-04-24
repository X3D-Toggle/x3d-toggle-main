#!/bin/sh
## Universal Template Component for the X3D Toggle Project
##
## `xui.sh`
##
## Serves as universal template for UI alignment.
## When executed with --gen-xui, it injects the equivalent C logic
## into bin/xui.c and bin/xui.h for runtime.
##
## This is where you edit the UI to your own preferences and then
## run `x3d-toggle framework --gen-xui` to compile the changes.

export BUFF_COLOR=32
export BUFF_RESET=32
export BUFF_BR=8
export BUFF_NL=8
export BUFF_MODE=64
export BUFF_PATH=128
export BUFF_STATE=16
export BUFF_DMODE=64
export BUFF_EPP=64
export BUFF_GOV=64
export BUFF_SMT=64
export BUFF_PLAT=64
export BUFF_RAW=64
export BUFF_DAEMON=64
export BUFF_STATUS=64
export BUFF_EBPF=64
export BUFF_INFO=128
export BUFF_LINE=256
export BUFF_DISPLAY=32

export X3D_XUI_LOADED=1
export NOTICE="💡"
export WARN="⚠️"
export ALRIGHT="✅"
export XOUT="❌"
export QUERY="❓"
export GEAR="⚙️"
export RELOAD="🔄"
export REFRESH="🔃"
export INFO="🌐"

export COLOR_RESET="\033[0m"
export COLOR_RED="\033[31m"
export COLOR_GREEN="\033[32m"
export COLOR_YELLOW="\033[33m"
export COLOR_BLUE="\033[34m"
export COLOR_CYAN="\033[36m"
export COLOR_BOLD="\033[1m"
export COLOR_DIM="\033[2m"
export COLOR_PURPLE="\033[35m"

export ARROW_UP="⬆ UP"
export ARROW_DOWN="⬇ DOWN"
export ARROW_RIGHT="➡ RIGHT"
export ARROW_LEFT="⬅ LEFT"

export SEQ_UP=$(printf "\033[A")
export SEQ_DOWN=$(printf "\033[B")
export SEQ_RIGHT=$(printf "\033[C")
export SEQ_LEFT=$(printf "\033[D")

export UP="${COLOR_GREEN}${ARROW_UP}${COLOR_RESET}"
export DOWN="${COLOR_YELLOW}${ARROW_DOWN}${COLOR_RESET}"
export RIGHT="${COLOR_CYAN}${ARROW_RIGHT}${COLOR_RESET}"
export LEFT="${COLOR_PURPLE}${ARROW_LEFT}${COLOR_RESET}"

export HTML_RESET="</font>"
export HTML_RED="<font color=\"#FF0000\">"
export HTML_GREEN="<font color=\"#00FF00\">"
export HTML_YELLOW="<font color=\"#FFFF00\">"
export HTML_BLUE="<font color=\"#0000FF\">"
export HTML_CYAN="<font color=\"#00FFFF\">"
export HTML_BOLD="<b>"

export CACHED="🐇"
export FREQU="🐆"
export CACHELIZ="🦎"
export CACHEBEAR="🐻"
export QUERY="❓"
export TOPSWAP="🔄"
export PINNED="🎯"
export HUT="🛖 "
export SLEEPY="🐨"
export STOPSIGN="🛑"
export SCHED="🐙"
export BOOST="🦅"
export CCD0="🛖 "
export CCD1="💤"
export DRIVER="⚙ "
export EPP="🦊"
export GOV="🐺"
export SMT="👥"
export ALRIGHT="✅"
export XOUT="❌ "
export WARN="⚠️"
export NOTICE="💡"
export WIPE="🧹"
export ROCKET="🚀"
export SPARKLE="✨"
export RELOAD="🔄"
export SCREEN="🖥️ "
export TOOLS="🛠️ "
export SEARCH="🔍"
export PACKAGE="📦"
export HAMMER="🔨"
export PADLOCK="🔐"
export SHIELD="🛡️ "
export GLOBE="🌐"
export PAUSE="⏸️ "
export TRASHCAN="🗑️ "
export UNLOCK="🔓 "
export GEAR="⚙️ "
export ALERT="ℹ️ "
export WIZARD="🧙"
export WAND="🪄"
export LLAP="🖖 "
export PINNED="📌"
export CHICKEN="🐥"
export MANUAL="🔧"
export DUALIZE="♊"
export CCD0="0️⃣ "
export CCD1="1️⃣ "
export DRIVER="🚛"
export PLAT="💻"
export XUI_LOADED=1

TERM_WIDTH=80

printf_br() {
    printf "\n"
}

printf_step() {
    for _l_str in "$@"; do
        case "$_l_str" in
            [0-9]*,*) _l_level="${_l_str%%,*}"; _l_content="${_l_str#*,}" ;;
            ,*)       _l_level=1; _l_content="${_l_str#*,}" ;;
            *)        _l_level=1; _l_content="$_l_str" ;;
        esac

        _l_prefix=""
        _l_idx=0; while [ "$_l_idx" -lt "$_l_level" ]; do
            _l_prefix="${_l_prefix}    "
            _l_idx=$((_l_idx + 1))
        done

        case "$_l_content" in
            "?"*|"❓"*) _l_prefix="${_l_prefix}    " ;;
        esac

        _l_expanded=$(printf "%b" "$_l_content")
        _l_newline="
"
        while [ -n "$_l_expanded" ]; do
            case "$_l_expanded" in
                *"$_l_newline"*)
                    _l_line="${_l_expanded%%$_l_newline*}"
                    _l_expanded="${_l_expanded#*$_l_newline}"
                    ;;
                *)
                    _l_line="$_l_expanded"
                    _l_expanded=""
                    ;;
            esac
            printf "%b%s\n" "$_l_prefix" "$_l_line"
        done
    done
}

printf_step_no_nl() {
    for _l_str in "$@"; do
        case "$_l_str" in
            [0-9]*,*) _l_level="${_l_str%%,*}"; _l_content="${_l_str#*,}" ;;
            ,*)       _l_level=1; _l_content="${_l_str#*,}" ;;
            *)        _l_level=1; _l_content="$_l_str" ;;
        esac

        _l_prefix=""
        _l_idx=0; while [ "$_l_idx" -lt "$_l_level" ]; do
            _l_prefix="${_l_prefix}    "
            _l_idx=$((_l_idx + 1))
        done

        case "$_l_content" in
            "?"*|"❓"*) _l_prefix="${_l_prefix}    " ;;
        esac

        # Atomic expansion of tokens and escapes
        _l_expanded=$(printf "%b" "$_l_content")
        _l_newline="
"
        # Process each line of the expanded content using native shell matching
        while [ -n "$_l_expanded" ]; do
            case "$_l_expanded" in
                *"$_l_newline"*)
                    _l_line="${_l_expanded%%$_l_newline*}"
                    _l_expanded="${_l_expanded#*$_l_newline}"
                    printf "%b%s\n" "$_l_prefix" "$_l_line"
                    ;;
                *)
                    _l_line="$_l_expanded"
                    _l_expanded=""
                    printf "%b%s" "$_l_prefix" "$_l_line"
                    ;;
            esac
        done
    done
}

printf_string() {
    for _l_str in "$@"; do
        case "$_l_str" in
            [0-9]*,*) _l_level="${_l_str%%,*}"; _l_content="${_l_str#*,}" ;;
            ,*)       _l_level=1; _l_content="${_l_str#*,}" ;;
            *)        _l_level=1; _l_content="$_l_str" ;;
        esac

        _l_prefix=""
        _l_idx=0; while [ "$_l_idx" -lt "$_l_level" ]; do
            _l_prefix="${_l_prefix}    "
            _l_idx=$((_l_idx + 1))
        done

        printf "%b%s\n" "$_l_prefix" "$_l_content"
    done
}

printf_divider() {
    printf_br
    _l_line_width=$((TERM_WIDTH - 4))
    _l_line=""
    _l_i=0; while [ "$_l_i" -lt "$_l_line_width" ]; do
        _l_line="${_l_line}="
        _l_i=$((_l_i + 1))
    done
    printf "%s\n" "$_l_line"
    printf_br
    printf_br
}

printf_center() {
    for _l_str in "$@"; do
        _l_len=${#_l_str}
        
        case "$_l_str" in 
            *"$ALRIGHT"*|*"$XOUT"*|*"$WARN"*|*"$NOTICE"*|*"$WIPE"*|*"$ROCKET"*|*"$SPARKLE"*|*"$LLAP"*|*"$CACHED"*|*"$FREQU"*|*"$CACHELIZ"*|*"$CACHEBEAR"*|*"$QUERY"*|*"$TOPSWAP"*|*"$PINNED"*|*"$HUT"*|*"$SLEEPY"*|*"$STOPSIGN"*|*"$SCHED"*|*"$BOOST"*|*"$CCD0"*|*"$CCD1"*|*"$DRIVER"*|*"$EPP"*|*"$GOV"*|*"$SMT"*|*"$PLAT"*|*"$CHICKEN"*|*"$MANUAL"*|*"$DUALIZE"*|*"$WIZARD"*|*"$WAND"*|*"$INFO"*|*"$UP"*|*"$DOWN"*|*"$LEFT"*|*"$RIGHT"*)
                _l_len=$((_l_len + 1)) ;;
        esac
        
        _l_pad=$(( (TERM_WIDTH - 4 - _l_len) / 2 ))
        if [ "$_l_pad" -lt 0 ]; then _l_pad=0; fi
        
        _l_spaces=""
        _l_i=0; while [ "$_l_i" -lt "$_l_pad" ]; do
            _l_spaces="${_l_spaces} "
            _l_i=$((_l_i + 1))
        done
        printf_step "1,${_l_spaces}${_l_str}"
    done
}

printf_signature() {
    printf_br
    printf_center "Please consider leaving feedback or" \
                  "contributing to the project if you" \
                  "are not satisfied with the utility. ${ROCKET}"
    printf_br
    printf_center "Live Long and Prosper ${LLAP}" \
                  "=/\\= Pyrotiger =/\\="
    printf_divider
}

if { [ "$1" = "--gen-xui" ] || [ "$1" = "--update" ]; } && [ "$(basename -- "$0")" = "xui.sh" ]; then
    if [ "$X3D_EXEC" != "1" ]; then exit 1; fi
    if [ -z "$XUI_LOADED" ] && [ -z "$NOTICE" ]; then
        printf_step "2,${WARN} Warning: xui.sh sourced but notice/color variables not exported correctly."
    fi
    rm -f "$X3D_BUILD/xui.h" "$X3D_BUILD/xui.c"

    printf "%s" "/* Shared UI View Layer Header for the X3D Toggle Project
 *
 * \`xui.h\`
 *
 * AUTO-GENERATED FILE. DO NOT EDIT DIRECTLY.
 * EDIT FILE: \`xui.sh\`
 */

#ifndef XUI_H
#define XUI_H

#include \"../include/libc.h\"

enum {
    BUFF_COLOR = ${BUFF_COLOR},
    BUFF_RESET = ${BUFF_RESET},
    BUFF_BR = ${BUFF_BR},
    BUFF_NL = ${BUFF_NL},
    BUFF_MODE = ${BUFF_MODE},
    BUFF_PATH = ${BUFF_PATH},
    BUFF_STATE = ${BUFF_STATE},
    BUFF_STATUS = ${BUFF_STATUS},
    BUFF_DMODE = ${BUFF_DMODE},
    BUFF_EPP = ${BUFF_EPP},
    BUFF_GOV = ${BUFF_GOV},
    BUFF_SMT = ${BUFF_SMT},
    BUFF_PLAT = ${BUFF_PLAT},
    BUFF_RAW = ${BUFF_RAW},
    BUFF_DAEMON = ${BUFF_DAEMON},
    BUFF_EBPF = ${BUFF_EBPF},
    BUFF_INFO = ${BUFF_INFO},
    BUFF_LINE = ${BUFF_LINE},
    BUFF_DISPLAY = ${BUFF_DISPLAY}
};

extern volatile int active_override;
extern volatile sig_atomic_t active_keep;

#define CACHED            \"${CACHED}\"
#define FREQU             \"${FREQU}\"
#define CACHELIZ          \"${CACHELIZ}\"
#define CACHEBEAR         \"${CACHEBEAR}\"
#define QUERY             \"${QUERY}\"
#define TOPSWAP           \"${TOPSWAP}\"
#define DUALIZE           \"${DUALIZE}\"
#define PINNED            \"${PINNED}\"
#define HUT               \"${HUT}\"
#define CHICKEN           \"${CHICKEN}\"
#define MANUAL            \"${MANUAL}\"
#define SLEEPY            \"${SLEEPY}\"
#define STOPSIGN          \"${STOPSIGN}\"
#define SCHED             \"${SCHED}\"
#define BOOST             \"${BOOST}\"
#define CCD0              \"${CCD0}\"
#define CCD1              \"${CCD1}\"
#define DRIVER            \"${DRIVER}\"
#define EPP               \"${EPP}\"
#define GOV               \"${GOV}\"
#define SMT               \"${SMT}\"
#define PLAT              \"${PLAT}\"
#define ALRIGHT           \"${ALRIGHT}\"
#define XOUT              \"${XOUT}\"
#define WARN              \"${WARN}\"
#define NOTICE            \"${NOTICE}\"
#define WIPE              \"${WIPE}\"
#define ROCKET            \"${ROCKET}\"
#define SPARKLE           \"${SPARKLE}\"
#define RELOAD            \"${RELOAD}\"
#define SCREEN            \"${SCREEN}\"
#define TOOLS             \"${TOOLS}\"
#define SEARCH            \"${SEARCH}\"
#define PACKAGE           \"${PACKAGE}\"
#define HAMMER            \"${HAMMER}\"
#define PADLOCK           \"${PADLOCK}\"
#define SHIELD            \"${SHIELD}\"
#define GLOBE             \"${GLOBE}\"
#define PAUSE             \"${PAUSE}\"
#define TRASHCAN          \"${TRASHCAN}\"
#define UNLOCK            \"${UNLOCK}\"
#define GEAR              \"${GEAR}\"
#define ALERT             \"${ALERT}\"
#define WIZARD            \"${WIZARD}\" 
#define WAND              \"${WAND}\" 
#define LLAP              \"${LLAP}\" 

#define COLOR_RESET  \"\\x1b[0m\"
#define COLOR_RED    \"\\x1b[31m\"
#define COLOR_GREEN  \"\\x1b[32m\"
#define COLOR_YELLOW \"\\x1b[33m\"
#define COLOR_BLUE   \"\\x1b[34m\"
#define COLOR_CYAN   \"\\x1b[36m\"
#define COLOR_BOLD   \"\\x1b[1m\"
#define COLOR_DIM    \"\\x1b[2m\"

#define HTML_RESET   \"</font>\"
#define HTML_RED     \"<font color=\\\"#FF0000\\\">\"
#define HTML_GREEN   \"<font color=\\\"#00FF00\\\">\"
#define HTML_YELLOW  \"<font color=\\\"#FFFF00\\\">\"
#define HTML_BLUE    \"<font color=\\\"#00FF00\\\">\"
#define HTML_CYAN    \"<font color=\\\"#00FFFF\\\">\"
#define HTML_BOLD    \"<b>\"

#define TERM_WIDTH 80

void printf_br(void);
void printf_string(const char *fmt, ...);
void printf_center(const char *str);
void printf_divider(void);
void printf_step(const char *fmt, ...);
void printf_step_no_nl(const char *fmt, ...);
void printf_upcoming(const char *feature);
void printf_signature(void);
int printf_vsn(char *buf, size_t size, const char *fmt, __builtin_va_list args);
int printf_sn(char *buf, size_t size, const char *fmt, ...);

#endif // XUI_H" > "$X3D_BUILD/xui.h"
    printf_br >> "$X3D_BUILD/xui.h"


    printf "%s" "/* Shared UI View Layer for the X3D Toggle Project
 *
 * \`xui.c\`
 *
 * AUTO-GENERATED FILE. DO NOT EDIT DIRECTLY.
 * EDIT FILE: \`xui.sh\`
 */

#include \"xui.h\"

typedef struct {
    const char *name;
    const char *val;
} XUI_Symbol;

static const XUI_Symbol xui_symbols[] = {
    {\"CACHED\", CACHED}, {\"FREQU\", FREQU}, {\"CACHELIZ\", CACHELIZ}, {\"CACHEBEAR\", CACHEBEAR},
    {\"QUERY\", QUERY}, {\"TOPSWAP\", TOPSWAP}, {\"DUALIZE\", DUALIZE}, {\"PINNED\", PINNED},
    {\"HUT\", HUT}, {\"CHICKEN\", CHICKEN}, {\"MANUAL\", MANUAL}, {\"SLEEPY\", SLEEPY},
    {\"STOPSIGN\", STOPSIGN}, {\"SCHED\", SCHED}, {\"BOOST\", BOOST}, {\"CCD0\", CCD0},
    {\"CCD1\", CCD1}, {\"DRIVER\", DRIVER}, {\"EPP\", EPP}, {\"GOV\", GOV}, {\"SMT\", SMT},
    {\"PLAT\", PLAT}, {\"ALRIGHT\", ALRIGHT}, {\"XOUT\", XOUT}, {\"WARN\", WARN},
    {\"NOTICE\", NOTICE}, {\"WIPE\", WIPE}, {\"ROCKET\", ROCKET}, {\"SPARKLE\", SPARKLE},
    {\"RELOAD\", RELOAD}, {\"SCREEN\", SCREEN}, {\"TOOLS\", TOOLS}, {\"SEARCH\", SEARCH},
    {\"PACKAGE\", PACKAGE}, {\"HAMMER\", HAMMER}, {\"PADLOCK\", PADLOCK}, {\"SHIELD\", SHIELD},
    {\"GLOBE\", GLOBE}, {\"PAUSE\", PAUSE}, {\"TRASHCAN\", TRASHCAN}, {\"UNLOCK\", UNLOCK},
    {\"GEAR\", GEAR}, {\"ALERT\", ALERT}, {\"WIZARD\", WIZARD}, {\"WAND\", WAND}, {\"LLAP\", LLAP},
    {\"COLOR_RESET\", COLOR_RESET}, {\"COLOR_RED\", COLOR_RED}, {\"COLOR_GREEN\", COLOR_GREEN},
    {\"COLOR_YELLOW\", COLOR_YELLOW}, {\"COLOR_BLUE\", COLOR_BLUE}, {\"COLOR_CYAN\", COLOR_CYAN},
    {\"COLOR_BOLD\", COLOR_BOLD}, {\"COLOR_DIM\", COLOR_DIM},
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
            if (!arg_s) arg_s = \"(null)\";
            
            int slen = 0;
            int vlen = 0;
            int in_escape = 0;
            
            while (arg_s[slen]) {
                if (arg_s[slen] == '\\x1b') in_escape = 1;
                else if (in_escape) { if (arg_s[slen] == 'm') in_escape = 0; }
                else {
                    unsigned char c = (unsigned char)arg_s[slen];
                    if ((c & 0xC0) != 0x80) vlen++; // Ignore UTF-8 continuation bytes
                }
                slen++;
            }
            
            if (!left) { while (width > vlen && i < size - 1) { buf[i++] = ' '; width--; } }
            for (int k = 0; k < slen && i < size - 1; k++) buf[i++] = arg_s[k];
            if (left) { while (width > vlen && i < size - 1) { buf[i++] = ' '; width--; } }
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
    write(1, \"\\n\", 1);
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
    while (content[len] != '\\0') len++;

    write(1, prefix, spaces);
    write(1, content, len);
    write(1, \"\\n\", 1);
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
    if (content[0] == '?' || !strncmp(content, \"❓\", 3)) {
        spaces += 4;
    }
    char prefix[64] = {0};
    for (int i = 0; i < spaces && i < 63; i++) prefix[i] = ' ';

    char *line = content;
    char *next;
    while ((next = strchr(line, '\\n'))) {
        *next = '\\0';
        write(1, prefix, spaces);
        write(1, line, strlen(line));
        write(1, \"\\n\", 1);
        line = next + 1;
    }
    if (*line) {
        write(1, prefix, spaces);
        write(1, line, strlen(line));
        write(1, \"\\n\", 1);
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
    if (content[0] == '?' || !strncmp(content, \"❓\", 3)) {
        spaces += 4;
    }
    char prefix[64] = {0};
    for (int i = 0; i < spaces && i < 63; i++) prefix[i] = ' ';
    write(1, prefix, spaces);
    write(1, content, strlen(content));
}

void printf_upcoming(const char *feature) {
    printf_br();
    printf_step(\"${ALERT}  Feature '%s' will be available in an upcoming feature update.\", feature);
}

void printf_divider(void) {
    printf_br();
    int line_width = TERM_WIDTH - 4;
    for (int i = 0; i < line_width; i++) {
        write(1, \"=\", 1);
    }
    printf_br();
    printf_br();
}

void printf_center(const char *str) {
    int len = 0;
    int in_escape = 0;
    for (int i = 0; str[i] != '\\0' && str[i] != '\\n'; i++) {
        if (str[i] == '\\x1b') in_escape = 1;
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
    printf_step(\"1,%*s%s\", pad, \"\", str);
}

void printf_signature(void) {
    printf_center (\"Please consider leaving feedback or\\n\"
                   \"contributing to the project if you\\n\"
                   \"are not satisfied with the utility. ${ROCKET}\");
    printf_br();
    printf_center (\"Live Long and Prosper ${LLAP}\\n\"
                   \"=/\\\\= Pyrotiger =/\\\\=\");
    printf_divider();
}

/* end of XUI.C */" > "$X3D_BUILD/xui.c"
    printf_br >> "$X3D_BUILD/xui.c"
    exit 0
fi

# end of X3D_XUI.SH