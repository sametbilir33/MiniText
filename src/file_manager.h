#ifndef MINITEXT_FILE_MANAGER_H
#define MINITEXT_FILE_MANAGER_H

#include <gtk/gtk.h>

gboolean file_manager_load(GFile *file,
                           GtkTextBuffer *buffer,
                           GError **error);
gboolean file_manager_save(GFile *file,
                           GtkTextBuffer *buffer,
                           GError **error);

#endif
