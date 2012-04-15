#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#include <glib.h>
#include <gdk/gdk.h>
#include "ptk-bookmarks.h"

typedef enum {
    WPM_STRETCH,
    WPM_FULL,
    WPM_CENTER,
    WPM_TILE
}WallpaperMode;

typedef struct
{
    /* General Settings */
    char encoding[ 32 ];
    gboolean show_hidden_files;
    gboolean show_side_pane;
    int side_pane_mode;
    gboolean show_thumbnail;
    int max_thumb_size;

    int big_icon_size;
    int small_icon_size;

    /* char* iconTheme; */
    char* terminal;

    int open_bookmark_method; /* 1: current tab, 2: new tab, 3: new window */
    int view_mode; /* icon view or detailed list view */
    int sort_order; /* Sort by name, size, time */
    int sort_type; /* ascending, descending */

    /* Window State */
    int splitter_pos;
    int width;
    int height;
    gboolean maximized;

    /* Desktop */
    gboolean show_desktop;
    gboolean show_wallpaper;
    char* wallpaper;
    WallpaperMode wallpaper_mode;
    int desktop_sort_by;
    int desktop_sort_type;
    gboolean show_wm_menu;
    GdkColor desktop_bg1;
    GdkColor desktop_bg2;
    GdkColor desktop_text;
    GdkColor desktop_shadow;

    /* Bookmarks */
    PtkBookmarks* bookmarks;
}
AppSettings;

extern AppSettings app_settings;

void load_settings();
void save_settings();
void free_settings();


#endif
