#include "util.h"
#include "menus.h"
#include "graphics.h"
#include "startgame.h"
#include "saves.h"
#include "cheats.h"
#include "settings.h"
#include <stdio.h>
#include <ctype.h>
#include <loadfile.h>
#include <iopcontrol.h>
#include <libpad.h>
#include <sifrpc.h>
#include <iopheap.h>
#include <kernel.h>
#include <sbv_patches.h>
#include <libmc.h>

#ifdef _DTL_T10000
extern u8  _sio2man_irx_start[];
extern int _sio2man_irx_size;
extern u8  _mcman_irx_start[];
extern int _mcman_irx_size;
extern u8  _mcserv_irx_start[];
extern int _mcserv_irx_size;
extern u8  _padman_irx_start[];
extern int _padman_irx_size;
#endif

extern u8  _iomanX_irx_start[];
extern int _iomanX_irx_size;
extern u8  _ps2kbd_irx_start[];
extern int _ps2kbd_irx_size;
extern u8  _usbd_irx_start[];
extern int _usbd_irx_size;
extern u8  _usb_mass_irx_start[];
extern int _usb_mass_irx_size;
static char padBuff[256] __attribute__ ((aligned(64)));

void loadModules()
{
    int ret;
    printf("\n ** Loading main modules **\n");

    /* IOP reset routine taken from ps2rd */
    SifInitRpc(0);

    #ifdef _DTL_T10000
    while (!SifIopReset("rom0:UDNL", 0));
    #else
    while (!SifIopReset("rom0:UDNL rom0:EELOADCNF", 0));
    #endif
    while (!SifIopSync());

    /* exit services */
    fioExit();
    SifExitIopHeap();
    SifLoadFileExit();
    SifExitRpc();
    SifExitCmd();

    FlushCache(0);
    FlushCache(2);

    /* init services */
    SifInitRpc(0);
    SifLoadFileInit();
    SifInitIopHeap();
    fioInit();

    FlushCache(0);
    FlushCache(2);

    sbv_patch_enable_lmb();
    sbv_patch_disable_prefix_check();

    #ifdef _DTL_T10000
    SifExecModuleBuffer(_sio2man_irx_start, _sio2man_irx_size, 0, NULL, &ret);
    SifExecModuleBuffer(_padman_irx_start, _padman_irx_size, 0, NULL, &ret);
    SifExecModuleBuffer(_mcman_irx_start, _mcman_irx_size, 0, NULL, &ret);
    SifExecModuleBuffer(_mcserv_irx_start, _mcserv_irx_size, 0, NULL, &ret);

    #else
    SifLoadModule("rom0:SIO2MAN", 0, NULL);
    SifLoadModule("rom0:PADMAN", 0, NULL);
    SifLoadModule("rom0:MCMAN", 0, NULL);
    SifLoadModule("rom0:MCSERV", 0, NULL);
    #endif
    SifExecModuleBuffer(_iomanX_irx_start, _iomanX_irx_size, 0, NULL, &ret);
    SifExecModuleBuffer(_usbd_irx_start, _usbd_irx_size, 0, NULL, &ret);
    SifExecModuleBuffer(_usb_mass_irx_start, _usb_mass_irx_size, 0, NULL, &ret);

    #ifdef _DTL_T10000
    mcInit(MC_TYPE_XMC);
    #else
    mcInit(MC_TYPE_MC);
    #endif

    padInit(0);
    padPortOpen(0, 0, padBuff);
    padSetMainMode(0, 0, PAD_MMODE_DIGITAL, PAD_MMODE_LOCK);
}

void handlePad()
{
    u32 pad_rapid = 0;
    static u32 old_pad = 0xffff;
    static u32 pad_pressed;
    struct padButtonStatus padStat;
    int state;
    static u32 time_held = 0;
    static int selected = 0;
    static int selectedDevice = 0;

    state = padGetState(0, 0);
    while((state != PAD_STATE_STABLE) && (state != PAD_STATE_FINDCTP1))
        state = padGetState(0, 0);

    padRead(0, 0, &padStat);
    pad_pressed = (0xFFFF ^ padStat.btns) & ~old_pad;
    old_pad = 0xFFFF ^ padStat.btns;
    
    // pad_rapid will have an initial delay when a button is held
    if((0xFFFF ^ padStat.btns) && (0xFFFF ^ padStat.btns) == old_pad)
    {
        if(time_held++ > 18 && time_held % 2 == 0) // don't go too fast!
            pad_rapid = (0xFFFF ^ padStat.btns);
        else
            pad_rapid = pad_pressed;
    }
    else
        time_held = 0;

    menuID_t currentMenu = menuGetActive();
    
    if(currentMenu == GAMEMENU ||
       currentMenu == CHEATMENU ||
       currentMenu == CODEMENU ||
       currentMenu == BOOTMENU ||
       currentMenu == SAVEMENU)
    {
        if(pad_rapid & PAD_UP)
            menuUp();

        else if(pad_rapid & PAD_DOWN)
            menuDown();
    }

    if(currentMenu == GAMEMENU)
    {
        if(pad_pressed & PAD_CROSS)
            menuSetActive(CHEATMENU);

        else if(pad_pressed & PAD_CIRCLE || pad_pressed & PAD_START)
        {
            menuSetActive(MAINMENU);
            selected = 0;
        }

        else if(pad_pressed & PAD_SELECT)
        {
            graphicsDrawAboutPage();
            old_pad |= PAD_CROSS | PAD_CIRCLE;
        }

        else if(pad_pressed & PAD_SQUARE)
        {
            displayContextMenu(GAMEMENU);
            old_pad |= PAD_CROSS | PAD_CIRCLE;
        }

        if(pad_rapid & PAD_R1)
        {
            int i;
            for(i = 0; i < 10; i++)
                menuDown();
        }

        else if(pad_rapid & PAD_L1)
        {
            int i;
            for(i = 0; i < 10; i++)
                menuUp();
        }

        if(pad_rapid & PAD_R2)
            menuDownAlpha();

        else if(pad_rapid & PAD_L2)
            menuUpAlpha();
        // TODO: Goto settings menu, mini menu, etc
    }

    else if(currentMenu == CHEATMENU)
    {
        if(pad_pressed & PAD_CROSS)
            menuToggleItem();

        else if(pad_pressed & PAD_CIRCLE)
            menuSetActive(GAMEMENU);

        else if(pad_pressed & PAD_SQUARE)
        {
            displayContextMenu(CHEATMENU);
            old_pad |= PAD_CROSS | PAD_CIRCLE;
        }
        
        else if(pad_pressed & PAD_START)
            menuSetActive(MAINMENU);

        if(pad_rapid & PAD_R1)
        {
            int i;
            for(i = 0; i < 10; i++)
                menuDown();
        }

        else if(pad_rapid & PAD_L1)
        {
            int i;
            for(i = 0; i < 10; i++)
                menuUp();
        }
    }

    else if(currentMenu == CODEMENU)
    {
        if(pad_pressed & PAD_CIRCLE)
            menuSetActive(CHEATMENU);

        else if(pad_pressed & PAD_SQUARE)
        {
            displayContextMenu(CODEMENU);
            old_pad |= PAD_CROSS | PAD_CIRCLE;
        }

        else if(pad_pressed & PAD_CROSS)
        {
            if(cheatsGetNumCodeLines() > 0)
            {
                cheatsEditCodeLine();
                old_pad |= PAD_CROSS | PAD_CIRCLE;
            }
        }

        if(pad_rapid & PAD_R1)
        {
            int i;
            for(i = 0; i < 10; i++)
                menuDown();
        }

        else if(pad_rapid & PAD_L1)
        {
            int i;
            for(i = 0; i < 10; i++)
                menuUp();
        }
    }

    else if(currentMenu == MAINMENU)
    {
        graphicsDrawMainMenu(selected);
        if(pad_pressed & PAD_CROSS)
        {
            if(selected == 0) // boot game
                menuSetActive(BOOTMENU);
            if(selected == 1) // game menu
                menuSetActive(GAMEMENU);
            if(selected == 2) // save manager (device menu)
                menuSetActive(SAVEDEVICEMENU);
        }

        else if(pad_pressed & PAD_SELECT)
        {
            graphicsDrawAboutPage();
            old_pad |= PAD_CROSS | PAD_CIRCLE;
        }

        else if(pad_pressed & PAD_CIRCLE)
            menuSetActive(GAMEMENU);

        else if(pad_pressed & PAD_RIGHT)
        {
            if(selected >= 3)
                selected = 0;
            else
                ++selected;
        }

        else if(pad_pressed & PAD_LEFT)
        {
            if (selected == 0)
                selected = 3;
            else
                --selected;
        }
    }
    
    else if(currentMenu == BOOTMENU)
    {
        if(pad_pressed & PAD_CROSS)
            menuToggleItem();
        
        else if(pad_pressed & PAD_CIRCLE)
        {
            menuRemoveAllItems();
            menuSetActive(MAINMENU);
        }

        else if(pad_pressed & PAD_SQUARE)
        {
            displayContextMenu(BOOTMENU);
            old_pad |= PAD_CROSS | PAD_CIRCLE;
        }
    }
    
    else if(currentMenu == SAVEDEVICEMENU)
    {
        graphicsDrawDeviceMenu(selectedDevice);
        graphicsDrawTextCentered(150, "Select device to view saves", WHITE);
        if(pad_pressed & PAD_CROSS)
        {
            menuSetActive(SAVEMENU);
            if(selectedDevice == 0)
                savesLoadSaveMenu(MC_SLOT_1);
            if(selectedDevice == 1)
                savesLoadSaveMenu(MC_SLOT_2);
            if(selectedDevice == 2)
                savesLoadSaveMenu(FLASH_DRIVE);
        }
        
        if(pad_pressed & PAD_CIRCLE)
        {
            menuSetActive(MAINMENU);
        }
        
        else if(pad_pressed & PAD_RIGHT)
        {
            if(selectedDevice >= 2)
                selectedDevice = 0;
            else
                ++selectedDevice;
        }

        else if(pad_pressed & PAD_LEFT)
        {
            if (selectedDevice == 0)
                selectedDevice = 2;
            else
                --selectedDevice;
        }
    }
    
    else if(currentMenu == SAVEMENU)
    {
        if(pad_pressed & PAD_CROSS)
        {
            // choose destination device
            menuToggleItem();
            // if user backs out of device menu, they'll be holding circle.
            old_pad |= PAD_CIRCLE;
        }
        else if(pad_pressed & PAD_CIRCLE)
        {
            menuRemoveAllItems();
            menuSetActive(SAVEDEVICEMENU);
        }
    }
}

void displayContextMenu(int menuID)
{
    int ret;

    if(menuID == GAMEMENU)
    {
        char *items[] = {"Add Game", "Rename Game", "Delete Game", "Cancel"};
        ret = displayPromptMenu(items, 4, "Game Options");

        if(ret == 0)
            cheatsAddGame();
        else if(ret == 1)
            cheatsRenameGame();
        else if(ret == 2)
        {
            char *items2[] = {"Yes", "No"};
            int choice = displayPromptMenu(items2, 2, "Are you sure you want to delete this game?");

            if(choice == 0)
                cheatsDeleteGame();
        }
    }

    else if(menuID == CHEATMENU)
    {
        if(cheatsGetNumCheats() > 0)
        {
            char *items[] = {"Add Cheat", "Edit Code Lines", "Rename Cheat", "Delete Cheat", "Cancel"};
            ret = displayPromptMenu(items, 5, "Cheat Options");

            if(ret == 0)
                cheatsAddCheat();
            else if(ret == 1)
                menuSetActive(CODEMENU);
            else if(ret == 2)
                cheatsRenameCheat();
            else if(ret == 3)
            {
                char *items2[] = {"Yes", "No"};
                int choice = displayPromptMenu(items2, 2, "Are you sure you want to delete this cheat?");

                if(choice == 0)
                    cheatsDeleteCheat();
            }
        }
        else
        {
            char *items[] = {"Add Cheat", "Cancel"};
            ret = displayPromptMenu(items, 2, "Cheat Options");

            if(ret == 0)
                cheatsAddCheat();
        }
    }

    else if(menuID == CODEMENU)
    {
        if(cheatsGetNumCodeLines() > 0)
        {
            char *items[] = {"Add Line", "Edit Line", "Delete Line", "Cancel"};
            ret = displayPromptMenu(items, 4, "Code Options");

            if(ret == 0)
                cheatsAddCodeLine();
            else if(ret == 1)
                cheatsEditCodeLine();
            else if(ret == 2)
            {
                char *items2[] = {"Yes", "No"};
                int choice = displayPromptMenu(items2, 2, "Are you sure you want to delete this code line?");

                if(choice == 0)
                    cheatsDeleteCodeLine();
            }
        }
        else
        {
            char *items[] = {"Add Line", "Cancel"};
            ret = displayPromptMenu(items, 2, "Code Options");

            if(ret == 0)
                cheatsAddCodeLine();
        }
    }

    else if(menuID == BOOTMENU)
    {
        char *items[] = {"Edit Path", "Cancel"};
        ret = displayPromptMenu(items, 2, "Boot Options");

        if(ret == 0)
            settingsRenameBootPath();
    }
}

char *keyBoardChars = "`1234567890-=" \
                      "qwertyuiop[]\\" \
                      "asdfghjkl;'  " \
                      "zxcvbnm,./   ";
char *keyBoardCharsUpper = "~!@#$%^&*()_+" \
                           "QWERTYUIOP{}|" \
                           "ASDFGHJKL:\"  "\
                           "ZXCVBNM<>?   ";
#define KEYBOARD_COLUMNS 13
#define KEYBOARD_ROWS 4
#define ACCEPT_ROW KEYBOARD_ROWS
#define CANCEL_ROW ACCEPT_ROW + 1

int displayInputMenu(char *dstStr, int dstLen, const char *initialStr, const char *prompt)
{
    struct padButtonStatus padStat;
    char tmp[1024];
    int tmpLoc, state;
    u32 old_pad = PAD_CROSS;
    u32 pad_pressed = 0;
    u32 pad_rapid = 0;
    static u32 time_held = 0;
    int row = 0;
    int column = 0;
    int upper = 0;
    float width = 0;
    int tickerX = 0;

    tmp[0] = '\0';

    if(initialStr)
        strncpy(tmp, initialStr, 1024);
    else
        tmp[0] = '\0';

    tmpLoc = strlen(tmp);

    do
    {
        if(tmp)
        {
            int newWidth = graphicsGetWidth(tmp) + 20;
            if(newWidth > width)
                width = newWidth;
        }

        if(width < 345)
            width = 345;

        graphicsDrawPromptBoxBlack(width, 220);

        if(prompt)
            graphicsDrawTextCentered(125, prompt, GREEN);

        if(tmp)
            graphicsDrawTextCentered(150, tmp, YELLOW);

        state = padGetState(0, 0);
        while((state != PAD_STATE_STABLE) && (state != PAD_STATE_FINDCTP1))
            state = padGetState(0, 0);
    
        padRead(0, 0, &padStat);
    
        pad_pressed = (0xFFFF ^ padStat.btns) & ~old_pad;
        old_pad = 0xFFFF ^ padStat.btns;

        // pad_rapid will have an initial delay when a button is held
        if((0xFFFF ^ padStat.btns) && (0xFFFF ^ padStat.btns) == old_pad)
        {
            if(time_held++ > 18 && time_held % 6 == 0) // don't go too fast!
                pad_rapid = (0xFFFF ^ padStat.btns);
            else
                pad_rapid = pad_pressed;
        }
        else
            time_held = 0;

        if(pad_pressed & PAD_CROSS)
        {
            if(row < KEYBOARD_ROWS && column < KEYBOARD_COLUMNS)
            {
                if(upper)
                    tmp[tmpLoc] = keyBoardCharsUpper[row*KEYBOARD_COLUMNS + column];
                else
                    tmp[tmpLoc] = keyBoardChars[row*KEYBOARD_COLUMNS + column];

                tmp[tmpLoc + 1] = '\0';
                tmpLoc++;
            }

            else if(row == ACCEPT_ROW)
            {
                if(strlen(tmp) > 0)
                {
                    strncpy(dstStr, tmp, dstLen);
                    return 1;
                }
                else
                    return 0;
            }

            else if(row == CANCEL_ROW)
                return 0;
        }

        else if(pad_pressed & PAD_R1)
        {
            tmp[tmpLoc] = ' ';
            tmp[tmpLoc + 1] = '\0';
            tmpLoc++;
        }

        else if(pad_rapid & PAD_SQUARE)
        {
            if(tmpLoc > 0)
                tmpLoc--;

            tmp[tmpLoc] = '\0';
        }

        else if(pad_pressed & PAD_R2)
            upper ^= 1;
        
        if(pad_rapid & PAD_UP)
        {
            if(row > 0)
                row--;
            else
                row = KEYBOARD_ROWS + 1;
        }

        else if(pad_rapid & PAD_DOWN)
        {
            if(row < KEYBOARD_ROWS + 1)
                row++;
            else
                row = 0;
        }

        else if(pad_rapid & PAD_LEFT)
        {
            if(row < KEYBOARD_ROWS) // Accept or Cancel not selected
            {
                if(column > 0)
                    column--;
                else
                    column = KEYBOARD_COLUMNS - 1;
            }
        }

        else if(pad_rapid & PAD_RIGHT)
        {
            if(row < KEYBOARD_ROWS)
            {
                if(column < KEYBOARD_COLUMNS - 1)
                    column++;
                else
                    column = 0;
            }
        }

        int i, j, color;
        for(i = 0; i < KEYBOARD_ROWS; i++)
        {
            for(j = 0; j < KEYBOARD_COLUMNS; j++)
            {
                if(i == row && j == column)
                {
                    color = YELLOW;
                    graphicsDrawQuad(162 + j*25, 175 + i*25, 25, 25, BLUE);
                }
                else
                    color = WHITE;

                if(upper)
                    graphicsDrawChar(162 + j*25, 175 + i*25, keyBoardCharsUpper[i*KEYBOARD_COLUMNS + j], color);
                else
                    graphicsDrawChar(162 + j*25, 175 + i*25, keyBoardChars[i*KEYBOARD_COLUMNS + j], color);
            }
        }

        if(row == ACCEPT_ROW)
            graphicsDrawQuad(320 - (width-20)/2.0, 175 + KEYBOARD_ROWS*25, width - 20, 22, BLUE);
        else if(row == CANCEL_ROW)
            graphicsDrawQuad(320 - (width-20)/2.0, 175 + (KEYBOARD_ROWS+1)*25, width - 20, 22, BLUE);

        graphicsDrawTextCentered(174 + KEYBOARD_ROWS*25, "Accept", (row == ACCEPT_ROW) ? YELLOW : WHITE);
        graphicsDrawTextCentered(174 + (KEYBOARD_ROWS+1)*25, "Cancel", (row == CANCEL_ROW) ? YELLOW : WHITE);

        // Draw help ticker
        graphicsDrawBackgroundBottom(80);
        if(tickerX < 1300)
            tickerX += 2;
        else
            tickerX = 0;

        char *helpText = "{R1} Space     "
                         "{R2} Toggle Upper/Lower Case     "
                         "{SQUARE} Backspace";
        graphicsDrawText(graphicsGetDisplayWidth() - tickerX, 405, helpText, WHITE);

        graphicsRender();

    } while(!(pad_pressed & PAD_CIRCLE));

    return 0;
}

char *codeKeyboardChars = "0123" \
                          "4567" \
                          "89AB" \
                          "CDEF";
#define CODE_KEYBOARD_COLUMNS 4
#define CODE_KEYBOARD_ROWS 4
#define CODE_KEYBOARD_ACCEPT_ROW CODE_KEYBOARD_ROWS
#define CODE_KEYBOARD_CANCEL_ROW CODE_KEYBOARD_ACCEPT_ROW + 1

int displayCodeEditMenu(u64 *code)
{
    struct padButtonStatus padStat;
    int state;
    u32 old_pad = PAD_CROSS;
    u32 pad_pressed = 0;
    u32 pad_rapid = 0;
    static u32 time_held = 0;
    int row = 0;
    int column = 0;
    char codeString[18];
    int codeLoc = 0;
    int tickerX = 0;

    if(!code)
        return 0;

    u32 addr = (u32)*((u32 *) code);
    u32 val = (u32)*((u32 *)code + 1);

    snprintf(codeString, 18, "%08X %08X", addr, val);

    do
    {
        graphicsDrawPromptBoxBlack(285, 220);

        state = padGetState(0, 0);
        while((state != PAD_STATE_STABLE) && (state != PAD_STATE_FINDCTP1))
            state = padGetState(0, 0);
    
        padRead(0, 0, &padStat);
    
        pad_pressed = (0xFFFF ^ padStat.btns) & ~old_pad;
        old_pad = 0xFFFF ^ padStat.btns;

        // pad_rapid will have an initial delay when a button is held
        if((0xFFFF ^ padStat.btns) && (0xFFFF ^ padStat.btns) == old_pad)
        {
            if(time_held++ > 18 && time_held % 6 == 0) // don't go too fast!
                pad_rapid = (0xFFFF ^ padStat.btns);
            else
                pad_rapid = pad_pressed;
        }
        else
            time_held = 0;

        if(pad_pressed & PAD_CROSS)
        {
            if(row < CODE_KEYBOARD_ROWS && column < CODE_KEYBOARD_COLUMNS)
            {
                codeString[codeLoc] = codeKeyboardChars[row*CODE_KEYBOARD_COLUMNS + column];

                if(codeLoc == 7)
                {
                    // Skip past space in the center
                    codeLoc += 2;
                }
                else if(codeLoc < 16)
                {
                    codeLoc++;
                }
            }
            else if(row == CODE_KEYBOARD_ACCEPT_ROW)
            {
                // Update code value
                sscanf(codeString, "%08X %08X", (u32 *)code, ((u32 *)code + 1));
                return 1;
            }
            else if(row == CODE_KEYBOARD_CANCEL_ROW)
            {
                return 0;
            }
        }

        else if(pad_pressed & PAD_L1)
        {
            if(codeLoc > 0)
            {
                if(codeLoc == 9)
                {
                    codeLoc -= 2;
                }
                else
                {
                    codeLoc--;
                }
            }
            else
            {
                codeLoc = 16;
            }
        }

        else if(pad_pressed & PAD_R1)
        {
            if(codeLoc < 16)
            {
                if(codeLoc == 7)
                {
                    // Skip past center space
                    codeLoc += 2;
                }
                else
                {
                    codeLoc++;
                }
            }
            else
            {
                codeLoc = 0;
            }
        }

        if(pad_rapid & PAD_UP)
        {
            if(row > 0)
                row--;
            else
                row = CODE_KEYBOARD_CANCEL_ROW;
        }
        
        else if(pad_rapid & PAD_DOWN)
        {
            if(row < CODE_KEYBOARD_ROWS + 1)
                row++;
            else
                row = 0;
        }

        else if(pad_rapid & PAD_LEFT)
        {
            if(row < CODE_KEYBOARD_ROWS)
            {
                if(column > 0)
                    column--;
                else
                    column = CODE_KEYBOARD_COLUMNS - 1;
            }
        }

        else if(pad_rapid & PAD_RIGHT)
        {
            if(row < CODE_KEYBOARD_ROWS)
            {
                if(column < CODE_KEYBOARD_COLUMNS - 1)
                    column++;
                else
                    column = 0;
            }
        }

        int i, j, color;
        for(i = 0; i < CODE_KEYBOARD_ROWS; i++)
        {
            for(j = 0; j < CODE_KEYBOARD_COLUMNS; j++)
            {
                if(i == row && j == column)
                {
                    // 320 - 25*2
                    // 224 - 25*2
                    color = YELLOW;
                    graphicsDrawQuad(270 + j*25, 175 + i*25, 25, 25, BLUE);
                }
                else
                {
                    color = WHITE;
                }

                graphicsDrawChar(graphicsGetDisplayWidth()/2.0 - 45 + j*25, 175 + i*25, codeKeyboardChars[i*CODE_KEYBOARD_COLUMNS + j], color);
            }
        }

        graphicsDrawTextCentered(125, "Edit Code Line", GREEN);

        for(i = 0; i < 17; i++)
        {
            if(i == codeLoc)
            {
                graphicsDrawQuad(graphicsGetDisplayWidth()/2.0 - (8*17) + i*16, 150, 15, 25, BLUE);
            }

            graphicsDrawChar(graphicsGetDisplayWidth()/2.0 - (8*17) + i*16, 150, codeString[i], WHITE);
        }

        if(row == CODE_KEYBOARD_ACCEPT_ROW)
            graphicsDrawQuad(graphicsGetDisplayWidth()/2.0 - 100, 175 + CODE_KEYBOARD_ROWS*25, 200, 22, BLUE);
        else if(row == CODE_KEYBOARD_CANCEL_ROW)
            graphicsDrawQuad(graphicsGetDisplayWidth()/2.0 - 100, 175 + (CODE_KEYBOARD_ROWS+1)*25, 200, 22, BLUE);

        graphicsDrawTextCentered(174 + CODE_KEYBOARD_ROWS*25, "Accept", (row == CODE_KEYBOARD_ACCEPT_ROW) ? YELLOW : WHITE);
        graphicsDrawTextCentered(174 + (CODE_KEYBOARD_ROWS+1)*25, "Cancel", (row == CODE_KEYBOARD_CANCEL_ROW) ? YELLOW : WHITE);

        // Draw help ticker
        graphicsDrawBackgroundBottom(80);
        if(tickerX < 1000)
            tickerX += 2;
        else
            tickerX = 0;

        char *helpText = "{L1}/{R1} Move Cursor     "
                         "{CROSS} Set Value";
        graphicsDrawText(graphicsGetDisplayWidth() - tickerX, 405, helpText, WHITE);

        graphicsRender();


    } while (!(pad_pressed & PAD_CIRCLE));

    return 0;
}

int displayPromptMenu(char **items, int numItems, const char *header)
{
    struct padButtonStatus padStat;
    u32 old_pad = PAD_CROSS;
    u32 pad_pressed = 0;
    int state, i, y;
    int selectedItem = 0;

    float maxLength = 0;
    int numHeaderLines;
    
    if(!items || numItems <= 0 || !header)
        return 0;
    
    maxLength = graphicsGetWidth(header);
    numHeaderLines = getNumLines(header);

    for(i = 0; i < numItems; i++)
    {
        float length = 0;

        length = graphicsGetWidth(items[i]);

        if(length > maxLength)
            maxLength = length;
    }

    do
    {
        state = padGetState(0, 0);
        while((state != PAD_STATE_STABLE) && (state != PAD_STATE_FINDCTP1))
            state = padGetState(0, 0);
    
        padRead(0, 0, &padStat);
    
        pad_pressed = (0xFFFF ^ padStat.btns) & ~old_pad;
        old_pad = 0xFFFF ^ padStat.btns;
        
        graphicsDrawPromptBoxBlack(maxLength + 20, (numItems + numHeaderLines) * 22 + 20);
        graphicsDrawTextCentered((graphicsGetDisplayHeight() / 2.0) - (numItems + numHeaderLines - 1)*11 - 16, header, GREEN);
        y = (graphicsGetDisplayHeight() / 2.0) - numItems*11 + numHeaderLines*11;
        for(i = 0; i < numItems; i++)
        {
            if(i == selectedItem)
                graphicsDrawQuad(320 - maxLength/2, y, maxLength, 22, BLUE);
            graphicsDrawTextCentered(y - 1, items[i], i == selectedItem ? YELLOW : WHITE);
            y += 22;
        }
        graphicsRender();
        
        if(pad_pressed & PAD_UP)
        {
            if(selectedItem == 0)
                selectedItem = numItems - 1;
            else
                --selectedItem;
        }

        else if(pad_pressed & PAD_DOWN)
        {
            if (selectedItem == numItems - 1)
                selectedItem = 0;
            else
                ++selectedItem;
        }

        else if(pad_pressed & PAD_CIRCLE)
        {
            // Exit menu
            return -1;
        }
    } while(!(pad_pressed & PAD_CROSS));
    
    return selectedItem;
}

int displayError(const char *error)
{
    char *items[] = {"OK"};
    return displayPromptMenu(items, 1, error);
}

int getNumLines(const char *str)
{
    if(!str)
        return 0;

    char const *cptr = str;
    int numLines = 0;

    while(*cptr)
    {
        if(numLines == 0)
        {
            numLines++;
        }
        if(*cptr == '\n')
        {
            numLines++;
        }

        cptr++;
    }

    return numLines;
}

void replaceIllegalChars(const char *str, char* valid, char replacement)
{
    char invalid[] = {'<', '>', ':', '"', '/', '\\', '|', '?', '*'};
    const char *c = str;
    char *cvalid = valid;
    int i;
    if(!str || !valid)
        return;
    
    while(*c)
    {
        for(i = 0; i < sizeof(invalid); i++)
        {
            if(*c == invalid[i])
            {
                *cvalid = replacement;
                break;
            }
            else
                *cvalid = *c;
        }
        c++;
        cvalid++;
    }
    
    *cvalid = '\0';
}

char *rtrim(char *str)
{
    char *end;
    
    end = str + strlen(str) - 1;
    while(end > str && isspace(*end)) end--;
    *(end+1) = '\0';
    
    return str;
}

const char *getFileExtension(const char *filename)
{
    const char *c;

    if(!filename)
        return NULL;

    // Find trailing period character
    c = filename + strlen(filename) - 1;
    while(c > filename && *c != '.') --c;

    if(c == filename)
        return NULL;
    else
        return ++c;
}

unsigned long mycrc32(unsigned long inCrc32, const void *buf, long bufLen)
{
    /*----------------------------------------------------------------------------*\
     *  CRC-32 version 2.0.0 by Craig Bruce, 2006-04-29.
     *
     *  This program generates the CRC-32 values for the files named in the
     *  command-line arguments.  These are the same CRC-32 values used by GZIP,
     *  PKZIP, and ZMODEM.  The Crc32_ComputeBuf() can also be detached and
     *  used independently.
     *
     *  THIS PROGRAM IS PUBLIC-DOMAIN SOFTWARE.
     *
     *  Based on the byte-oriented implementation "File Verification Using CRC"
     *  by Mark R. Nelson in Dr. Dobb's Journal, May 1992, pp. 64-67.
     *
     *  v1.0.0: original release.
     *  v1.0.1: fixed printf formats.
     *  v1.0.2: fixed something else.
     *  v1.0.3: replaced CRC constant table by generator function.
     *  v1.0.4: reformatted code, made ANSI C.  1994-12-05.
     *  v2.0.0: rewrote to use memory buffer & static table, 2006-04-29.
    \*----------------------------------------------------------------------------*/

    static const unsigned long crcTable[256] = {
   0x00000000,0x77073096,0xEE0E612C,0x990951BA,0x076DC419,0x706AF48F,0xE963A535,
   0x9E6495A3,0x0EDB8832,0x79DCB8A4,0xE0D5E91E,0x97D2D988,0x09B64C2B,0x7EB17CBD,
   0xE7B82D07,0x90BF1D91,0x1DB71064,0x6AB020F2,0xF3B97148,0x84BE41DE,0x1ADAD47D,
   0x6DDDE4EB,0xF4D4B551,0x83D385C7,0x136C9856,0x646BA8C0,0xFD62F97A,0x8A65C9EC,
   0x14015C4F,0x63066CD9,0xFA0F3D63,0x8D080DF5,0x3B6E20C8,0x4C69105E,0xD56041E4,
   0xA2677172,0x3C03E4D1,0x4B04D447,0xD20D85FD,0xA50AB56B,0x35B5A8FA,0x42B2986C,
   0xDBBBC9D6,0xACBCF940,0x32D86CE3,0x45DF5C75,0xDCD60DCF,0xABD13D59,0x26D930AC,
   0x51DE003A,0xC8D75180,0xBFD06116,0x21B4F4B5,0x56B3C423,0xCFBA9599,0xB8BDA50F,
   0x2802B89E,0x5F058808,0xC60CD9B2,0xB10BE924,0x2F6F7C87,0x58684C11,0xC1611DAB,
   0xB6662D3D,0x76DC4190,0x01DB7106,0x98D220BC,0xEFD5102A,0x71B18589,0x06B6B51F,
   0x9FBFE4A5,0xE8B8D433,0x7807C9A2,0x0F00F934,0x9609A88E,0xE10E9818,0x7F6A0DBB,
   0x086D3D2D,0x91646C97,0xE6635C01,0x6B6B51F4,0x1C6C6162,0x856530D8,0xF262004E,
   0x6C0695ED,0x1B01A57B,0x8208F4C1,0xF50FC457,0x65B0D9C6,0x12B7E950,0x8BBEB8EA,
   0xFCB9887C,0x62DD1DDF,0x15DA2D49,0x8CD37CF3,0xFBD44C65,0x4DB26158,0x3AB551CE,
   0xA3BC0074,0xD4BB30E2,0x4ADFA541,0x3DD895D7,0xA4D1C46D,0xD3D6F4FB,0x4369E96A,
   0x346ED9FC,0xAD678846,0xDA60B8D0,0x44042D73,0x33031DE5,0xAA0A4C5F,0xDD0D7CC9,
   0x5005713C,0x270241AA,0xBE0B1010,0xC90C2086,0x5768B525,0x206F85B3,0xB966D409,
   0xCE61E49F,0x5EDEF90E,0x29D9C998,0xB0D09822,0xC7D7A8B4,0x59B33D17,0x2EB40D81,
   0xB7BD5C3B,0xC0BA6CAD,0xEDB88320,0x9ABFB3B6,0x03B6E20C,0x74B1D29A,0xEAD54739,
   0x9DD277AF,0x04DB2615,0x73DC1683,0xE3630B12,0x94643B84,0x0D6D6A3E,0x7A6A5AA8,
   0xE40ECF0B,0x9309FF9D,0x0A00AE27,0x7D079EB1,0xF00F9344,0x8708A3D2,0x1E01F268,
   0x6906C2FE,0xF762575D,0x806567CB,0x196C3671,0x6E6B06E7,0xFED41B76,0x89D32BE0,
   0x10DA7A5A,0x67DD4ACC,0xF9B9DF6F,0x8EBEEFF9,0x17B7BE43,0x60B08ED5,0xD6D6A3E8,
   0xA1D1937E,0x38D8C2C4,0x4FDFF252,0xD1BB67F1,0xA6BC5767,0x3FB506DD,0x48B2364B,
   0xD80D2BDA,0xAF0A1B4C,0x36034AF6,0x41047A60,0xDF60EFC3,0xA867DF55,0x316E8EEF,
   0x4669BE79,0xCB61B38C,0xBC66831A,0x256FD2A0,0x5268E236,0xCC0C7795,0xBB0B4703,
   0x220216B9,0x5505262F,0xC5BA3BBE,0xB2BD0B28,0x2BB45A92,0x5CB36A04,0xC2D7FFA7,
   0xB5D0CF31,0x2CD99E8B,0x5BDEAE1D,0x9B64C2B0,0xEC63F226,0x756AA39C,0x026D930A,
   0x9C0906A9,0xEB0E363F,0x72076785,0x05005713,0x95BF4A82,0xE2B87A14,0x7BB12BAE,
   0x0CB61B38,0x92D28E9B,0xE5D5BE0D,0x7CDCEFB7,0x0BDBDF21,0x86D3D2D4,0xF1D4E242,
   0x68DDB3F8,0x1FDA836E,0x81BE16CD,0xF6B9265B,0x6FB077E1,0x18B74777,0x88085AE6,
   0xFF0F6A70,0x66063BCA,0x11010B5C,0x8F659EFF,0xF862AE69,0x616BFFD3,0x166CCF45,
   0xA00AE278,0xD70DD2EE,0x4E048354,0x3903B3C2,0xA7672661,0xD06016F7,0x4969474D,
   0x3E6E77DB,0xAED16A4A,0xD9D65ADC,0x40DF0B66,0x37D83BF0,0xA9BCAE53,0xDEBB9EC5,
   0x47B2CF7F,0x30B5FFE9,0xBDBDF21C,0xCABAC28A,0x53B39330,0x24B4A3A6,0xBAD03605,
   0xCDD70693,0x54DE5729,0x23D967BF,0xB3667A2E,0xC4614AB8,0x5D681B02,0x2A6F2B94,
   0xB40BBE37,0xC30C8EA1,0x5A05DF1B,0x2D02EF8D };
    unsigned long crc32;
    unsigned char *byteBuf;
    long i;

    /** accumulate crc32 for buffer **/
    crc32 = inCrc32 ^ 0xFFFFFFFF;
    byteBuf = (unsigned char*) buf;
    for (i=0; i < bufLen; i++) {
        crc32 = (crc32 >> 8) ^ crcTable[ (crc32 ^ byteBuf[i]) & 0xFF ];
    }
    return( crc32 ^ 0xFFFFFFFF );
}
