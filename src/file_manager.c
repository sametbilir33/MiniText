#include "file_manager.h"

#include <string.h>

gboolean
file_manager_load(GFile *file, GtkTextBuffer *buffer, GError **error)
{
    char *contents = NULL;
    gsize length = 0;

    if (!g_file_load_contents(file, NULL, &contents, &length, NULL, error))
        return FALSE;

    if (length > G_MAXINT || contents == NULL || !g_utf8_validate(contents, (gssize)length, NULL)) {
        if (error != NULL)
            g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "Dosya UTF-8 metin biçiminde değil veya çok büyük.");
        g_free(contents);
        return FALSE;
    }

    gtk_text_buffer_begin_irreversible_action(buffer);
    gtk_text_buffer_set_text(buffer, contents, (gint)length);
    gtk_text_buffer_end_irreversible_action(buffer);
    g_free(contents);
    return TRUE;
}

gboolean
file_manager_save(GFile *file, GtkTextBuffer *buffer, GError **error)
{
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);

    char *contents = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    if (contents == NULL)
        return FALSE;

    gboolean success = g_file_replace_contents(file, contents, strlen(contents), NULL, FALSE, G_FILE_CREATE_NONE, NULL, NULL, error);
    g_free(contents);
    return success;
}
