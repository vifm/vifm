#ifndef __KEYS_BUILDIN_M_H__
#define __KEYS_BUILDIN_M_H__

#include "menus.h"
#include "ui.h"

void init_buildin_m_keys(int *key_mode);
void enter_menu_mode(menu_info *m, FileView *active_view);
void menu_pre(void);
void menu_post(void);
void execute_menu_command(FileView *view, char *command, menu_info *ptr);

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
