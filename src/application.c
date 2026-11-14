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

    GtkWindow *window = gtk_application_get_active_window(GTK_APPLICATION(user_data));
    if (window != NULL) mini_text_window_activate_action(window, MINITEXT_WINDOW_ACTION_NEW);
}

static void
on_action_open(GSimpleAction *action,
               GVariant *parameter,
               gpointer user_data)
{
    (void)action;
    (void)parameter;

    GtkWindow *window = gtk_application_get_active_window(GTK_APPLICATION(user_data));
    if (window != NULL) mini_text_window_activate_action(window, MINITEXT_WINDOW_ACTION_OPEN);
}

static void
on_action_save(GSimpleAction *action,
               GVariant *parameter,
               gpointer user_data)
{
    (void)action;
    (void)parameter;

    GtkWindow *window = gtk_application_get_active_window(GTK_APPLICATION(user_data));
    if (window != NULL) mini_text_window_activate_action(window, MINITEXT_WINDOW_ACTION_SAVE);
}

static void
on_action_save_as(GSimpleAction *action,
                  GVariant *parameter,
                  gpointer user_data)
{
    (void)action;
    (void)parameter;

    GtkWindow *window = gtk_application_get_active_window(GTK_APPLICATION(user_data));
    if (window != NULL) mini_text_window_activate_action(window, MINITEXT_WINDOW_ACTION_SAVE_AS);
}

static void
on_action_find(GSimpleAction *action,
               GVariant *parameter,
               gpointer user_data)
{
    (void)action;
    (void)parameter;

    GtkWindow *window = gtk_application_get_active_window(GTK_APPLICATION(user_data));
    if (window != NULL) mini_text_window_activate_action(window, MINITEXT_WINDOW_ACTION_FIND);
}

static void
on_action_replace(GSimpleAction *action,
                  GVariant *parameter,
                  gpointer user_data)
{
    (void)action;
    (void)parameter;

    GtkWindow *window = gtk_application_get_active_window(GTK_APPLICATION(user_data));
    if (window != NULL) mini_text_window_activate_action(window, MINITEXT_WINDOW_ACTION_REPLACE);
}

static void
on_action_undo(GSimpleAction *action,
               GVariant *parameter,
               gpointer user_data)
{
    (void)action;
    (void)parameter;

    GtkWindow *window = gtk_application_get_active_window(GTK_APPLICATION(user_data));
    if (window != NULL) mini_text_window_activate_action(window, MINITEXT_WINDOW_ACTION_UNDO);
}

static void
on_action_redo(GSimpleAction *action,
               GVariant *parameter,
               gpointer user_data)
{
    (void)action;
    (void)parameter;

    GtkWindow *window = gtk_application_get_active_window(GTK_APPLICATION(user_data));
    if (window != NULL) mini_text_window_activate_action(window, MINITEXT_WINDOW_ACTION_REDO);
}

static void
on_action_go_to_line(GSimpleAction *action,
                     GVariant *parameter,
                     gpointer user_data)
{
    (void)action;
    (void)parameter;

    GtkWindow *window = gtk_application_get_active_window(GTK_APPLICATION(user_data));
    if (window != NULL) mini_text_window_activate_action(window, MINITEXT_WINDOW_ACTION_GO_TO_LINE);
}

static void
on_action_close_tab(GSimpleAction *action,
                    GVariant *parameter,
                    gpointer user_data)
{
    (void)action;
    (void)parameter;

    GtkWindow *window = gtk_application_get_active_window(GTK_APPLICATION(user_data));
    if (window != NULL) mini_text_window_activate_action(window, MINITEXT_WINDOW_ACTION_CLOSE_TAB);
}

static void
on_action_next_tab(GSimpleAction *action,
                   GVariant *parameter,
                   gpointer user_data)
{
    (void)action;
    (void)parameter;

    GtkWindow *window = gtk_application_get_active_window(GTK_APPLICATION(user_data));
    if (window != NULL) mini_text_window_activate_action(window, MINITEXT_WINDOW_ACTION_NEXT_TAB);
}

static void
on_action_previous_tab(GSimpleAction *action,
                       GVariant *parameter,
                       gpointer user_data)
{
    (void)action;
    (void)parameter;

    GtkWindow *window = gtk_application_get_active_window(GTK_APPLICATION(user_data));
    if (window != NULL) mini_text_window_activate_action(window, MINITEXT_WINDOW_ACTION_PREVIOUS_TAB);
}

static void
on_action_select_tab(GSimpleAction *action,
                     GVariant *parameter,
                     gpointer user_data)
{
    (void)parameter;

    GtkWindow *window = gtk_application_get_active_window(GTK_APPLICATION(user_data));
    if (window != NULL) {
        int position = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(action), "tab-position"));
        mini_text_window_activate_action(window, MINITEXT_WINDOW_ACTION_SELECT_TAB_1 + position);
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
    GtkWindow *parent = gtk_application_get_active_window(app);

    AdwAboutDialog *dialog = ADW_ABOUT_DIALOG(adw_about_dialog_new());
    adw_about_dialog_set_application_name(dialog, "MiniText");
    adw_about_dialog_set_application_icon(
        dialog,
        "io.github.sametbilir33.MiniText"
    );
    adw_about_dialog_set_version(dialog, "1.0.0");
    adw_about_dialog_set_developer_name(dialog, "Samet Bilir");
    adw_about_dialog_set_comments(
        dialog,
        "Minimalist bir metin düzenleyici."
    );
    adw_about_dialog_set_website(
        dialog,
        "https://github.com/sametbilir33/MiniText"
    );
    adw_about_dialog_set_issue_url(
        dialog,
        "https://github.com/sametbilir33/MiniText/issues"
    );
    adw_about_dialog_set_license_type(dialog, GTK_LICENSE_GPL_3_0);
    adw_dialog_present(ADW_DIALOG(dialog), GTK_WIDGET(parent));
}

static void
on_action_shortcuts(GSimpleAction *action,
                    GVariant *parameter,
                    gpointer user_data)
{
    (void)action;
    (void)parameter;

    GtkWindow *window = gtk_application_get_active_window(GTK_APPLICATION(user_data));
    if (window != NULL) mini_text_window_activate_action(window, MINITEXT_WINDOW_ACTION_SHORTCUTS);
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

    GList *windows = gtk_application_get_windows(application);
    if (windows != NULL) {
        gtk_window_present(GTK_WINDOW(windows->data));
        return;
    }

    GtkWindow *window = mini_text_window_new(application);
    gtk_window_present(window);
}

static void
on_open(GApplication *application,
        GFile **files,
        gint n_files,
        const char *hint,
        gpointer user_data)
{
    (void)hint;
    (void)user_data;

    GtkApplication *gtk_application = GTK_APPLICATION(application);
    GtkWindow *window = gtk_application_get_active_window(gtk_application);

    if (window == NULL) {
        window = mini_text_window_new(gtk_application);
    }

    gtk_window_present(window);

    for (gint i = 0; i < n_files; i++) {
        mini_text_window_open_file(window, files[i]);
    }
}

int
mini_text_application_run(int argc, char **argv)
{
    AdwApplication *application = adw_application_new(
    "io.github.sametbilir33.MiniText",
    G_APPLICATION_HANDLES_OPEN
    );

    add_action(application, "new", G_CALLBACK(on_action_new));
    add_action(application, "open", G_CALLBACK(on_action_open));
    add_action(application, "save", G_CALLBACK(on_action_save));
    add_action(application, "save-as", G_CALLBACK(on_action_save_as));
    add_action(application, "find", G_CALLBACK(on_action_find));
    add_action(application, "replace", G_CALLBACK(on_action_replace));
    add_action(application, "undo", G_CALLBACK(on_action_undo));
    add_action(application, "redo", G_CALLBACK(on_action_redo));
    add_action(application, "go-to-line", G_CALLBACK(on_action_go_to_line));
    add_action(application, "close-tab", G_CALLBACK(on_action_close_tab));
    add_action(application, "next-tab", G_CALLBACK(on_action_next_tab));
    add_action(
        application,
        "previous-tab",
        G_CALLBACK(on_action_previous_tab)
    );
    add_action(application, "about", G_CALLBACK(on_action_about));
    add_action(application, "shortcuts", G_CALLBACK(on_action_shortcuts));
    for (int position = 0; position < 9; position++) {
        char *name = g_strdup_printf("tab-%d", position + 1);
        GSimpleAction *action = g_simple_action_new(name, NULL);
        g_object_set_data(
            G_OBJECT(action),
            "tab-position",
            GINT_TO_POINTER(position)
        );
        g_signal_connect(action, "activate", G_CALLBACK(on_action_select_tab),
                         application);
        g_action_map_add_action(
            G_ACTION_MAP(application),
            G_ACTION(action)
        );
        g_object_unref(action);
        g_free(name);
    }
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
        "app.replace",
        (const char *[]){"<Primary>h", NULL}
    );
    gtk_application_set_accels_for_action(
        GTK_APPLICATION(application),
        "app.undo",
        (const char *[]){"<Primary>z", NULL}
    );
    gtk_application_set_accels_for_action(
        GTK_APPLICATION(application),
        "app.redo",
        (const char *[]){"<Primary><Shift>z", "<Primary>y", NULL}
    );
    gtk_application_set_accels_for_action(
        GTK_APPLICATION(application),
        "app.go-to-line",
        (const char *[]){"<Primary>l", NULL}
    );
    gtk_application_set_accels_for_action(
        GTK_APPLICATION(application),
        "app.about",
        (const char *[]){"<Primary>comma", NULL}
    );
    gtk_application_set_accels_for_action(
        GTK_APPLICATION(application),
        "app.shortcuts",
        (const char *[]){"<Primary>question", NULL}
    );
    gtk_application_set_accels_for_action(
        GTK_APPLICATION(application),
        "app.close-tab",
        (const char *[]){"<Primary>w", NULL}
    );
    gtk_application_set_accels_for_action(
        GTK_APPLICATION(application),
        "app.next-tab",
        (const char *[]){"<Primary>Tab", NULL}
    );
    gtk_application_set_accels_for_action(
        GTK_APPLICATION(application),
        "app.previous-tab",
        (const char *[]){"<Primary><Shift>Tab", NULL}
    );
    for (int position = 0; position < 9; position++) {
        char *action = g_strdup_printf("app.tab-%d", position + 1);
        char *accelerator = g_strdup_printf("<Primary>%d", position + 1);
        gtk_application_set_accels_for_action(
            GTK_APPLICATION(application),
            action,
            (const char *[]){accelerator, NULL}
        );
        g_free(accelerator);
        g_free(action);
    }

    g_signal_connect(application, "open", G_CALLBACK(on_open), NULL);

    int status = g_application_run(
        G_APPLICATION(application),
        argc,
        argv
    );

    g_object_unref(application);
    return status;
}
