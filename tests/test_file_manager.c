#include "file_manager.h"

#include <glib/gstdio.h>

static void
test_save_and_load_round_trip(void)
{
    GError *error = NULL;
    char *directory = g_dir_make_tmp("minitext-test-XXXXXX", &error);
    g_assert_no_error(error);
    g_assert_nonnull(directory);

    char *path = g_build_filename(directory, "document.txt", NULL);
    GFile *file = g_file_new_for_path(path);
    GtkTextBuffer *source = gtk_text_buffer_new(NULL);
    GtkTextBuffer *destination = gtk_text_buffer_new(NULL);
    gtk_text_buffer_set_enable_undo(destination, TRUE);
    const char *text = "MiniText UTF-8: Türkçe\nİkinci satır\n";
    gtk_text_buffer_set_text(source, text, -1);

    g_assert_true(file_manager_save(file, source, &error));
    g_assert_no_error(error);

    g_assert_true(file_manager_load(file, destination, &error));
    g_assert_no_error(error);
    g_assert_false(gtk_text_buffer_get_can_undo(destination));

    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(destination, &start, &end);
    char *loaded_text = gtk_text_buffer_get_text(destination, &start, &end, FALSE);
    g_assert_cmpstr(loaded_text, ==, text);
    g_free(loaded_text);

    const char *replacement = "Overwritten file\n";
    gtk_text_buffer_set_text(source, replacement, -1);
    g_assert_true(file_manager_save(file, source, &error));
    g_assert_no_error(error);
    gtk_text_buffer_set_text(destination, "", -1);
    g_assert_true(file_manager_load(file, destination, &error));
    g_assert_no_error(error);
    g_assert_false(gtk_text_buffer_get_can_undo(destination));

    gtk_text_buffer_get_bounds(destination, &start, &end);
    loaded_text = gtk_text_buffer_get_text(destination, &start, &end, FALSE);
    g_assert_cmpstr(loaded_text, ==, replacement);
    g_free(loaded_text);

    g_object_unref(destination);
    g_object_unref(source);
    g_object_unref(file);
    g_assert_cmpint(g_unlink(path), ==, 0);
    g_assert_cmpint(g_rmdir(directory), ==, 0);
    g_free(path);
    g_free(directory);
}

static void
test_load_rejects_invalid_utf8(void)
{
    GError *error = NULL;
    char *directory = g_dir_make_tmp("minitext-test-XXXXXX", &error);
    g_assert_no_error(error);
    g_assert_nonnull(directory);

    char *path = g_build_filename(directory, "invalid.txt", NULL);
    GFile *file = g_file_new_for_path(path);
    const char invalid_utf8[] = {(char)0xff};
    g_assert_true(g_file_replace_contents(
        file,
        invalid_utf8,
        sizeof(invalid_utf8),
        NULL,
        FALSE,
        G_FILE_CREATE_NONE,
        NULL,
        NULL,
        &error
    ));
    g_assert_no_error(error);

    GtkTextBuffer *buffer = gtk_text_buffer_new(NULL);
    gtk_text_buffer_set_text(buffer, "preserved", -1);
    g_assert_false(file_manager_load(file, buffer, &error));
    g_assert_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA);
    g_clear_error(&error);

    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    char *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    g_assert_cmpstr(text, ==, "preserved");
    g_free(text);

    g_object_unref(buffer);
    g_object_unref(file);
    g_assert_cmpint(g_unlink(path), ==, 0);
    g_assert_cmpint(g_rmdir(directory), ==, 0);
    g_free(path);
    g_free(directory);
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/file-manager/save-load-round-trip",
                    test_save_and_load_round_trip);
    g_test_add_func("/file-manager/reject-invalid-utf8",
                    test_load_rejects_invalid_utf8);
    return g_test_run();
}
