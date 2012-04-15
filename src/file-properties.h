#ifndef _FILE_PROPERTIES_H_
#define _FILE_PROPERTIES_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* Convert file size in bytes to a human readable string */
void file_size_to_string( char* buf, guint64 size );

GtkWidget* file_properties_dlg_new( GtkWindow* parent,
                                    GList* sel_files,
                                    GList* sel_mime_types );

void on_filePropertiesDlg_response          (GtkDialog       *dialog,
                                             gint             response_id,
                                             gpointer         user_data);

G_END_DECLS

#endif

