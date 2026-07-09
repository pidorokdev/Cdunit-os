/*
 * Dunit OS - GUI file associations
 */

#ifndef _GUI_FILE_ASSOC_H
#define _GUI_FILE_ASSOC_H

typedef enum gui_file_type {
  GUI_FILE_UNKNOWN = 0,
  GUI_FILE_DIRECTORY,
  GUI_FILE_TEXT,
  GUI_FILE_IMAGE,
  GUI_FILE_AUDIO,
  GUI_FILE_SCRIPT,
} gui_file_type_t;

gui_file_type_t gui_file_type(const char *path);
int gui_open_path(const char *path);

#endif /* _GUI_FILE_ASSOC_H */
