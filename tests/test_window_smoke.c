#include "window.h"

#include <adwaita.h>
#include <glib/gstdio.h>
#include <string.h>

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

static AdwTabView *
find_tab_view(GtkWidget *widget)
{
    if (ADW_IS_TAB_VIEW(widget))
        return ADW_TAB_VIEW(widget);

    for (GtkWidget *child = gtk_widget_get_first_child(widget);
         child != NULL;
         child = gtk_widget_get_next_sibling(child)) {
        AdwTabView *tab_view = find_tab_view(child);
        if (tab_view != NULL)
            return tab_view;
    }

    return NULL;
}

static AdwTabBar *
find_tab_bar(GtkWidget *widget)
{
    if (ADW_IS_TAB_BAR(widget))
        return ADW_TAB_BAR(widget);

    for (GtkWidget *child = gtk_widget_get_first_child(widget);
         child != NULL;
         child = gtk_widget_get_next_sibling(child)) {
        AdwTabBar *tab_bar = find_tab_bar(child);
        if (tab_bar != NULL)
            return tab_bar;
    }

    return NULL;
}

static AdwWindowTitle *
find_window_title(GtkWidget *widget)
{
    if (ADW_IS_WINDOW_TITLE(widget))
        return ADW_WINDOW_TITLE(widget);

    for (GtkWidget *child = gtk_widget_get_first_child(widget);
         child != NULL;
         child = gtk_widget_get_next_sibling(child)) {
        AdwWindowTitle *title = find_window_title(child);
        if (title != NULL)
            return title;
    }

    return NULL;
}

static GtkMenuButton *
find_menu_button(GtkWidget *widget)
{
    if (GTK_IS_MENU_BUTTON(widget))
        return GTK_MENU_BUTTON(widget);

    for (GtkWidget *child = gtk_widget_get_first_child(widget);
         child != NULL;
         child = gtk_widget_get_next_sibling(child)) {
        GtkMenuButton *button = find_menu_button(child);
        if (button != NULL)
            return button;
    }

    return NULL;
}

static GtkDropTarget *
find_drop_target(GtkWidget *widget)
{
    GListModel *controllers = gtk_widget_observe_controllers(widget);
    for (guint index = 0; index < g_list_model_get_n_items(controllers); index++) {
        GtkEventController *controller =
            g_list_model_get_item(controllers, index);
        if (GTK_IS_DROP_TARGET(controller)) {
            gsize n_types = 0;
            const GType *types = gtk_drop_target_get_gtypes(
                GTK_DROP_TARGET(controller),
                &n_types
            );
            for (gsize type_index = 0; type_index < n_types; type_index++) {
                if (types[type_index] == GDK_TYPE_FILE_LIST) {
                    g_object_unref(controllers);
                    return GTK_DROP_TARGET(controller);
                }
            }
        }
        g_object_unref(controller);
    }
    g_object_unref(controllers);

    for (GtkWidget *child = gtk_widget_get_first_child(widget);
         child != NULL;
         child = gtk_widget_get_next_sibling(child)) {
        GtkDropTarget *target = find_drop_target(child);
        if (target != NULL)
            return target;
    }

    return NULL;
}

static GtkDropTarget *
find_drop_target_on_ancestor(GtkWidget *widget)
{
    for (GtkWidget *current = widget;
         current != NULL;
         current = gtk_widget_get_parent(current)) {
        GtkDropTarget *target = find_drop_target(current);
        if (target != NULL)
            return target;
    }

    return NULL;
}

static gboolean
drop_files(GtkDropTarget *target, GFile **files, gsize count)
{
    GdkFileList *file_list = gdk_file_list_new_from_array(files, count);
    GValue value = G_VALUE_INIT;
    g_value_init(&value, GDK_TYPE_FILE_LIST);
    g_value_set_boxed(&value, file_list);

    gboolean handled = FALSE;
    g_signal_emit_by_name(target, "drop", &value, 0.0, 0.0, &handled);

    g_value_unset(&value);
    g_boxed_free(GDK_TYPE_FILE_LIST, file_list);
    return handled;
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

static GtkEntry *
find_entry_by_placeholder(GtkWidget *widget, const char *placeholder)
{
    if (GTK_IS_ENTRY(widget) &&
        g_strcmp0(
            gtk_entry_get_placeholder_text(GTK_ENTRY(widget)),
            placeholder
        ) == 0) {
        return GTK_ENTRY(widget);
    }

    for (GtkWidget *child = gtk_widget_get_first_child(widget);
         child != NULL;
         child = gtk_widget_get_next_sibling(child)) {
        GtkEntry *entry = find_entry_by_placeholder(child, placeholder);
        if (entry != NULL)
            return entry;
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
    g_assert_cmpint(minimum_width, <=, 460);
    g_assert_cmpint(minimum_height, <=, 260);

    GtkTextView *text_view = find_text_view(content);
    g_assert_nonnull(text_view);
    AdwWindowTitle *window_title = find_window_title(content);
    g_assert_nonnull(window_title);
    g_assert_cmpstr(adw_window_title_get_title(window_title), ==, "Yeni Belge");
    GtkSearchEntry *search_entry = find_search_entry(content);
    g_assert_nonnull(search_entry);
    GtkMenuButton *menu_button = find_menu_button(content);
    g_assert_nonnull(menu_button);
    g_assert_cmpint(
        gtk_accessible_get_accessible_role(GTK_ACCESSIBLE(menu_button)),
        ==,
        GTK_ACCESSIBLE_ROLE_BUTTON
    );
    GMenuModel *menu_model = gtk_menu_button_get_menu_model(menu_button);
    g_assert_nonnull(menu_model);
    g_assert_cmpint(g_menu_model_get_n_items(menu_model), ==, 2);
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
    guint keyval = 0;
    GdkModifierType modifiers = 0;
    gtk_accelerator_parse("<Primary>question", &keyval, &modifiers);
    g_assert_cmpuint(keyval, ==, GDK_KEY_question);
    g_assert_true((modifiers & GDK_CONTROL_MASK) != 0);
    g_assert_false((modifiers & GDK_SHIFT_MASK) != 0);
    char *question_shortcut = gtk_accelerator_get_label(keyval, modifiers);
    g_assert_cmpstr(question_shortcut, ==, "Ctrl+?");
    g_free(question_shortcut);
    gtk_accelerator_parse("<Primary>comma", &keyval, &modifiers);
    g_assert_cmpuint(keyval, ==, GDK_KEY_comma);
    g_assert_true((modifiers & GDK_CONTROL_MASK) != 0);

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

    mini_text_window_activate_action(
        window,
        MINITEXT_WINDOW_ACTION_REPLACE
    );
    GtkEntry *replace_entry = find_entry_by_placeholder(
        gtk_window_get_child(window),
        "Yeni metin"
    );
    GtkButton *replace_current_button = find_button_by_label(
        gtk_window_get_child(window),
        "Değiştir"
    );
    GtkButton *replace_all_button = find_button_by_label(
        gtk_window_get_child(window),
        "Tümünü Değiştir"
    );
    g_assert_nonnull(replace_entry);
    g_assert_nonnull(replace_current_button);
    g_assert_nonnull(replace_all_button);
    gtk_editable_set_text(GTK_EDITABLE(search_entry), "Resize");
    gtk_editable_set_text(GTK_EDITABLE(replace_entry), "Changed");

    g_signal_emit_by_name(replace_current_button, "clicked");
    GtkTextIter document_start;
    GtkTextIter document_end;
    gtk_text_buffer_get_bounds(buffer, &document_start, &document_end);
    char *current_replaced_text = gtk_text_buffer_get_text(
        buffer,
        &document_start,
        &document_end,
        FALSE
    );
    char *first_changed = g_strstr_len(current_replaced_text, -1, "Changed");
    g_assert_nonnull(first_changed);
    g_assert_null(g_strstr_len(first_changed + strlen("Changed"), -1, "Changed"));
    g_free(current_replaced_text);
    mini_text_window_activate_action(window, MINITEXT_WINDOW_ACTION_UNDO);

    g_signal_emit_by_name(replace_all_button, "clicked");

    gtk_text_buffer_get_bounds(buffer, &document_start, &document_end);
    char *replaced_text = gtk_text_buffer_get_text(
        buffer,
        &document_start,
        &document_end,
        FALSE
    );
    g_assert_null(g_strstr_len(replaced_text, -1, "Resize"));
    g_assert_nonnull(g_strstr_len(replaced_text, -1, "Changed"));
    g_free(replaced_text);

    mini_text_window_activate_action(window, MINITEXT_WINDOW_ACTION_UNDO);
    gtk_text_buffer_get_bounds(buffer, &document_start, &document_end);
    char *undone_text = gtk_text_buffer_get_text(
        buffer,
        &document_start,
        &document_end,
        FALSE
    );
    g_assert_nonnull(g_strstr_len(undone_text, -1, "Resize"));
    g_assert_null(g_strstr_len(undone_text, -1, "Changed"));
    g_free(undone_text);

    mini_text_window_activate_action(window, MINITEXT_WINDOW_ACTION_REDO);
    gtk_text_buffer_get_bounds(buffer, &document_start, &document_end);
    char *redone_text = gtk_text_buffer_get_text(
        buffer,
        &document_start,
        &document_end,
        FALSE
    );
    g_assert_null(g_strstr_len(redone_text, -1, "Resize"));
    g_assert_nonnull(g_strstr_len(redone_text, -1, "Changed"));
    g_free(redone_text);

    gtk_editable_set_text(GTK_EDITABLE(search_entry), "İstanbul");
    g_signal_emit_by_name(search_entry, "search-changed");
    g_assert_true(gtk_text_buffer_get_selection_bounds(
        buffer,
        &match_start,
        &match_end
    ));
    char *turkish_match = gtk_text_buffer_get_text(
        buffer,
        &match_start,
        &match_end,
        FALSE
    );
    g_assert_cmpstr(turkish_match, ==, "İstanbul");
    g_free(turkish_match);

    AdwTabView *tab_view = find_tab_view(content);
    AdwTabBar *tab_bar = find_tab_bar(content);
    g_assert_nonnull(tab_view);
    g_assert_nonnull(tab_bar);
    g_assert_cmpint(adw_tab_view_get_n_pages(tab_view), ==, 1);
    g_assert_false(gtk_widget_get_visible(GTK_WIDGET(tab_bar)));
    GtkTextBuffer *original_buffer = buffer;

    GError *error = NULL;
    char *directory = g_dir_make_tmp("minitext-tabs-XXXXXX", &error);
    g_assert_no_error(error);
    g_assert_nonnull(directory);
    char *path_one = g_build_filename(directory, "one.txt", NULL);
    char *path_two = g_build_filename(directory, "two.txt", NULL);
    char *path_three = g_build_filename(directory, "three.txt", NULL);
    char *path_four = g_build_filename(directory, "four.txt", NULL);
    char *invalid_path = g_build_filename(directory, "invalid.bin", NULL);
    GFile *file_one = g_file_new_for_path(path_one);
    GFile *file_two = g_file_new_for_path(path_two);
    GFile *file_three = g_file_new_for_path(path_three);
    GFile *file_four = g_file_new_for_path(path_four);
    GFile *invalid_file = g_file_new_for_path(invalid_path);
    const char *one_text = "Dropped file one";
    const char *two_text = "Dropped file two";
    const char *three_text = "Dropped file three";
    const char *four_text = "Opened from an empty draft";
    g_assert_true(g_file_replace_contents(
        file_one, one_text, strlen(one_text), NULL, FALSE,
        G_FILE_CREATE_NONE, NULL, NULL, &error
    ));
    g_assert_no_error(error);
    g_assert_true(g_file_replace_contents(
        file_two, two_text, strlen(two_text), NULL, FALSE,
        G_FILE_CREATE_NONE, NULL, NULL, &error
    ));
    g_assert_no_error(error);
    g_assert_true(g_file_replace_contents(
        file_three, three_text, strlen(three_text), NULL, FALSE,
        G_FILE_CREATE_NONE, NULL, NULL, &error
    ));
    g_assert_no_error(error);
    g_assert_true(g_file_replace_contents(
        file_four, four_text, strlen(four_text), NULL, FALSE,
        G_FILE_CREATE_NONE, NULL, NULL, &error
    ));
    g_assert_no_error(error);
    const char invalid_utf8[] = {(char)0xff};
    g_assert_true(g_file_replace_contents(
        invalid_file, invalid_utf8, sizeof(invalid_utf8), NULL, FALSE,
        G_FILE_CREATE_NONE, NULL, NULL, &error
    ));
    g_assert_no_error(error);

    GtkDropTarget *tab_drop_target =
        find_drop_target(GTK_WIDGET(tab_bar));
    g_assert_nonnull(tab_drop_target);
    GFile *one_file[] = {file_one};
    g_assert_true(drop_files(tab_drop_target, one_file, 1));
    g_assert_cmpint(adw_tab_view_get_n_pages(tab_view), ==, 2);
    g_assert_true(gtk_widget_get_visible(GTK_WIDGET(tab_bar)));
    GtkTextView *file_one_view = find_text_view(
        adw_tab_page_get_child(adw_tab_view_get_selected_page(tab_view))
    );
    g_assert_nonnull(file_one_view);
    GtkTextBuffer *file_one_buffer = gtk_text_view_get_buffer(file_one_view);
    g_assert_true(file_one_buffer != original_buffer);
    GtkTextIter opened_start;
    GtkTextIter opened_end;
    gtk_text_buffer_get_bounds(file_one_buffer, &opened_start, &opened_end);
    char *opened_text = gtk_text_buffer_get_text(
        file_one_buffer,
        &opened_start,
        &opened_end,
        FALSE
    );
    g_assert_cmpstr(opened_text, ==, one_text);
    g_free(opened_text);

    GtkDropTarget *editor_drop_target = find_drop_target_on_ancestor(
        GTK_WIDGET(file_one_view)
    );
    g_assert_nonnull(editor_drop_target);
    g_assert_true(drop_files(editor_drop_target, one_file, 1));
    g_assert_cmpint(adw_tab_view_get_n_pages(tab_view), ==, 2);
    g_assert_true(
        gtk_text_view_get_buffer(find_text_view(adw_tab_page_get_child(
            adw_tab_view_get_selected_page(tab_view)
        ))) == file_one_buffer
    );

    GtkDropTarget *window_drop_target = find_drop_target(content);
    g_assert_nonnull(window_drop_target);
    GFile *multiple_files[] = {file_two, file_three};
    g_assert_true(drop_files(window_drop_target, multiple_files, 2));
    g_assert_cmpint(adw_tab_view_get_n_pages(tab_view), ==, 4);
    g_assert_true(gtk_text_view_get_buffer(text_view) == original_buffer);
    gboolean found_two = FALSE;
    gboolean found_three = FALSE;
    for (int position = 0; position < adw_tab_view_get_n_pages(tab_view);
         position++) {
        GtkTextView *page_view = find_text_view(adw_tab_page_get_child(
            adw_tab_view_get_nth_page(tab_view, position)
        ));
        GtkTextBuffer *page_buffer = gtk_text_view_get_buffer(page_view);
        GtkTextIter page_start;
        GtkTextIter page_end;
        gtk_text_buffer_get_bounds(page_buffer, &page_start, &page_end);
        char *page_text = gtk_text_buffer_get_text(
            page_buffer,
            &page_start,
            &page_end,
            FALSE
        );
        found_two |= g_strcmp0(page_text, two_text) == 0;
        found_three |= g_strcmp0(page_text, three_text) == 0;
        g_free(page_text);
    }
    g_assert_true(found_two);
    g_assert_true(found_three);

    GFile *invalid_files[] = {invalid_file};
    g_assert_true(drop_files(window_drop_target, invalid_files, 1));
    g_assert_cmpint(adw_tab_view_get_n_pages(tab_view), ==, 4);

    mini_text_window_activate_action(window, MINITEXT_WINDOW_ACTION_NEW);
    g_assert_cmpint(adw_tab_view_get_n_pages(tab_view), ==, 5);
    GtkTextView *new_view = find_text_view(
        adw_tab_page_get_child(adw_tab_view_get_selected_page(tab_view))
    );
    GtkTextBuffer *new_buffer = gtk_text_view_get_buffer(new_view);
    GtkTextIter new_buffer_end;
    gtk_text_buffer_get_end_iter(new_buffer, &new_buffer_end);
    gtk_text_buffer_insert(new_buffer, &new_buffer_end, "unsaved draft", -1);
    mini_text_window_activate_action(window, MINITEXT_WINDOW_ACTION_UNDO);
    gtk_text_buffer_get_bounds(new_buffer, &document_start, &document_end);
    char *undone_draft = gtk_text_buffer_get_text(
        new_buffer,
        &document_start,
        &document_end,
        FALSE
    );
    g_assert_cmpstr(undone_draft, ==, "");
    g_free(undone_draft);
    mini_text_window_activate_action(window, MINITEXT_WINDOW_ACTION_REDO);
    g_assert_cmpstr(
        adw_tab_page_get_title(adw_tab_view_get_selected_page(tab_view)),
        ==,
        "*Yeni Belge"
    );
    GFile *already_open[] = {file_one};
    g_assert_true(drop_files(find_drop_target_on_ancestor(GTK_WIDGET(new_view)),
                             already_open, 1));
    g_assert_cmpint(adw_tab_view_get_n_pages(tab_view), ==, 5);
    gtk_text_buffer_get_bounds(new_buffer, &document_start, &document_end);
    char *preserved_draft = gtk_text_buffer_get_text(
        new_buffer,
        &document_start,
        &document_end,
        FALSE
    );
    g_assert_cmpstr(preserved_draft, ==, "unsaved draft");
    g_free(preserved_draft);

    mini_text_window_activate_action(
        window,
        MINITEXT_WINDOW_ACTION_SELECT_TAB_1
    );
    g_assert_true(
        adw_tab_view_get_selected_page(tab_view) ==
        adw_tab_view_get_nth_page(tab_view, 0)
    );
    mini_text_window_activate_action(
        window,
        MINITEXT_WINDOW_ACTION_NEXT_TAB
    );
    g_assert_true(
        adw_tab_view_get_selected_page(tab_view) ==
        adw_tab_view_get_nth_page(tab_view, 1)
    );
    mini_text_window_activate_action(
        window,
        MINITEXT_WINDOW_ACTION_CLOSE_TAB
    );
    g_assert_cmpint(adw_tab_view_get_n_pages(tab_view), ==, 4);

    g_object_unref(tab_drop_target);
    g_object_unref(editor_drop_target);
    g_object_unref(window_drop_target);
    g_object_unref(file_one);
    g_object_unref(file_two);
    g_object_unref(file_three);
    g_object_unref(invalid_file);
    g_assert_cmpint(g_unlink(path_one), ==, 0);
    g_assert_cmpint(g_unlink(path_two), ==, 0);
    g_assert_cmpint(g_unlink(path_three), ==, 0);
    g_assert_cmpint(g_unlink(invalid_path), ==, 0);
    g_free(path_one);
    g_free(path_two);
    g_free(path_three);
    g_free(invalid_path);

    mini_text_window_activate_action(
        window,
        MINITEXT_WINDOW_ACTION_SHORTCUTS
    );
    GtkWindow *shortcuts_window = NULL;
    top_levels = gtk_window_get_toplevels();
    for (guint index = 0; index < g_list_model_get_n_items(top_levels); index++) {
        GtkWindow *candidate = g_list_model_get_item(top_levels, index);
        if (candidate != window &&
            gtk_window_get_transient_for(candidate) == window &&
            g_strcmp0(
                gtk_window_get_title(candidate),
                "Klavye Kısayolları"
            ) == 0) {
            shortcuts_window = candidate;
            break;
        }
        g_object_unref(candidate);
    }
    g_assert_nonnull(shortcuts_window);
    g_assert_true(GTK_IS_SHORTCUTS_WINDOW(shortcuts_window));
    gtk_window_destroy(shortcuts_window);
    g_object_unref(shortcuts_window);

    GtkWindow *empty_draft_window = mini_text_window_new(
        gtk_window_get_application(window)
    );
    g_object_ref(empty_draft_window);
    AdwTabView *empty_draft_tab_view = find_tab_view(
        gtk_window_get_child(empty_draft_window)
    );
    GtkTextView *empty_draft_view = find_text_view(
        adw_tab_page_get_child(
            adw_tab_view_get_selected_page(empty_draft_tab_view)
        )
    );
    GtkTextBuffer *empty_draft_buffer = gtk_text_view_get_buffer(
        empty_draft_view
    );
    g_assert_cmpint(adw_tab_view_get_n_pages(empty_draft_tab_view), ==, 1);
    g_assert_true(mini_text_window_open_file(empty_draft_window, file_four));
    g_assert_cmpint(adw_tab_view_get_n_pages(empty_draft_tab_view), ==, 1);
    g_assert_true(
        gtk_text_view_get_buffer(find_text_view(adw_tab_page_get_child(
            adw_tab_view_get_selected_page(empty_draft_tab_view)
        ))) == empty_draft_buffer
    );
    GtkTextIter opened_draft_start;
    GtkTextIter opened_draft_end;
    gtk_text_buffer_get_bounds(
        empty_draft_buffer,
        &opened_draft_start,
        &opened_draft_end
    );
    char *opened_draft_text = gtk_text_buffer_get_text(
        empty_draft_buffer,
        &opened_draft_start,
        &opened_draft_end,
        FALSE
    );
    g_assert_cmpstr(opened_draft_text, ==, four_text);
    g_free(opened_draft_text);
    gtk_window_destroy(empty_draft_window);
    g_object_unref(empty_draft_window);
    g_object_unref(file_four);
    g_assert_cmpint(g_unlink(path_four), ==, 0);
    g_assert_cmpint(g_rmdir(directory), ==, 0);
    g_free(path_four);
    g_free(directory);

    GtkWindow *last_tab_window = mini_text_window_new(
        gtk_window_get_application(window)
    );
    g_object_ref(last_tab_window);
    AdwTabView *last_tab_view = find_tab_view(
        gtk_window_get_child(last_tab_window)
    );
    mini_text_window_activate_action(
        last_tab_window,
        MINITEXT_WINDOW_ACTION_CLOSE_TAB
    );
    g_assert_cmpint(adw_tab_view_get_n_pages(last_tab_view), ==, 1);
    g_assert_cmpstr(
        adw_tab_page_get_title(adw_tab_view_get_selected_page(last_tab_view)),
        ==,
        "Yeni Belge"
    );
    gtk_window_destroy(last_tab_window);
    g_object_unref(last_tab_window);

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
    g_string_append(text, "Türkçe karakter: İstanbul\n");
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
