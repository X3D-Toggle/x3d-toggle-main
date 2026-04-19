/* Wrapper for enabling dev mode enviroment for x3D Toggle
 *
 * `dev-install.sh`
 *
 * The wrapper is designed to to pass through developer mode enviroment options
 * to the standard user via CLI and populates the active frontend with
 * Developer Mode automatically ONLY IF `DEV_MODE=0` is set to `DEV_MODE=1` in
 * the `x3d-settings.conf` and populates the active frontend with Developer
 * Mode Options
 */

/* if `DEV_MODE=0`, then ignore rest of this file and execute `x3d-toggle.c` as
 * standard user mode but if `DEV_MODE=1`, then read this file and inject rest
 * of this file into `x3d-toggle.c` as developer user mode, implementing
 * Developer UX in addition to standard user UX. First time run of Developer UX
 * will install Developer Environment suite.
 */
