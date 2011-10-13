#include <iostream>
#include <fstream>
#include <string>
#include <stack>
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

struct Bookmark {
	char *location;
	char *visited;
	char *created;
	char *modified;
};

extern "C" {

void init_widgets(GladeXML *xml) {
	folder_tree = glade_xml_get_widget(xml, "folder_tree");
	url_list = glade_xml_get_widget(xml, "url_list");
	statusbar = glade_xml_get_widget(xml, "statusbar");
	loadfile_dlg = glade_xml_get_widget(xml, "loadfile_dlg");
	savefile_dlg = glade_xml_get_widget(xml, "savefile_dlg");
	about_dlg = glade_xml_get_widget(xml, "about_dlg");

	// Glade doesn't support word wrap
	GtkWidget *about_text = glade_xml_get_widget(xml, "about_text");
	gtk_text_set_word_wrap(GTK_TEXT(about_text), TRUE);
}

// ------------------------------------------------------------
// Tree browsing routines
// ------------------------------------------------------------
void on_tree_item_select(GtkWidget *w, gpointer data) {
	GtkCList *clist = GTK_CLIST(url_list);
	GtkTree *tree = GTK_TREE(GTK_TREE_ITEM_SUBTREE(w));

	// Clear the url list
	gtk_clist_clear(clist);

	// Freeze the list so we don't update one-at-a-time
	gtk_clist_freeze(clist);

	// Add the data items in the tree to the list
	GList *items = tree->children;
	gchar *text[] = {NULL, NULL, NULL, NULL};
	Bookmark *b;
	for(GList *item = g_list_first(items); item != g_list_last(items);
		item = g_list_next(item)) {

		// Don't add the current element if it is a subtree
		if(GTK_TREE_ITEM_SUBTREE(item->data) != NULL) continue;

		gtk_label_get(GTK_LABEL(GTK_BIN(item->data)->child), &text[0]);
		b = (Bookmark *)gtk_object_get_data(GTK_OBJECT(item->data),
			"bookmark");
		text[1] = b->location;
		text[2] = b->visited;
		text[3] = b->created;
		gtk_clist_append(clist, text);
	}

	// Now thaw the list, and we are done
	gtk_clist_thaw(clist);	
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
		*dest = new char[t.size() + 1];
		strcpy(*dest, t.c_str());
	} else {
		*dest = NULL;
	}
}

void on_load_ok(GtkWidget *w, gpointer data) {
	gtk_widget_hide(loadfile_dlg);

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
	while(str.substr(0, 4) != "<DL>") getline(in, str);

	// Lastly, read the file, adding items to the tree as necessary
	// This is basically an over-simplified XML parser, so we could
	// use libxml for this if we wanted.
	GtkTree *tree = GTK_TREE(folder_tree);
	stack<GtkTree*> s;
	GtkWidget *item;
	Bookmark *b;
	for(;;) {
		getline(in, str);
		string t = str.substr(str.find_first_not_of(" \t"));
		if(t.substr(0, 4) == "<DT>") {
			// Data item
			// FIX ME!! This is highly unoptimized.
			b = new Bookmark;

			get_substr("A HREF=", str, &b->location);
			get_substr("ADD_DATE=", str, &b->created);
			get_substr("LAST_VISIT=", str, &b->visited);
			get_substr("LAST_MODIFIED=", str, &b->modified);

			// Get the title
			t = t.substr(0, t.find_last_of("<"));
			t = t.substr(t.find_last_of(">") + 1);
			gchar *label = (gchar*)t.c_str();
			item = gtk_tree_item_new_with_label(label);
			gtk_object_set_data(GTK_OBJECT(item), "bookmark", b);
			gtk_tree_append(tree, item);
		} else if(t.substr(0, 4) == "<DL>") {
			// New subtree
			GtkWidget *subtree = gtk_tree_new();
			gtk_tree_item_set_subtree(GTK_TREE_ITEM(item), subtree);
			gtk_widget_show(item);

			// Add a signal so we know when the item has been
			// selected.
			gtk_signal_connect(GTK_OBJECT(item), "select",
				GTK_SIGNAL_FUNC(on_tree_item_select),
				NULL);

			// FIX ME!! It would be nice if we could set the tree
			// to be collapsed by default.

			// Save the parent tree and set the new one as current
			s.push(tree);
			tree = GTK_TREE(subtree);
		} else if(t.substr(0, 5) == "</DL>") {
			// Close off old subtree
			gtk_widget_show(GTK_WIDGET(tree));
			if(s.size() == 0) break;
			tree = s.top();
			s.pop();
		} else if(t.substr(0, 4) == "<HR>") {
			// Separator
		} else {
			g_warning("Invalid bookmark data encountered.");
			g_print("%s\n", t.c_str());
		}
	}
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

main(int argc, char *argv[]) {
	GladeXML *xml;

	gtk_init(&argc, &argv);
	glade_init();

	xml = glade_xml_new("gtkbookmark.glade", NULL);
	if(!xml) {
		g_warning("Error creating the interface");
		exit(1);
	}

	init_widgets(xml);
	glade_xml_signal_autoconnect(xml);
	gtk_main();

	// We will never reach this point
	return 0;
}

