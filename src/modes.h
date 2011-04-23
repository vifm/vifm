#ifndef __MODES_H__
#define __MODES_H__

enum
{
	NORMAL_MODE,
	CMDLINE_MODE,
	VISUAL_MODE,
	MENU_MODE,
	MODES_COUNT
};

void modes_pre(void);
void modes_post(void);

void init_modes(void);

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
