#include "application.h"

#include "window.h"

#include <adwaita.h>

static void
on_action_new(GSimpleAction *action,
              GVariant *parameter,
              gpointer user_data)
{
    (void)action;
    (void)parameter;

    GtkWindow *window =
        gtk_application_get_active_window(GTK_APPLICATION(user_data));
    if (window != NULL)
        mini_text_window_activate_action(window, MINITEXT_WINDOW_ACTION_NEW);
}

static void
on_action_open(GSimpleAction *action,
               GVariant *parameter,
               gpointer user_data)
{
    (void)action;
    (void)parameter;

    GtkWindow *window =
        gtk_application_get_active_window(GTK_APPLICATION(user_data));
    if (window != NULL)
        mini_text_window_activate_action(window, MINITEXT_WINDOW_ACTION_OPEN);
}

static void
on_action_save(GSimpleAction *action,
               GVariant *parameter,
               gpointer user_data)
{
    (void)action;
    (void)parameter;

    GtkWindow *window =
        gtk_application_get_active_window(GTK_APPLICATION(user_data));
    if (window != NULL)
        mini_text_window_activate_action(window, MINITEXT_WINDOW_ACTION_SAVE);
}

static void
on_action_save_as(GSimpleAction *action,
                  GVariant *parameter,
                  gpointer user_data)
{
    (void)action;
    (void)parameter;

    GtkWindow *window =
        gtk_application_get_active_window(GTK_APPLICATION(user_data));
    if (window != NULL) {
        mini_text_window_activate_action(
            window,
            MINITEXT_WINDOW_ACTION_SAVE_AS
        );
    }
}

static void
on_action_find(GSimpleAction *action,
               GVariant *parameter,
               gpointer user_data)
{
    (void)action;
    (void)parameter;

    GtkWindow *window =
        gtk_application_get_active_window(GTK_APPLICATION(user_data));
    if (window != NULL)
        mini_text_window_activate_action(window, MINITEXT_WINDOW_ACTION_FIND);
}

static void
on_action_go_to_line(GSimpleAction *action,
                     GVariant *parameter,
                     gpointer user_data)
{
    (void)action;
    (void)parameter;

    GtkWindow *window =
        gtk_application_get_active_window(GTK_APPLICATION(user_data));
    if (window != NULL) {
        mini_text_window_activate_action(
            window,
            MINITEXT_WINDOW_ACTION_GO_TO_LINE
        );
    }
}

static void
on_action_about(GSimpleAction *action,
                GVariant *parameter,
                gpointer user_data)
{
    (void)action;
    (void)parameter;

    GtkApplication *app = GTK_APPLICATION(user_data);
    GtkWidget *parent = GTK_WIDGET(
    gtk_application_get_active_window(app)
);

    adw_show_about_dialog(
        parent,
        "application-name", "MiniText",
        "application-icon", "io.github.sametbilir33.MiniText",
        "developer-name", "Samet Bilir",
        "comments", "Minimalist bir metin düzenleyici.",
        "website", "https://github.com/sametbilir33",
        NULL
    );
}

static void
add_action(AdwApplication *application,
           const char *name,
           GCallback callback)
{
    GSimpleAction *action = g_simple_action_new(name, NULL);
    g_signal_connect(action, "activate", callback, application);
    g_action_map_add_action(G_ACTION_MAP(application), G_ACTION(action));
    g_object_unref(action);
}

static void
on_activate(GtkApplication *application, gpointer user_data)
{
    (void)user_data;

GtkIconTheme *theme = gtk_icon_theme_get_for_display(
    gdk_display_get_default()
);

gtk_icon_theme_add_search_path(theme, "data/icons");

    GList *windows = gtk_application_get_windows(application);
    if (windows != NULL) {
        gtk_window_present(GTK_WINDOW(windows->data));
        return;
    }

    GtkWindow *window = mini_text_window_new(application);
    gtk_window_present(window);
}

int
mini_text_application_run(int argc, char **argv)
{
    AdwApplication *application = adw_application_new(
        "io.github.sametbilir33.MiniText",
        G_APPLICATION_DEFAULT_FLAGS
    );

    add_action(application, "new", G_CALLBACK(on_action_new));
    add_action(application, "open", G_CALLBACK(on_action_open));
    add_action(application, "save", G_CALLBACK(on_action_save));
    add_action(application, "save-as", G_CALLBACK(on_action_save_as));
    add_action(application, "find", G_CALLBACK(on_action_find));
    add_action(application, "go-to-line", G_CALLBACK(on_action_go_to_line));
    add_action(application, "about", G_CALLBACK(on_action_about));
    gtk_application_set_accels_for_action(
        GTK_APPLICATION(application),
        "app.new",
        (const char *[]){"<Primary>n", NULL}
    );
    gtk_application_set_accels_for_action(
        GTK_APPLICATION(application),
        "app.open",
        (const char *[]){"<Primary>o", NULL}
    );
    gtk_application_set_accels_for_action(
        GTK_APPLICATION(application),
        "app.save",
        (const char *[]){"<Primary>s", NULL}
    );
    gtk_application_set_accels_for_action(
        GTK_APPLICATION(application),
        "app.save-as",
        (const char *[]){"<Primary><Shift>s", NULL}
    );
    gtk_application_set_accels_for_action(
        GTK_APPLICATION(application),
        "app.find",
        (const char *[]){"<Primary>f", NULL}
    );
    gtk_application_set_accels_for_action(
        GTK_APPLICATION(application),
        "app.go-to-line",
        (const char *[]){"<Primary>l", NULL}
    );

    g_signal_connect(application, "activate", G_CALLBACK(on_activate), NULL);

    int status = g_application_run(
        G_APPLICATION(application),
        argc,
        argv
    );

    g_object_unref(application);
    return status;
}
