#include "window.h"

#include "editor.h"
#include "file_manager.h"

#include <adwaita.h>

typedef struct {
    guint ref_count;
    struct _MiniTextWindowState *state;
    MiniTextEditor *editor;
    GFile *file;
    AdwTabPage *page;
    gboolean modified;
    gboolean loading;
} MiniTextDocument;

typedef struct _MiniTextWindowState {
    GtkWindow *window;
    AdwTabView *tab_view;
    AdwTabBar *tab_bar;
    AdwWindowTitle *window_title;
    MiniTextEditor *editor;
    GtkSearchBar *search_bar;
    GtkSearchEntry *search_entry;
    GtkEntry *replace_entry;
    GtkWidget *replace_revealer;
    GtkCheckButton *case_sensitive;
    GtkLabel *search_status;
    GFile *file;
    gboolean modified;
    gboolean loading;
    gboolean closing_window;
    gboolean allow_window_close;
    gboolean close_prompt_active;
    gboolean close_requested_during_prompt;
    GtkCssProvider *drop_css_provider;
} MiniTextWindowState;

typedef struct {
    GtkWindow *window;
    MiniTextDocument *document;
    AdwTabPage *page;
    gboolean window_close;
} MiniTextCloseContext;

typedef struct {
    GtkWindow *window;
    MiniTextDocument *document;
    MiniTextCloseContext *close_context;
} MiniTextSaveContext;

static void continue_window_close(MiniTextWindowState *state);
static void add_file_drop_target(MiniTextWindowState *state, GtkWidget *widget);
static void show_discard_dialog(MiniTextWindowState *state,
                                MiniTextDocument *document,
                                AdwTabPage *page,
                                gboolean window_close);

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

static void
on_error_dialog_response(GObject *source,
                         GAsyncResult *result,
                         gpointer user_data)
{
    (void)user_data;
    (void)adw_alert_dialog_choose_finish(
        ADW_ALERT_DIALOG(source),
        result
    );
}

static void
show_error(MiniTextWindowState *state,
           const char *message,
           const char *detail)
{
    AdwAlertDialog *dialog = ADW_ALERT_DIALOG(
        adw_alert_dialog_new(message, detail)
    );
    adw_alert_dialog_add_response(dialog, "close", "Kapat");
    adw_alert_dialog_set_close_response(dialog, "close");
    adw_alert_dialog_set_default_response(dialog, "close");
    adw_alert_dialog_choose(
        dialog,
        GTK_WIDGET(state->window),
        NULL,
        on_error_dialog_response,
        NULL
    );
    g_object_unref(dialog);
}

static void
set_search_status(MiniTextWindowState *state, const char *message)
{
    gtk_label_set_text(state->search_status, message);
    gtk_widget_set_visible(GTK_WIDGET(state->search_status), message[0] != '\0');
}

static void
update_title(MiniTextWindowState *state)
{
    char *name = state->file != NULL
        ? g_file_get_basename(state->file)
        : g_strdup("Yeni Belge");
    gtk_window_set_title(state->window, name);
    adw_window_title_set_title(state->window_title, name);
    adw_window_title_set_subtitle(
        state->window_title,
        state->modified ? "Değiştirildi" : "MiniText"
    );
    g_free(name);
}

static void
update_tab_bar_visibility(MiniTextWindowState *state)
{
    gtk_widget_set_visible(
        GTK_WIDGET(state->tab_bar),
        adw_tab_view_get_n_pages(state->tab_view) > 1
    );
}

static void
on_tab_view_page_count_changed(GObject *object,
                               GParamSpec *pspec,
                               gpointer user_data)
{
    (void)object;
    (void)pspec;
    update_tab_bar_visibility(user_data);
}

static MiniTextDocument *
document_ref(MiniTextDocument *document)
{
    document->ref_count++;
    return document;
}

static void
document_unref(MiniTextDocument *document)
{
    if (--document->ref_count != 0)
        return;

    g_clear_object(&document->file);
    mini_text_editor_free(document->editor);
    g_free(document);
}

static MiniTextDocument *
get_page_document(AdwTabPage *page)
{
    return g_object_get_data(G_OBJECT(page), "mini-text-document");
}

static MiniTextDocument *
get_active_document(MiniTextWindowState *state)
{
    AdwTabPage *page = adw_tab_view_get_selected_page(state->tab_view);
    return page != NULL ? get_page_document(page) : NULL;
}

static void
update_document_title(MiniTextDocument *document)
{
    char *name = document->file != NULL
        ? g_file_get_basename(document->file)
        : g_strdup("Yeni Belge");
    char *tooltip = document->file != NULL
        ? g_file_get_uri(document->file)
        : g_strdup(name);
    char *title = g_strdup_printf("%s%s",
                                  document->modified ? "*" : "",
                                  name);

    adw_tab_page_set_title(document->page, title);
    adw_tab_page_set_tooltip(document->page, tooltip);
    if (get_active_document(document->state) == document) {
        document->state->editor = document->editor;
        g_set_object(&document->state->file, document->file);
        document->state->modified = document->modified;
        document->state->loading = document->loading;
        update_title(document->state);
    }

    g_free(title);
    g_free(name);
    g_free(tooltip);
}

static void
on_selected_page_changed(GObject *object,
                         GParamSpec *pspec,
                         gpointer user_data)
{
    (void)object;
    (void)pspec;
    MiniTextWindowState *state = user_data;
    MiniTextDocument *document = get_active_document(state);
    state->editor = document != NULL ? document->editor : NULL;
    g_clear_object(&state->file);
    if (document != NULL && document->file != NULL)
        state->file = g_object_ref(document->file);
    state->modified = document != NULL && document->modified;
    state->loading = document != NULL && document->loading;
    update_title(state);
}

static void
on_text_changed(GtkTextBuffer *buffer, gpointer user_data)
{
    (void)buffer;

    MiniTextDocument *document = user_data;
    if (document->loading)
        return;

    document->modified = TRUE;
    update_document_title(document);
}

static gboolean
finish_document_save(MiniTextWindowState *state,
                     MiniTextDocument *document,
                     GFile *file)
{
    GError *error = NULL;
    if (!file_manager_save(
            file,
            mini_text_editor_get_buffer(document->editor),
            &error)) {
        show_error(state, "Dosya kaydedilemedi", error->message);
        g_error_free(error);
        return FALSE;
    }

    g_set_object(&document->file, file);
    document->modified = FALSE;
    update_document_title(document);
    return TRUE;
}

static gboolean
finish_save(MiniTextWindowState *state, GFile *file)
{
    MiniTextDocument *document = get_active_document(state);
    return document != NULL &&
        finish_document_save(state, document, file);
}

static void
free_close_context(MiniTextCloseContext *context)
{
    if (context == NULL)
        return;

    g_clear_object(&context->window);
    document_unref(context->document);
    g_clear_object(&context->page);
    g_free(context);
}

static void
resolve_close_context(MiniTextCloseContext *context, gboolean proceed)
{
    MiniTextWindowState *state = get_state(context->window);
    state->close_prompt_active = FALSE;

    if (context->window_close) {
        if (proceed)
            continue_window_close(state);
        else
            state->closing_window = FALSE;
    } else if (proceed) {
        AdwTabView *view = state->tab_view;
        if (adw_tab_view_get_n_pages(view) <= 1) {
            GtkTextBuffer *buffer = mini_text_editor_get_buffer(
                context->document->editor
            );
            context->document->loading = TRUE;
            gtk_text_buffer_begin_irreversible_action(buffer);
            gtk_text_buffer_set_text(buffer, "", -1);
            gtk_text_buffer_end_irreversible_action(buffer);
            context->document->loading = FALSE;
            g_clear_object(&context->document->file);
            context->document->modified = FALSE;
            update_document_title(context->document);
            adw_tab_view_close_page_finish(view, context->page, FALSE);
        } else {
            adw_tab_view_close_page_finish(view, context->page, TRUE);
        }
    } else if (!context->window_close) {
        adw_tab_view_close_page_finish(
            state->tab_view,
            context->page,
            FALSE
        );
    }

    if (!context->window_close && state->close_requested_during_prompt) {
        state->close_requested_during_prompt = FALSE;
        state->closing_window = TRUE;
        continue_window_close(state);
    }

    free_close_context(context);
}

static void
continue_window_close(MiniTextWindowState *state)
{
    int page_count = adw_tab_view_get_n_pages(state->tab_view);
    for (int position = 0; position < page_count; position++) {
        AdwTabPage *page = adw_tab_view_get_nth_page(
            state->tab_view,
            position
        );
        MiniTextDocument *document = get_page_document(page);
        if (document->modified) {
            show_discard_dialog(state, document, NULL, TRUE);
            return;
        }
    }

    state->allow_window_close = TRUE;
    gtk_window_close(state->window);
}

static void
on_save_response(GObject *source,
                 GAsyncResult *result,
                 gpointer user_data)
{
    MiniTextSaveContext *context = user_data;
    GtkWindow *window = context->window;
    MiniTextWindowState *state = get_state(window);
    GError *error = NULL;
    GFile *file = gtk_file_dialog_save_finish(
        GTK_FILE_DIALOG(source),
        result,
        &error
    );

    if (file != NULL) {
        gboolean saved = finish_document_save(
            state,
            context->document,
            file
        );
        g_object_unref(file);
        if (context->close_context != NULL) {
            MiniTextCloseContext *close_context = context->close_context;
            context->close_context = NULL;
            resolve_close_context(close_context, saved);
        }
    } else if (error != NULL &&
               !g_error_matches(
                   error,
                   GTK_DIALOG_ERROR,
                   GTK_DIALOG_ERROR_DISMISSED)) {
        show_error(state, "Kaydetme hatası", error->message);
    }

    if (file == NULL && context->close_context != NULL) {
        MiniTextCloseContext *close_context = context->close_context;
        context->close_context = NULL;
        resolve_close_context(close_context, FALSE);
    }
    g_clear_error(&error);
    g_object_unref(context->window);
    document_unref(context->document);
    free_close_context(context->close_context);
    g_free(context);
}

static void
show_save_dialog_for_document(MiniTextWindowState *state,
                              MiniTextDocument *document,
                              MiniTextCloseContext *close_context)
{
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Dosyayı Kaydet");

    char *initial_name = document->file != NULL
        ? g_file_get_basename(document->file)
        : g_strdup("belge.txt");
    gtk_file_dialog_set_initial_name(dialog, initial_name);
    g_free(initial_name);

    if (document->file != NULL)
        gtk_file_dialog_set_initial_file(dialog, document->file);

    MiniTextSaveContext *context = g_new0(MiniTextSaveContext, 1);
    context->window = g_object_ref(state->window);
    context->document = document_ref(document);
    context->close_context = close_context;

    gtk_file_dialog_save(
        dialog,
        state->window,
        NULL,
        on_save_response,
        context
    );
    g_object_unref(dialog);
}

static void
show_save_dialog(MiniTextWindowState *state)
{
    MiniTextDocument *document = get_active_document(state);
    if (document != NULL)
        show_save_dialog_for_document(state, document, NULL);
}

static MiniTextDocument *
create_document(MiniTextWindowState *state)
{
    MiniTextDocument *document = g_new0(MiniTextDocument, 1);
    document->ref_count = 1;
    document->state = state;
    document->editor = mini_text_editor_new();
    gtk_text_buffer_set_enable_undo(
        mini_text_editor_get_buffer(document->editor),
        TRUE
    );
    g_signal_connect(
        mini_text_editor_get_buffer(document->editor),
        "changed",
        G_CALLBACK(on_text_changed),
        document
    );
    add_file_drop_target(
        state,
        mini_text_editor_get_widget(document->editor)
    );
    return document;
}

static void
add_document(MiniTextWindowState *state, MiniTextDocument *document)
{
    GtkWidget *child = mini_text_editor_get_widget(document->editor);
    document->page = adw_tab_view_append(state->tab_view, child);
    g_object_set_data_full(
        G_OBJECT(document->page),
        "mini-text-document",
        document,
        (GDestroyNotify)document_unref
    );
    adw_tab_view_set_selected_page(state->tab_view, document->page);
    update_document_title(document);
}

static MiniTextDocument *
find_document_for_file(MiniTextWindowState *state, GFile *file)
{
    int page_count = adw_tab_view_get_n_pages(state->tab_view);
    for (int position = 0; position < page_count; position++) {
        AdwTabPage *page = adw_tab_view_get_nth_page(
            state->tab_view,
            position
        );
        MiniTextDocument *document = get_page_document(page);
        if (document->file != NULL && g_file_equal(document->file, file))
            return document;
    }

    return NULL;
}

static MiniTextDocument *
get_reusable_empty_document(MiniTextWindowState *state)
{
    MiniTextDocument *document = get_active_document(state);
    if (document == NULL || document->file != NULL || document->modified)
        return NULL;

    GtkTextBuffer *buffer = mini_text_editor_get_buffer(document->editor);
    return gtk_text_buffer_get_char_count(buffer) == 0 ? document : NULL;
}

static gboolean
open_file_in_tab(MiniTextWindowState *state,
                 GFile *file,
                 gboolean reuse_empty_document)
{
    MiniTextDocument *existing = find_document_for_file(state, file);
    if (existing != NULL) {
        adw_tab_view_set_selected_page(state->tab_view, existing->page);
        return TRUE;
    }

    MiniTextDocument *empty_document = reuse_empty_document
        ? get_reusable_empty_document(state)
        : NULL;
    MiniTextDocument *document = create_document(state);
    GError *error = NULL;
    document->loading = TRUE;
    gboolean loaded = file_manager_load(
        file,
        mini_text_editor_get_buffer(document->editor),
        &error
    );
    document->loading = FALSE;
    if (!loaded) {
    const char *message = error != NULL
        ? error->message
        : "Dosya açılamadı. Dosya mevcut olmayabilir veya erişilemiyor olabilir.";

    show_error(state, "Dosya açılamadı", message);
    g_clear_error(&error);
    document_unref(document);
    return FALSE;
}

    if (empty_document != NULL) {
        GtkTextBuffer *loaded_buffer = mini_text_editor_get_buffer(
            document->editor
        );
        GtkTextIter start;
        GtkTextIter end;
        gtk_text_buffer_get_bounds(loaded_buffer, &start, &end);
        char *contents = gtk_text_buffer_get_text(
            loaded_buffer,
            &start,
            &end,
            FALSE
        );

        GtkTextBuffer *empty_buffer = mini_text_editor_get_buffer(
            empty_document->editor
        );
        empty_document->loading = TRUE;
        gtk_text_buffer_begin_irreversible_action(empty_buffer);
        gtk_text_buffer_set_text(empty_buffer, contents, -1);
        gtk_text_buffer_end_irreversible_action(empty_buffer);
        empty_document->loading = FALSE;
        g_set_object(&empty_document->file, file);
        empty_document->modified = FALSE;
        update_document_title(empty_document);

        g_free(contents);
        document_unref(document);
        return TRUE;
    }

    g_set_object(&document->file, file);
    add_document(state, document);
    return TRUE;
}

gboolean
mini_text_window_open_file(GtkWindow *window, GFile *file)
{
    MiniTextWindowState *state = get_state(window);
    return state != NULL && open_file_in_tab(state, file, TRUE);
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
        mini_text_window_open_file(window, file);
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
on_discard_response(GObject *source,
                    GAsyncResult *result,
                    gpointer user_data)
{
    MiniTextCloseContext *context = user_data;
    MiniTextWindowState *state = get_state(context->window);
    const char *response = adw_alert_dialog_choose_finish(
        ADW_ALERT_DIALOG(source),
        result
    );

    if (g_strcmp0(response, "cancel") == 0) {
        resolve_close_context(context, FALSE);
    } else if (g_strcmp0(response, "save") == 0) {
        if (context->document->file != NULL) {
            gboolean saved = finish_document_save(
                state,
                context->document,
                context->document->file
            );
            resolve_close_context(context, saved);
        } else {
            show_save_dialog_for_document(
                state,
                context->document,
                context
            );
        }
    } else if (g_strcmp0(response, "discard") == 0) {
        resolve_close_context(context, TRUE);
    } else {
        resolve_close_context(context, FALSE);
    }

}

static void
show_discard_dialog(MiniTextWindowState *state,
                    MiniTextDocument *document,
                    AdwTabPage *page,
                    gboolean window_close)
{
    state->close_prompt_active = TRUE;
    MiniTextCloseContext *context = g_new0(MiniTextCloseContext, 1);
    context->window = g_object_ref(state->window);
    context->document = document_ref(document);
    context->page = page != NULL ? g_object_ref(page) : NULL;
    context->window_close = window_close;

    AdwAlertDialog *dialog = ADW_ALERT_DIALOG(
        adw_alert_dialog_new(
            "Kaydedilmemiş Değişiklikler",
            "Bu belgeyi kaydetmek istiyor musunuz?"
        )
    );
    adw_alert_dialog_add_responses(
        dialog,
        "cancel", "İptal",
        "save", "Kaydet",
        "discard", "Kaydetmeden Kapat",
        NULL
    );
    adw_alert_dialog_set_response_appearance(
        dialog,
        "discard",
        ADW_RESPONSE_DESTRUCTIVE
    );
    adw_alert_dialog_set_close_response(dialog, "cancel");
    adw_alert_dialog_set_default_response(dialog, "save");
    adw_alert_dialog_choose(
        dialog,
        GTK_WIDGET(state->window),
        NULL,
        on_discard_response,
        context
    );
    g_object_unref(dialog);
}

static gboolean
on_tab_close_page(AdwTabView *view,
                  AdwTabPage *page,
                  gpointer user_data)
{
    MiniTextWindowState *state = user_data;
    if (state->close_prompt_active) {
        adw_tab_view_close_page_finish(view, page, FALSE);
        return GDK_EVENT_STOP;
    }

    MiniTextDocument *document = get_page_document(page);
    if (document->modified) {
        show_discard_dialog(state, document, page, FALSE);
    } else if (adw_tab_view_get_n_pages(view) > 1) {
        adw_tab_view_close_page_finish(view, page, TRUE);
    } else {
        GtkTextBuffer *buffer = mini_text_editor_get_buffer(document->editor);
        document->loading = TRUE;
        gtk_text_buffer_begin_irreversible_action(buffer);
        gtk_text_buffer_set_text(buffer, "", -1);
        gtk_text_buffer_end_irreversible_action(buffer);
        document->loading = FALSE;
        g_clear_object(&document->file);
        document->modified = FALSE;
        update_document_title(document);
        adw_tab_view_close_page_finish(view, page, FALSE);
    }

    return GDK_EVENT_STOP;
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
        set_search_status(state, "");
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
    GtkTextSearchFlags flags = gtk_check_button_get_active(
        state->case_sensitive
    ) ? (GtkTextSearchFlags)0 : GTK_TEXT_SEARCH_CASE_INSENSITIVE;
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
        set_search_status(state, "Eşleşme bulunamadı");
        return FALSE;
    }

    gtk_widget_remove_css_class(GTK_WIDGET(state->search_entry), "error");
    set_search_status(state, "");
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

static gboolean
on_search_entry_key_pressed(GtkEventControllerKey *controller,
                            guint keyval,
                            guint keycode,
                            GdkModifierType state_flags,
                            gpointer user_data)
{
    (void)controller;
    (void)keycode;

    MiniTextWindowState *state = user_data;
    if (keyval == GDK_KEY_Escape) {
        gtk_search_bar_set_search_mode(state->search_bar, FALSE);
        gtk_widget_grab_focus(
            GTK_WIDGET(mini_text_editor_get_text_view(state->editor))
        );
        return TRUE;
    }

    if (keyval != GDK_KEY_Return && keyval != GDK_KEY_KP_Enter)
        return FALSE;

    if ((state_flags & GDK_SHIFT_MASK) != 0)
        search_document(state, TRUE, FALSE);
    else
        search_document(state, FALSE, FALSE);
    return TRUE;
}

static gboolean
text_matches_query(const char *text,
                   const char *query,
                   gboolean case_sensitive)
{
    if (case_sensitive)
        return g_strcmp0(text, query) == 0;

    char *folded_text = g_utf8_casefold(text, -1);
    char *folded_query = g_utf8_casefold(query, -1);
    gboolean matches = g_strcmp0(folded_text, folded_query) == 0;
    g_free(folded_text);
    g_free(folded_query);
    return matches;
}

static void
on_replace_current(GtkButton *button, gpointer user_data)
{
    (void)button;
    MiniTextWindowState *state = user_data;
    const char *query = gtk_editable_get_text(
        GTK_EDITABLE(state->search_entry)
    );
    if (query[0] == '\0')
        return;

    GtkTextBuffer *buffer = mini_text_editor_get_buffer(state->editor);
    GtkTextIter start;
    GtkTextIter end;
    gboolean has_selection = gtk_text_buffer_get_selection_bounds(
        buffer,
        &start,
        &end
    );
    gboolean case_sensitive = gtk_check_button_get_active(
        state->case_sensitive
    );
    char *selected_text = has_selection
        ? gtk_text_buffer_get_text(buffer, &start, &end, FALSE)
        : NULL;
    gboolean current_match = selected_text != NULL &&
        text_matches_query(selected_text, query, case_sensitive);
    g_free(selected_text);

    if (!current_match) {
        if (!search_document(state, FALSE, FALSE))
            return;
        gtk_text_buffer_get_selection_bounds(buffer, &start, &end);
    }

    const char *replacement = gtk_editable_get_text(
        GTK_EDITABLE(state->replace_entry)
    );
    gtk_text_buffer_begin_user_action(buffer);
    gtk_text_buffer_delete(buffer, &start, &end);
    gtk_text_buffer_insert(buffer, &start, replacement, -1);
    gtk_text_buffer_end_user_action(buffer);

    if (!search_document(state, FALSE, FALSE))
        set_search_status(state, "Eşleşme değiştirildi; başka eşleşme yok");
}

static void
on_replace_all(GtkButton *button, gpointer user_data)
{
    (void)button;
    MiniTextWindowState *state = user_data;
    const char *query = gtk_editable_get_text(
        GTK_EDITABLE(state->search_entry)
    );
    if (query[0] == '\0')
        return;

    const char *replacement = gtk_editable_get_text(
        GTK_EDITABLE(state->replace_entry)
    );
    GtkTextBuffer *buffer = mini_text_editor_get_buffer(state->editor);
    GtkTextSearchFlags flags = gtk_check_button_get_active(
        state->case_sensitive
    ) ? (GtkTextSearchFlags)0 : GTK_TEXT_SEARCH_CASE_INSENSITIVE;
    GtkTextIter iter;
    gtk_text_buffer_get_start_iter(buffer, &iter);
    guint replaced_count = 0;

    gtk_text_buffer_begin_user_action(buffer);
    GtkTextIter match_start;
    GtkTextIter match_end;
    while (gtk_text_iter_forward_search(
        &iter,
        query,
        flags,
        &match_start,
        &match_end,
        NULL
    )) {
        gtk_text_buffer_delete(buffer, &match_start, &match_end);
        gtk_text_buffer_insert(buffer, &match_start, replacement, -1);
        iter = match_start;
        replaced_count++;
    }
    gtk_text_buffer_end_user_action(buffer);

    gtk_widget_remove_css_class(GTK_WIDGET(state->search_entry), "error");
    if (replaced_count == 0) {
        set_search_status(state, "Eşleşme bulunamadı");
    } else {
        char *message = g_strdup_printf(
            "%u eşleşme değiştirildi",
            replaced_count
        );
        set_search_status(state, message);
        g_free(message);
    }
}

static void
on_case_sensitive_toggled(GtkCheckButton *button, gpointer user_data)
{
    (void)button;
    MiniTextWindowState *state = user_data;
    search_document(state, FALSE, TRUE);
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

static gboolean
on_go_to_line_key_pressed(GtkEventControllerKey *controller,
                          guint keyval,
                          guint keycode,
                          GdkModifierType state_flags,
                          gpointer user_data)
{
    (void)controller;
    (void)keycode;
    (void)state_flags;

    if (keyval != GDK_KEY_Escape)
        return FALSE;

    gtk_window_destroy(GTK_WINDOW(user_data));
    return TRUE;
}

static void
show_keyboard_shortcuts(MiniTextWindowState *state)
{
    static const char shortcuts_ui[] =
        "<interface>"
        "  <object class='GtkShortcutsWindow' id='shortcuts'>"
        "    <property name='title'>Klavye Kısayolları</property>"
        "    <property name='modal'>1</property>"
        "    <property name='default-width'>650</property>"
        "    <child>"
        "      <object class='GtkShortcutsSection'>"
        "        <property name='section-name'>shortcuts</property>"
        "        <property name='max-height'>12</property>"
        "        <child>"
        "          <object class='GtkShortcutsGroup'>"
        "            <property name='title'>Dosya ve Sekmeler</property>"
        "            <child><object class='GtkShortcutsShortcut'>"
        "              <property name='accelerator'>&lt;Control&gt;n</property>"
        "              <property name='title'>Yeni sekme</property>"
        "            </object></child>"
        "            <child><object class='GtkShortcutsShortcut'>"
        "              <property name='accelerator'>&lt;Control&gt;o</property>"
        "              <property name='title'>Dosya aç</property>"
        "            </object></child>"
        "            <child><object class='GtkShortcutsShortcut'>"
        "              <property name='accelerator'>&lt;Control&gt;s</property>"
        "              <property name='title'>Kaydet</property>"
        "            </object></child>"
        "            <child><object class='GtkShortcutsShortcut'>"
        "              <property name='accelerator'>&lt;Control&gt;&lt;Shift&gt;s</property>"
        "              <property name='title'>Farklı kaydet</property>"
        "            </object></child>"
        "            <child><object class='GtkShortcutsShortcut'>"
        "              <property name='accelerator'>&lt;Control&gt;w</property>"
        "              <property name='title'>Sekmeyi kapat</property>"
        "            </object></child>"
        "            <child><object class='GtkShortcutsShortcut'>"
        "              <property name='accelerator'>&lt;Control&gt;Tab</property>"
        "              <property name='title'>Sonraki sekme</property>"
        "            </object></child>"
        "            <child><object class='GtkShortcutsShortcut'>"
        "              <property name='accelerator'>&lt;Control&gt;&lt;Shift&gt;Tab</property>"
        "              <property name='title'>Önceki sekme</property>"
        "            </object></child>"
        "            <child><object class='GtkShortcutsShortcut'>"
        "              <property name='accelerator'>&lt;Control&gt;1...&lt;Control&gt;9</property>"
        "              <property name='title'>Sekme 1-9</property>"
        "            </object></child>"
        "          </object>"
        "        </child>"
        "        <child>"
        "          <object class='GtkShortcutsGroup'>"
        "            <property name='title'>Düzenleme</property>"
        "            <child><object class='GtkShortcutsShortcut'>"
        "              <property name='accelerator'>&lt;Control&gt;z</property>"
        "              <property name='title'>Geri al</property>"
        "            </object></child>"
        "            <child><object class='GtkShortcutsShortcut'>"
        "              <property name='accelerator'>&lt;Control&gt;&lt;Shift&gt;z</property>"
        "              <property name='title'>Yinele</property>"
        "            </object></child>"
        "            <child><object class='GtkShortcutsShortcut'>"
        "              <property name='accelerator'>&lt;Control&gt;y</property>"
        "              <property name='title'>Yinele</property>"
        "            </object></child>"
        "            <child><object class='GtkShortcutsShortcut'>"
        "              <property name='accelerator'>&lt;Control&gt;l</property>"
        "              <property name='title'>Satıra git</property>"
        "            </object></child>"
        "          </object>"
        "        </child>"
        "        <child>"
        "          <object class='GtkShortcutsGroup'>"
        "            <property name='title'>Bul ve Yardım</property>"
        "            <child><object class='GtkShortcutsShortcut'>"
        "              <property name='accelerator'>&lt;Control&gt;f</property>"
        "              <property name='title'>Bul</property>"
        "            </object></child>"
        "            <child><object class='GtkShortcutsShortcut'>"
        "              <property name='accelerator'>&lt;Control&gt;h</property>"
        "              <property name='title'>Bul ve değiştir</property>"
        "            </object></child>"
        "            <child><object class='GtkShortcutsShortcut'>"
        "              <property name='accelerator'>&lt;Control&gt;question</property>"
        "              <property name='title'>Klavye kısayolları</property>"
        "            </object></child>"
        "            <child><object class='GtkShortcutsShortcut'>"
        "              <property name='accelerator'>&lt;Control&gt;comma</property>"
        "              <property name='title'>Hakkında</property>"
        "            </object></child>"
        "          </object>"
        "        </child>"
        "      </object>"
        "    </child>"
        "  </object>"
        "</interface>";

    GtkBuilder *builder = gtk_builder_new();
    GError *error = NULL;
    if (!gtk_builder_add_from_string(
            builder,
            shortcuts_ui,
            sizeof(shortcuts_ui) - 1,
            &error)) {
        show_error(state, "Klavye kısayolları açılamadı", error->message);
        g_clear_error(&error);
        g_object_unref(builder);
        return;
    }

    GtkWindow *shortcuts = GTK_WINDOW(
        g_object_ref(gtk_builder_get_object(builder, "shortcuts"))
    );
    g_object_unref(builder);
    gtk_window_set_transient_for(shortcuts, state->window);
    gtk_window_present(shortcuts);
}

static GdkDragAction
on_drop_enter(GtkDropTarget *target,
              double x,
              double y,
              gpointer user_data)
{
    (void)target;
    (void)x;
    (void)y;
    gtk_widget_add_css_class(GTK_WIDGET(user_data), "drop-active");
    return GDK_ACTION_COPY;
}

static void
on_drop_leave(GtkDropTarget *target, gpointer user_data)
{
    (void)target;
    gtk_widget_remove_css_class(GTK_WIDGET(user_data), "drop-active");
}

static gboolean
on_files_dropped(GtkDropTarget *target,
                 const GValue *value,
                 double x,
                 double y,
                 gpointer user_data)
{
    (void)target;
    (void)x;
    (void)y;

    MiniTextWindowState *state = user_data;
    GdkFileList *file_list = g_value_get_boxed(value);
    GSList *files = gdk_file_list_get_files(file_list);
    gtk_widget_remove_css_class(
        gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(target)),
        "drop-active"
    );
    gboolean handled = FALSE;
    for (GSList *item = files; item != NULL; item = item->next) {
        open_file_in_tab(state, G_FILE(item->data), FALSE);
        handled = TRUE;
    }
    return handled;
}

static void
add_file_drop_target(MiniTextWindowState *state, GtkWidget *widget)
{
    GtkDropTarget *target = gtk_drop_target_new(
        GDK_TYPE_FILE_LIST,
        GDK_ACTION_COPY
    );
    gtk_drop_target_set_preload(target, TRUE);
    g_signal_connect(
        target,
        "enter",
        G_CALLBACK(on_drop_enter),
        widget
    );
    g_signal_connect(
        target,
        "leave",
        G_CALLBACK(on_drop_leave),
        widget
    );
    g_signal_connect(
        target,
        "drop",
        G_CALLBACK(on_files_dropped),
        state
    );
    gtk_widget_add_controller(widget, GTK_EVENT_CONTROLLER(target));
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

    GtkWidget *header = adw_header_bar_new();
    GtkWidget *title = adw_window_title_new("Satıra Git", NULL);
    adw_header_bar_set_title_widget(ADW_HEADER_BAR(header), title);
    gtk_window_set_titlebar(dialog, header);

    GtkWidget *body = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(body, 18);
    gtk_widget_set_margin_bottom(body, 18);
    gtk_widget_set_margin_start(body, 18);
    gtk_widget_set_margin_end(body, 18);
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *label = gtk_label_new_with_mnemonic("_Satır numarası:");
    GtkWidget *spin_button = gtk_spin_button_new_with_range(
        1,
        line_count,
        1
    );
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), spin_button);
    gtk_spin_button_set_value(
        GTK_SPIN_BUTTON(spin_button),
        MIN(current_line, line_count)
    );
    gtk_box_append(GTK_BOX(row), label);
    gtk_box_append(GTK_BOX(row), spin_button);
    GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *cancel_button = gtk_button_new_with_label("İptal");
    GtkWidget *go_button = gtk_button_new_with_label("Git");
    gtk_widget_add_css_class(go_button, "suggested-action");
    gtk_widget_set_halign(buttons, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(buttons), cancel_button);
    gtk_box_append(GTK_BOX(buttons), go_button);
    gtk_box_append(GTK_BOX(body), row);
    gtk_box_append(GTK_BOX(body), buttons);
    gtk_window_set_child(dialog, body);

    gtk_window_set_default_widget(dialog, go_button);
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
    GtkEventController *keys = gtk_event_controller_key_new();
    g_signal_connect(
        keys,
        "key-pressed",
        G_CALLBACK(on_go_to_line_key_pressed),
        dialog
    );
    gtk_widget_add_controller(GTK_WIDGET(dialog), keys);
    gtk_window_present(dialog);
    gtk_widget_grab_focus(spin_button);
}

static gboolean
on_close_request(GtkWindow *window, gpointer user_data)
{
    (void)user_data;

    MiniTextWindowState *state = get_state(window);
    if (state->allow_window_close)
        return FALSE;

    if (state->close_prompt_active) {
        if (!state->closing_window)
            state->close_requested_during_prompt = TRUE;
        return TRUE;
    }

    if (!state->closing_window) {
        state->closing_window = TRUE;
        continue_window_close(state);
    }
    return TRUE;
}

static void
on_window_state_free(gpointer user_data)
{
    MiniTextWindowState *state = user_data;
    g_clear_object(&state->file);
    if (state->drop_css_provider != NULL) {
        gtk_style_context_remove_provider_for_display(
            gdk_display_get_default(),
            GTK_STYLE_PROVIDER(state->drop_css_provider)
        );
        g_clear_object(&state->drop_css_provider);
    }
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
    state->tab_view = ADW_TAB_VIEW(adw_tab_view_new());
    state->tab_bar = ADW_TAB_BAR(adw_tab_bar_new());
    g_object_set_data_full(
        G_OBJECT(window),
        "mini-text-window-state",
        state,
        on_window_state_free
    );

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    state->drop_css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(
        state->drop_css_provider,
        ".drop-active { box-shadow: inset 0 0 0 2px @accent_bg_color; }"
    );
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(state->drop_css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    add_file_drop_target(state, root);

    GtkWidget *header = adw_header_bar_new();
    GtkWidget *open_button =
        gtk_button_new_from_icon_name("document-open-symbolic");
    GtkWidget *save_button =
        gtk_button_new_from_icon_name("document-save-symbolic");
    GtkWidget *find_button =
        gtk_button_new_from_icon_name("edit-find-symbolic");
    set_button_accessible_name(open_button, "Aç");
    set_button_accessible_name(save_button, "Kaydet");
    set_button_accessible_name(find_button, "Bul");
    gtk_widget_set_tooltip_text(open_button, "Aç (Ctrl+O)");
    gtk_widget_set_tooltip_text(save_button, "Kaydet (Ctrl+S)");
    gtk_widget_set_tooltip_text(find_button, "Bul (Ctrl+F)");
    gtk_actionable_set_action_name(GTK_ACTIONABLE(open_button), "app.open");
    gtk_actionable_set_action_name(GTK_ACTIONABLE(save_button), "app.save");
    gtk_actionable_set_action_name(GTK_ACTIONABLE(find_button), "app.find");
    state->window_title = ADW_WINDOW_TITLE(
        adw_window_title_new("Yeni Belge", "MiniText")
    );
    adw_header_bar_pack_start(ADW_HEADER_BAR(header), open_button);
    adw_header_bar_pack_start(ADW_HEADER_BAR(header), find_button);
    adw_header_bar_set_title_widget(
        ADW_HEADER_BAR(header),
        GTK_WIDGET(state->window_title)
    );
    adw_header_bar_pack_end(ADW_HEADER_BAR(header), save_button);

    state->search_bar = GTK_SEARCH_BAR(gtk_search_bar_new());
    state->search_entry = GTK_SEARCH_ENTRY(gtk_search_entry_new());
    gtk_search_entry_set_placeholder_text(
        state->search_entry,
        "Metinde ara"
    );
    gtk_widget_set_hexpand(GTK_WIDGET(state->search_entry), TRUE);
    GtkWidget *search_content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
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
    gtk_box_append(GTK_BOX(search_content), search_controls);

    state->replace_revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(
        GTK_REVEALER(state->replace_revealer),
        GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN
    );
    GtkWidget *replace_controls = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    state->replace_entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(state->replace_entry, "Yeni metin");
    gtk_widget_set_hexpand(GTK_WIDGET(state->replace_entry), TRUE);
    GtkWidget *replace_button_current =
        gtk_button_new_with_label("Değiştir");
    GtkWidget *replace_button_all =
        gtk_button_new_with_label("Tümünü Değiştir");
    GtkWidget *replace_buttons = gtk_box_new(
        GTK_ORIENTATION_HORIZONTAL,
        6
    );
    gtk_widget_set_halign(replace_buttons, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(replace_controls), GTK_WIDGET(state->replace_entry));
    gtk_box_append(GTK_BOX(replace_buttons), replace_button_current);
    gtk_box_append(GTK_BOX(replace_buttons), replace_button_all);
    gtk_box_append(GTK_BOX(replace_controls), replace_buttons);
    GtkWidget *replace_content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_append(GTK_BOX(replace_content), replace_controls);
    state->case_sensitive = GTK_CHECK_BUTTON(
        gtk_check_button_new_with_label("Büyük/küçük harfe duyarlı")
    );
    gtk_box_append(GTK_BOX(replace_content), GTK_WIDGET(state->case_sensitive));
    gtk_revealer_set_child(
        GTK_REVEALER(state->replace_revealer),
        replace_content
    );
    gtk_box_append(
        GTK_BOX(search_content),
        state->replace_revealer
    );
    state->search_status = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(state->search_status, 0.0f);
    gtk_widget_add_css_class(GTK_WIDGET(state->search_status), "dim-label");
    gtk_widget_set_visible(GTK_WIDGET(state->search_status), FALSE);
    gtk_box_append(GTK_BOX(search_content), GTK_WIDGET(state->search_status));
    gtk_widget_set_margin_top(search_content, 6);
    gtk_widget_set_margin_bottom(search_content, 6);
    gtk_widget_set_margin_start(search_content, 12);
    gtk_widget_set_margin_end(search_content, 12);
    gtk_search_bar_set_child(state->search_bar, search_content);
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
    GtkEventController *search_keys = gtk_event_controller_key_new();
    g_signal_connect(
        search_keys,
        "key-pressed",
        G_CALLBACK(on_search_entry_key_pressed),
        state
    );
    gtk_widget_add_controller(
        GTK_WIDGET(state->search_entry),
        search_keys
    );
    GtkEventController *replace_keys = gtk_event_controller_key_new();
    g_signal_connect(
        replace_keys,
        "key-pressed",
        G_CALLBACK(on_search_entry_key_pressed),
        state
    );
    gtk_widget_add_controller(
        GTK_WIDGET(state->replace_entry),
        replace_keys
    );
    g_signal_connect(
        replace_button_current,
        "clicked",
        G_CALLBACK(on_replace_current),
        state
    );
    g_signal_connect(
        replace_button_all,
        "clicked",
        G_CALLBACK(on_replace_all),
        state
    );
    g_signal_connect(
        state->case_sensitive,
        "toggled",
        G_CALLBACK(on_case_sensitive_toggled),
        state
    );

    gtk_box_append(GTK_BOX(root), header);
    adw_tab_bar_set_view(state->tab_bar, state->tab_view);
    adw_tab_bar_set_autohide(state->tab_bar, TRUE);
    add_file_drop_target(state, GTK_WIDGET(state->tab_bar));
    GtkWidget *new_button = gtk_button_new_from_icon_name("list-add-symbolic");
    gtk_widget_add_css_class(new_button, "flat");
    gtk_widget_set_tooltip_text(new_button, "Yeni Sekme (Ctrl+N)");
    set_button_accessible_name(new_button, "Yeni Sekme");
    gtk_actionable_set_action_name(GTK_ACTIONABLE(new_button), "app.new");
    adw_tab_bar_set_end_action_widget(state->tab_bar, new_button);
    gtk_box_append(GTK_BOX(root), GTK_WIDGET(state->tab_bar));
    gtk_box_append(GTK_BOX(root), GTK_WIDGET(state->search_bar));
    gtk_box_append(GTK_BOX(root), GTK_WIDGET(state->tab_view));
    gtk_widget_set_vexpand(GTK_WIDGET(state->tab_view), TRUE);
    adw_application_window_set_content(ADW_APPLICATION_WINDOW(window), root);

    g_signal_connect(
        state->tab_view,
        "close-page",
        G_CALLBACK(on_tab_close_page),
        state
    );
    g_signal_connect(
        state->tab_view,
        "notify::selected-page",
        G_CALLBACK(on_selected_page_changed),
        state
    );
    g_signal_connect(
        state->tab_view,
        "notify::n-pages",
        G_CALLBACK(on_tab_view_page_count_changed),
        state
    );
    g_signal_connect(window, "close-request", G_CALLBACK(on_close_request), NULL);
    add_document(state, create_document(state));
    update_tab_bar_visibility(state);
    update_title(state);

    // Hamburger menü
    GMenu *menu = g_menu_new();
    GMenu *document_menu = g_menu_new();
    GMenu *help_menu = g_menu_new();
    g_menu_append(document_menu, "Bul ve Değiştir", "app.replace");
    g_menu_append(document_menu, "Satıra Git", "app.go-to-line");
    g_menu_append(document_menu, "Farklı Kaydet", "app.save-as");
    g_menu_append(help_menu, "Klavye Kısayolları", "app.shortcuts");
    g_menu_append(help_menu, "MiniText Hakkında", "app.about");
    g_menu_append_section(menu, NULL, G_MENU_MODEL(document_menu));
    g_menu_append_section(menu, NULL, G_MENU_MODEL(help_menu));

    GtkWidget *menu_button = gtk_menu_button_new();

    gtk_menu_button_set_icon_name(
        GTK_MENU_BUTTON(menu_button),
        "open-menu-symbolic"
    );
    gtk_widget_set_tooltip_text(menu_button, "Ana Menü");
    set_button_accessible_name(menu_button, "Ana Menü");

    gtk_menu_button_set_menu_model(
        GTK_MENU_BUTTON(menu_button),
        G_MENU_MODEL(menu)
    );

    adw_header_bar_pack_end(
        ADW_HEADER_BAR(header),
        menu_button
    );

    g_object_unref(menu);
    g_object_unref(document_menu);
    g_object_unref(help_menu);

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
        add_document(state, create_document(state));
        break;
    case MINITEXT_WINDOW_ACTION_OPEN:
        show_open_dialog(state);
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
        gtk_revealer_set_reveal_child(
            GTK_REVEALER(state->replace_revealer),
            FALSE
        );
        gtk_search_bar_set_search_mode(state->search_bar, TRUE);
        gtk_widget_grab_focus(GTK_WIDGET(state->search_entry));
        break;
    case MINITEXT_WINDOW_ACTION_REPLACE:
        gtk_revealer_set_reveal_child(
            GTK_REVEALER(state->replace_revealer),
            TRUE
        );
        gtk_search_bar_set_search_mode(state->search_bar, TRUE);
        gtk_widget_grab_focus(GTK_WIDGET(state->search_entry));
        break;
    case MINITEXT_WINDOW_ACTION_UNDO:
        gtk_text_buffer_undo(mini_text_editor_get_buffer(state->editor));
        break;
    case MINITEXT_WINDOW_ACTION_REDO:
        gtk_text_buffer_redo(mini_text_editor_get_buffer(state->editor));
        break;
    case MINITEXT_WINDOW_ACTION_GO_TO_LINE:
        show_go_to_line_dialog(state);
        break;
    case MINITEXT_WINDOW_ACTION_SHORTCUTS:
        show_keyboard_shortcuts(state);
        break;
    case MINITEXT_WINDOW_ACTION_CLOSE_TAB: {
        AdwTabPage *page = adw_tab_view_get_selected_page(state->tab_view);
        if (page != NULL)
            adw_tab_view_close_page(state->tab_view, page);
        break;
    }
    case MINITEXT_WINDOW_ACTION_NEXT_TAB: {
        int count = adw_tab_view_get_n_pages(state->tab_view);
        if (count > 1) {
            AdwTabPage *page = adw_tab_view_get_selected_page(state->tab_view);
            int position = adw_tab_view_get_page_position(
                state->tab_view,
                page
            );
            int next = (position + 1) % count;
            adw_tab_view_set_selected_page(
                state->tab_view,
                adw_tab_view_get_nth_page(state->tab_view, next)
            );
        }
        break;
    }
    case MINITEXT_WINDOW_ACTION_PREVIOUS_TAB: {
        int count = adw_tab_view_get_n_pages(state->tab_view);
        if (count > 1) {
            AdwTabPage *page = adw_tab_view_get_selected_page(state->tab_view);
            int position = adw_tab_view_get_page_position(
                state->tab_view,
                page
            );
            int previous = (position + count - 1) % count;
            adw_tab_view_set_selected_page(
                state->tab_view,
                adw_tab_view_get_nth_page(state->tab_view, previous)
            );
        }
        break;
    }
    default:
        if (action >= MINITEXT_WINDOW_ACTION_SELECT_TAB_1 &&
            action <= MINITEXT_WINDOW_ACTION_SELECT_TAB_9) {
            int position = action - MINITEXT_WINDOW_ACTION_SELECT_TAB_1;
            if (position < adw_tab_view_get_n_pages(state->tab_view)) {
                adw_tab_view_set_selected_page(
                    state->tab_view,
                    adw_tab_view_get_nth_page(state->tab_view, position)
                );
            }
        }
        break;
    }
}
