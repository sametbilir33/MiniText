#ifndef MINITEXT_LINE_NUMBERS_H
#define MINITEXT_LINE_NUMBERS_H

#include <gtk/gtk.h>

GtkWidget *line_numbers_new(GtkTextView *text_view,
                            GtkAdjustment *vertical_adjustment);

#endif
