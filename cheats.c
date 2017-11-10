#include "cheats.h"
#include "database.h"
#include "textcheats.h"
#include "menus.h"
#include "graphics.h"
#include "util.h"
#include "hash.h"
#include <debug.h>
#include <kernel.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <erl.h>
#include <floatlib.h>

static cheatsGame_t *gamesHead = NULL;
static cheatsGame_t *activeGame = NULL;
static cheatDatabaseType_t dbType;
static hashTable_t *gameHashes = NULL;
static hashTable_t *cheatHashes = NULL;
static FILE *historyFile;
static int numGames = 0;
static int numCheats = 0;
static int numEnabledCheats = 0;
static int numEnabledCodes = 0;

extern unsigned char _engine_erl_start[];

typedef struct cheatDatabaseHandler {
    char name[28]; // cheat database format name
    char extention[4]; // file extention
    
    cheatsGame_t* (*open)(const char *path, unsigned int *numGames);
    int (*save)(const char *path);
} cheatDatabaseHandler_t;

static cheatDatabaseHandler_t CDBHandler = {"Binary Database (.cdb)", "cdb", cdbOpen, cdbSave};
static cheatDatabaseHandler_t TXTHandler = {"Text File (.txt)", "txt", textCheatsOpen, textCheatsSave};

int killCheats()
{
    int first = 1;
    printf("\n ** Killing Cheats **\n");

    cheatsGame_t *game = gamesHead;
    while(game)
    {
        cheatsGame_t *next = game->next;

        if(game->cheats != NULL) {
            if(first && dbType == BINARY) // Binary DB allocates cheats structs and code lines once.
            {
                free(game->cheats);
                free(game->cheats->codeLines);
                first = 0;
            }

            free(game->cheats->codeLines);
            free(game->cheats);
        }

        game = next;
    }
    
    hashDestroyTable(gameHashes);
    free(gamesHead);

    return 1;
}

static void populateGameHashTable()
{
    if(gameHashes != NULL)
        hashDestroyTable(gameHashes);

    gameHashes = hashNewTable(numGames);

    cheatsGame_t *game = gamesHead;
    
    while(game != NULL)
    {
        int titleLen = strlen(game->title);

        if(titleLen > 0)
        {
            unsigned int hash = hashFunction(game->title, titleLen);
            hashAdd(gameHashes, game, hash);
        }

        game = game->next;
    }
}

static void populateCheatHashTable()
{
    if(cheatHashes != NULL)
        hashDestroyTable(cheatHashes);

    cheatHashes = hashNewTable(numCheats);

    cheatsCheat_t *cheat = activeGame->cheats;

    while(cheat != NULL)
    {
        unsigned int hash = hashFunction(cheat->codeLines, cheat->numCodeLines * 8);
        hashAdd(cheatHashes, cheat, hash);

        cheat = cheat->next;
    }
}

int cheatsLoadHistory()
{
    int i;
    FILE *historyFile;
    size_t historyLength;
    unsigned int lastGameHash, cheatHash;
    menuItem_t *lastGameMenu;
    cheatsCheat_t *cheat;

    if(!gameHashes)
        return 0;

    historyFile = fopen("CheatHistory.bin", "rb");

    if(historyFile)
    {
        fseek(historyFile, 0, SEEK_END);
        historyLength = ftell(historyFile);
        fseek(historyFile, 0, SEEK_SET);

        fread(&lastGameHash, 4, 1, historyFile);
        lastGameMenu = (menuItem_t *)hashFind(gameHashes, lastGameHash);

        if(lastGameMenu != NULL)
        {
            cheatsSetActiveGame((cheatsGame_t *) lastGameMenu->extra);
            populateCheatHashTable();

            for(i = 0; i < historyLength - 4; i+= 4)
            {
                fread(&cheatHash, 4, 1, historyFile);
                cheat = (cheatsCheat_t *)hashFind(cheatHashes, cheatHash);

                if(cheat != NULL)
                    cheatsToggleCheat(cheat);
            }

            menuSetActiveItem(lastGameMenu);
            hashDestroyTable(cheatHashes);
        }

        fclose(historyFile);
    }

    return 1;
}

// Determine cheat database handler by filename.
static cheatDatabaseHandler_t *getCheatDatabaseHandler(const char *path)
{
    const char *extension;
    
    if(!path)
        return NULL;
    
    extension = getFileExtension(path);
    
    if(strcmp(extension, CDBHandler.extention) == 0)
        return &CDBHandler;
    else if(strcmp(extension, TXTHandler.extention) == 0)
        return &TXTHandler;
    else
        return NULL;
}

// CheatDB --> Game --> Cheat --> Code
int cheatsOpenDatabase(const char* path)
{
    cheatDatabaseHandler_t *handler;

    handler = getCheatDatabaseHandler(path);

    if(handler)
    {
        gamesHead = handler->open(path, &numGames);
        printf("loaded %d games\n", numGames);
    }
    else
    {
        char error[255];
        sprintf(error, "Unsupported cheat database filetype: \"%s\"!", getFileExtension(path));
        displayError(error);

        // TODO: Use default empty database
        numGames = 0;
    }

    return numGames;
}

int cheatsSaveDatabase(const char* path, cheatDatabaseType_t t);

int cheatsLoadGameMenu()
{
    if(gamesHead!=NULL)
    {
        cheatsGame_t *node = gamesHead;
        menuItem_t *items = calloc(numGames, sizeof(menuItem_t));
        menuItem_t *item = items;
        unsigned int hash;

        gameHashes = hashNewTable(numGames);
        
        while(node)
        {
            item->type = NORMAL;
            item->text = calloc(1, strlen(node->title) + 1);
            strcpy(item->text, node->title);
            item->extra = node;

            hash = hashFunction(item->text, strlen(item->text));
            hashAdd(gameHashes, item, hash);

            menuInsertItem(item);
            node = node->next;
            item++;
        }

        return 1;
    }

    return 0;
}

cheatsGame_t* cheatsLoadCheatMenu(cheatsGame_t* game)
{
    if(gamesHead!=NULL && game)
    {
        /* Build the menu */
        cheatsCheat_t *cheat = game->cheats;
        menuItem_t *items = calloc(game->numCheats, sizeof(menuItem_t));
        menuItem_t *item = items;
        numCheats = 0;

        while(cheat != NULL)
        {
            if(!cheat->skip)
            {
                if(cheat->type == CHEATNORMAL)
                {
                    numCheats++;
                    item->type = NORMAL;
                }
                else
                    item->type = HEADER;

                item->text = calloc(1, strlen(cheat->title) + 1);
                strcpy(item->text, cheat->title);

                item->extra = (void *)cheat;

                menuInsertItem(item);
            }

            cheat = cheat->next;
            item++;
        }

        return game;
    }

    return NULL;
}

cheatsCheat_t* cheatsLoadCodeMenu(cheatsCheat_t *cheat)
{
    if(cheat)
    {
        /* Build the menu */
        menuItem_t *items = calloc(cheat->numCodeLines, sizeof(menuItem_t));
        menuItem_t *item = items;

        int i;
        for(i = 0; i < cheat->numCodeLines; i++)
        {
            u32 addr = (u32)*((u32 *)cheat->codeLines + 2*i);
            u32 val = (u32)*((u32 *)cheat->codeLines + 2*i + 1);

            item->text = malloc(18);
            snprintf(item->text, 18, "%08X %08X", addr, val);

            item->type = NORMAL;
            item->extra = (void *)(cheat->codeLines + i);

            menuInsertItem(item);

            item++;
        }

        return cheat;
    }

    return NULL;
}

int cheatsAddGame()
{
    cheatsGame_t *node = gamesHead;
    cheatsGame_t *newGame = calloc(1, sizeof(cheatsGame_t));
    if(!newGame)
        return 0;

    if(displayInputMenu(newGame->title, 80, NULL, "Enter Game Title") == 0)
    {
        free(newGame);
        return 0;
    }

    if(node == NULL)
    {
        gamesHead = newGame;
    }
    else
    {
        unsigned int hash = hashFunction(newGame->title, strlen(newGame->title));
        if(hashFind(gameHashes, hash))
        {
            displayError("Game title already exists!");
            free(newGame);
            return 0;
        }

        while(node->next)
            node = node->next;

        node->next = newGame;
    }

    numGames++;

    menuItem_t *item = calloc(1, sizeof(menuItem_t));
    item->type = NORMAL;
    item->text = calloc(1, strlen(newGame->title) + 1);
    item->extra = newGame;
    strcpy(item->text, newGame->title);

    menuInsertItem(item);
    menuSetActiveItem(item);

    populateGameHashTable();

    return 1;
}

int cheatsRenameGame()
{
    char title[80];

    if(menuGetActive() != GAMEMENU)
        return 0;

    cheatsGame_t *selectedGame = menuGetActiveItemExtra();

    if(!selectedGame)
        return 0;
    
    if(displayInputMenu(title, 80, selectedGame->title, "Enter Game Title") == 0)
        return 0;

    unsigned int hash = hashFunction(title, strlen(title));
    
    if(hashFind(gameHashes, hash))
    {
        displayError("Game title already exists!");
        return 0;
    }
    else
    {
        strncpy(selectedGame->title, title, 80);
        menuRenameActiveItem(selectedGame->title);

        populateGameHashTable();

        return 1;
    }
}

int cheatsDeleteGame()
{
    if(menuGetActive() != GAMEMENU)
        return 0;

    cheatsGame_t *selectedGame = menuGetActiveItemExtra();
    cheatsDeactivateGame(selectedGame);

    if(selectedGame == gamesHead)
    {
        gamesHead = selectedGame->next;
    }

    // With binary databases, the game structs are allocated as one chunk,
    // so we will use a NULL title to denotate a deleted game.
    selectedGame->title[0] = '\0';

    menuRemoveActiveItem();

    numGames--;

    populateGameHashTable();

    return 1;
}

int cheatsGetNumGames()
{
    return numGames;
}

int cheatsAddCheat()
{
    cheatsGame_t *game = menuGetActiveExtra();
    cheatsCheat_t *newCheat = calloc(1, sizeof(cheatsCheat_t));
    if(!newCheat)
        return 0;

    cheatsCheat_t *node = game->cheats;

    if(displayInputMenu(newCheat->title, 80, NULL, "Enter Cheat Title") == 0)
    {
        free(newCheat);
        return 0;
    }

    if(game->cheats == NULL)
    {
        game->cheats = newCheat;
    }
    else
    {
        cheatsCheat_t *node = game->cheats;

        while(node->next)
            node = node->next;

        node->next = newCheat;
    }

    game->numCheats++;
    numCheats++;

    menuItem_t *item = calloc(1, sizeof(menuItem_t));
    item->type = NORMAL;
    item->text = calloc(1, strlen(newCheat->title) + 1);
    item->extra = newCheat;
    strcpy(item->text, newCheat->title);

    menuInsertItem(item);
    menuSetActiveItem(item);

    node = game->cheats;

    return 1;
}

int cheatsRenameCheat()
{
    char title[80];

    if(menuGetActive() != CHEATMENU)
        return 0;

    cheatsCheat_t *selectedCheat = menuGetActiveItemExtra();

    if(!selectedCheat)
        return 0;
    
    if(displayInputMenu(title, 80, selectedCheat->title, "Enter Cheat Title") == 0)
        return 0;

    strncpy(selectedCheat->title, title, 80);
    menuRenameActiveItem(selectedCheat->title);

    return 1;
}

int cheatsDeleteCheat()
{
    if(menuGetActive() != CHEATMENU)
        return 0;

    cheatsCheat_t *selectedCheat = menuGetActiveItemExtra();

    if(selectedCheat->enabled)
    {
        cheatsToggleCheat(selectedCheat);
    }

    selectedCheat->skip = 1;
    if(selectedCheat->type == NORMAL)
        numCheats--;
    menuRemoveActiveItem();

    return 1;
}

int cheatsAddCodeLine()
{
    u64 newCode;
    char newCodeLine[18];
    menuItem_t *item;

    if(menuGetActive() != CODEMENU && menuGetActive() != ENABLECODEMENU)
        return 0;
    
    if(displayCodeEditMenu(&newCode) == 0)
        return 0;

    cheatsCheat_t *cheat = menuGetActiveExtra();
    if(cheat->codeLines == NULL)
    {
        cheat->codeLines = calloc(1, sizeof(u64));
    }
    else
    {
        cheat->codeLines = realloc(cheat->codeLines, (cheat->numCodeLines + 1) * sizeof(u64));
    }

    memcpy(cheat->codeLines + cheat->numCodeLines, &newCode, sizeof(u64));

    u32 addr = (u32)*((u32 *)&newCode);
    u32 val = (u32)*((u32 *)&newCode + 1);
    snprintf(newCodeLine, 18, "%08X %08X", addr, val);

    item = calloc(1, sizeof(menuItem_t));
    item->type = NORMAL;
    item->text = strdup(newCodeLine);
    item->extra = cheat->codeLines + cheat->numCodeLines;
    menuInsertItem(item);
    menuSetActiveItem(item);
    cheat->numCodeLines++;

    return 1;
}

int cheatsEditCodeLine()
{
    char newCodeLine[18];

    if(menuGetActive() != CODEMENU)
        return 0;

    u64 *selectedCode = menuGetActiveItemExtra();

    if(!selectedCode)
        return 0;

    if(displayCodeEditMenu(selectedCode) == 0)
        return 0;
    
    u32 addr = (u32)*((u32 *)selectedCode);
    u32 val = (u32)*((u32 *)selectedCode + 1);
    snprintf(newCodeLine, 18, "%08X %08X", addr, val);
    menuRenameActiveItem(newCodeLine);

    return 1;
}

// Delete currently selected code line
int cheatsDeleteCodeLine()
{

}

int cheatsGetNumCodeLines()
{
    if(menuGetActive() != CODEMENU)
        return 0;

    cheatsCheat_t *cheat = (cheatsCheat_t *)menuGetActiveExtra();
    if(!cheat)
        return 0;

    return cheat->numCodeLines;
}

int cheatsGetNumCheats()
{
    return numCheats;
}

int cheatsToggleCheat(cheatsCheat_t *cheat)
{
    if(cheat && cheat->type != CHEATHEADER)
    {
        if(!cheat->enabled)
        {
            if((numEnabledCodes + cheat->numCodeLines) >= 250)
            {
                displayError("Too many codes enabled. Try disabling some.");
                return 0;
            }
            else if(cheat->numCodeLines == 0)
            {
                displayError("This cheat doesn't contain any code lines.\nPlease add some on the next screen.");
                menuSetActive(CODEMENU);
                return 0;
            }
            
            cheat->enabled = 1;
            numEnabledCheats++;
            numEnabledCodes += cheat->numCodeLines;
        }
        else
        {
            cheat->enabled = 0;
            numEnabledCheats--;
            numEnabledCodes -= cheat->numCodeLines;

            if(numEnabledCheats == 0)
            {
                activeGame = NULL;
            }
        }
    }
    else
        return 0;
    return 1;
}

void cheatsDrawStats()
{
    if(activeGame && activeGame->title)
    {
        if(numEnabledCheats > 0)
        {
            char active_cheats[32];
            if(numEnabledCheats == 1)
                snprintf(active_cheats, 32, "%i active cheat", numEnabledCheats);
            else
                snprintf(active_cheats, 32, "%i active cheats", numEnabledCheats);
            
            graphicsDrawText(482, 25, active_cheats, WHITE);

            if(!activeGame->enableCheats)
                graphicsDrawText(482, 47, "Auto Hook", WHITE);
            else
                graphicsDrawText(482, 47, "Normal Hook", WHITE);
        }
    }
    
    static menuID_t activeMenu = -1;
    static int x = 0;
    char msg[200];
    
    menuID_t oldMenu = activeMenu;
    activeMenu = menuGetActive();
    
    if(activeMenu != oldMenu)
        x = 0;

    if(activeMenu == GAMEMENU)
    {
        if (x < 1700)
            x+= 2;
        else
            x = 0;

        snprintf(msg, 200, "{CROSS} Cheat List     "
                           "{SQUARE} Options     "
                           "{CIRCLE} Main Menu     "
                           "{L1}/{R1} Page Up/Down     "
                           "{L2}/{R2} Alphabetical Up/Down");
    }

    else if(activeMenu == CHEATMENU)
    {
        if (x < 1500)
            x+= 2;
        else
            x = 0;

        snprintf(msg, 150, "{CROSS} Enable/Disable Cheat     "
                           "{SQUARE} Options     "
                           "{CIRCLE} Game List    "
                           "{L1}/{R1} Page Up/Down");
    }

    else if(activeMenu == CODEMENU)
    {
        if(x < 1200)
            x += 2;
        else
            x = 0;

        snprintf(msg, 100, "{CROSS} Edit Code Line     "
                           "{SQUARE} Options     "
                           "{CIRCLE} Cheat Menu");
    }

    graphicsDrawText(640 - x, 405, msg, WHITE);
}

int cheatsIsActiveGame(const cheatsGame_t *game)
{
    return game == activeGame;
}

void cheatsDeactivateGame(cheatsGame_t *game)
{
    if(game)
    {
        cheatsCheat_t *cheat = game->cheats;
        while(cheat)
        {
            cheat->enabled = 0;
            cheat = cheat->next;
        }

        if(game == activeGame)
        {
            numEnabledCheats = 0;
            numEnabledCodes = 0;
            activeGame = NULL;
        }
    }
}

int cheatsSetActiveGame(cheatsGame_t *game)
{
    /* Disable all active cheats if a new game was selected */
    if(game != activeGame)
        cheatsDeactivateGame(activeGame);

    activeGame = game;

    return 1;
}

void SetupERL()
{
    struct erl_record_t *erl;

    erl_add_global_symbol("GetSyscallHandler", (u32)GetSyscallHandler);
    erl_add_global_symbol("SetSyscall", (u32)SetSyscall);

    /* Install cheat engine ERL */
    erl = load_erl_from_mem_to_addr(_engine_erl_start, 0x00080000, 0, NULL);
    if(!erl)
    {
        printf("Error loading cheat engine ERL!\n");
        SleepThread();
    }

    erl->flags |= ERL_FLAG_CLEAR;
    FlushCache(0);

    printf("Installed cheat engine ERL. Getting symbols...\n");
    struct symbol_t *sym;
    #define GET_SYMBOL(var, name) \
    sym = erl_find_local_symbol(name, erl); \
    if (sym == NULL) { \
        printf("%s: could not find symbol '%s'\n", __FUNCTION__, name); \
        SleepThread(); \
    } \
    printf("%08x %s\n", (u32)sym->address, name); \
    var = (typeof(var))sym->address

    GET_SYMBOL(get_max_hooks, "get_max_hooks");
    GET_SYMBOL(get_num_hooks, "get_num_hooks");
    GET_SYMBOL(add_hook, "add_hook");
    GET_SYMBOL(clear_hooks, "clear_hooks");
    GET_SYMBOL(get_max_codes, "get_max_codes");
    GET_SYMBOL(set_max_codes, "set_max_codes");
    GET_SYMBOL(get_num_codes, "get_num_codes");
    GET_SYMBOL(add_code, "add_code");
    GET_SYMBOL(clear_codes, "clear_codes");

    printf("Symbols loaded.\n");
}

static void readCodes(cheatsCheat_t *cheats)
{
    int i;
    u32 addr, val;
    int nextCodeCanBeHook = 1;
    cheatsCheat_t *cheat = cheats;

    while(cheat)
    {
        if(cheat->enabled && !cheat->skip)
        {
            if(historyFile)
            {
                // Save cheat's hash
                unsigned int cheatHash = hashFunction(cheat->codeLines, cheat->numCodeLines * 8);
                fwrite(&cheatHash, 4, 1, historyFile);
            }

            for(i = 0; i < cheat->numCodeLines; ++i)
            {
                addr = (u32)*((u32 *)cheat->codeLines + 2*i);
                val = (u32)*((u32 *)cheat->codeLines + 2*i + 1);

                if(((addr & 0xfe000000) == 0x90000000) && nextCodeCanBeHook == 1)
                {
                    printf("hook: %08X %08X\n", addr, val);
                    add_hook(addr, val);
                }
                else
                {
                    printf("code: %08X %08X\n", addr, val);
                    add_code(addr, val);
                }

                if ((addr & 0xf0000000) == 0x40000000 || (addr & 0xf0000000) == 0x30000000)
                    nextCodeCanBeHook = 0;
                else
                    nextCodeCanBeHook = 1;
            }
        }

        cheat = cheat->next;
    }
}

/* TODO: Only reads one enable cheat for now. */
void readEnableCodes(cheatsCheat_t *enableCheats)
{
    int i;
    u32 addr, val;
    cheatsCheat_t *cheat = enableCheats;

    if(!cheat)
        return;

    for(i = 0; i < cheat->numCodeLines; ++i)
    {
        addr = (u32)*((u32 *)cheat->codeLines + 2*i);
        val = (u32)*((u32 *)cheat->codeLines + 2*i + 1);

        if((addr & 0xfe000000) == 0x90000000)
        {
            printf("hook: %08X %08X\n", addr, val);
            add_hook(addr, val);
        }
    }
}

void cheatsInstallCodesForEngine()
{
    if(activeGame != NULL)
    {
        SetupERL();

        historyFile = fopen("CheatHistory.bin", "wb");
        if(historyFile)
        {
            unsigned int gameHash = hashFunction(activeGame->title, strlen(activeGame->title));
            fwrite(&gameHash, 4, 1, historyFile);
        }

        printf("Reading enable cheats\n");
        readEnableCodes(activeGame->enableCheats);
        printf("Reading cheats\n");
        readCodes(activeGame->cheats);
        printf("Done readin cheats\n");

        if(historyFile)
            fclose(historyFile);
    }
}
