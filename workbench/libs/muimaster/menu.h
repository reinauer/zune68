/*
    Copyright (C) 2002, The AROS Development Team. 
    All rights reserved.
    
*/

#ifndef _MUI_MENU_H
#define _MUI_MENU_H

struct NewMenu;
struct Window;
struct ZMenu;

struct ZMenu *zune_open_menu(struct Window *wnd, struct NewMenu *newmenu);
struct ZMenu *zune_open_menu_at(struct Window *wnd, struct NewMenu *newmenu,
    LONG x, LONG y);
void zune_mouse_update(struct ZMenu *menu, int left_down);
/* returns the user data of the selected entry */
int zune_poll_menu(struct ZMenu *menu, struct MenuItem **selected);
struct MenuItem *zune_leave_menu(struct ZMenu *zmenu);
void zune_close_menu(struct ZMenu *menu);
struct Menu *zune_get_menu_pointer(struct ZMenu *menu);

#endif
