#include <cstdio>
#include <cstring>

#pragma warning(push, 0)
#include <FL/filename.H>
#include <FL/Fl_Preferences.H>
#pragma warning(pop)

#include "version.h"
#include "utils.h"
#include "config.h"

static char *trim_suffix(const char *s) {
	// remove trailing ".t#", e.g. tileset "overworld.t2" -> name "overworld"
#ifdef _WIN32
	char *t = _strdup(s);
#else
	char *t = strdup(s);
#endif
	char *dot = strchr(t, '.');
	if (dot) { *dot = '\0'; }
	return t;
}

Fl_Preferences Config::global_config(Fl_Preferences::USER, PROGRAM_AUTHOR, PROGRAM_NAME);

bool Config::_monochrome = false, Config::_256_tiles = false;

int Config::get(const char *key, int default_) {
	int value;
	global_config.get(key, value, default_);
	return value;
}

void Config::set(const char *key, int value) {
	global_config.set(key, value);
}

const char *Config::gfx_tileset_dir() {
	return "gfx" DIR_SEP "tilesets" DIR_SEP;
}

const char *Config::palette_macro() {
	return "\ttilepal";
}

bool Config::project_path_from_blk_path(const char *blk_path, char *project_path) {
	if (!dir_name(blk_path, project_path)) {
		return false;
	}
	int depth = -1;
	for (char *c = project_path; *c; c++) {
		depth += *c == '/' || *c == '\\';
	}
	char main_asm[FL_PATH_MAX] = {};
	for (int i = 0; i < depth; i++) {
		strcpy(main_asm, project_path);
		strcat(main_asm, "main.asm");
		if (file_exists(main_asm)) { // the project directory contains main.asm
			return true;
		}
		strcat(project_path, ".." DIR_SEP); // go up a level
	}
	return false;
}

void Config::palette_map_path(char *dest, const char *root, const char *tileset) {
	if (monochrome()) { return; }
	// try gfx/tilesets/*_palette_map.asm (pokecrystal)
	sprintf(dest, "%s%s%s_palette_map.asm", root, gfx_tileset_dir(), tileset);
	if (file_exists(dest)) { return; }
	// try color/tilesets/*.asm (Red++ 3)
	char *name = trim_suffix(tileset);
	sprintf(dest, "%scolor" DIR_SEP "tilesets" DIR_SEP "%s.asm", root, name);
	free(name);
	if (file_exists(dest)) { return; }
	// last resort: tilesets/*_palette_map.asm (old pokecrystal)
	sprintf(dest, "%stilesets" DIR_SEP "%s_palette_map.asm", root, tileset);
}

void Config::tileset_path(char *dest, const char *root, const char *tileset) {
	// try gfx/tilesets/*.png
	sprintf(dest, "%s%s%s.png", root, gfx_tileset_dir(), tileset);
	if (file_exists(dest)) { return; }
	// try gfx/tilesets/*.2bpp
	sprintf(dest, "%s%s%s.2bpp", root, gfx_tileset_dir(), tileset);
	if (file_exists(dest)) { return; }
	// last resort: gfx/tilesets/*.2bpp.lz
	sprintf(dest, "%s%s%s.2bpp.lz", root, gfx_tileset_dir(), tileset);
}

void Config::tileset_png_path(char *dest, const char *root, const char *tileset) {
	sprintf(dest, "%s%s%s.png", root, gfx_tileset_dir(), tileset);
}

void Config::metatileset_path(char *dest, const char *root, const char *tileset) {
	// try data/tilesets/*_metatiles.bin (pokecrystal)
	sprintf(dest, "%sdata" DIR_SEP "tilesets" DIR_SEP "%s_metatiles.bin", root, tileset);
	if (file_exists(dest)) { return; }
	// try gfx/blocksets/*.bst (pokered)
	char *name = trim_suffix(tileset);
	sprintf(dest, "%sgfx" DIR_SEP "blocksets" DIR_SEP "%s.bst", root, name);
	free(name);
	if (file_exists(dest)) { return; }
	// last resort: tilesets/*_metatiles.bin (old pokecrystal)
	sprintf(dest, "%stilesets" DIR_SEP "%s_metatiles.bin", root, tileset);
}

void Config::map_constants_path(char *dest, const char *root) {
	// try constants/map_dimension_constants.asm (Prism)
	sprintf(dest, "%sconstants" DIR_SEP "map_dimension_constants.asm", root);
	if (file_exists(dest)) { return; }
	// last resort: constants/map_constants.asm (pokecrystal, pokered)
	sprintf(dest, "%sconstants" DIR_SEP "map_constants.asm", root);
}

void Config::map_headers_path(char *dest, const char *root) {
	// try data/maps/maps.asm (pokecrystal)
	sprintf(dest, "%sdata" DIR_SEP "maps" DIR_SEP "maps.asm", root);
	if (file_exists(dest)) { return; }
	// last resort: maps/map_headers.asm (old pokecrystal)
	sprintf(dest, "%smaps" DIR_SEP "map_headers.asm", root);
}

void Config::tileset_constants_path(char *dest, const char *root) {
	// try constants/tileset_constants.asm (pokecrystal)
	sprintf(dest, "%sconstants" DIR_SEP "tileset_constants.asm", root);
	if (file_exists(dest)) { return; }
	// last resort: constants/tilemap_constants.asm (old pokecrystal)
	sprintf(dest, "%sconstants" DIR_SEP "tilemap_constants.asm", root);
}

void Config::bg_tiles_pal_path(char *dest, const char *root) {
	// try gfx/tilesets/bg_tiles.pal (pokecrystal)
	sprintf(dest, "%sgfx" DIR_SEP "tilesets" DIR_SEP "bg_tiles.pal", root);
	if (file_exists(dest)) { return; }
	// last resort: tilesets/bg.pal (old pokecrystal)
	sprintf(dest, "%stilesets" DIR_SEP "bg.pal", root);
}
