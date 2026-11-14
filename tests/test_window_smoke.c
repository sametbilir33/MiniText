#include "window.h"

#include <adwaita.h>

static GtkTextView *
find_text_view(GtkWidget *widget)
{
    if (GTK_IS_TEXT_VIEW(widget))
        return GTK_TEXT_VIEW(widget);

    for (GtkWidget *child = gtk_widget_get_first_child(widget);
         child != NULL;
         child = gtk_widget_get_next_sibling(child)) {
        GtkTextView *text_view = find_text_view(child);
        if (text_view != NULL)
            return text_view;
    }

    return NULL;
}

static GtkSearchEntry *
find_search_entry(GtkWidget *widget)
{
    if (GTK_IS_SEARCH_ENTRY(widget))
        return GTK_SEARCH_ENTRY(widget);

    for (GtkWidget *child = gtk_widget_get_first_child(widget);
         child != NULL;
         child = gtk_widget_get_next_sibling(child)) {
        GtkSearchEntry *search_entry = find_search_entry(child);
        if (search_entry != NULL)
            return search_entry;
    }

    return NULL;
}

static GtkImage *
find_icon(GtkWidget *widget, const char *icon_name)
{
    if (GTK_IS_IMAGE(widget) &&
        g_strcmp0(gtk_image_get_icon_name(GTK_IMAGE(widget)), icon_name) == 0) {
        return GTK_IMAGE(widget);
    }

    for (GtkWidget *child = gtk_widget_get_first_child(widget);
         child != NULL;
         child = gtk_widget_get_next_sibling(child)) {
        GtkImage *image = find_icon(child, icon_name);
        if (image != NULL)
            return image;
    }

    return NULL;
}

static GtkSpinButton *
find_spin_button(GtkWidget *widget)
{
    if (GTK_IS_SPIN_BUTTON(widget))
        return GTK_SPIN_BUTTON(widget);

    for (GtkWidget *child = gtk_widget_get_first_child(widget);
         child != NULL;
         child = gtk_widget_get_next_sibling(child)) {
        GtkSpinButton *spin_button = find_spin_button(child);
        if (spin_button != NULL)
            return spin_button;
    }

    return NULL;
}

static GtkButton *
find_button_by_label(GtkWidget *widget, const char *label)
{
    if (GTK_IS_LABEL(widget) &&
        g_strcmp0(gtk_label_get_text(GTK_LABEL(widget)), label) == 0) {
        for (GtkWidget *parent = gtk_widget_get_parent(widget);
             parent != NULL;
             parent = gtk_widget_get_parent(parent)) {
            if (GTK_IS_BUTTON(parent))
                return GTK_BUTTON(parent);
        }
    }

    for (GtkWidget *child = gtk_widget_get_first_child(widget);
         child != NULL;
         child = gtk_widget_get_next_sibling(child)) {
        GtkButton *button = find_button_by_label(child, label);
        if (button != NULL)
            return button;
    }

    return NULL;
}

static gboolean
close_window(gpointer user_data)
{
    GtkWindow *window = GTK_WINDOW(user_data);
    GtkWidget *content = gtk_window_get_child(window);
    int minimum_width = 0;
    int minimum_height = 0;
    gtk_widget_measure(
        content,
        GTK_ORIENTATION_HORIZONTAL,
        -1,
        &minimum_width,
        NULL,
        NULL,
        NULL
    );
    gtk_widget_measure(
        content,
        GTK_ORIENTATION_VERTICAL,
        -1,
        &minimum_height,
        NULL,
        NULL,
        NULL
    );
    g_assert_true(gtk_window_get_resizable(window));
    g_assert_cmpint(minimum_width, <=, 440);
    g_assert_cmpint(minimum_height, <=, 200);

    GtkTextView *text_view = find_text_view(content);
    g_assert_nonnull(text_view);
    g_assert_nonnull(find_icon(content, "go-jump-symbolic"));
    GtkTextIter first_line;
    gtk_text_buffer_get_start_iter(gtk_text_view_get_buffer(text_view),
                                   &first_line);
    int wrapped_line_y = 0;
    int wrapped_line_height = 0;
    GdkRectangle first_character;
    gtk_text_view_get_line_yrange(
        text_view,
        &first_line,
        &wrapped_line_y,
        &wrapped_line_height
    );
    gtk_text_view_get_iter_location(text_view, &first_line, &first_character);
    g_assert_cmpint(wrapped_line_height, >, first_character.height);

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(text_view);
    GtkTextIter match_start;
    GtkTextIter match_end;
    g_assert_true(gtk_text_buffer_get_selection_bounds(
        buffer,
        &match_start,
        &match_end
    ));
    char *match = gtk_text_buffer_get_text(
        buffer,
        &match_start,
        &match_end,
        FALSE
    );
    g_assert_cmpstr(match, ==, "Resize");
    g_free(match);

    GtkWindow *goto_dialog = NULL;
    GListModel *top_levels = gtk_window_get_toplevels();
    for (guint index = 0; index < g_list_model_get_n_items(top_levels); index++) {
        GtkWindow *candidate = g_list_model_get_item(top_levels, index);
        if (candidate != window &&
            gtk_window_get_transient_for(candidate) == window &&
            g_strcmp0(gtk_window_get_title(candidate), "Satıra Git") == 0) {
            goto_dialog = candidate;
            break;
        }
        g_object_unref(candidate);
    }
    g_assert_nonnull(goto_dialog);
    GtkWidget *dialog_content = gtk_window_get_child(goto_dialog);
    GtkSpinButton *spin_button = find_spin_button(dialog_content);
    GtkButton *go_button = find_button_by_label(dialog_content, "Git");
    g_assert_nonnull(spin_button);
    g_assert_nonnull(go_button);
    gtk_spin_button_set_value(spin_button, 25);
    g_signal_emit_by_name(go_button, "clicked");
    g_object_unref(goto_dialog);

    GtkTextIter cursor;
    gtk_text_buffer_get_iter_at_mark(
        buffer,
        &cursor,
        gtk_text_buffer_get_insert(buffer)
    );
    g_assert_cmpint(gtk_text_iter_get_line(&cursor), ==, 24);

    gtk_window_destroy(window);
    g_object_unref(window);
    return G_SOURCE_REMOVE;
}

static void
on_activate(GtkApplication *application, gpointer user_data)
{
    (void)user_data;

    GtkWindow *window = mini_text_window_new(application);
    gtk_window_set_default_size(window, 360, 280);

    GtkTextView *text_view = find_text_view(gtk_window_get_child(window));
    g_assert_nonnull(text_view);
    GString *text = g_string_sized_new(24000);
    for (int character = 0; character < 600; character++)
        g_string_append_c(text, 'W');
    g_string_append_c(text, '\n');
    for (int line = 0; line < 2000; line++)
        g_string_append(text, "Resize test line\n");
    gtk_text_buffer_set_text(
        gtk_text_view_get_buffer(text_view),
        text->str,
        (gint)text->len
    );
    g_string_free(text, TRUE);

    gtk_window_present(window);
    mini_text_window_activate_action(window, MINITEXT_WINDOW_ACTION_FIND);
    GtkSearchEntry *search_entry =
        find_search_entry(gtk_window_get_child(window));
    g_assert_nonnull(search_entry);
    gtk_editable_set_text(GTK_EDITABLE(search_entry), "rEsIzE");
    mini_text_window_activate_action(
        window,
        MINITEXT_WINDOW_ACTION_GO_TO_LINE
    );
    g_timeout_add_seconds(1, close_window, g_object_ref(window));
}

int
main(int argc, char **argv)
{
    AdwApplication *application = adw_application_new(
        "com.example.MiniText.Test",
        G_APPLICATION_NON_UNIQUE
    );
    g_signal_connect(application, "activate", G_CALLBACK(on_activate), NULL);
    int status = g_application_run(G_APPLICATION(application), argc, argv);
    g_object_unref(application);
    return status;
}
