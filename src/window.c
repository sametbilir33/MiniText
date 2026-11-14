#include "window.h"

#include "editor.h"
#include "file_manager.h"

#include <adwaita.h>

typedef enum {
    PENDING_ACTION_NONE,
    PENDING_ACTION_NEW,
    PENDING_ACTION_OPEN,
    PENDING_ACTION_CLOSE
} PendingAction;

typedef struct {
    GtkWindow *window;
    MiniTextEditor *editor;
    GtkSearchBar *search_bar;
    GtkSearchEntry *search_entry;
    GFile *file;
    gboolean modified;
    gboolean loading;
    PendingAction pending_action;
} MiniTextWindowState;

static void continue_pending_action(MiniTextWindowState *state);

static MiniTextWindowState *
get_state(GtkWindow *window)
{
    return g_object_get_data(G_OBJECT(window), "mini-text-window-state");
}

static void
set_button_accessible_name(GtkWidget *button, const char *name)
{
    gtk_accessible_update_property(
        GTK_ACCESSIBLE(button),
        GTK_ACCESSIBLE_PROPERTY_LABEL,
        name,
        -1
    );
}

static GtkWidget *
new_labeled_icon_button(const char *icon_name, const char *label)
{
    GtkWidget *button = gtk_button_new();
    GtkWidget *content = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *icon = gtk_image_new_from_icon_name(icon_name);
    GtkWidget *text = gtk_label_new(label);

    gtk_box_append(GTK_BOX(content), icon);
    gtk_box_append(GTK_BOX(content), text);
    gtk_button_set_child(GTK_BUTTON(button), content);
    set_button_accessible_name(button, label);
    return button;
}

static void
show_error(MiniTextWindowState *state,
           const char *message,
           const char *detail)
{
    GtkAlertDialog *dialog = gtk_alert_dialog_new("%s", message);
    gtk_alert_dialog_set_detail(dialog, detail);
    gtk_alert_dialog_show(dialog, state->window);
    g_object_unref(dialog);
}

static void
update_title(MiniTextWindowState *state)
{
    char *name = state->file != NULL
        ? g_file_get_basename(state->file)
        : g_strdup("Yeni Belge");
    char *title = g_strdup_printf("%s%s",
                                  state->modified ? "● " : "",
                                  name);

    gtk_window_set_title(state->window, title);
    g_free(title);
    g_free(name);
}

static void
on_text_changed(GtkTextBuffer *buffer, gpointer user_data)
{
    (void)buffer;

    MiniTextWindowState *state = user_data;
    if (state->loading)
        return;

    state->modified = TRUE;
    update_title(state);
}

static gboolean
finish_save(MiniTextWindowState *state, GFile *file)
{
    GError *error = NULL;
    if (!file_manager_save(
            file,
            mini_text_editor_get_buffer(state->editor),
            &error)) {
        show_error(state, "Dosya kaydedilemedi", error->message);
        g_error_free(error);
        return FALSE;
    }

    g_set_object(&state->file, file);
    state->modified = FALSE;
    update_title(state);
    return TRUE;
}

static void
on_save_response(GObject *source,
                 GAsyncResult *result,
                 gpointer user_data)
{
    GtkWindow *window = GTK_WINDOW(user_data);
    MiniTextWindowState *state = get_state(window);
    GError *error = NULL;
    GFile *file = gtk_file_dialog_save_finish(
        GTK_FILE_DIALOG(source),
        result,
        &error
    );
    gboolean save_dialog_completed = file != NULL;

    if (file != NULL) {
        gboolean saved = finish_save(state, file);
        g_object_unref(file);

        if (state->pending_action != PENDING_ACTION_NONE) {
            if (saved)
                continue_pending_action(state);
            else
                state->pending_action = PENDING_ACTION_NONE;
        }
    } else if (error != NULL &&
               !g_error_matches(
                   error,
                   GTK_DIALOG_ERROR,
                   GTK_DIALOG_ERROR_DISMISSED)) {
        show_error(state, "Kaydetme hatası", error->message);
    }

    g_clear_error(&error);
    if (!save_dialog_completed &&
        state->pending_action != PENDING_ACTION_NONE) {
        state->pending_action = PENDING_ACTION_NONE;
    }
    g_object_unref(window);
}

static void
show_save_dialog(MiniTextWindowState *state)
{
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Dosyayı Kaydet");

    char *initial_name = state->file != NULL
        ? g_file_get_basename(state->file)
        : g_strdup("belge.txt");
    gtk_file_dialog_set_initial_name(dialog, initial_name);
    g_free(initial_name);

    if (state->file != NULL)
        gtk_file_dialog_set_initial_file(dialog, state->file);

    gtk_file_dialog_save(
        dialog,
        state->window,
        NULL,
        on_save_response,
        g_object_ref(state->window)
    );
    g_object_unref(dialog);
}

static void
on_open_response(GObject *source,
                 GAsyncResult *result,
                 gpointer user_data)
{
    GtkWindow *window = GTK_WINDOW(user_data);
    MiniTextWindowState *state = get_state(window);
    GError *error = NULL;
    GFile *file = gtk_file_dialog_open_finish(
        GTK_FILE_DIALOG(source),
        result,
        &error
    );

    if (file != NULL) {
        state->loading = TRUE;
        gboolean loaded = file_manager_load(
            file,
            mini_text_editor_get_buffer(state->editor),
            &error
        );
        state->loading = FALSE;

        if (loaded) {
            g_set_object(&state->file, file);
            state->modified = FALSE;
            update_title(state);
        } else {
            show_error(state, "Dosya açılamadı", error->message);
        }

        g_object_unref(file);
    } else if (error != NULL &&
               !g_error_matches(
                   error,
                   GTK_DIALOG_ERROR,
                   GTK_DIALOG_ERROR_DISMISSED)) {
        show_error(state, "Dosya açılamadı", error->message);
    }

    g_clear_error(&error);
    g_object_unref(window);
}

static void
show_open_dialog(MiniTextWindowState *state)
{
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Dosya Aç");
    gtk_file_dialog_open(
        dialog,
        state->window,
        NULL,
        on_open_response,
        g_object_ref(state->window)
    );
    g_object_unref(dialog);
}

static void
clear_document(MiniTextWindowState *state)
{
    state->loading = TRUE;
    gtk_text_buffer_set_text(
        mini_text_editor_get_buffer(state->editor),
        "",
        -1
    );
    state->loading = FALSE;
    g_clear_object(&state->file);
    state->modified = FALSE;
    update_title(state);
}

static void
continue_pending_action(MiniTextWindowState *state)
{
    PendingAction action = state->pending_action;
    state->pending_action = PENDING_ACTION_NONE;

    switch (action) {
    case PENDING_ACTION_NEW:
        clear_document(state);
        break;
    case PENDING_ACTION_OPEN:
        show_open_dialog(state);
        break;
    case PENDING_ACTION_CLOSE:
        gtk_window_destroy(state->window);
        break;
    case PENDING_ACTION_NONE:
        break;
    }
}

static void
on_discard_response(GObject *source,
                    GAsyncResult *result,
                    gpointer user_data)
{
    GtkWindow *window = GTK_WINDOW(user_data);
    MiniTextWindowState *state = get_state(window);
    GError *error = NULL;
    int choice = gtk_alert_dialog_choose_finish(
        GTK_ALERT_DIALOG(source),
        result,
        &error
    );

    if (error != NULL || choice == 0) {
        state->pending_action = PENDING_ACTION_NONE;
    } else if (choice == 1) {
        if (state->file != NULL) {
            if (finish_save(state, state->file))
                continue_pending_action(state);
            else
                state->pending_action = PENDING_ACTION_NONE;
        } else {
            show_save_dialog(state);
        }
    } else if (choice == 2) {
        continue_pending_action(state);
    } else {
        state->pending_action = PENDING_ACTION_NONE;
    }

    g_clear_error(&error);
    g_object_unref(window);
}

static void
show_discard_dialog(MiniTextWindowState *state, PendingAction action)
{
    state->pending_action = action;
    GtkAlertDialog *dialog = gtk_alert_dialog_new(
        "Kaydedilmemiş değişiklikler var"
    );
    gtk_alert_dialog_set_detail(
        dialog,
        "Devam edersen mevcut değişiklikler kaybolacak."
    );
    const char *buttons[] = {
        "İptal",
        "Kaydet",
        "Değişiklikleri At",
        NULL
    };
    gtk_alert_dialog_set_buttons(dialog, buttons);
    gtk_alert_dialog_set_cancel_button(dialog, 0);
    gtk_alert_dialog_set_default_button(dialog, 0);
    gtk_alert_dialog_choose(
        dialog,
        state->window,
        NULL,
        on_discard_response,
        g_object_ref(state->window)
    );
    g_object_unref(dialog);
}

static void
request_document_action(MiniTextWindowState *state, PendingAction action)
{
    if (state->pending_action != PENDING_ACTION_NONE)
        return;

    state->pending_action = action;
    if (state->modified)
        show_discard_dialog(state, action);
    else
        continue_pending_action(state);
}

static gboolean
search_document(MiniTextWindowState *state,
                gboolean backwards,
                gboolean from_start)
{
    const char *query = gtk_editable_get_text(
        GTK_EDITABLE(state->search_entry)
    );
    if (query[0] == '\0') {
        gtk_widget_remove_css_class(
            GTK_WIDGET(state->search_entry),
            "error"
        );
        return FALSE;
    }

    GtkTextBuffer *buffer = mini_text_editor_get_buffer(state->editor);
    GtkTextIter iter;
    if (from_start) {
        if (backwards)
            gtk_text_buffer_get_end_iter(buffer, &iter);
        else
            gtk_text_buffer_get_start_iter(buffer, &iter);
    } else {
        GtkTextIter selection_start;
        GtkTextIter selection_end;
        if (gtk_text_buffer_get_selection_bounds(
                buffer,
                &selection_start,
                &selection_end)) {
            iter = backwards ? selection_start : selection_end;
        } else {
            gtk_text_buffer_get_iter_at_mark(
                buffer,
                &iter,
                gtk_text_buffer_get_insert(buffer)
            );
        }
    }

    GtkTextIter match_start;
    GtkTextIter match_end;
    GtkTextSearchFlags flags = GTK_TEXT_SEARCH_CASE_INSENSITIVE;
    gboolean found = backwards
        ? gtk_text_iter_backward_search(
              &iter,
              query,
              flags,
              &match_start,
              &match_end,
              NULL
          )
        : gtk_text_iter_forward_search(
              &iter,
              query,
              flags,
              &match_start,
              &match_end,
              NULL
          );

    if (!found) {
        if (backwards)
            gtk_text_buffer_get_end_iter(buffer, &iter);
        else
            gtk_text_buffer_get_start_iter(buffer, &iter);

        found = backwards
            ? gtk_text_iter_backward_search(
                  &iter,
                  query,
                  flags,
                  &match_start,
                  &match_end,
                  NULL
              )
            : gtk_text_iter_forward_search(
                  &iter,
                  query,
                  flags,
                  &match_start,
                  &match_end,
                  NULL
              );
    }

    if (!found) {
        gtk_widget_add_css_class(
            GTK_WIDGET(state->search_entry),
            "error"
        );
        return FALSE;
    }

    gtk_widget_remove_css_class(GTK_WIDGET(state->search_entry), "error");
    gtk_text_buffer_select_range(buffer, &match_end, &match_start);
    gtk_text_view_scroll_to_iter(
        mini_text_editor_get_text_view(state->editor),
        &match_start,
        0.1,
        TRUE,
        0.0,
        0.5
    );
    return TRUE;
}

static void
on_search_changed(GtkSearchEntry *entry, gpointer user_data)
{
    (void)entry;
    search_document(user_data, FALSE, TRUE);
}

static void
on_find_next(GtkButton *button, gpointer user_data)
{
    (void)button;
    search_document(user_data, FALSE, FALSE);
}

static void
on_find_previous(GtkButton *button, gpointer user_data)
{
    (void)button;
    search_document(user_data, TRUE, FALSE);
}

static void
on_go_to_line_accept(GtkButton *button, gpointer user_data)
{
    (void)button;
    GtkWindow *dialog = GTK_WINDOW(user_data);
    GtkWindow *window = gtk_window_get_transient_for(dialog);
    MiniTextWindowState *state = get_state(window);
    GtkSpinButton *spin_button = g_object_get_data(
        G_OBJECT(dialog),
        "line-number-spin-button"
    );
    int line_number = gtk_spin_button_get_value_as_int(spin_button);
    GtkTextBuffer *buffer = mini_text_editor_get_buffer(state->editor);
    GtkTextIter iter;
    if (gtk_text_buffer_get_iter_at_line(buffer, &iter, line_number - 1)) {
        gtk_text_buffer_place_cursor(buffer, &iter);
        gtk_text_view_scroll_to_iter(
            mini_text_editor_get_text_view(state->editor),
            &iter,
            0.0,
            TRUE,
            0.0,
            0.5
        );
    }

    gtk_window_destroy(dialog);
}

static void
on_go_to_line_cancel(GtkButton *button, gpointer user_data)
{
    (void)button;
    gtk_window_destroy(GTK_WINDOW(user_data));
}

static void
show_go_to_line_dialog(MiniTextWindowState *state)
{
    GtkTextBuffer *buffer = mini_text_editor_get_buffer(state->editor);
    GtkTextIter cursor;
    gtk_text_buffer_get_iter_at_mark(
        buffer,
        &cursor,
        gtk_text_buffer_get_insert(buffer)
    );
    int current_line = gtk_text_iter_get_line(&cursor) + 1;
    int line_count = MAX(gtk_text_buffer_get_line_count(buffer), 1);

    GtkWindow *dialog = GTK_WINDOW(gtk_window_new());
    gtk_window_set_title(dialog, "Satıra Git");
    gtk_window_set_transient_for(dialog, state->window);
    gtk_window_set_modal(dialog, TRUE);
    gtk_window_set_resizable(dialog, FALSE);

    GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *label = gtk_label_new("Satır numarası:");
    GtkWidget *spin_button = gtk_spin_button_new_with_range(
        1,
        line_count,
        1
    );
    gtk_spin_button_set_value(
        GTK_SPIN_BUTTON(spin_button),
        MIN(current_line, line_count)
    );
    gtk_box_append(GTK_BOX(row), label);
    gtk_box_append(GTK_BOX(row), spin_button);
    GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *cancel_button =
        new_labeled_icon_button("window-close-symbolic", "İptal");
    GtkWidget *go_button =
        new_labeled_icon_button("object-select-symbolic", "Git");
    gtk_widget_set_halign(buttons, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(buttons), cancel_button);
    gtk_box_append(GTK_BOX(buttons), go_button);
    gtk_box_append(GTK_BOX(content), row);
    gtk_box_append(GTK_BOX(content), buttons);
    gtk_widget_set_margin_top(content, 12);
    gtk_widget_set_margin_bottom(content, 12);
    gtk_widget_set_margin_start(content, 12);
    gtk_widget_set_margin_end(content, 12);
    gtk_window_set_child(dialog, content);
    gtk_window_set_default_widget(dialog, go_button);
    gtk_widget_grab_focus(spin_button);
    g_object_set_data(
        G_OBJECT(dialog),
        "line-number-spin-button",
        spin_button
    );
    g_signal_connect(
        go_button,
        "clicked",
        G_CALLBACK(on_go_to_line_accept),
        dialog
    );
    g_signal_connect(
        cancel_button,
        "clicked",
        G_CALLBACK(on_go_to_line_cancel),
        dialog
    );
    gtk_window_present(dialog);
}

static gboolean
on_close_request(GtkWindow *window, gpointer user_data)
{
    (void)user_data;

    MiniTextWindowState *state = get_state(window);
    if (!state->modified)
        return FALSE;

    if (state->pending_action == PENDING_ACTION_NONE)
        show_discard_dialog(state, PENDING_ACTION_CLOSE);
    return TRUE;
}

static void
on_window_state_free(gpointer user_data)
{
    MiniTextWindowState *state = user_data;
    g_clear_object(&state->file);
    mini_text_editor_free(state->editor);
    g_free(state);
}

GtkWindow *
mini_text_window_new(GtkApplication *application)
{
    GtkWidget *window = adw_application_window_new(application);
    gtk_window_set_title(GTK_WINDOW(window), "MiniText");
    gtk_window_set_default_size(GTK_WINDOW(window), 850, 600);
    gtk_window_set_resizable(GTK_WINDOW(window), TRUE);

    MiniTextWindowState *state = g_new0(MiniTextWindowState, 1);
    state->window = GTK_WINDOW(window);
    state->editor = mini_text_editor_new();
    g_object_set_data_full(
        G_OBJECT(window),
        "mini-text-window-state",
        state,
        on_window_state_free
    );

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *header = adw_header_bar_new();
    GtkWidget *new_button =
        gtk_button_new_from_icon_name("document-new-symbolic");
    GtkWidget *open_button =
        gtk_button_new_from_icon_name("document-open-symbolic");
    GtkWidget *save_button =
        gtk_button_new_from_icon_name("document-save-symbolic");
    GtkWidget *save_as_button =
        gtk_button_new_from_icon_name("document-save-as-symbolic");
    GtkWidget *find_button =
        gtk_button_new_from_icon_name("edit-find-symbolic");
    GtkWidget *go_to_line_button =
        gtk_button_new_from_icon_name("view-list-symbolic");

    set_button_accessible_name(new_button, "Yeni");
    set_button_accessible_name(open_button, "Aç");
    set_button_accessible_name(save_button, "Kaydet");
    set_button_accessible_name(save_as_button, "Farklı Kaydet");
    set_button_accessible_name(find_button, "Bul");
    set_button_accessible_name(go_to_line_button, "Satıra Git");
    gtk_widget_set_tooltip_text(new_button, "Yeni (Ctrl+N)");
    gtk_widget_set_tooltip_text(open_button, "Aç (Ctrl+O)");
    gtk_widget_set_tooltip_text(save_button, "Kaydet (Ctrl+S)");
    gtk_widget_set_tooltip_text(save_as_button, "Farklı Kaydet (Ctrl+Shift+S)");
    gtk_widget_set_tooltip_text(find_button, "Bul (Ctrl+F)");
    gtk_widget_set_tooltip_text(go_to_line_button, "Satıra Git (Ctrl+L)");
    gtk_actionable_set_action_name(GTK_ACTIONABLE(new_button), "app.new");
    gtk_actionable_set_action_name(GTK_ACTIONABLE(open_button), "app.open");
    gtk_actionable_set_action_name(GTK_ACTIONABLE(save_button), "app.save");
    gtk_actionable_set_action_name(
        GTK_ACTIONABLE(save_as_button),
        "app.save-as"
    );
    gtk_actionable_set_action_name(GTK_ACTIONABLE(find_button), "app.find");
    gtk_actionable_set_action_name(
        GTK_ACTIONABLE(go_to_line_button),
        "app.go-to-line"
    );
    adw_header_bar_pack_start(ADW_HEADER_BAR(header), new_button);
    adw_header_bar_pack_start(ADW_HEADER_BAR(header), open_button);
    adw_header_bar_pack_start(ADW_HEADER_BAR(header), find_button);
    adw_header_bar_pack_start(ADW_HEADER_BAR(header), go_to_line_button);
    adw_header_bar_pack_end(ADW_HEADER_BAR(header), save_as_button);
    adw_header_bar_pack_end(ADW_HEADER_BAR(header), save_button);

    state->search_bar = GTK_SEARCH_BAR(gtk_search_bar_new());
    state->search_entry = GTK_SEARCH_ENTRY(gtk_search_entry_new());
    gtk_search_entry_set_placeholder_text(
        state->search_entry,
        "Metinde ara"
    );
    gtk_widget_set_hexpand(GTK_WIDGET(state->search_entry), TRUE);
    GtkWidget *search_controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *previous_button =
        gtk_button_new_from_icon_name("go-previous-symbolic");
    GtkWidget *next_button =
        gtk_button_new_from_icon_name("go-next-symbolic");
    set_button_accessible_name(previous_button, "Önceki eşleşme");
    set_button_accessible_name(next_button, "Sonraki eşleşme");
    gtk_widget_set_tooltip_text(previous_button, "Önceki eşleşme");
    gtk_widget_set_tooltip_text(next_button, "Sonraki eşleşme");
    gtk_box_append(GTK_BOX(search_controls), GTK_WIDGET(state->search_entry));
    gtk_box_append(GTK_BOX(search_controls), previous_button);
    gtk_box_append(GTK_BOX(search_controls), next_button);
    gtk_search_bar_set_child(state->search_bar, search_controls);
    gtk_search_bar_set_show_close_button(state->search_bar, TRUE);
    gtk_search_bar_set_key_capture_widget(state->search_bar, window);
    g_signal_connect(
        state->search_entry,
        "search-changed",
        G_CALLBACK(on_search_changed),
        state
    );
    g_signal_connect(
        previous_button,
        "clicked",
        G_CALLBACK(on_find_previous),
        state
    );
    g_signal_connect(
        next_button,
        "clicked",
        G_CALLBACK(on_find_next),
        state
    );

    gtk_box_append(GTK_BOX(root), header);
    gtk_box_append(GTK_BOX(root), GTK_WIDGET(state->search_bar));
    gtk_box_append(GTK_BOX(root), mini_text_editor_get_widget(state->editor));
    gtk_widget_set_vexpand(mini_text_editor_get_widget(state->editor), TRUE);
    adw_application_window_set_content(ADW_APPLICATION_WINDOW(window), root);

    g_signal_connect(
        mini_text_editor_get_buffer(state->editor),
        "changed",
        G_CALLBACK(on_text_changed),
        state
    );
    g_signal_connect(window, "close-request", G_CALLBACK(on_close_request), NULL);
    update_title(state);
    return GTK_WINDOW(window);
}

void
mini_text_window_activate_action(GtkWindow *window,
                                 MiniTextWindowAction action)
{
    MiniTextWindowState *state = get_state(window);
    if (state == NULL)
        return;

    switch (action) {
    case MINITEXT_WINDOW_ACTION_NEW:
        request_document_action(state, PENDING_ACTION_NEW);
        break;
    case MINITEXT_WINDOW_ACTION_OPEN:
        request_document_action(state, PENDING_ACTION_OPEN);
        break;
    case MINITEXT_WINDOW_ACTION_SAVE:
        if (state->file != NULL)
            finish_save(state, state->file);
        else
            show_save_dialog(state);
        break;
    case MINITEXT_WINDOW_ACTION_SAVE_AS:
        show_save_dialog(state);
        break;
    case MINITEXT_WINDOW_ACTION_FIND:
        gtk_search_bar_set_search_mode(state->search_bar, TRUE);
        gtk_widget_grab_focus(GTK_WIDGET(state->search_entry));
        break;
    case MINITEXT_WINDOW_ACTION_GO_TO_LINE:
        show_go_to_line_dialog(state);
        break;
    }
}
