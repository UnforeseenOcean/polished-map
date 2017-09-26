#ifndef VIZ_WINDOW_H
#define VIZ_WINDOW_H

#pragma warning(push, 0)
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Native_File_Chooser.H>
#pragma warning(pop)

#include "utils.h"
#include "widgets.h"
#include "modal-dialog.h"
#include "option-dialogs.h"
#include "metatileset.h"
#include "map.h"
#include "help-window.h"
#include "block-window.h"

#define METATILES_PER_ROW 4
#define METATILE_PX_SIZE (TILE_SIZE * METATILE_SIZE)

class Main_Window : public Fl_Double_Window {
private:
	// GUI containers
	Fl_Menu_Bar *_menu_bar;
	Toolbar *_toolbar;
	Workspace *_sidebar, *_map_scroll;
	Toolbar *_status_bar;
	Fl_Group *_map_group;
	// GUI inputs
	DnD_Receiver *_dnd_receiver;
	Fl_Menu_Item *_aero_theme_mi, *_metro_theme_mi, *_greybird_theme_mi, *_blue_theme_mi, *_dark_theme_mi;
	Fl_Menu_Item *_grid_mi, *_zoom_mi, *_ids_mi, *_hex_mi, *_full_screen_mi, *_prism_mi;
	Toolbar_Button *_new_tb, *_open_tb, *_save_tb, *_print_tb, *_undo_tb, *_redo_tb, *_resize_tb;
	Toolbar_Toggle_Button *_grid_tb, *_zoom_tb, *_ids_tb, *_hex_tb;
	// GUI outputs
	Status_Bar_Field *_metatile_count, *_map_dimensions, *_hover_x, *_hover_y, *_hover_id;
	// Dialogs
	Fl_Native_File_Chooser *_blk_open_chooser, *_blk_save_chooser, *_new_dir_chooser, *_png_chooser;
	Modal_Dialog *_error_dialog, *_warning_dialog, *_success_dialog, *_unsaved_dialog, *_about_dialog;
	Map_Options_Dialog *_map_options_dialog;
	Resize_Dialog *_resize_dialog;
	Help_Window *_help_window;
	Block_Window *_block_window;
	// Data
	std::string _directory, _blk_file;
	Metatileset _metatileset;
	Map _map;
	// Metatile button properties
	Metatile_Button *_metatile_buttons[MAX_NUM_METATILES];
	Metatile_Button *_selected;
	// Work properties
	bool _unsaved;
	// Window size cache
	int _wx, _wy, _ww, _wh;
#ifndef _WIN32
	// Window icons
	Pixmap _icon_pixmap, _icon_mask;
#endif
public:
	Main_Window(int x, int y, int w, int h, const char *l = NULL);
	~Main_Window();
	void show(void);
	inline bool grid(void) const { return _grid_mi && !!_grid_mi->value(); }
	inline bool zoom(void) const { return _zoom_mi && !!_zoom_mi->value(); }
	inline bool ids(void) const { return _ids_mi && !!_ids_mi->value(); }
	inline bool hex(void) const { return _hex_mi && !!_hex_mi->value(); }
	inline bool prism(void) const { return _prism_mi && !!_prism_mi->value(); }
	inline int metatile_size(void) const { return zoom() ? METATILE_PX_SIZE * 2 : METATILE_PX_SIZE; }
	inline bool unsaved(void) const { return _map.modified() || _metatileset.modified(); }
	const char *modified_filename(void);
	void draw_metatile(int x, int y, uint8_t id) const;
	void update_status(Block *b);
	void flood_fill(Block *b, uint8_t f, uint8_t t);
	void open_map(const char *filename);
private:
	void open_map(const char *directory, const char *filename);
	void resize_map(int w, int h);
	bool save_map(void);
	bool save_metatileset(void);
	void edit_metatile(Metatile *mt);
	void update_zoom(void);
	void update_labels(void);
	// Drag-and-drop
	static void drag_and_drop_cb(DnD_Receiver *dndr, Main_Window *mw);
	// File menu
	static void new_cb(Fl_Widget *w, Main_Window *mw);
	static void open_cb(Fl_Widget *w, Main_Window *mw);
	static void save_cb(Fl_Widget *w, Main_Window *mw);
	static void save_as_cb(Fl_Widget *w, Main_Window *mw);
	static void save_metatiles_cb(Fl_Widget *w, Main_Window *mw);
	static void close_cb(Fl_Widget *w, Main_Window *mw);
	static void print_cb(Fl_Widget *w, Main_Window *mw);
	static void exit_cb(Fl_Widget *w, Main_Window *mw);
	// Edit menu
	static void undo_cb(Fl_Widget *w, Main_Window *mw);
	static void redo_cb(Fl_Widget *w, Main_Window *mw);
	static void resize_cb(Fl_Widget *w, Main_Window *mw);
	// View menu
	static void aero_theme_cb(Fl_Menu_ *m, Main_Window *mw);
	static void metro_theme_cb(Fl_Menu_ *m, Main_Window *mw);
	static void greybird_theme_cb(Fl_Menu_ *m, Main_Window *mw);
	static void blue_theme_cb(Fl_Menu_ *m, Main_Window *mw);
	static void dark_theme_cb(Fl_Menu_ *m, Main_Window *mw);
	static void grid_cb(Fl_Menu_ *m, Main_Window *mw);
	static void zoom_cb(Fl_Menu_ *m, Main_Window *mw);
	static void ids_cb(Fl_Menu_ *m, Main_Window *mw);
	static void hex_cb(Fl_Menu_ *m, Main_Window *mw);
	static void full_screen_cb(Fl_Menu_ *m, Main_Window *mw);
	// Options menu
	static void prism_cb(Fl_Menu_ *m, Main_Window *mw);
	// Toolbar buttons
	static void grid_tb_cb(Toolbar_Toggle_Button *tb, Main_Window *mw);
	static void zoom_tb_cb(Toolbar_Toggle_Button *tb, Main_Window *mw);
	static void ids_tb_cb(Toolbar_Toggle_Button *tb, Main_Window *mw);
	static void hex_tb_cb(Toolbar_Toggle_Button *tb, Main_Window *mw);
	// Help menu
	static void help_cb(Fl_Widget *w, Main_Window *mw);
	static void about_cb(Fl_Widget *w, Main_Window *mw);
	// Metatiles sidebar
	static void select_metatile_cb(Metatile_Button *mb, Main_Window *mw);
	// Map
	static void change_block_cb(Block *b, Main_Window *mw);
};

#endif
