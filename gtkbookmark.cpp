// The GTK Bookmark Editor
// Copyright (C) 2000 Paul Brannan <pbranna@clemson.edu>
// The GTK Bookmark Editor is released under the GNU public license.  This is
// free software, and you are welcome to redistribute it under the license
// conditions.  For details, please see the LICENSE file.

#include <iostream>
#include <fstream>
#include <string>
#include <stack>
#include <queue>
#include <gtk/gtk.h>
#include <glade/glade.h>

using namespace std;

// Global Widgets
GtkWidget *folder_tree;
GtkWidget *url_list;
GtkWidget *statusbar;
GtkWidget *loadfile_dlg;
GtkWidget *savefile_dlg;
GtkWidget *about_dlg;
GtkWidget *main_window;
GtkWidget *bookmark_dlg;

// Bookmark Properties Widgets
GtkEntry *bookmark_title_entry;
GtkEntry *bookmark_location_entry;
GtkEntry *bookmark_created_entry;
GtkEntry *bookmark_visited_entry;
GtkEntry *bookmark_modified_entry;

// Global Pixmaps
GdkPixmap *folder_pixmap;
GdkBitmap *folder_mask;

// Global data
GtkCTreeNode *current_tree_node = NULL;

// Bookmark data
enum {
	FOLDER,
	BOOKMARK,
	SEPARATOR
} BookmarkType;
struct Bookmark {
	char *text;
	char *location;
	char *visited;
	char *created;
	char *modified;
	int bookmark_type;
	GtkCTreeNode *child;
};
gint bookmark_compare_func(const void *a, const void *b) {
	return a == b;
}

// Drag and drop supporting data
enum {
	BOOKMARK_TARGET,
	FOLDER_TARGET,
	NUM_TARGETS
} TargetEnum;
GtkTargetEntry bookmark_target =
	{"bookmark_target", GTK_TARGET_SAME_APP, BOOKMARK_TARGET};
GtkTargetEntry folder_target =
	{"folder_target", GTK_TARGET_SAME_APP, FOLDER_TARGET};
GtkTargetEntry targets[] = {bookmark_target, folder_target};
gint oldrow = G_MAXINT;

extern "C" {

// ------------------------------------------------------------
// Initialization
// ------------------------------------------------------------

void init_widgets(GladeXML *xml) {
	folder_tree = glade_xml_get_widget(xml, "ctree");
	url_list = glade_xml_get_widget(xml, "url_list");
	statusbar = glade_xml_get_widget(xml, "statusbar");
	loadfile_dlg = glade_xml_get_widget(xml, "loadfile_dlg");
	savefile_dlg = glade_xml_get_widget(xml, "savefile_dlg");
	about_dlg = glade_xml_get_widget(xml, "about_dlg");
	main_window = glade_xml_get_widget(xml, "main_window");
	bookmark_dlg = glade_xml_get_widget(xml, "bookmark_dlg");

	bookmark_title_entry =
		GTK_ENTRY(glade_xml_get_widget(xml, "bookmark_title_entry"));
	bookmark_location_entry =
		GTK_ENTRY(glade_xml_get_widget(xml, "bookmark_location_entry"));
	bookmark_created_entry =
		GTK_ENTRY(glade_xml_get_widget(xml, "bookmark_created_entry"));
	bookmark_modified_entry =
		GTK_ENTRY(glade_xml_get_widget(xml, "bookmark_modified_entry"));
	bookmark_visited_entry =
		GTK_ENTRY(glade_xml_get_widget(xml, "bookmark_visited_entry"));

	// Glade doesn't support word wrap
	GtkWidget *about_text = glade_xml_get_widget(xml, "about_text");
	gtk_text_set_word_wrap(GTK_TEXT(about_text), TRUE);

	// Make the list a drag source
	gtk_drag_source_set(GTK_WIDGET(url_list), GDK_BUTTON1_MASK,
		&bookmark_target, 1, GDK_ACTION_MOVE);

	// And make the tree a drag destination
	gtk_drag_dest_set(GTK_WIDGET(folder_tree),
		GTK_DEST_DEFAULT_ALL, targets, NUM_TARGETS,
		GDK_ACTION_MOVE);

	// Make both the tree and the list reorderable
	// gtk_ctree_set_reorderable(GTK_CLIST(folder_tree), TRUE);
	gtk_clist_set_reorderable(GTK_CLIST(url_list), TRUE);
}

void init_data() {
	GdkColor white;
	gdk_color_white(gdk_colormap_get_system(), &white);
	folder_pixmap = gdk_pixmap_create_from_xpm(main_window->window,
		&folder_mask, &white, "folder.xpm");
}

// ------------------------------------------------------------
// Miscellaneous routines
// ------------------------------------------------------------

void sync_display() {
	while(gtk_events_pending()) gtk_main_iteration();
	gdk_flush();
}

guint32 get_time() {
    GTimeVal t;
    g_get_current_time(&t);
    return (t.tv_sec * 1000) + (t.tv_usec / 1000);
}

// ------------------------------------------------------------
// Tree browsing routines
// ------------------------------------------------------------

// NOTE: the prototype for the select_row event in the GTK docs
// is wrong (the second parameter is a guint).
void on_folder_tree_select_row(GtkCTree *tree, guint row, gint column,
	gpointer data) {

	// If we don't call this then debugging this function is difficult
	// (the X display gets locked)
	sync_display();

	GtkCList *clist = GTK_CLIST(url_list);
	gtk_clist_clear(clist);

	// Get the data associated with the selected row, if it exists
	GtkCTreeNode *node = gtk_ctree_node_nth(tree, row);
	current_tree_node = node;
	if(!node) return;
	GList *bookmark_list = (GList *)gtk_ctree_node_get_row_data(
		tree, GTK_CTREE_NODE(node));

	// Freeze the list so we don't update one-at-a-time
	gtk_clist_freeze(clist);

	// Add the data items in the tree to the list
	const gchar *text[] = {NULL, NULL, NULL, NULL};
	GList *l;
	Bookmark *b;
	for(l = bookmark_list; l != NULL; l = g_list_next(l)) {

		b = (Bookmark*)l->data;

		text[1] = b->text?b->text:"";
		text[2] = b->location?b->location:"";
		text[3] = b->visited?b->visited:"";
		text[4] = b->created?b->created:"";
		row = gtk_clist_append(clist, (gchar**)text);
		gtk_clist_set_row_data(clist, row, b);
		if(b->bookmark_type == FOLDER)
			gtk_clist_set_pixmap(clist, row, 0, folder_pixmap, folder_mask);
	}

	// Now thaw the list, and we are done
	gtk_clist_thaw(clist);	
}

gboolean on_url_list_button_press_event(GtkWidget *w, GdkEventButton *event,
	gpointer data) {

	if(event->button == 1 && event->type == GDK_2BUTTON_PRESS) {
		GtkCList *clist = GTK_CLIST(w);
		int row = clist->click_cell.row;
		Bookmark *b = (Bookmark*)gtk_clist_get_row_data(clist, row);
		if(b->bookmark_type == FOLDER) return TRUE;

		string command = "netscape ";
		command += b->location;
		command += "&";
		system(command.c_str());
		return TRUE;
	}

	return FALSE;
}

// ------------------------------------------------------------
// Drag and drop
// ------------------------------------------------------------

void on_folder_tree_drag_data_received(GtkWidget *w, GdkDragContext
	*drag_context, gint x, gint y, GtkSelectionData *drag_data, guint info,
	guint time, gpointer data) {

	// Get the row that the mouse was over when this event occurred
	gint row, column;
	if(!gtk_clist_get_selection_info(GTK_CLIST(w), x, y, &row, &column))
		return;

	Bookmark *b = new Bookmark;
	Bookmark *origb = (Bookmark*)drag_data->data;
	b->text = strdup(origb->text);
	b->location = strdup(origb->location);
	b->visited = strdup(origb->visited);
	b->created = strdup(origb->created);
	b->bookmark_type = origb->bookmark_type;
	b->child = origb->child;

	GtkCTree *ctree = GTK_CTREE(w);
	GtkCTreeNode *node = gtk_ctree_node_nth(ctree, row);

	if(b->bookmark_type == FOLDER) {
		// If the node isn't visible when we move it, we get a failed
		// assertion.
		gtk_clist_freeze(GTK_CLIST(ctree));
		bool do_collapse = FALSE;
		GtkCTreeNode *collapse_node = GTK_CTREE_ROW(b->child)->parent;
		if(gtk_ctree_node_is_visible(ctree, b->child) != GTK_VISIBILITY_FULL) {
			do_collapse = TRUE;
			gtk_ctree_expand(ctree, collapse_node);
		}
		gtk_ctree_move(ctree, b->child, node, NULL);
		if(do_collapse) {
			gtk_ctree_collapse(ctree, collapse_node);
		}
		gtk_clist_thaw(GTK_CLIST(ctree));
	}
	GList *bookmark_list = (GList*)gtk_ctree_node_get_row_data(ctree, node);
	bookmark_list = g_list_append(bookmark_list, b);
	gtk_ctree_node_set_row_data(ctree, node, bookmark_list);
}

// There should be a way to get the selected cells WITHOUT using CList's
// private data!
void on_url_list_drag_data_get(GtkWidget *w, GdkDragContext *drag_context,
	GtkSelectionData *drag_data, guint info, guint time, gpointer data) {

	// for(GList *l = GTK_CLIST(w)->selection; l != NULL; l = g_list_next(l)) {
	// 	g_print("%d\n", (int)l->data);
	// }
	GtkCList *clist = GTK_CLIST(w);
	int row = clist->click_cell.row;
	Bookmark *b = (Bookmark*)gtk_clist_get_row_data(clist, row);
	
	gtk_selection_data_set(drag_data, drag_data->target, 8,
		(guchar*)b, sizeof(*b));
}

void on_url_list_drag_data_delete(GtkWidget *w, GdkDragContext *drag_context,
	gpointer user_data) {

	GtkCList *clist = GTK_CLIST(w);
	int row = clist->click_cell.row;
	Bookmark *b = (Bookmark*)gtk_clist_get_row_data(clist, row);
	if(b->text) free(b->text);
	if(b->created) free(b->created);
	if(b->visited) free(b->visited);
	if(b->modified) free(b->modified);
	gtk_clist_remove(clist, row);
}

// A function to hide the fact that we are using the clist internals to
// perform the highlighting
void gtk_clist_drag_highlight_row(GtkWidget *w, gint row) {
	if(oldrow != row) {
		GtkCListClass *clc = GTK_CLIST_CLASS(gtk_type_class(GTK_TYPE_CLIST));
		if(oldrow != G_MAXINT){
			(*clc->draw_drag_highlight)(GTK_CLIST(w),  NULL, oldrow,
				GTK_CLIST_DRAG_INTO);
		}
		(*clc->draw_drag_highlight)(GTK_CLIST(w),  NULL, row,
			GTK_CLIST_DRAG_INTO);
		oldrow = row;
	}
}

// We do a bit of hacking to get the highlights to work right
// This mostly works by magic, so it might not be thread-safe
gboolean on_folder_tree_drag_motion(GtkWidget *w, GdkDragContext *drag_context,
	gint x, gint y, guint time, gpointer data) {

	static gboolean in_motion = FALSE;
	static int last_abort_time = 0;
	static guint event_time;
	static queue<gint> s;

	// Get the row that the mouse was over when this event occurred
	// If the row is not valid, then unhighlight the currently highlighted row
	gint row, column;
	if(!gtk_clist_get_selection_info(GTK_CLIST(w), x, y, &row, &column)) {
		if(oldrow != G_MAXINT) {
			GtkCListClass *clc =
				GTK_CLIST_CLASS(gtk_type_class(GTK_TYPE_CLIST));
			(*clc->draw_drag_highlight)(GTK_CLIST(w),  NULL, oldrow,
				GTK_CLIST_DRAG_INTO);
			oldrow = G_MAXINT;
		}
		return FALSE;
	}

	// If this function was called during sync_display() below, then put
	// it on the queue for later processing
    if(in_motion) {
        if(get_time() - last_abort_time < 500 || last_abort_time == 0) {
            last_abort_time = get_time();
			s.push(row);
            return TRUE;
        }
        last_abort_time = get_time();
    }

	// If we don't call sync_display(), then we won't get a highlight on
	// the first item the mouse touches
    in_motion = TRUE;
	sync_display();
	in_motion = FALSE;

	// Highlight the rows in the order that the mouse went over them
	gtk_clist_drag_highlight_row(w, row);
	while(s.size() != 0) {
		gtk_clist_drag_highlight_row(w, s.front());
		s.pop();
	}

	return TRUE;
}

gboolean on_folder_tree_drag_leave(GtkWidget *w, GdkDragContext *drag_context,
	guint time, gpointer data) {

	// Unhighlight anything that might be highlightesd
	if(oldrow != G_MAXINT) {
		GtkCListClass *clc = GTK_CLIST_CLASS(gtk_type_class(GTK_TYPE_CLIST));
		(*clc->draw_drag_highlight)(GTK_CLIST(w),  NULL, oldrow,
			GTK_CLIST_DRAG_INTO);
		oldrow = G_MAXINT;
	}

	return TRUE;
}

// ------------------------------------------------------------
// Bookmark properties
// ------------------------------------------------------------

void on_new_activate(GtkWidget *w, gpointer data) {
	gchar *bookmarks = "Bookmarks";
	gtk_ctree_insert_node(GTK_CTREE(folder_tree), NULL,
		NULL, &bookmarks, 0, NULL, NULL, NULL, NULL, FALSE, FALSE);
}

void on_bookmark_delete_activate(GtkWidget *w, gpointer data) {
	if(current_tree_node == NULL) {
		g_print("No tree branch selected.\n");
		return;
	}
	
	GtkCList *clist = GTK_CLIST(url_list);
	int row = clist->focus_row;
	Bookmark *b = (Bookmark*)gtk_clist_get_row_data(clist, row);
	if(!b) {
		g_print("No bookmark selected (row = %d).\n", row);
		return;
	}

	// First, delete the data
	if(b->text) free(b->text);
	if(b->location) free(b->location);
	if(b->created) free(b->created);
	if(b->modified) free(b->modified);
	if(b->visited) free(b->visited);

	// Next, delete the entry from the list
	gtk_clist_remove(clist, row);

	// If this is a folder, unlink it from the tree
	if(b->bookmark_type == FOLDER) {
		gtk_ctree_remove_node(GTK_CTREE(folder_tree), b->child);
	}

	// FIX ME!! We should delete all the children of this node -- otherwise
	// we have a memory leak.  The program still works, though, so this can
	// wait.

	// Lastly, delete the bookmark
	delete b;
}

void on_bookmark_properties_activate(GtkWidget *w, gpointer data) {
	if(current_tree_node == NULL) {
		g_print("No tree branch selected.\n");
		return;
	}

	GtkCList *clist = GTK_CLIST(url_list);
	int row = clist->focus_row;
	Bookmark *b = (Bookmark*)gtk_clist_get_row_data(clist, row);
	if(!b) {
		g_print("No bookmark selected (row = %d).\n", row);
		return;
	}

	gtk_entry_set_text(bookmark_title_entry, b->text);
	gtk_entry_set_text(bookmark_location_entry, b->location);
	gtk_entry_set_text(bookmark_visited_entry, b->visited);
	gtk_entry_set_text(bookmark_created_entry, b->created);
	gtk_entry_set_text(bookmark_modified_entry, b->modified);

	gtk_widget_show(bookmark_dlg);
}

void on_new_bookmark_activate(GtkWidget *w, gpointer data) {
	if(current_tree_node == NULL) return;

	// Create a new bookmark
	Bookmark *b = new Bookmark;
	b->text = strdup("");
	b->location = strdup("");
	b->created = strdup("");
	b->modified = strdup("");
	b->visited = strdup("");
	b->bookmark_type = BOOKMARK;

	// Add a bookmark to the tree
	GList *bookmark_list = (GList *)gtk_ctree_node_get_row_data(
		GTK_CTREE(folder_tree), GTK_CTREE_NODE(current_tree_node));
	bookmark_list = g_list_append(bookmark_list, b);
	gtk_ctree_node_set_row_data(GTK_CTREE(folder_tree), current_tree_node,
		bookmark_list);

	// Now add this bookmark to the viewing list
	gchar *text[] = {NULL, b->text, b->location, b->visited, b->created};
	GtkCList *clist = GTK_CLIST(url_list);
	gint row = gtk_clist_append(clist, (gchar**)text);
	gtk_clist_set_row_data(clist, row, b);
	gtk_clist_select_row(clist, row, 0);
	GtkCListClass *clc = GTK_CLIST_CLASS(gtk_type_class(GTK_TYPE_CLIST));
	(*clc->toggle_focus_row)(clist);
	clist->focus_row = row;
	(*clc->toggle_focus_row)(clist);

	// Now, go to the properties dialog
	on_bookmark_properties_activate(w, NULL);
}

void on_new_folder_activate(GtkWidget *w, gpointer data) {
	if(current_tree_node == NULL) return;

	// Create a new bookmark
	Bookmark *b = new Bookmark;
	b->text = strdup("");
	b->location = strdup("");
	b->created = strdup("");
	b->modified = strdup("");
	b->visited = strdup("");
	b->bookmark_type = FOLDER;

	// Add this folder to the tree
	b->child = gtk_ctree_insert_node(GTK_CTREE(folder_tree), current_tree_node,
		NULL, &b->text, 0, NULL, NULL, NULL, NULL, FALSE, FALSE);

	// Add a bookmark to the tree
	GList *bookmark_list = (GList *)gtk_ctree_node_get_row_data(
		GTK_CTREE(folder_tree), GTK_CTREE_NODE(current_tree_node));
	bookmark_list = g_list_append(bookmark_list, b);
	gtk_ctree_node_set_row_data(GTK_CTREE(folder_tree), current_tree_node,
		bookmark_list);

	// Now add this bookmark to the viewing list
	gchar *text[] = {NULL, b->text, b->location, b->visited, b->created};
	GtkCList *clist = GTK_CLIST(url_list);
	gint row = gtk_clist_append(clist, (gchar**)text);
	gtk_clist_set_row_data(clist, row, b);
	gtk_clist_set_pixmap(clist, row, 0, folder_pixmap, folder_mask);
	gtk_clist_select_row(clist, row, 0);
	GtkCListClass *clc = GTK_CLIST_CLASS(gtk_type_class(GTK_TYPE_CLIST));
	(*clc->toggle_focus_row)(clist);
	clist->focus_row = row;
	(*clc->toggle_focus_row)(clist);

	// Now, go to the properties dialog
	on_bookmark_properties_activate(w, NULL);
}

void on_bookmark_properties_ok_clicked(GtkWidget *w, gpointer data) {
	GtkCList *clist = GTK_CLIST(url_list);
	int row = clist->focus_row;
	Bookmark *b = (Bookmark*)gtk_clist_get_row_data(clist, row);

	if(b->text) free(b->text);
	if(b->location) free(b->location);
	if(b->visited) free(b->visited);
	if(b->modified) free(b->modified);
	if(b->created) free(b->created);

	b->text = strdup(gtk_entry_get_text(bookmark_title_entry));
	b->location = strdup(gtk_entry_get_text(bookmark_location_entry));
	b->visited = strdup(gtk_entry_get_text(bookmark_visited_entry));
	b->modified = strdup(gtk_entry_get_text(bookmark_modified_entry));
	b->created = strdup(gtk_entry_get_text(bookmark_created_entry));

	if(b->bookmark_type == FOLDER) {
		gtk_entry_set_editable(bookmark_location_entry, FALSE);
		gtk_entry_set_editable(bookmark_visited_entry, FALSE);
		gtk_entry_set_editable(bookmark_modified_entry, FALSE);
	} else {
		gtk_entry_set_editable(bookmark_location_entry, TRUE);
		gtk_entry_set_editable(bookmark_visited_entry, TRUE);
		gtk_entry_set_editable(bookmark_modified_entry, TRUE);
	}

	gtk_clist_set_text(clist, row, 1, b->text);
	gtk_clist_set_text(clist, row, 2, b->location);
	gtk_clist_set_text(clist, row, 3, b->created);
	gtk_clist_set_text(clist, row, 4, b->visited);

	gtk_widget_hide(bookmark_dlg);
}

void on_bookmark_properties_cancel_clicked(GtkWidget *w, gpointer data) {
	gtk_widget_hide(bookmark_dlg);
}

// ------------------------------------------------------------
// Sort options
// ------------------------------------------------------------
void on_auto_sort_activate(GtkWidget *w, gpointer data) {
}

void on_insert_beginning_activate(GtkWidget *w, gpointer data) {
}

void on_insert_end_activate(GtkWidget *w, gpointer data) {
}

void on_copy_beginning_activate(GtkWidget *w, gpointer data) {
}

void on_copy_end_activate(GtkWidget *w, gpointer data) {
}

void on_sort_by_title_activate(GtkWidget *w, gpointer data) {
}

void on_sort_by_location_activate(GtkWidget *w, gpointer data) {
}

void on_sort_by_visited_activate(GtkWidget *w, gpointer data) {
}

void on_sort_by_created_activate(GtkWidget *w, gpointer data) {
}

// ------------------------------------------------------------
// File loading functions
// ------------------------------------------------------------

void on_load_activate(GtkWidget *w, gpointer data) {
	gtk_widget_show(loadfile_dlg);
}

void get_substr(const char *search, const string& str, char **dest) {
	// FIX ME!! This is highly unoptimized.
	int loc = str.find(search);
	if(loc != -1) {
		string t = str.substr(loc + strlen(search) + 1);
		t = t.substr(0, t.find("\""));
		*dest = strdup(t.c_str());
	} else {
		*dest = strdup("");
	}
}

void on_load_ok(GtkWidget *w, gpointer data) {
	gtk_widget_hide(loadfile_dlg);
	sync_display();

	// First, get the filename from the dialog
	char *filename = gtk_file_selection_get_filename(
		GTK_FILE_SELECTION(loadfile_dlg));

	// Now, open the file for reading
	ifstream in(filename);

	// Read the header
	string str;
	getline(in, str);
	if(str != "<!DOCTYPE NETSCAPE-Bookmark-file-1>")
		g_warning("This is probably not a Netscape bookmark file.");
	while(str.substr(0, 7) != "<TITLE>") getline(in, str);
	int j;
	for(j = 8; str[j] != 0 && str[j] != '<'; j++);
	str[j] = 0;
	gchar *bookmark_title = strdup(str.c_str() + 7);
	while(str.substr(0, 4) != "<DL>") getline(in, str);

	// Don't update the tree until we are done
	gtk_clist_freeze(GTK_CLIST(folder_tree));

	// Add an entry for the uppermost level
	GtkCTree *tree = GTK_CTREE(folder_tree);
	GtkCTreeNode *root = gtk_ctree_insert_node(tree, NULL, NULL,
		&bookmark_title, 0, NULL, NULL, NULL, NULL, FALSE, TRUE);

	// Lastly, read the file, adding items to the tree as necessary
	// This is basically an over-simplified XML parser, so we could
	// use libxml for this if we wanted.
	stack<GtkCTreeNode*> s;
	GtkCTreeNode *item = NULL, *parent = root;
	Bookmark *b;
	string label;
	GList *bookmark_list = NULL;
	for(;;) {
		getline(in, str);
		string t = str.substr(str.find_first_not_of(" \t"));
		if(t.substr(0, 4) == "<DT>") {
			// -= Data item =-

			// FIX ME!! This is highly unoptimized.
			b = new Bookmark;
			b->bookmark_type = BOOKMARK;

			get_substr("A HREF=", str, &b->location);
			get_substr("ADD_DATE=", str, &b->created);
			get_substr("LAST_VISIT=", str, &b->visited);
			get_substr("LAST_MODIFIED=", str, &b->modified);

			// Get the title
			t = t.substr(0, t.find_last_of("<"));
			t = t.substr(t.find_last_of(">") + 1);
			label = t;
			b->text = strdup(label.c_str());

			// Add this bookmark to the current bookmark list
			bookmark_list = g_list_append(bookmark_list, b);

		} else if(t.substr(0, 4) == "<DL>") {
			// -= New subtree =-

			gchar *label_str = (gchar*)label.c_str();
			item = gtk_ctree_insert_node(
				tree,						// The tree to insert to
				parent,						// The parent of the item
				NULL,						// Insert at the end of the tree
				&label_str,					// The label of the item
				0,							// Spacing b/t the pixmap and text
				NULL,						// pixmap_closed
				NULL,						// mask_closed
				NULL,						// pixmap_opened
				NULL,						// mask_opened
				FALSE,						// This node will NOT be a leaf
				FALSE						// This node should not be expanded
			);
			// gtk_ctree_node_set_pixtext(tree, item, 0, label_str, 0,
			//	folder_pixmap, folder_mask);
				
			b->bookmark_type = FOLDER;
			b->child = item;

			// Save the parent tree and set the new one as current
			gtk_ctree_node_set_row_data(tree, parent, bookmark_list);
			s.push(parent);
			bookmark_list = NULL;
			parent = item;
			item = NULL;

		} else if(t.substr(0, 5) == "</DL>") {
			// -= Close off old subtree =-

			gtk_ctree_node_set_row_data(tree, parent, bookmark_list);

			if(s.size() == 0) break;
			item = parent;
			parent = s.top();
			bookmark_list = (GList*)gtk_ctree_node_get_row_data(tree, parent);
			s.pop();

		} else if(t.substr(0, 4) == "<HR>") {
			// -= Separator =-

			b = new Bookmark;
			b->text = strdup("--------------");
			b->location = strdup("--------------");
			b->created = strdup("");
			b->modified = strdup("");
			b->visited = strdup("");
			b->bookmark_type = SEPARATOR;
			bookmark_list = g_list_append(bookmark_list, b);

		} else {
			g_warning("Invalid bookmark data encountered.");
		}
	}

	gtk_clist_thaw(GTK_CLIST(folder_tree));

	gtk_widget_show(GTK_WIDGET(tree));
}

void on_load_cancel(GtkWidget *w, gpointer data) {
	gtk_widget_hide(loadfile_dlg);
}

// ------------------------------------------------------------
// File saving functions
// ------------------------------------------------------------

void on_save_activate(GtkWidget *w, gpointer data) {
	gtk_widget_show(savefile_dlg);
}

void on_save_ok(GtkWidget *w, gpointer data) {
	gtk_widget_hide(savefile_dlg);
	sync_display();

	// First, get the filename from the dialog
	char *filename = gtk_file_selection_get_filename(
		GTK_FILE_SELECTION(savefile_dlg));

	// Now, open the file for writing
	ofstream out(filename);

	// Write out the header
	out << "<!DOCTYPE NETSCAPE-Bookmark-file-1>" << endl
	    << "<!-- This is an automatically generated file." << endl
	    << "It will be read and overwritten." << endl
	    << "Do Not Edit! -->" << endl;

	// Node 0 is the root node -- the one containing the title
	GtkCTree *ctree = GTK_CTREE(folder_tree);
	GtkCTreeNode *root = gtk_ctree_node_nth(ctree, 0);

	// I wanted to use gtk_ctree_node_get_text() for this, but it doesn't work
	gchar *title = GTK_CTREE_ROW(root)->row.cell->u.text;
	out << "<TITLE>" << title << "</TITLE>" << endl
	    << "<H1>" << title << "</H1>" << endl
	    << endl
	    << "<DL><p>" << endl;

	// We recurse the tree using a stack and our own bookmark data structure,
	// since the tree structure does not lend itself well toward mixing
	// folders and bookmarks (with the tree struture we are limited to putting
	// folders before bookmarks).
	int depth = 1;
	GtkCTreeNode *node = root;
	GtkCTreeNode *oldnode;
	GList *l = (GList *)gtk_ctree_node_get_row_data(
		ctree, GTK_CTREE_NODE(node));
	Bookmark *b;
	stack<GList*> s;
	while(depth > 0) {
		b = (Bookmark*)l->data;
		for(int j = 0; j < depth; j++) out << "    ";
		if(b->bookmark_type == FOLDER) {
			// Note: FOLDED == collapsed, FOLDER == expanded
			out << "<DT><H3 FOLDED ADD_DATE=\"" << b->created << "\">"
			    << b->text << "</H3>" << endl;
			for(int j = 0; j < depth; j++) out << "    ";
			out << "<DL><p>" << endl;
		    depth++;
			node = b->child;
			s.push(l);
			l = (GList*)gtk_ctree_node_get_row_data(ctree,
				GTK_CTREE_NODE(node));
		} else if(b->bookmark_type == SEPARATOR) {
			out << "<HR>" << endl;
			l = g_list_next(l);
		} else {
			out << "<DT><A HREF=\"" << b->location << "\" "
			    << "ADD_DATE=\"" << b->created << "\" "
			    << "LAST_VISITED=\"" << b->visited << "\" "
			    << "LAST_MODIFIED=\"" << b->modified << "\">"
		    	<< b->text << "</A>" << endl;
			l = g_list_next(l);
		}

		while(l == NULL) {
			depth--;
			for(int j = 0; j < depth; j++) out << "    ";
			out << "</DL><p>" << endl;
			if(s.size() == 0) break;
			l = s.top();
			s.pop();
			l = g_list_next(l);
		}
	}

	out.close();
}

void on_save_cancel(GtkWidget *w, gpointer data) {
	gtk_widget_hide(savefile_dlg);
}

// ------------------------------------------------------------
// Help functions
// ------------------------------------------------------------

void on_about_activate(GtkMenuItem *w, gpointer data) {
	gtk_widget_show(about_dlg);
}

void on_about_ok(GtkWidget *w, gpointer data) {
	gtk_widget_hide(about_dlg);
}

// ------------------------------------------------------------
// Exit routines
// ------------------------------------------------------------

gint on_main_window_delete_event(GtkWidget *w, gpointer data) {
	gtk_main_quit();
	return FALSE;
}

gint on_main_window_destroy_event(GtkWidget *w, gpointer data) {
	// glarea widgets don't get destroyed properly, but we don't have
	// any of those.
	gtk_main_quit();
	return FALSE;
}

void on_exit_activate(GtkWidget *w, gpointer data) {
	gtk_main_quit();
}

} // extern "C"

// ------------------------------------------------------------
// Main
// ------------------------------------------------------------

int main(int argc, char *argv[]) {
	GladeXML *xml;

	gtk_init(&argc, &argv);
	glade_init();

	xml = glade_xml_new("gtkbookmark.glade", NULL);
	if(!xml) {
		g_warning("Error creating the interface");
		exit(1);
	}

	init_widgets(xml);
	init_data();
	glade_xml_signal_autoconnect(xml);
	gtk_main();

	// We will never reach this point
	return 0;
}

