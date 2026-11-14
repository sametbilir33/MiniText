#include "line_numbers.h"

#include <string.h>

typedef struct {
    GtkTextView *text_view;
    GtkTextBuffer *buffer;
    GtkDrawingArea *drawing_area;
    int text_view_width;
    int text_view_height;
    gboolean metrics_update_pending;
} LineNumbers;

static void
line_numbers_free(gpointer user_data)
{
    LineNumbers *line_numbers = user_data;
    g_clear_object(&line_numbers->text_view);
    g_free(line_numbers);
}

static void
update_metrics(LineNumbers *line_numbers)
{
    line_numbers->text_view_width =
        gtk_widget_get_width(GTK_WIDGET(line_numbers->text_view));
    line_numbers->text_view_height =
        gtk_widget_get_height(GTK_WIDGET(line_numbers->text_view));

    GtkTextIter end;
    gtk_text_buffer_get_end_iter(line_numbers->buffer, &end);

    int line_y = 0;
    int line_height = 0;
    gtk_text_view_get_line_yrange(
        line_numbers->text_view,
        &end,
        &line_y,
        &line_height
    );

    int content_height = MAX(
        gtk_widget_get_height(GTK_WIDGET(line_numbers->text_view)),
        gtk_text_view_get_top_margin(line_numbers->text_view) +
            line_y + line_height +
            gtk_text_view_get_bottom_margin(line_numbers->text_view)
    );
    gtk_drawing_area_set_content_height(
        line_numbers->drawing_area,
        MAX(content_height, 1)
    );

    int line_count = gtk_text_buffer_get_line_count(line_numbers->buffer);
    char *last_line = g_strdup_printf("%d", MAX(line_count, 1));
    PangoLayout *layout = gtk_widget_create_pango_layout(
        GTK_WIDGET(line_numbers->drawing_area),
        "0"
    );
    int character_width = 0;
    pango_layout_get_pixel_size(layout, &character_width, NULL);
    g_object_unref(layout);

    gtk_drawing_area_set_content_width(
        line_numbers->drawing_area,
        (int)strlen(last_line) * character_width + 16
    );
    g_free(last_line);
    gtk_widget_queue_draw(GTK_WIDGET(line_numbers->drawing_area));
}

static gboolean
update_metrics_idle(gpointer user_data)
{
    GtkWidget *drawing_area = GTK_WIDGET(user_data);
    LineNumbers *line_numbers =
        g_object_get_data(G_OBJECT(drawing_area), "line-numbers");
    line_numbers->metrics_update_pending = FALSE;

    if (gtk_widget_get_root(GTK_WIDGET(line_numbers->text_view)) != NULL)
        update_metrics(line_numbers);

    return G_SOURCE_REMOVE;
}

static void
on_buffer_changed(GtkTextBuffer *buffer, gpointer user_data)
{
    (void)buffer;
    GtkWidget *drawing_area = GTK_WIDGET(user_data);
    LineNumbers *line_numbers =
        g_object_get_data(G_OBJECT(drawing_area), "line-numbers");
    update_metrics(line_numbers);
}

static void
on_adjustment_changed(GtkAdjustment *adjustment, gpointer user_data)
{
    (void)adjustment;
    gtk_widget_queue_draw(GTK_WIDGET(user_data));
}

static void
draw_line_numbers(GtkDrawingArea *drawing_area,
                  cairo_t *cr,
                  int width,
                  int height,
                  gpointer user_data)
{
    (void)width;
    (void)height;

    LineNumbers *line_numbers = user_data;
    int text_view_width =
        gtk_widget_get_width(GTK_WIDGET(line_numbers->text_view));
    int text_view_height =
        gtk_widget_get_height(GTK_WIDGET(line_numbers->text_view));
    if (!line_numbers->metrics_update_pending &&
        (text_view_width != line_numbers->text_view_width ||
         text_view_height != line_numbers->text_view_height)) {
        line_numbers->metrics_update_pending = TRUE;
        g_idle_add_full(
            G_PRIORITY_DEFAULT_IDLE,
            update_metrics_idle,
            g_object_ref(drawing_area),
            g_object_unref
        );
    }

    GdkRectangle visible_rect;
    gtk_text_view_get_visible_rect(line_numbers->text_view, &visible_rect);

    GtkTextIter iter;
    gtk_text_view_get_line_at_y(
        line_numbers->text_view,
        &iter,
        visible_rect.y,
        NULL
    );

    GdkRGBA color;
    gtk_widget_get_color(GTK_WIDGET(drawing_area), &color);
    gdk_cairo_set_source_rgba(cr, &color);

    gboolean first_line = TRUE;
    do {
        gtk_text_iter_set_line_offset(&iter, 0);

        GdkRectangle line_rect;
        gtk_text_view_get_iter_location(
            line_numbers->text_view,
            &iter,
            &line_rect
        );
        if (first_line && line_rect.y < visible_rect.y) {
            GtkTextIter visible_iter;
            if (gtk_text_view_get_iter_at_location(
                    line_numbers->text_view,
                    &visible_iter,
                    visible_rect.x +
                        gtk_text_view_get_left_margin(line_numbers->text_view),
                    visible_rect.y)) {
                gtk_text_view_get_iter_location(
                    line_numbers->text_view,
                    &visible_iter,
                    &line_rect
                );
            }
        }

        if (line_rect.y >= visible_rect.y + visible_rect.height)
            break;

        line_rect.y += gtk_text_view_get_top_margin(line_numbers->text_view);

        char *number = g_strdup_printf("%d", gtk_text_iter_get_line(&iter) + 1);
        PangoLayout *layout = gtk_widget_create_pango_layout(
            GTK_WIDGET(drawing_area),
            number
        );
        int text_width = 0;
        int text_height = 0;
        pango_layout_get_pixel_size(layout, &text_width, &text_height);
        cairo_move_to(
            cr,
            gtk_drawing_area_get_content_width(drawing_area) - text_width - 8,
            line_rect.y + (line_rect.height - text_height) / 2
        );
        pango_cairo_show_layout(cr, layout);
        g_object_unref(layout);
        g_free(number);
        first_line = FALSE;
    } while (gtk_text_iter_forward_line(&iter));
}

GtkWidget *
line_numbers_new(GtkTextView *text_view,
                 GtkAdjustment *vertical_adjustment)
{
    GtkWidget *scrolled_window = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(scrolled_window),
        GTK_POLICY_NEVER,
        GTK_POLICY_EXTERNAL
    );
    gtk_scrolled_window_set_vadjustment(
        GTK_SCROLLED_WINDOW(scrolled_window),
        vertical_adjustment
    );

    GtkWidget *drawing_area = gtk_drawing_area_new();
    gtk_widget_add_css_class(drawing_area, "dim-label");
    gtk_widget_add_css_class(drawing_area, "monospace");

    LineNumbers *line_numbers = g_new0(LineNumbers, 1);
    line_numbers->text_view = g_object_ref(text_view);
    line_numbers->buffer = gtk_text_view_get_buffer(text_view);
    line_numbers->drawing_area = GTK_DRAWING_AREA(drawing_area);
    g_object_set_data_full(
        G_OBJECT(drawing_area),
        "line-numbers",
        line_numbers,
        line_numbers_free
    );
    gtk_drawing_area_set_draw_func(
        GTK_DRAWING_AREA(drawing_area),
        draw_line_numbers,
        line_numbers,
        NULL
    );

    g_signal_connect_object(
        line_numbers->buffer,
        "changed",
        G_CALLBACK(on_buffer_changed),
        G_OBJECT(drawing_area),
        0
    );
    g_signal_connect_object(
        vertical_adjustment,
        "value-changed",
        G_CALLBACK(on_adjustment_changed),
        G_OBJECT(drawing_area),
        0
    );

    gtk_scrolled_window_set_child(
        GTK_SCROLLED_WINDOW(scrolled_window),
        drawing_area
    );
    update_metrics(line_numbers);
    return scrolled_window;
}
