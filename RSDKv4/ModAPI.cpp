#include "RetroEngine.hpp"

#if RETRO_USE_MOD_LOADER
std::vector<ModInfo> modList;
int activeMod = -1;

char modsPath[0x100];

bool redirectSave = false;
char savePath[0x100];

char modTypeNames[OBJECT_COUNT][0x40];
char modScriptPaths[OBJECT_COUNT][0x40];
byte modScriptFlags[OBJECT_COUNT];
byte modObjCount = 0;

char playerNames[PLAYER_MAX][0x20];
byte playerCount = 0;

#include <filesystem>
#include <locale>

int OpenModMenu()
{
    Engine.gameMode      = ENGINE_INITMODMENU;
    Engine.modMenuCalled = true;
    return 1;
}

#if RETRO_PLATFORM == RETRO_ANDROID
namespace fs = std::__fs::filesystem; // this is so we can avoid using c++17, which causes a ton of warnings w asio and looks ugly
#else
namespace fs = std::filesystem;
#endif

void initMods()
{
    printLog("initMods: modsPath='%s'", modsPath);
    modList.clear();
    forceUseScripts   = forceUseScripts_Config;
    disableFocusPause = disableFocusPause_Config;
    redirectSave      = false;
    sprintf(savePath, "");
    char modBuf[0x100];
    // `modsPath` may already point to the `.../mods/` directory or to the parent
    // folder. Normalize so `modPath` always points to the actual `mods` directory.
    if (StrLength(modsPath) > 0) {
        int len = StrLength(modsPath);
        // If modsPath already ends with "mods" or "mods/", use it directly
        if ((len >= 4 && StrComp(&modsPath[len - 4], "mods") == 0) ||
            (len >= 5 && StrComp(&modsPath[len - 5], "mods/") == 0)) {
            StrCopy(modBuf, modsPath);
        }
        else {
            sprintf(modBuf, "%smods", modsPath);
        }
    }
    else {
        sprintf(modBuf, "mods");
    }
    fs::path modPath(modBuf);


    if (fs::exists(modPath) && fs::is_directory(modPath)) {
        std::string mod_config = modPath.string() + "/modconfig.ini";
        FileIO *configFile     = fOpen(mod_config.c_str(), "r");
        if (configFile) {
            fClose(configFile);
            IniParser modConfig(mod_config.c_str(), false);

            printLog("Found modconfig.ini: %s (entries=%d)", mod_config.c_str(), (int)modConfig.items.size());
            for (int m = 0; m < modConfig.items.size(); ++m) {
                bool active = false;
                ModInfo info;
                const char *key = modConfig.items[m].key;
                modConfig.GetBool("mods", modConfig.items[m].key, &active);
                printLog("modconfig entry: '%s' active=%d", key, active);
                printLog("-> loadMod(folder='%s')", key);
                if (loadMod(&info, modPath.string(), modConfig.items[m].key, active)) {
                    printLog("loadMod succeeded for '%s' (name='%s')", key, info.name.c_str());
                    modList.push_back(info);
                }
                else {
                    printLog("loadMod failed for '%s'", key);
                }
            }
        }

        std::error_code ec;
        auto rdi = fs::directory_iterator(modPath, ec);
        if (!ec) {
            //yay ranged for unrolling
            auto de_b = fs::begin(rdi);
            auto de_e = fs::end(rdi);
            for (; de_b != de_e; de_b.increment(ec)) {
                if (ec) {
                    printLog("Error scanning mods folder");
                    break;
                }
                auto de = *de_b;
                //end ranged for unrolling
                if (de.is_directory()) {
                    fs::path modDirPath = de.path();

                    ModInfo info;

                    std::string modDir            = modDirPath.string().c_str();
                    const std::string mod_inifile = modDir + "/mod.ini";
                    std::string folder            = modDirPath.filename().string();

                    bool flag = true;
                    for (int m = 0; m < modList.size(); ++m) {
                        if (modList[m].folder == folder) {
                            flag = false;
                            break;
                        }
                    }

                    if (flag) {
                        printLog("Directory-scan: attempting loadMod(folder='%s', active=0)", modDirPath.filename().string().c_str());
                        if (loadMod(&info, modPath.string(), modDirPath.filename().string(), false)) {
                            printLog("Directory-scan: loadMod succeeded for '%s'", modDirPath.filename().string().c_str());
                            modList.push_back(info);
                        }
                        else {
                            printLog("Directory-scan: loadMod failed for '%s'", modDirPath.filename().string().c_str());
                        }
                    }
                }
            }
        } else printLog("Error scanning mods folder");
    }

    printLog("initMods: finished, discovered %d mods", (int)modList.size());

    forceUseScripts   = false;
    skipStartMenu     = skipStartMenu_Config;
    disableFocusPause = disableFocusPause_Config;
    forceUseScripts   = forceUseScripts_Config;
    sprintf(savePath, "");
    redirectSave = false;
    for (int m = 0; m < modList.size(); ++m) {
        if (!modList[m].active)
            continue;
        if (modList[m].useScripts)
            forceUseScripts = true;
        if (modList[m].skipStartMenu)
            skipStartMenu = true;
        if (modList[m].disableFocusPause)
            disableFocusPause = true;
        if (modList[m].useScripts)
            forceUseScripts = true;
        if (modList[m].redirectSave) {
            sprintf(savePath, "%s", modList[m].savePath.c_str());
            redirectSave = true;
        }
    }

    ReadSaveRAMData();
    ReadUserdata();
}
bool loadMod(ModInfo *info, std::string modsPath, std::string folder, bool active)
{
    if (!info)
        return false;

    info->fileMap.clear();
    info->name    = "";
    info->desc    = "";
    info->author  = "";
    info->version = "";
    info->folder  = "";
    info->active  = false;

    const std::string modDir = modsPath + "/" + folder;

    FileIO *f = fOpen((modDir + "/mod.ini").c_str(), "r");
    if (f) {
        fClose(f);
        printLog("loadMod: found mod.ini at %s (active=%d)", modDir.c_str(), active);
        IniParser modSettings((modDir + "/mod.ini").c_str(), false);

        info->name    = "Unnamed Mod";
        info->desc    = "";
        info->author  = "Unknown Author";
        info->version = "1.0.0";
        info->folder  = folder;

        char infoBuf[0x100];
        // Name
        StrCopy(infoBuf, "");
        modSettings.GetString("", "Name", infoBuf);
        if (!StrComp(infoBuf, ""))
            info->name = infoBuf;
        // Desc
        StrCopy(infoBuf, "");
        modSettings.GetString("", "Description", infoBuf);
        if (!StrComp(infoBuf, ""))
            info->desc = infoBuf;
        // Author
        StrCopy(infoBuf, "");
        modSettings.GetString("", "Author", infoBuf);
        if (!StrComp(infoBuf, ""))
            info->author = infoBuf;
        // Version
        StrCopy(infoBuf, "");
        modSettings.GetString("", "Version", infoBuf);
        if (!StrComp(infoBuf, ""))
            info->version = infoBuf;

        info->active = active;

        scanModFolder(info);

        printLog("scanModFolder completed for '%s', fileMap entries=%d", info->folder.c_str(), (int)info->fileMap.size());

        info->useScripts = false;
        modSettings.GetBool("", "TxtScripts", &info->useScripts);
        if (info->useScripts && info->active)
            forceUseScripts = true;

        info->skipStartMenu = false;
        modSettings.GetBool("", "SkipStartMenu", &info->skipStartMenu);
        if (info->skipStartMenu && info->active)
            skipStartMenu = true;

        info->disableFocusPause = false;
        modSettings.GetBool("", "DisableFocusPause", &info->disableFocusPause);
        if (info->disableFocusPause && info->active)
            disableFocusPause = true;

        info->redirectSave = false;
        modSettings.GetBool("", "RedirectSaveRAM", &info->redirectSave);
        if (info->redirectSave && info->active) {
            char path[0x100];
            sprintf(path, "mods/%s/", folder.c_str());
            info->savePath = path;
        }

        printLog("loadMod: name='%s' folder='%s' active=%d useScripts=%d skipStartMenu=%d disableFocusPause=%d redirectSave=%d", info->name.c_str(), info->folder.c_str(), info->active, info->useScripts, info->skipStartMenu, info->disableFocusPause, info->redirectSave);

        return true;
    }
    return false;
}

void scanModFolder(ModInfo *info)
{
    if (!info)
        return;

    printLog("scanModFolder: folder='%s' modsPath='%s'", info->folder.c_str(), modsPath);

    char modBuf[0x100];
    // Normalize `modsPath` the same way `initMods()` does so we don't end up
    // with paths like ".../mods/mods/..." when `modsPath` already ends with
    // "mods" or "mods/".
    if (StrLength(modsPath) > 0) {
        int len = StrLength(modsPath);
        if ((len >= 4 && StrComp(&modsPath[len - 4], "mods") == 0) ||
            (len >= 5 && StrComp(&modsPath[len - 5], "mods/") == 0)) {
            StrCopy(modBuf, modsPath);
        }
        else {
            sprintf(modBuf, "%smods", modsPath);
        }
    }
    else {
        sprintf(modBuf, "mods");
    }

    fs::path modPath(modBuf);

    const std::string modDir = modPath.string() + "/" + info->folder;

    info->fileMap.clear();

    // Check for Data/ replacements
    fs::path dataPath(modDir + "/Data");

    if (fs::exists(dataPath) && fs::is_directory(dataPath)) {
        printLog("scanModFolder: found Data folder: %s", dataPath.string().c_str());
        try {
            auto data_rdi = fs::recursive_directory_iterator(dataPath);
            for (auto &data_de : data_rdi) {
                if (data_de.is_regular_file()) {
                    char modBuf[0x100];
                    StrCopy(modBuf, data_de.path().string().c_str());
                    char folderTest[4][0x10] = {
                        "Data/",
                        "Data\\",
                        "data/",
                        "data\\",
                    };
                    int tokenPos = -1;
                    for (int i = 0; i < 4; ++i) {
                        tokenPos = FindStringToken(modBuf, folderTest[i], 1);
                        if (tokenPos >= 0)
                            break;
                    }

                    if (tokenPos >= 0) {
                        char buffer[0x80];
                        for (int i = StrLength(modBuf); i >= tokenPos; --i) {
                            buffer[i - tokenPos] = modBuf[i] == '\\' ? '/' : modBuf[i];
                        }

                        // printLog(modBuf);
                        std::string path(buffer);
                        std::string modPath(modBuf);
                        char pathLower[0x100];
                        memset(pathLower, 0, sizeof(char) * 0x100);
                        for (int c = 0; c < path.size(); ++c) {
                            pathLower[c] = tolower(path.c_str()[c]);
                        }

                        info->fileMap.insert(std::pair<std::string, std::string>(pathLower, modBuf));
                        printLog("scanModFolder: mapping '%s' -> '%s'", pathLower, modBuf);
                    }
                }
            }
        } catch (fs::filesystem_error fe) {
            printLog("Data Folder Scanning Error: ");
            printLog(fe.what());
        }
    }

    // Check for Scripts/ replacements
    fs::path scriptPath(modDir + "/Scripts");

    if (fs::exists(scriptPath) && fs::is_directory(scriptPath)) {
        printLog("scanModFolder: found Scripts folder: %s", scriptPath.string().c_str());
        try {
            auto data_rdi = fs::recursive_directory_iterator(scriptPath);
            for (auto &data_de : data_rdi) {
                if (data_de.is_regular_file()) {
                    char modBuf[0x100];
                    StrCopy(modBuf, data_de.path().string().c_str());
                    char folderTest[4][0x10] = {
                        "Scripts/",
                        "Scripts\\",
                        "scripts/",
                        "scripts\\",
                    };
                    int tokenPos = -1;
                    for (int i = 0; i < 4; ++i) {
                        tokenPos = FindStringToken(modBuf, folderTest[i], 1);
                        if (tokenPos >= 0)
                            break;
                    }

                    if (tokenPos >= 0) {
                        char buffer[0x80];
                        for (int i = StrLength(modBuf); i >= tokenPos; --i) {
                            buffer[i - tokenPos] = modBuf[i] == '\\' ? '/' : modBuf[i];
                        }

                        // printLog(modBuf);
                        std::string path(buffer);
                        std::string modPath(modBuf);
                        char pathLower[0x100];
                        memset(pathLower, 0, sizeof(char) * 0x100);
                        for (int c = 0; c < path.size(); ++c) {
                            pathLower[c] = tolower(path.c_str()[c]);
                        }

                        info->fileMap.insert(std::pair<std::string, std::string>(pathLower, modBuf));
                        printLog("scanModFolder: mapping '%s' -> '%s'", pathLower, modBuf);
                    }
                }
            }
        } catch (fs::filesystem_error fe) {
            printLog("Script Folder Scanning Error: ");
            printLog(fe.what());
        }
    }

    // Check for Bytecode/ replacements
    fs::path bytecodePath(modDir + "/Bytecode");

    if (fs::exists(bytecodePath) && fs::is_directory(bytecodePath)) {
        printLog("scanModFolder: found Bytecode folder: %s", bytecodePath.string().c_str());
        try {
            auto data_rdi = fs::recursive_directory_iterator(bytecodePath);
            for (auto &data_de : data_rdi) {
                if (data_de.is_regular_file()) {
                    char modBuf[0x100];
                    StrCopy(modBuf, data_de.path().string().c_str());
                    char folderTest[4][0x10] = {
                        "Bytecode/",
                        "Bytecode\\",
                        "bytecode/",
                        "bytecode\\",
                    };
                    int tokenPos = -1;
                    for (int i = 0; i < 4; ++i) {
                        tokenPos = FindStringToken(modBuf, folderTest[i], 1);
                        if (tokenPos >= 0)
                            break;
                    }

                    if (tokenPos >= 0) {
                        char buffer[0x80];
                        for (int i = StrLength(modBuf); i >= tokenPos; --i) {
                            buffer[i - tokenPos] = modBuf[i] == '\\' ? '/' : modBuf[i];
                        }

                        // printLog(modBuf);
                        std::string path(buffer);
                        std::string modPath(modBuf);
                        char pathLower[0x100];
                        memset(pathLower, 0, sizeof(char) * 0x100);
                        for (int c = 0; c < path.size(); ++c) {
                            pathLower[c] = tolower(path.c_str()[c]);
                        }

                        info->fileMap.insert(std::pair<std::string, std::string>(pathLower, modBuf));
                        printLog("scanModFolder: mapping '%s' -> '%s'", pathLower, modBuf);
                    }
                }
            }
        } catch (fs::filesystem_error fe) {
            printLog("Bytecode Folder Scanning Error: ");
            printLog(fe.what());
        }
    }
}

void saveMods()
{
    char modBuf[0x100];
    sprintf(modBuf, "%smods", modsPath);
    fs::path modPath(modBuf);

    if (fs::exists(modPath) && fs::is_directory(modPath)) {
        std::string mod_config = modPath.string() + "/modconfig.ini";
        IniParser modConfig;

        for (int m = 0; m < modList.size(); ++m) {
            ModInfo *info = &modList[m];

            modConfig.SetBool("mods", info->folder.c_str(), info->active);
        }

        modConfig.Write(mod_config.c_str(), false);
    }
}

void RefreshEngine()
{
    // Reload entire engine
    Engine.LoadGameConfig("Data/Game/GameConfig.bin");
#if RETRO_USING_SDL2
    if (Engine.window) {
        char gameTitle[0x40];
        sprintf(gameTitle, "%s%s", Engine.gameWindowText, Engine.usingDataFile ? "" : " (Using Data Folder)");
        SDL_SetWindowTitle(Engine.window, gameTitle);
    }
#elif RETRO_USING_SDL1
    char gameTitle[0x40];
    sprintf(gameTitle, "%s%s", Engine.gameWindowText, Engine.usingDataFile ? "" : " (Using Data Folder)");
    SDL_WM_SetCaption(gameTitle, NULL);
#endif
    ClearMeshData();
    ClearTextures(true);

    nativeEntityCountBackup = 0;
    memset(backupEntityList, 0, sizeof(backupEntityList));
    memset(objectEntityBackup, 0, sizeof(objectEntityBackup));

    nativeEntityCountBackupS = 0;
    memset(backupEntityListS, 0, sizeof(backupEntityListS));
    memset(objectEntityBackupS, 0, sizeof(objectEntityBackupS));

    for (int i = 0; i < FONTLIST_COUNT; ++i) {
        fontList[i].count = 2;
    }

    ReleaseStageSfx();
    ReleaseGlobalSfx();
    LoadGlobalSfx();
    InitLocalizedStrings();

    for (nativeEntityPos = 0; nativeEntityPos < nativeEntityCount; ++nativeEntityPos) {
        NativeEntity *entity = &objectEntityBank[activeEntityList[nativeEntityPos]];
        entity->createPtr(entity);
    }

    Engine.gameType = GAME_SONIC2;
    if (strstr(Engine.gameWindowText, "Sonic 1")) {
        Engine.gameType = GAME_SONIC1;
    }

    forceUseScripts   = false;
    skipStartMenu     = skipStartMenu_Config;
    disableFocusPause = disableFocusPause_Config;
    forceUseScripts   = forceUseScripts_Config;
    sprintf(savePath, "");
    redirectSave = false;
    for (int m = 0; m < modList.size(); ++m) {
        if (!modList[m].active)
            continue;
        if (modList[m].useScripts)
            forceUseScripts = true;
        if (modList[m].skipStartMenu)
            skipStartMenu = true;
        if (modList[m].disableFocusPause)
            disableFocusPause = true;
        if (modList[m].useScripts)
            forceUseScripts = true;
        if (modList[m].redirectSave) {
            sprintf(savePath, "%s", modList[m].savePath.c_str());
            redirectSave = true;
        }
    }

    saveMods();

    ReadSaveRAMData();
    ReadUserdata();
}

void GetModCount() { scriptEng.checkResult = (int)modList.size(); }
void GetModName(int *textMenu, int *highlight, uint *id, int *unused)
{
    if (*id >= modList.size())
        return;

    TextMenu *menu                       = &gameMenu[*textMenu];
    menu->entryHighlight[menu->rowCount] = *highlight;
    AddTextMenuEntry(menu, modList[*id].name.c_str());
}

void GetModDescription(int *textMenu, int *highlight, uint *id, int *unused)
{
    if (*id >= modList.size())
        return;

    TextMenu *menu                       = &gameMenu[*textMenu];
    menu->entryHighlight[menu->rowCount] = *highlight;
    AddTextMenuEntry(menu, modList[*id].desc.c_str());
}

void GetModAuthor(int *textMenu, int *highlight, uint *id, int *unused)
{
    if (*id >= modList.size())
        return;

    TextMenu *menu                       = &gameMenu[*textMenu];
    menu->entryHighlight[menu->rowCount] = *highlight;
    AddTextMenuEntry(menu, modList[*id].author.c_str());
}

void GetModVersion(int *textMenu, int *highlight, uint *id, int *unused)
{
    if (*id >= modList.size())
        return;

    TextMenu *menu                       = &gameMenu[*textMenu];
    menu->entryHighlight[menu->rowCount] = *highlight;
    AddTextMenuEntry(menu, modList[*id].version.c_str());
}

void GetModActive(uint *id, int *unused)
{
    scriptEng.checkResult = false;
    if (*id >= modList.size())
        return;
    scriptEng.checkResult = modList[*id].active;
}

void SetModActive(uint *id, int *active)
{
    if (*id >= modList.size())
        return;

    modList[*id].active = *active;
}

int GetSceneID(byte listID, const char *sceneName)
{
    if (listID >= 3)
        return -1;

    char scnName[0x40];
    int scnPos = 0;
    int pos    = 0;
    while (sceneName[scnPos]) {
        if (sceneName[scnPos] != ' ')
            scnName[pos++] = sceneName[scnPos];
        ++scnPos;
    }
    scnName[pos] = 0;

    for (int s = 0; s < stageListCount[listID]; ++s) {
        char nameBuffer[0x40];

        scnPos = 0;
        pos    = 0;
        while (stageList[listID][s].name[scnPos]) {
            if (stageList[listID][s].name[scnPos] != ' ')
                nameBuffer[pos++] = stageList[listID][s].name[scnPos];
            ++scnPos;
        }
        nameBuffer[pos] = 0;

        if (StrComp(scnName, nameBuffer)) {
            return s;
        }
    }
    return -1;
}

#endif
