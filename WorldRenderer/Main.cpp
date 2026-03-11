#include "WorldRenderer.h"
#include "Context.h"
#include "Utils/ResourceManager.h"
#include "Item/ItemInfoManager.h"
#include "Utils/Timer.h"
#include "IO/Log.h"
#include "Math/Random.h"
#include "Utils/StringUtils.h"

#include <signal.h>
void SignalStop(int32 signum) 
{ 
    LOGGER_LOG_WARN("Received signal %d", signum);
    GetContext()->Stop();
}

bool ReadArgs(int argc, char const* argv[]) 
{
    bool idSet = false;

    for(int i = 1; i < argc; ++i) {
        if(string(argv[i]) == "--id") {
            uint16 id = ToUInt(argv[i+1]);
            if(id <= 0) {
                LOGGER_LOG_ERROR("server id must bigger than 0");
                return false;
            }

            GetContext()->SetID(id);
            idSet = true;
        }
    }

    if(!idSet) {
        LOGGER_LOG_ERROR("No --id param detected it must set!");
        return false;
    }

    return true;
}

int main(int argc, char const* argv[])
{
    if(!ReadArgs(argc, argv)) {
        return 0;
    }

    GetContext()->Init();

    GameConfig* pGameConfig = GetContext()->GetGameConfig();
    if(!pGameConfig->LoadConfig(GetProgramPath() + "/config.txt")) {
        LOGGER_LOG_ERROR("Failed to load config.txt");
        return 0;
    }

    if(!pGameConfig->LoadServersClient(GetProgramPath() + "/servers.txt", GetContext()->GetID()) != 2) {
        LOGGER_LOG_ERROR("Failed to load servers.txt");
        return 0;
    }

    if(!IsFolderExists(pGameConfig->rendererStaticPath)) {
        LOGGER_LOG_ERROR("Static file %s is not exist?", pGameConfig->rendererStaticPath.c_str());
        return 0;
    }

    ResourceManager* pResMgr = GetResourceManager();
    pResMgr->SetResourcePath(pGameConfig->rendererStaticPath);

    if(!GetItemInfoManager()->Load(GetProgramPath() + "/items.txt")) {
        LOGGER_LOG_ERROR("Failed to load items.txt");
        return 0;
    }

    SetRandomSeed(Time::GetSystemTime());
    RandomizeRandomSeed();

    GetResourceManager()->Kill();
    GetContext()->Kill();
    return 0;
}
