#ifndef MINITEXT_WINDOW_H
#define MINITEXT_WINDOW_H

#include <gtk/gtk.h>

typedef enum {
    MINITEXT_WINDOW_ACTION_NEW,
    MINITEXT_WINDOW_ACTION_OPEN,
    MINITEXT_WINDOW_ACTION_SAVE,
    MINITEXT_WINDOW_ACTION_SAVE_AS,
    MINITEXT_WINDOW_ACTION_FIND,
    MINITEXT_WINDOW_ACTION_REPLACE,
    MINITEXT_WINDOW_ACTION_UNDO,
    MINITEXT_WINDOW_ACTION_REDO,
    MINITEXT_WINDOW_ACTION_GO_TO_LINE
} MiniTextWindowAction;

GtkWindow *mini_text_window_new(GtkApplication *application);
void mini_text_window_activate_action(GtkWindow *window,
                                      MiniTextWindowAction action);

#endif
