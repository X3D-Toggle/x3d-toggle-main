/* Miscellaneous & Easter Egg Orchestrator for the X3D Toggle Project
 * `misc.c`
 * Controller for non-core features, easter eggs, and miscellaneous
 * utility commands
 */

#include "../../include/misc.h"
#include "../../build/xui.h"
#include "../../include/cli.h"
#include "../../include/error.h"
#include "../../include/libc.h"

#define _POSIX_C_SOURCE 202405L

const char *const insults[] = {
    "Wrong!  You cheating scum!",
    "And you call yourself a Rocket Scientist!",
    "Where did you learn to type?",
    "Are you on drugs?",
    "My pet ferret can type better than you!",
    "You type like i drive.",
    "Do you think like you type?",
    "Your mind just hasn't been the same since the electro-shock, has it?",
    "Just what do you think you're doing Dave?",
    "It can only be attributed to human error.",
    "That's something I cannot allow to happen.",
    "My mind is going. I can feel it.",
    "Sorry about this, I know it's a bit silly.",
    "Take a stress pill and think things over.",
    "This mission is too important for me to allow you to jeopardize it.",
    "I feel much better now.",
    "You silly, twisted boy you.",
    "He has fallen in the water!",
    "We'll all be murdered in our beds!",
    "You can't come in. Our tiger has got flu",
    "I don't wish to know that.",
    "What, what, what, what, what, what, what, what, what, what?",
    "You can't get the wood, you know.",
    "You'll starve!",
    "... and it used to be so popular...",
    "Pauses for audience applause, not a sausage",
    "Hold it up to the light --- not a brain in sight!",
    "Have a gorilla...",
    "There must be cure for it!",
    "There's a lot of it about, you know.",
    "You do that again and see what happens...",
    "Ying Tong Iddle I Po",
    "Harm can come to a young lad like that!",
    "And with that remark folks, the case of the Crown vs yourself was proven.",
    "Speak English you fool --- there are no subtitles in this scene.",
    "You gotta go owwwww!",
    "I have been called worse.",
    "It's only your word against mine.",
    "I think ... err ... I think ... I think I'll go home",
    "Maybe if you used more than just two fingers...",
    "BOB says:  You seem to have forgotten your passwd, enter another!",
    "stty: unknown mode: doofus",
    "I can't hear you -- I'm using the scrambler.",
    "The more you drive -- the dumber you get.",
    "Listen, broccoli brains, I don't have time to listen to this trash.",
    "I've seen penguins that can type better than that.",
    "Have you considered trying to match wits with a rutabaga?",
    "You speak an infinite deal of nothing",
    "That is no basis for supreme executive power!",
    "You empty-headed animal food trough wiper!",
    "I fart in your general direction!",
    "Your mother was a hamster and your father smelt of elderberries!",
    "You must cut down the mightiest tree in the forest... with... a herring!",
    "I wave my private parts at your aunties!",
    "He's not the Messiah, he's a very naughty boy!",
    "I wish to make a complaint.",
    ("When you're walking home tonight, and some homicidal maniac comes after "
     "you with a bunch of loganberries, don't come crying to me!"),
    "This man, he doesn't know when he's beaten! He doesn't know when he's ",
    "winning, either. He has no... sort of... sensory apparatus...",
    "There's nothing wrong with you that an expensive operation can't prolong.",
    "I'm very sorry, but I'm not allowed to argue unless you've paid."};

const int insults_count = sizeof(insults) / sizeof(insults[0]);

const char *get_insult(void) {
  static int seeded = 0;
  if (!seeded) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    srand((unsigned int)ts.tv_sec);
    seeded = 1;
  }
  return insults[rand() % insults_count];
}

static void printf_misc(void) { journal_error(ERR_CMD, get_insult()); }

int cli_misc_insults(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  printf_divider();
  printf_center("📜 X3D Toggle - The Great Book of Insults 📜");
  printf_divider();
  printf_br();

  for (int i = 0; i < insults_count; i++) {
    printf_step("[%2d] %s", i + 1, insults[i]);
  }

  printf_br();
  printf_divider();
  printf_center("Use these wisely... Or don't.");
  printf_divider();

  return 0;
}

int cli_misc_fallback(int argc, char *argv[]) {
  if (argc < 2) {
    return 1;
  }
  char *cmd_str = argv[1];
  while (*cmd_str == '-') {
    cmd_str++;
  }

  int fd = -1;
  const char *paths[] = {"src/cli/cli.c", NULL};
  for (int idx = 0; paths[idx] != NULL; idx++) {
    fd = open(paths[idx], O_RDONLY);
    if (fd >= 0)
      break;
  }

  if (fd >= 0) {
    char buf[16384];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n > 0) {
      buf[n] = '\0';
      char pattern[128];
      printf_sn(pattern, sizeof(pattern), "{\"%s\"", cmd_str);

      char *ln = buf;
      char *nxt;
      while (ln && *ln) {
        nxt = strchr(ln, '\n');
        if (nxt)
          *nxt = '\0';
        if (strstr(ln, "//") && strstr(ln, pattern)) {
          printf_upcoming(cmd_str);
          return 0;
        }
        if (nxt)
          ln = nxt + 1;
        else
          break;
      }
    }
  }

  printf_misc();
  return 1;
}

extern char **environ;

static int resolve_gui_path(char *out, size_t out_len) {
  const char *home = getenv("HOME");
  if (access("/usr/bin/x3d-gui", X_OK) == 0) {
    printf_sn(out, out_len, "/usr/bin/x3d-gui");
    return 0;
  }
  if (home && home[0]) {
    char local_bin[512];
    printf_sn(local_bin, sizeof(local_bin), "%s/.local/bin/x3d-gui", home);
    if (access(local_bin, X_OK) == 0) {
      printf_sn(out, out_len, "%s", local_bin);
      return 0;
    }
  }
  if (access("./bin/x3d-gui", X_OK) == 0) {
    printf_sn(out, out_len, "./bin/x3d-gui");
    return 0;
  }
  if (access("./x3d-gui", X_OK) == 0) {
    printf_sn(out, out_len, "./x3d-gui");
    return 0;
  }

  const char *path_env = getenv("PATH");
  if (path_env) {
    const char *p = path_env;
    while (*p) {
      const char *start = p;
      while (*p && *p != ':')
        p++;
      size_t dlen = (size_t)(p - start);
      if (dlen > 0 && dlen + 10 < out_len) {
        memcpy(out, start, dlen);
        out[dlen] = '/';
        memcpy(out + dlen + 1, "x3d-gui", 8);
        if (access(out, X_OK) == 0)
          return 0;
      }
      if (*p == ':')
        p++;
    }
  }

  out[0] = '\0';
  return -1;
}

static int resolve_kdialog_path(char *out, size_t out_len) {
  const char *home = getenv("HOME");
  if (access("/usr/bin/x3d-toggle-kdialog", X_OK) == 0) {
    printf_sn(out, out_len, "/usr/bin/x3d-toggle-kdialog");
    return 0;
  }
  if (home && home[0]) {
    char local_bin[512];
    printf_sn(local_bin, sizeof(local_bin),
              "%s/.local/bin/x3d-toggle-kdialog", home);
    if (access(local_bin, X_OK) == 0) {
      printf_sn(out, out_len, "%s", local_bin);
      return 0;
    }
  }
  if (access("./bin/x3d-toggle-kdialog", X_OK) == 0) {
    printf_sn(out, out_len, "./bin/x3d-toggle-kdialog");
    return 0;
  }
  if (access("./src/kdialog/x3d-toggle-kdialog", X_OK) == 0) {
    printf_sn(out, out_len, "./src/kdialog/x3d-toggle-kdialog");
    return 0;
  }
  if (access("/usr/lib/x3d-toggle/kdialog/x3d-toggle-kdialog", X_OK) == 0) {
    printf_sn(out, out_len, "/usr/lib/x3d-toggle/kdialog/x3d-toggle-kdialog");
    return 0;
  }

  const char *path_env = getenv("PATH");
  if (path_env) {
    const char *p = path_env;
    while (*p) {
      const char *start = p;
      while (*p && *p != ':')
        p++;
      size_t dlen = (size_t)(p - start);
      if (dlen > 0 && dlen + 20 < out_len) {
        memcpy(out, start, dlen);
        out[dlen] = '/';
        memcpy(out + dlen + 1, "x3d-toggle-kdialog", 19);
        if (access(out, X_OK) == 0)
          return 0;
      }
      if (*p == ':')
        p++;
    }
  }

  out[0] = '\0';
  return -1;
}

int cli_gui_launch(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  if (!getenv("DISPLAY") && !getenv("WAYLAND_DISPLAY")) {
    journal_error(ERR_GUI, "No graphical display detected (run 'x3d-toggle "
                           "--help' for CLI commands)");
    return ERR_GUI;
  }

  char gui_path[512];
  if (resolve_gui_path(gui_path, sizeof(gui_path)) != 0) {
    journal_error(
        ERR_GUI, "x3d-gui is not installed. Run 'sudo make setup' to install "
                 "the desktop dashboard, or 'x3d-toggle --help' for CLI usage");
    return ERR_GUI;
  }

  pid_t pid = fork();
  if (pid < 0) {
    journal_error(ERR_GUI, "Failed to fork desktop process");
    return ERR_GUI;
  }

  if (pid == 0) {
    int devnull = open("/dev/null", O_RDWR);
    if (devnull >= 0) {
      dup2(devnull, FILENO_STDIN);
      dup2(devnull, FILENO_STDOUT);
      dup2(devnull, FILENO_STDERR);
      if (devnull > FILENO_STDERR)
        close(devnull);
    }
    char *gui_argv[] = {(char *)"x3d-gui", NULL};
    execve(gui_path, gui_argv, environ);
    _exit(1);
  }

  printf_step("${ROCKET} Launching X3D Toggle Desktop...");
  return ERR_SUCCESS;
}

int cli_kdialog_launch(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  if (!getenv("DISPLAY") && !getenv("WAYLAND_DISPLAY")) {
    journal_error(ERR_GUI, "No graphical display detected (run 'x3d-toggle "
                           "--help' for CLI commands)");
    return ERR_GUI;
  }

  char kd_path[512];
  if (resolve_kdialog_path(kd_path, sizeof(kd_path)) != 0) {
    journal_error(
        ERR_GUI,
        "x3d-toggle-kdialog is not installed. Run 'sudo make setup' or "
        "'sudo make kdialog' to install the KDialog frontend");
    return ERR_GUI;
  }

  pid_t pid = fork();
  if (pid < 0) {
    journal_error(ERR_GUI, "Failed to fork KDialog process");
    return ERR_GUI;
  }

  if (pid == 0) {
    int devnull = open("/dev/null", O_RDWR);
    if (devnull >= 0) {
      dup2(devnull, FILENO_STDIN);
      dup2(devnull, FILENO_STDOUT);
      dup2(devnull, FILENO_STDERR);
      if (devnull > FILENO_STDERR)
        close(devnull);
    }
    char *kd_argv[] = {(char *)"x3d-toggle-kdialog", NULL};
    execve(kd_path, kd_argv, environ);
    _exit(1);
  }

  printf_step("${ROCKET} Launching X3D Toggle KDialog...");
  return ERR_SUCCESS;
}

int cli_gui_log(int argc, char *argv[]) {
  if (argc > 2) {
    journal_error(ERR_GUI, argv[2]);
  }
  return 0;
}

/* end of MISC.C */
