/* Shared UI View Layer Header for the X3D Toggle Project
 *
 * `xui.h`
 *
 * AUTO-GENERATED FILE. DO NOT EDIT DIRECTLY.
 * EDIT FILE: `xui.sh`
 */

#ifndef XUI_H
#define XUI_H

#include "../include/libc.h"

enum {
    BUFF_COLOR = 32,
    BUFF_RESET = 32,
    BUFF_BR = 8,
    BUFF_NL = 8,
    BUFF_MODE = 64,
    BUFF_PATH = 128,
    BUFF_STATE = 16,
    BUFF_STATUS = 64,
    BUFF_DMODE = 64,
    BUFF_EPP = 64,
    BUFF_GOV = 64,
    BUFF_SMT = 64,
    BUFF_PLAT = 64,
    BUFF_RAW = 64,
    BUFF_DAEMON = 64,
    BUFF_EBPF = 64,
    BUFF_INFO = 128,
    BUFF_LINE = 256,
    BUFF_DISPLAY = 32
};

extern volatile int active_override;
extern volatile sig_atomic_t active_keep;

#define CACHED            "🐇"
#define FREQU             "🐆"
#define CACHELIZ          "🦎"
#define CACHEBEAR         "🐻"
#define QUERY             "❓"
#define TOPSWAP           "🔄"
#define DUALIZE           "♊"
#define PINNED            "📌"
#define HUT               "🛖 "
#define CHICKEN           "🐥"
#define MANUAL            "🔧"
#define SLEEPY            "🐨"
#define STOPSIGN          "🛑"
#define SCHED             "🐙"
#define BOOST             "🦅"
#define CCD0              "0️⃣ "
#define CCD1              "1️⃣ "
#define DRIVER            "🚛"
#define EPP               "🦊"
#define GOV               "🐺"
#define SMT               "👥"
#define PLAT              "💻"
#define ALRIGHT           "✅"
#define XOUT              "❌ "
#define WARN              "⚠️"
#define NOTICE            "💡"
#define WIPE              "🧹"
#define ROCKET            "🚀"
#define SPARKLE           "✨"
#define RELOAD            "🔄"
#define SCREEN            "🖥️ "
#define TOOLS             "🛠️ "
#define SEARCH            "🔍"
#define PACKAGE           "📦"
#define HAMMER            "🔨"
#define PADLOCK           "🔐"
#define SHIELD            "🛡️ "
#define GLOBE             "🌐"
#define PAUSE             "⏸️ "
#define TRASHCAN          "🗑️ "
#define UNLOCK            "🔓 "
#define GEAR              "⚙️ "
#define ALERT             "ℹ️ "
#define WIZARD            "🧙" 
#define WAND              "🪄" 
#define LLAP              "🖖 " 

#define COLOR_RESET  "\x1b[0m"
#define COLOR_RED    "\x1b[31m"
#define COLOR_GREEN  "\x1b[32m"
#define COLOR_YELLOW "\x1b[33m"
#define COLOR_BLUE   "\x1b[34m"
#define COLOR_CYAN   "\x1b[36m"
#define COLOR_BOLD   "\x1b[1m"
#define COLOR_DIM    "\x1b[2m"

#define HTML_RESET   "</font>"
#define HTML_RED     "<font color=\"#FF0000\">"
#define HTML_GREEN   "<font color=\"#00FF00\">"
#define HTML_YELLOW  "<font color=\"#FFFF00\">"
#define HTML_BLUE    "<font color=\"#00FF00\">"
#define HTML_CYAN    "<font color=\"#00FFFF\">"
#define HTML_BOLD    "<b>"

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

#endif // XUI_H
