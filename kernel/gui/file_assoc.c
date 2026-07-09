/*
 * Dunit OS - GUI file associations
 */

#include "file_assoc.h"
#include "fs/vfs.h"
#include "types.h"

struct terminal;
struct window;

extern struct window *gui_create_file_manager_path(int x, int y,
                                                   const char *path);
extern struct window *gui_create_window(const char *title, int x, int y, int w,
                                        int h);
extern void gui_open_image_viewer(const char *path);
extern void gui_open_notepad(const char *path);
extern void gui_play_mp3_file(const char *path);
extern void gui_set_window_userdata(struct window *win, void *data);

extern struct terminal *term_create(int x, int y, int cols, int rows);
extern void term_execute_command(struct terminal *term, const char *cmd);
extern void term_puts(struct terminal *term, const char *str);
extern void term_set_active(struct terminal *term);
extern void term_set_content_pos(struct terminal *t, int x, int y);

static int str_ends_with_ci(const char *str, const char *suffix) {
  int len_str = 0;
  int len_suf = 0;

  while (str && str[len_str])
    len_str++;
  while (suffix && suffix[len_suf])
    len_suf++;

  if (!str || !suffix || len_suf > len_str)
    return 0;

  for (int i = 0; i < len_suf; i++) {
    char a = str[len_str - len_suf + i];
    char b = suffix[i];
    if (a >= 'A' && a <= 'Z')
      a += 32;
    if (b >= 'A' && b <= 'Z')
      b += 32;
    if (a != b)
      return 0;
  }
  return 1;
}

gui_file_type_t gui_file_type(const char *path) {
  if (!path || !path[0])
    return GUI_FILE_UNKNOWN;

  struct file *entry = vfs_open(path, O_RDONLY, 0);
  if (!entry)
    return GUI_FILE_UNKNOWN;

  int is_dir = entry->f_dentry && entry->f_dentry->d_inode &&
               S_ISDIR(entry->f_dentry->d_inode->i_mode);
  vfs_close(entry);
  if (is_dir)
    return GUI_FILE_DIRECTORY;

  if (str_ends_with_ci(path, ".txt") || str_ends_with_ci(path, ".md") ||
      str_ends_with_ci(path, ".c") || str_ends_with_ci(path, ".h"))
    return GUI_FILE_TEXT;

  if (str_ends_with_ci(path, ".jpg") || str_ends_with_ci(path, ".jpeg") ||
      str_ends_with_ci(path, ".png") || str_ends_with_ci(path, ".bmp"))
    return GUI_FILE_IMAGE;

  if (str_ends_with_ci(path, ".mp3"))
    return GUI_FILE_AUDIO;

  if (str_ends_with_ci(path, ".py") || str_ends_with_ci(path, ".nano"))
    return GUI_FILE_SCRIPT;

  return GUI_FILE_UNKNOWN;
}

static int gui_open_script(const char *path) {
  static int term_spawn_x = 120;
  static int term_spawn_y = 100;

  struct window *win =
      gui_create_window("Terminal", term_spawn_x, term_spawn_y, 500, 350);
  if (!win)
    return -1;

  int content_x = term_spawn_x + 2;
  int content_y = term_spawn_y + 30;
  struct terminal *term = term_create(content_x, content_y, 60, 18);
  if (!term)
    return -1;

  gui_set_window_userdata(win, term);
  term_set_active(term);
  term_set_content_pos(term, content_x, content_y);

  char run_cmd[300] = "run ";
  int j = 4;
  for (int i = 0; path[i] && j < (int)sizeof(run_cmd) - 1; i++)
    run_cmd[j++] = path[i];
  run_cmd[j] = '\0';

  term_execute_command(term, run_cmd);
  term_puts(term, "\n\033[32mroot@dunit\033[0m:\033[34m~\033[0m# ");

  term_spawn_x = (term_spawn_x + 40) % 300 + 80;
  term_spawn_y = (term_spawn_y + 35) % 200 + 70;
  return 0;
}

int gui_open_path(const char *path) {
  switch (gui_file_type(path)) {
  case GUI_FILE_DIRECTORY:
    return gui_create_file_manager_path(200, 100, path) ? 0 : -1;
  case GUI_FILE_TEXT:
    gui_open_notepad(path);
    return 0;
  case GUI_FILE_IMAGE:
    gui_open_image_viewer(path);
    return 0;
  case GUI_FILE_AUDIO:
    gui_play_mp3_file(path);
    return 0;
  case GUI_FILE_SCRIPT:
    return gui_open_script(path);
  case GUI_FILE_UNKNOWN:
  default:
    return -1;
  }
}
