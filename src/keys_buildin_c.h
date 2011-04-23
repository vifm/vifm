#ifndef __KEYS_BUILDIN_C_H__
#define __KEYS_BUILDIN_C_H__

enum CmdLineSubModes {
	CMD_SUBMODE,
	MENU_CMD_SUBMODE,
	SEARCH_FORWARD_SUBMODE,
	SEARCH_BACKWARD_SUBMODE,
	MENU_SEARCH_FORWARD_SUBMODE,
	MENU_SEARCH_BACKWARD_SUBMODE,
};

void init_buildin_c_keys(int *key_mode);
void enter_cmdline_mode(enum CmdLineSubModes cl_sub_mode, const wchar_t *cmd,
		void *ptr);

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
