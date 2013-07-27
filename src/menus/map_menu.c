/* vifm
 * Copyright (C) 2001 Ken Steen.
 * Copyright (C) 2011 xaizek.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "map_menu.h"

#include <ctype.h> /* tolower() */
#include <stdlib.h> /* malloc() realloc() free() */
#include <string.h> /* strdup() strlen() strcat() */
#include <wchar.h> /* wcsncmp() wcslen() */

#include "../modes/menu.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../utils/utils.h"
#include "../ui.h"
#include "menus.h"

static void add_mapping_item(menu_info *m, const wchar_t map_info[]);
static char * uchar2str(const wchar_t c[], size_t *len);

int
show_map_menu(FileView *view, const char mode_str[], wchar_t *list[],
		const wchar_t start[])
{
	int x;
	const size_t start_len = wcslen(start);

	static menu_info m;
	init_menu_info(&m, MAP, strdup("No mapping found"));
	m.title = format_str(" Mappings for %s mode ", mode_str);

	x = 0;
	while(list[x] != NULL)
	{
		if(list[x][0] != L'\0' && wcsncmp(start, list[x], start_len) != 0)
		{
			free(list[x]);
			x++;
			continue;
		}

		if(list[x][0] != '\0')
		{
			add_mapping_item(&m, list[x]);
			m.len++;
		}
		else if(m.len != 0)
		{
			add_to_string_array(&m.items, m.len, 1, "");
			m.len++;
		}

		free(list[x]);
		x++;
	}
	free(list);

	if(m.len > 0 && m.items[m.len - 1][0] == '\0')
	{
		free(m.items[m.len - 1]);
		m.len--;
	}

	return display_menu(&m, view);
}

static void
add_mapping_item(menu_info *m, const wchar_t map_info[])
{
	enum { MAP_WIDTH = 10 };
	size_t len;
	int i, str_len, buf_len;

	str_len = wcslen(map_info);
	buf_len = 0;
	for(i = 0; i < str_len; i += len)
		buf_len += strlen(uchar2str(map_info + i, &len));

	if(str_len > 0)
		buf_len += 1 + wcslen(map_info + str_len + 1)*4 + 1;
	else
		buf_len += 1 + 0 + 1;

	m->items = realloc(m->items, sizeof(char *)*(m->len + 1));
	m->items[m->len] = malloc(buf_len + MAP_WIDTH);
	m->items[m->len][0] = '\0';
	for(i = 0; i < str_len; i += len)
		strcat(m->items[m->len], uchar2str(map_info + i, &len));

	for(i = strlen(m->items[m->len]); i < MAP_WIDTH; i++)
		strcat(m->items[m->len], " ");

	strcat(m->items[m->len], " ");

	for(i = str_len + 1; map_info[i] != L'\0'; i += len)
	{
		if(map_info[i] == L' ')
		{
			strcat(m->items[m->len], " ");
			len = 1;
		}
		else
		{
			strcat(m->items[m->len], uchar2str(map_info + i, &len));
		}
	}
}

static char *
uchar2str(const wchar_t c[], size_t *len)
{
	/* TODO: refactor this function uchar2str() */

	static char buf[32];

	*len = 1;
	switch(*c)
	{
		case L' ':
			strcpy(buf, "<space>");
			break;
		case L'\033':
			if(c[1] == L'[' && c[2] == 'Z')
			{
				strcpy(buf, "<s-tab>");
				*len += 2;
				break;
			}
			if(c[1] != L'\0' && c[1] != L'\033')
			{
				strcpy(buf, "<m-a>");
				buf[3] += c[1] - L'a';
				++*len;
				break;
			}
			strcpy(buf, "<esc>");
			break;
		case L'\177':
			strcpy(buf, "<del>");
			break;
		case KEY_HOME:
			strcpy(buf, "<home>");
			break;
		case KEY_END:
			strcpy(buf, "<end>");
			break;
		case KEY_LEFT:
			strcpy(buf, "<left>");
			break;
		case KEY_RIGHT:
			strcpy(buf, "<right>");
			break;
		case KEY_UP:
			strcpy(buf, "<up>");
			break;
		case KEY_DOWN:
			strcpy(buf, "<down>");
			break;
		case KEY_BACKSPACE:
			strcpy(buf, "<bs>");
			break;
		case KEY_BTAB:
			strcpy(buf, "<s-tab>");
			break;
		case KEY_DC:
			strcpy(buf, "<delete>");
			break;
		case KEY_PPAGE:
			strcpy(buf, "<pageup>");
			break;
		case KEY_NPAGE:
			strcpy(buf, "<pagedown>");
			break;
		case '\r':
			strcpy(buf, "<cr>");
			break;

		default:
			if(*c == '\n' || (*c > L' ' && *c < 256))
			{
				buf[0] = *c;
				buf[1] = '\0';
			}
			else if(*c >= KEY_F0 && *c < KEY_F0 + 10)
			{
				strcpy(buf, "<f0>");
				buf[2] += *c - KEY_F0;
			}
			else if(*c >= KEY_F0 + 13 && *c <= KEY_F0 + 21)
			{
				strcpy(buf, "<s-f1>");
				buf[4] += *c - (KEY_F0 + 13);
			}
			else if(*c >= KEY_F0 + 22 && *c <= KEY_F0 + 24)
			{
				strcpy(buf, "<s-f10>");
				buf[5] += *c - (KEY_F0 + 22);
			}
			else if(*c >= KEY_F0 + 25 && *c <= KEY_F0 + 33)
			{
				strcpy(buf, "<c-f1>");
				buf[4] += *c - (KEY_F0 + 25);
			}
			else if(*c >= KEY_F0 + 34 && *c <= KEY_F0 + 36)
			{
				strcpy(buf, "<c-f10>");
				buf[5] += *c - (KEY_F0 + 34);
			}
			else if(*c >= KEY_F0 + 37 && *c <= KEY_F0 + 45)
			{
				strcpy(buf, "<a-f1>");
				buf[4] += *c - (KEY_F0 + 37);
			}
			else if(*c >= KEY_F0 + 46 && *c <= KEY_F0 + 48)
			{
				strcpy(buf, "<a-f10>");
				buf[5] += *c - (KEY_F0 + 46);
			}
			else if(*c >= KEY_F0 + 10 && *c < KEY_F0 + 63)
			{
				strcpy(buf, "<f00>");
				buf[2] += (*c - KEY_F0)/10;
				buf[3] += (*c - KEY_F0)%10;
			}
			else
			{
				strcpy(buf, "<c-A>");
				buf[3] = tolower(buf[3] + *c - 1);
			}
			break;
	}
	return buf;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
