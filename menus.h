/*
 * Menu Manager
 * Create, modify, transverse, and display menus.
 */

#ifndef MENUS_H
#define MENUS_H

#include <tamtypes.h>
#include "cheats.h"

typedef enum { GAMEMENU, CHEATMENU, CHEATMINIMENU, CODEMENU, CODEMINIMENU, SETTINGSMENU, ABOUTMENU, MAINMENU, BOOTMENU, SAVEDEVICEMENU, SAVEMENU, NUMMENUS } menuID_t;
typedef enum { NORMAL, HEADER } menutype_t;
typedef struct menuItem {
    menutype_t type;
    char *text;
    void *extra; // Optional: Associate additional data with the menuItem.
} menuItem_t;

typedef struct menuState {
    menuID_t identifier;
    int isSorted;
    char *text;
    cheatsGame_t *game; // Optional: Cheat menu uses this to associate entry with a game.
    
    menuItem_t **items;
    unsigned int currentItem;
    unsigned int numItems;
    unsigned int chunks;
} menuState_t;

int initMenus();
int killMenus();

int menuSetActive(menuID_t id);
menuID_t menuGetActive();

int menuInsertItem(menuItem_t *item);
int menuRemoveActiveItem();
int menuRemoveAllItems();
int menuSetActiveItem(menuItem_t *item);
int menuRenameActiveItem(const char *str);
void *menuGetActiveItemExtra();

int menuUp();
int menuDown();
int menuUpAlpha();
int menuDownAlpha();
void menuToggleItem();

int menuRender();

#endif
