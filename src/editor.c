#include "editor.h"

#include "line_numbers.h"

struct _MiniTextEditor {
    GtkWidget *widget;
    GtkTextView *text_view;
    GtkTextBuffer *buffer;
    GtkAdjustment *vertical_adjustment;
};

MiniTextEditor *
mini_text_editor_new(void)
{
    MiniTextEditor *editor = g_new0(MiniTextEditor, 1);
    editor->buffer = gtk_text_buffer_new(NULL);

    GtkWidget *text_view = gtk_text_view_new_with_buffer(editor->buffer);
    editor->text_view = GTK_TEXT_VIEW(text_view);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(text_view), TRUE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(text_view), 16);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(text_view), 16);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(text_view), 16);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(text_view), 16);

    editor->vertical_adjustment = gtk_adjustment_new(
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0
    );
    g_object_ref_sink(editor->vertical_adjustment);
    GtkWidget *line_number_scroller = line_numbers_new(
        GTK_TEXT_VIEW(text_view),
        editor->vertical_adjustment
    );
    GtkWidget *text_scroller = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(text_scroller),
        GTK_POLICY_AUTOMATIC,
        GTK_POLICY_AUTOMATIC
    );
    gtk_scrolled_window_set_vadjustment(
        GTK_SCROLLED_WINDOW(text_scroller),
        editor->vertical_adjustment
    );
    gtk_scrolled_window_set_child(
        GTK_SCROLLED_WINDOW(text_scroller),
        text_view
    );
    GtkWidget *content = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(content, "view");
    gtk_box_append(GTK_BOX(content), line_number_scroller);
    gtk_box_append(GTK_BOX(content), text_scroller);
    gtk_widget_set_hexpand(text_scroller, TRUE);
    gtk_widget_set_vexpand(content, TRUE);
    editor->widget = content;
    g_object_ref_sink(editor->widget);

    return editor;
}

void
mini_text_editor_free(MiniTextEditor *editor)
{
    if (editor == NULL)
        return;

    g_object_unref(editor->widget);
    g_clear_object(&editor->buffer);
    g_clear_object(&editor->vertical_adjustment);
    g_free(editor);
}

GtkWidget *
mini_text_editor_get_widget(MiniTextEditor *editor)
{
    return editor->widget;
}

GtkTextBuffer *
mini_text_editor_get_buffer(MiniTextEditor *editor)
{
    return editor->buffer;
}

GtkTextView *
mini_text_editor_get_text_view(MiniTextEditor *editor)
{
    return editor->text_view;
}
