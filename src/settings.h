#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#include <glib.h>
#include "ptk-bookmarks.h"

typedef struct
{
    /* General Settings */
    gboolean singleInstance;
    char* encoding[ 32 ];
    gboolean showHiddenFiles;
    gboolean showSidePane;
    int sidePaneMode;
    gboolean showThumbnail;
    int maxThumbSize;

    int bigIconSize;
    int smallIconSize;

    /* char* iconTheme; */
    char* terminal;

    int openBookmarkMethod; /* 1: current tab, 2: new tab, 3: new window */
    int viewMode; /* icon view or detailed list view */
    int sortOrder; /* Sort by name, size, time */
    int sortType; /* ascending, descending */

    /* Window State */
    int splitterPos;
    int width;
    int height;

    /* Bookmarks */
    PtkBookmarks* bookmarks;
}
AppSettings;

extern AppSettings appSettings;

void load_settings();
void save_settings();
void free_settings();


#endif
