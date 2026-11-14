#ifndef MINITEXT_EDITOR_H
#define MINITEXT_EDITOR_H

#include <gtk/gtk.h>

typedef struct _MiniTextEditor MiniTextEditor;

MiniTextEditor *mini_text_editor_new(void);
void mini_text_editor_free(MiniTextEditor *editor);
GtkWidget *mini_text_editor_get_widget(MiniTextEditor *editor);
GtkTextBuffer *mini_text_editor_get_buffer(MiniTextEditor *editor);
GtkTextView *mini_text_editor_get_text_view(MiniTextEditor *editor);

#endif
