#include <cctype>
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <cstdlib>

#pragma warning(push, 0)
#include <FL/Enumerations.H>
#include <FL/Fl_Double_Window.H>
#pragma warning(pop)

#include "preferences.h"
#include "config.h"
#include "themes.h"
#include "widgets.h"
#include "utils.h"
#include "option-dialogs.h"

Option_Dialog::Option_Dialog(int w, const char *t) : _width(w), _title(t), _canceled(false),
	_dialog(NULL), _content(NULL), _ok_button(NULL), _cancel_button(NULL) {}

Option_Dialog::~Option_Dialog() {
	delete _dialog;
	delete _content;
	delete _ok_button;
	delete _cancel_button;
}

void Option_Dialog::initialize() {
	if (_dialog) { return; }
	Fl_Group *prev_current = Fl_Group::current();
	Fl_Group::current(NULL);
	// Populate dialog
	_dialog = new Fl_Double_Window(0, 0, 0, 0, _title);
	_content = new Fl_Group(0, 0, 0, 0);
	_content->begin();
	initialize_content();
	_content->end();
	_dialog->begin();
	_ok_button = new Default_Button(0, 0, 0, 0, "OK");
	_cancel_button = new OS_Button(0, 0, 0, 0, "Cancel");
	_dialog->end();
	// Initialize dialog
	_dialog->resizable(NULL);
	_dialog->callback((Fl_Callback *)cancel_cb, this);
	_dialog->set_modal();
	// Initialize dialog's children
	_ok_button->tooltip("OK (Enter)");
	_ok_button->callback((Fl_Callback *)close_cb, this);
	_cancel_button->shortcut(FL_Escape);
	_cancel_button->tooltip("Cancel (Esc)");
	_cancel_button->callback((Fl_Callback *)cancel_cb, this);
	Fl_Group::current(prev_current);
}

void Option_Dialog::refresh() {
	_canceled = false;
	_dialog->copy_label(_title);
	// Refresh widget positions and sizes
	int dy = 10;
	dy += refresh_content(_width - 20, dy) + 16;
#ifdef _WIN32
	_ok_button->resize(_width - 184, dy, 80, 22);
	_cancel_button->resize(_width - 90, dy, 80, 22);
#else
	_cancel_button->resize(_width - 184, dy, 80, 22);
	_ok_button->resize(_width - 90, dy, 80, 22);
#endif
	dy += _cancel_button->h() + 10;
	_dialog->size_range(_width, dy, _width, dy);
	_dialog->size(_width, dy);
	_dialog->redraw();
}

void Option_Dialog::show(const Fl_Widget *p) {
	initialize();
	refresh();
	int x = p->x() + (p->w() - _dialog->w()) / 2;
	int y = p->y() + (p->h() - _dialog->h()) / 2;
	_dialog->position(x, y);
	_ok_button->take_focus();
	_dialog->show();
	while (_dialog->shown()) { Fl::wait(); }
}

void Option_Dialog::close_cb(Fl_Widget *, Option_Dialog *od) {
	od->finish();
	od->_dialog->hide();
}

void Option_Dialog::cancel_cb(Fl_Widget *, Option_Dialog *od) {
	od->_canceled = true;
	od->_dialog->hide();
}

Map_Options_Dialog::Map_Options_Dialog(const char *t) : Option_Dialog(280, t), _max_tileset_name_length(0), _max_roof_name_length(0),
	_map_width(NULL), _map_height(NULL), _map_size(NULL), _tileset(NULL), _roof(NULL) {}

Map_Options_Dialog::~Map_Options_Dialog() {
	delete _map_width;
	delete _map_height;
	delete _map_size;
	delete _tileset;
	delete _roof;
}

const char *Map_Options_Dialog::tileset(void) const {
	if (!_tileset->mvalue()) { return NULL; }
	const char *t = _tileset->mvalue()->label();
	return original_name(t);
}

const char *Map_Options_Dialog::roof(void) const {
	if (!_roof->mvalue() || _roof->value() == 0) { return NULL; }
	return _roof->mvalue()->label();
}

const char *Map_Options_Dialog::original_name(const char *pretty_name) const {
	Dictionary::const_iterator it = _original_names.find(pretty_name);
	if (it == _original_names.end()) { return pretty_name; }
	return it->second.c_str();
}

inline static bool isupperordigit(int c) { return isupper(c) || isdigit(c); }
inline static int toconstant(int c) { return isalnum(c) ? toupper(c) : '_'; }

static void guess_map_constant(const char *name, char *constant) {
	char prev = (char)toconstant(*name);
	*constant++ = (char)toupper(*name++);

	const char *p = strchr(name, '.');
	size_t n = p ? p - name : strlen(name); // ignore extension and attribute data
	for (size_t i = 0; i < n; i++) {
		char c = *name;
		if ((islower(prev) && isupperordigit(c)) || // ...zA... -> ...Z_A...
			(i < n - 1 && isupperordigit(prev) && isupper(c) && islower(*(name+1)))) { // ...ZAb... -> ...Z_AB...
			*constant++ = '_';
		}
		prev = c;
		*constant++ = (char)toconstant(*name++);
	}
	*constant++ = ',';
	*constant = '\0';
}

bool Map_Options_Dialog::guess_map_size(const char *filename, const char *directory, Map_Attributes &attrs) {
	if (!filename) { return false; }

	size_t fs = file_size(filename);
	char buffer[32];
#ifdef __GNUC__
	sprintf(buffer, "(%zu B)", fs);
#else
	sprintf(buffer, "(%u B)", (uint32_t)fs);
#endif
	_map_size->copy_label(buffer);

	std::cmatch cm;
	std::regex rx(".+\\.([0-9]+)x([0-9]+)(?:\\.[A-Za-z0-9_-]+)?\\.[Bb][Ll][Kk]");
	std::regex_match(filename, cm, rx);
	size_t n = cm.size();
	if (n == 3) {
		std::string sw(cm[1]), sh(cm[2]);
		_map_width->value(std::stoi(sw));
		_map_height->value(std::stoi(sh));
		return true;
	}

	char map_constants[FL_PATH_MAX] = {};
	Config::map_constants_path(map_constants, directory);

	std::ifstream ifs(map_constants);
	if (!ifs.good()) { return false; }

	const char *name = fl_filename_name(filename);
	char constant[FL_PATH_MAX] = {};
	guess_map_constant(name, constant);

	attrs.group = 0;
	while (ifs.good()) {
		bool w_x_h = false;
		std::string line;
		std::getline(ifs, line);
		trim(line);
		if (starts_with(line, "newgroup") && line[strlen("newgroup")] != ':') {
			attrs.group++;
			continue;
		}
		else if (starts_with(line, "map_const")) {
			// "map_const": pokecrystal
			line.erase(0, strlen("map_const") + 1); // include next whitespace character
			w_x_h = true;
		}
		else if (starts_with(line, "mapgroup")) {
			// "mapgroup": pokecrystal pre-2018
			line.erase(0, strlen("mapgroup") + 1); // include next whitespace character
		}
		else if (starts_with(line, "mapconst")) {
			// "mapconst": pokered
			line.erase(0, strlen("mapconst") + 1); // include next whitespace character
		}
		else {
			continue;
		}
		if (!starts_with(line, constant)) { continue; }
		line.erase(0, strlen(constant));
		std::istringstream lss(line);
		int w, h;
		char comma;
		if (w_x_h) {
			lss >> w >> comma >> h;
		}
		else {
			lss >> h >> comma >> w;
		}
		if (1 <= w && w <= 255 && 1 <= h && h <= 255) {
			_map_width->value(w);
			_map_height->value(h);
			return true;
		}
		return false;
	}

	return false;
}

std::string Map_Options_Dialog::guess_map_tileset(const char *filename, const char *directory, Map_Attributes &attrs) {
	if (!filename) { return ""; }

	std::cmatch cm;
	std::regex rx(".+\\.([A-Za-z0-9_-]+)\\.[Bb][Ll][Kk]");
	std::regex_match(filename, cm, rx);
	size_t n = cm.size();
	if (n == 2) {
		return cm[1].str();
	}

	const char *name = fl_filename_name(filename);
	char map_name[FL_PATH_MAX] = {};
	strcpy(map_name, name);
	char *dot = strchr(map_name, '.');
	if (dot) { *dot = '\0'; }

	char map_headers[FL_PATH_MAX] = {};
	bool map_headers_exist = Config::map_headers_path(map_headers, directory);

	if (map_headers_exist) {
		std::ifstream ifs(map_headers);
		if (!ifs.good()) { return ""; }

		while (ifs.good()) {
			std::string line;
			std::getline(ifs, line);
			remove_comment(line);
			trim(line);

			std::istringstream lss(line);

			std::string macro;
			lss >> macro;
			if (macro != "map_header" && macro != "map") { continue; }

			std::string map_label;
			std::getline(lss, map_label, ',');
			trim(map_label);
			if (map_label != map_name) { continue; }

			std::string tileset_name;
			std::getline(lss, tileset_name, ',');
			trim(tileset_name);
			if (starts_with(tileset_name, "TILESET_")) {
				tileset_name.erase(0, strlen("TILESET_"));
				std::transform(tileset_name.begin(), tileset_name.end(), tileset_name.begin(), tolower);
			}
			else if (starts_with(tileset_name, "$")) {
				tileset_name.erase(0, 1);
				int ti = std::stoi(tileset_name, NULL, 16);
				char tileset_num[16] = {};
				sprintf(tileset_num, "%02d", ti);
				tileset_name = tileset_num;
			}
			else if (std::all_of(tileset_name.begin(), tileset_name.end(), isdigit)) {
				if (tileset_name.length() == 1) {
					tileset_name = "0" + tileset_name;
				}
			}
			else {
				tileset_name.erase();
			}

			std::string environment;
			std::getline(lss, environment, ',');
			trim(environment);
			attrs.environment = environment;

			std::string landmark;
			std::getline(lss, landmark, ',');
			trim(landmark);
			std::transform(landmark.begin(), landmark.end(), landmark.begin(), tolower);
			attrs.landmark = landmark;

			std::string skip_token;
			std::getline(lss, skip_token, ','); // music
			std::getline(lss, skip_token, ','); // phone service flag

			std::string palette;
			std::getline(lss, palette, ',');
			trim(palette);
			attrs.palette = palette;

			return tileset_name;
		}
	}
	else {
		char map_header[FL_PATH_MAX] = {};
		Config::map_header_path(map_header, directory, map_name);

		std::ifstream ifs(map_header);
		if (!ifs.good()) { return ""; }

		while (ifs.good()) {
			std::string line;
			std::getline(ifs, line);
			remove_comment(line);
			trim(line);

			std::istringstream lss(line);

			std::string db;
			lss >> db;
			if (db != "db") { continue; }

			std::string tileset_name;
			lss >> tileset_name;
			std::transform(tileset_name.begin(), tileset_name.end(), tileset_name.begin(), tolower);
			return tileset_name;
		}
	}

	return "";
}

void Map_Options_Dialog::guess_tileset_names(const char *directory, Dictionary &pretty_names, Dictionary &guessable_names) {
	char tileset_constants[FL_PATH_MAX] = {};
	Config::tileset_constants_path(tileset_constants, directory);

	std::ifstream ifs(tileset_constants);
	if (!ifs.good()) { return; }

	int id = 1;
	char original[16] = {};
	while (ifs.good()) {
		std::string line;
		std::getline(ifs, line);
		std::istringstream lss(line);
		std::string token;
		lss >> token;
		if (token != "const") { continue; }
		lss >> token;
		if (starts_with(token, "TILESET_")) { token.erase(0, strlen("TILESET_")); }
		sprintf(original, "%02d", id++);
		std::string pretty = original + (": " + token);
		pretty_names[original] = pretty;
		std::string guessable = token;
		std::transform(guessable.begin(), guessable.end(), guessable.begin(), tolower);
		guessable_names[original] = guessable;
	}

	return;
}

std::string Map_Options_Dialog::add_tileset(const char *t, int ext_len, const Dictionary &pretty_names) {
	std::string v(t);
	v.erase(v.size() - ext_len, ext_len);
	std::string p(v);

	if (p.length() == 2 && isdigit(p[0]) && isdigit(p[1])) {
		Dictionary::const_iterator it = pretty_names.find(p);
		if (it != pretty_names.end()) {
			p = it->second;
			_original_names[p] = it->first;
		}
	}

	if (_tileset->find_index(p.c_str()) == -1) {
		_tileset->add(p.c_str());
		int m = text_width(p.c_str(), 6);
		_max_tileset_name_length = MAX(m, _max_tileset_name_length);
	}

	return v;
}

std::string Map_Options_Dialog::add_roof(const char *r, int ext_len) {
	std::string v(r);
	v.erase(v.size() - ext_len, ext_len);

	if (_roof->find_index(v.c_str()) == -1) {
		_roof->add(v.c_str());
		int m = text_width(v.c_str(), 6);
		_max_roof_name_length = MAX(m, _max_roof_name_length);
	}

	return v;
}

bool Map_Options_Dialog::limit_blk_options(const char *filename, const char *directory, Map_Attributes &attrs) {
	initialize();

	// Initialize map size
	if (!guess_map_size(filename, directory, attrs)) {
		_map_width->value(1);
		_map_height->value(1);
	}

	// Initialize tileset choices

	_tileset->clear();
	_max_tileset_name_length = 0;

	char tileset_directory[FL_PATH_MAX] = {};
	strcpy(tileset_directory, directory);
	strcat(tileset_directory, Config::gfx_tileset_dir());

	dirent **list;
	int n = fl_filename_list(tileset_directory, &list);
	if (n < 0) { return false; }

	std::string guessed_tileset_name = guess_map_tileset(filename, directory, attrs);
	Dictionary pretty_names, guessable_names;
	guess_tileset_names(directory, pretty_names, guessable_names);
	int v = 0;
	for (int i = 0; i < n; i++) {
		const char *name = list[i]->d_name;
		if (ends_with(name, ".colored.png")) { continue; } // ignore utils/metatiles.py renders
		int ext_len = ends_with(name, ".2bpp.lz") ? 8 :
			          ends_with(name, ".2bpp.unique.lz") ? 15 : // for Red++ 3.0's generic+unique tilesets
			          ends_with(name, ".png") ? 4 : 0;
		if (ext_len) {
			std::string guessable_name(add_tileset(name, ext_len, pretty_names));
			if (guessed_tileset_name == guessable_name) { v = _tileset->size() - 2; } // ignore terminating NULL
			Dictionary::const_iterator it = guessable_names.find(guessable_name);
			if (it != guessable_names.end()) {
				guessable_name = it->second;
			}
			if (guessed_tileset_name == guessable_name) { v = _tileset->size() - 2; } // ignore terminating NULL
		}
	}
	_tileset->value(v);

	fl_filename_free_list(&list, n);

	// Initialize roof choices

	_roof->clear();
	_roof->add("(none)");
	_roof->value(0);
	_max_roof_name_length = text_width("(none)", 6);

	char roof_directory[FL_PATH_MAX] = {};
	strcpy(roof_directory, directory);
	strcat(roof_directory, Config::gfx_roof_dir());

	n = fl_filename_list(roof_directory, &list);
	if (n >= 0) {
		for (int i = 0; i < n; i++) {
			const char *name = list[i]->d_name;
			int ext_len = ends_with(name, ".2bpp") ? 5 :
						  ends_with(name, ".png") ? 4 : 0;
			if (ext_len) {
				add_roof(name, ext_len);
			}
		}
		fl_filename_free_list(&list, n);
	}

	if (num_roofs() > 0) {
		_roof->activate();
	}
	else {
		_roof->deactivate();
	}

	return true;
}

void Map_Options_Dialog::initialize_content() {
	// Populate content group
	_map_width = new OS_Spinner(0, 0, 0, 0, "Width:");
	_map_height = new OS_Spinner(0, 0, 0, 0, "Height:");
	_map_size = new Label(0, 0, 0, 0);
	_tileset = new Dropdown(0, 0, 0, 0, "Tileset:");
	_roof = new Dropdown(0, 0, 0, 0, "Roof:");
	// Initialize content group's children
	_map_width->align(FL_ALIGN_LEFT);
	_map_width->range(1, 255);
	_map_height->align(FL_ALIGN_LEFT);
	_map_height->range(1, 255);
	_tileset->align(FL_ALIGN_LEFT);
	_roof->align(FL_ALIGN_LEFT);
	// Initialize data
	_original_names.clear();
}

int Map_Options_Dialog::refresh_content(int ww, int dy) {
	int wgt_w = 0, wgt_h = 22, win_m = 10, wgt_m = 4;
	int ch = wgt_h + wgt_m + wgt_h + wgt_m + wgt_h;
	_content->resize(win_m, dy, ww, ch);

	int wgt_off = win_m + MAX(text_width(_map_width->label(), 2), text_width(_tileset->label(), 2));

	wgt_w = text_width("999", 2) + wgt_h;
	_map_width->resize(wgt_off, dy, wgt_w, wgt_h);
	_map_height->resize(_map_width->x() + _map_width->w() + 10 + text_width("Height:"), dy, wgt_w, wgt_h);

	int mso = _map_height->x() + _map_height->w();
	_map_size->resize(mso + 10, dy, ww - mso, wgt_h);
	dy += _map_height->h() + wgt_m;

	wgt_w = _max_tileset_name_length + wgt_h;
	_tileset->resize(wgt_off, dy, wgt_w, wgt_h);
	dy += _tileset->h() + wgt_m;

	wgt_w = _max_roof_name_length + wgt_h;
	_roof->resize(wgt_off, dy, wgt_w, wgt_h);

	return ch;
}

class Anchor_Button : public OS_Button {
private:
	int _anchor;
public:
	Anchor_Button(int a) : OS_Button(0, 0, 0, 0), _anchor(a) {
		type(FL_RADIO_BUTTON);
		align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);
	}
	inline int anchor(void) const { return _anchor; }
};

Resize_Dialog::Resize_Dialog(const char *t) : Option_Dialog(220, t), _map_width(NULL), _map_height(NULL), _anchor_buttons() {}

Resize_Dialog::~Resize_Dialog() {
	delete _map_width;
	delete _map_height;
	for (int i = 0; i < 9; i++) { delete _anchor_buttons[i]; }
}

Resize_Dialog::Hor_Align Resize_Dialog::horizontal_anchor() const {
	if (_anchor_buttons[0]->value() || _anchor_buttons[3]->value() || _anchor_buttons[6]->value()) { return LEFT; }
	if (_anchor_buttons[2]->value() || _anchor_buttons[5]->value() || _anchor_buttons[8]->value()) { return RIGHT; }
	return CENTER;
}

Resize_Dialog::Vert_Align Resize_Dialog::vertical_anchor() const {
	if (_anchor_buttons[0]->value() || _anchor_buttons[1]->value() || _anchor_buttons[2]->value()) { return TOP; }
	if (_anchor_buttons[6]->value() || _anchor_buttons[7]->value() || _anchor_buttons[8]->value()) { return BOTTOM; }
	return MIDDLE;
}

int Resize_Dialog::anchor() const {
	for (int i = 0; i < 9; i++) {
		if (_anchor_buttons[i]->value()) { return i; }
	}
	return 4;
}

void Resize_Dialog::anchor(int a) {
	_anchor_buttons[a]->do_callback();
}

void Resize_Dialog::anchor_label(int x, int y, const char *l) {
	if (0 <= x && x < 3 && 0 <= y && y < 3) {
		_anchor_buttons[y * 3 + x]->label(l);
	}
}

void Resize_Dialog::initialize_content() {
	// Populate content group
	_map_width = new OS_Spinner(0, 0, 0, 0, "Width:");
	_map_height = new OS_Spinner(0, 0, 0, 0, "Height:");
	for (int i = 0; i < 9; i++) {
		Anchor_Button *ab = new Anchor_Button(i);
		ab->callback((Fl_Callback *)anchor_button_cb, this);
		_anchor_buttons[i] = ab;
	}
	// Initialize content group's children
	_map_width->align(FL_ALIGN_LEFT);
	_map_width->range(1, 255);
	_map_height->align(FL_ALIGN_LEFT);
	_map_height->range(1, 255);
	anchor(Preferences::get("resize-anchor", 4));
}

int Resize_Dialog::refresh_content(int ww, int dy) {
	int wgt_w = 0, wgt_h = 22, win_m = 10, wgt_m = 4;
	int ch = (wgt_h + wgt_m) * 2 + wgt_h;
	_content->resize(win_m, dy, ww, ch);

	int wgt_off = win_m + MAX(text_width(_map_width->label(), 2), text_width(_map_height->label(), 2));

	wgt_w = text_width("999", 2) + wgt_h;
	_map_width->resize(wgt_off, dy, wgt_w, wgt_h);
	_map_height->resize(wgt_off, _map_width->y() + _map_width->h() + wgt_m, wgt_w, wgt_h);
	wgt_off += wgt_w + 20;

	wgt_w = wgt_h = 24;
	wgt_m = 2;
	for (int ay = 0; ay < 3; ay++) {
		for (int ax = 0; ax < 3; ax++) {
			_anchor_buttons[ay*3+ax]->resize(wgt_off + (wgt_w + wgt_m) * ax, dy, wgt_w, wgt_h);
		}
		dy += wgt_h + wgt_m;
	}

	return ch;
}

void Resize_Dialog::anchor_button_cb(Anchor_Button *ab, Resize_Dialog *rd) {
	ab->setonly();
	for (int i = 0; i < 9; i++) {
		rd->_anchor_buttons[i]->label(NULL);
	}
	int a = ab->anchor();
	int y = a / 3, x = a % 3;
	rd->anchor_label(x - 1, y - 1, "@7>"); // top-left
	rd->anchor_label(x,     y - 1, "@8>"); // top
	rd->anchor_label(x + 1, y - 1, "@9>"); // top-right
	rd->anchor_label(x - 1,  y,    "@4>"); // left
	rd->anchor_label(x,      y,    "@-2square"); // center
	rd->anchor_label(x + 1,  y,    "@>");  // right
	rd->anchor_label(x - 1, y + 1, "@1>"); // bottom-left
	rd->anchor_label(x,     y + 1, "@2>"); // bottom
	rd->anchor_label(x + 1, y + 1, "@3>"); // bottom-right
	rd->_dialog->redraw();
}

Add_Sub_Dialog::Add_Sub_Dialog(const char *t) : Option_Dialog(194, t), _num_metatiles(NULL) {}

Add_Sub_Dialog::~Add_Sub_Dialog() {
	delete _num_metatiles;
}

void Add_Sub_Dialog::initialize_content() {
	// Populate content group
	_num_metatiles = new OS_Spinner(0, 0, 0, 0, "Blocks:");
	// Initialize content group's children
	_num_metatiles->align(FL_ALIGN_LEFT);
	_num_metatiles->range(1, 256);
}

int Add_Sub_Dialog::refresh_content(int ww, int dy) {
	int wgt_h = 22, win_m = 10;
	_content->resize(win_m, dy, ww, wgt_h);

	int wgt_off = win_m + text_width(_num_metatiles->label(), 2);
	int wgt_w = text_width("999", 2) + wgt_h;
	_num_metatiles->resize(wgt_off, dy, wgt_w, wgt_h);

	return wgt_h;
}

Tileset_Options_Dialog::Tileset_Options_Dialog(const char *t, Map_Options_Dialog *mod) : Option_Dialog(280, t),
	_tileset(NULL), _map_options_dialog(mod) {}

Tileset_Options_Dialog::~Tileset_Options_Dialog() {
	delete _tileset;
}

bool Tileset_Options_Dialog::limit_tileset_options(const char *old_tileset_name) {
	if (!_map_options_dialog) { return false; }
	initialize();
	// Copy _map_options_dialog's tileset choices
	_tileset->clear();
	int n = _map_options_dialog->_tileset->size() - 1; // ignore terminating NULL
	int v = 0;
	for (int i = 0; i < n; i++) {
		const Fl_Menu_Item &item = _map_options_dialog->_tileset->menu()[i];
		const char *name = item.label();
		const char *original_name = _map_options_dialog->original_name(name);
		if (!strcmp(old_tileset_name, original_name)) { v = i; }
		_tileset->add(name);
	}
	_tileset->value(v);
	return true;
}

const char *Tileset_Options_Dialog::tileset(void) const {
	if (!_tileset->mvalue()) { return NULL; }
	const char *t = _tileset->mvalue()->label();
	Dictionary::const_iterator it = _map_options_dialog->_original_names.find(t);
	if (it == _map_options_dialog->_original_names.end()) { return t; }
	return it->second.c_str();
}

void Tileset_Options_Dialog::initialize_content() {
	// Populate content group
	_tileset = new Dropdown(0, 0, 0, 0, "Tileset:");
	// Initialize content group's children
	_tileset->align(FL_ALIGN_LEFT);
}

int Tileset_Options_Dialog::refresh_content(int ww, int dy) {
	int wgt_w = 0, wgt_h = 22, win_m = 10;
	int ch = wgt_h;
	_content->resize(win_m, dy, ww, ch);

	int wgt_off = win_m + text_width(_tileset->label(), 2);

	wgt_w = _map_options_dialog->_max_tileset_name_length + wgt_h;
	_tileset->resize(wgt_off, dy, wgt_w, wgt_h);

	return ch;
}

Roof_Options_Dialog::Roof_Options_Dialog(const char *t, Map_Options_Dialog *mod) : Option_Dialog(280, t),
	_roof(NULL), _map_options_dialog(mod) {}

Roof_Options_Dialog::~Roof_Options_Dialog() {
	delete _roof;
}

bool Roof_Options_Dialog::limit_roof_options(const char *old_roof_name) {
	if (!_map_options_dialog) { return false; }
	initialize();
	// Copy _map_options_dialog's roof choices
	_roof->clear();
	int n = _map_options_dialog->_roof->size() - 1; // ignore terminating NULL
	int v = 0;
	for (int i = 0; i < n; i++) {
		const Fl_Menu_Item &item = _map_options_dialog->_roof->menu()[i];
		const char *name = item.label();
		if (old_roof_name && !strcmp(old_roof_name, name)) { v = i; }
		_roof->add(name);
	}
	_roof->value(v);
	return true;
}

const char *Roof_Options_Dialog::roof(void) const {
	if (!_roof->mvalue() || !_roof->value()) { return NULL; }
	return _roof->mvalue()->label();
}

void Roof_Options_Dialog::initialize_content() {
	// Populate content group
	_roof = new Dropdown(0, 0, 0, 0, "Roof:");
	// Initialize content group's children
	_roof->align(FL_ALIGN_LEFT);
}

int Roof_Options_Dialog::refresh_content(int ww, int dy) {
	int wgt_w = 0, wgt_h = 22, win_m = 10;
	int ch = wgt_h;
	_content->resize(win_m, dy, ww, ch);

	int wgt_off = win_m + text_width(_roof->label(), 2);

	wgt_w = _map_options_dialog->_max_roof_name_length + wgt_h;
	_roof->resize(wgt_off, dy, wgt_w, wgt_h);

	return ch;
}
