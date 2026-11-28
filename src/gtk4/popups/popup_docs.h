/* Document Viewer, Dry-Run Validator & Journal Maintenance Popups Header
 * `popups/popup_docs.h`
 */

#ifndef X3D_POPUP_DOCS_H
#define X3D_POPUP_DOCS_H

#include "../gui_popups.h"

void popup_docs_show_viewer(GtkWindow *parent);
void popup_docs_show_dry_run(GtkWindow *parent, X3dGuiIpc *ipc);
void popup_docs_show_journal_maint(GtkWindow *parent, gboolean is_rotate);

#endif /* X3D_POPUP_DOCS_H */
