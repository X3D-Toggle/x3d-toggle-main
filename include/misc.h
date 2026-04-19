/* Miscellaneous Helpers for the X3D Toggle Project
 *
 * `misc.h` -Header only
 */

#ifndef MISC_H
#define MISC_H

extern const char *const insults[];
extern const int insults_count;

const char *get_insult(void);

int cli_misc_insults(int argc, char *argv[]);
int cli_misc_fallback(int argc, char *argv[]);

#endif // MISC_H
