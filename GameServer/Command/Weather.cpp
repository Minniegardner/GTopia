#include "Weather.h"
#include "Utils/StringUtils.h"
#include "../World/WorldManager.h"

const CommandInfo& Weather::GetInfo()
{
    static CommandInfo info =
    {
        "/weather <weather_id>",
        "Set world weather",
        ROLE_PERM_COMMAND_MOD,
        {
            CompileTimeHashString("weather")
        }
    };

    return info;
}

void Weather::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
    if(!pWorld) {
        pPlayer->SendOnConsoleMessage("`4Failed to find world.");
        return;
    }

    if(args.size() != 2) {
        pPlayer->SendOnConsoleMessage("Weather requires 1 arguement (Weather ID)");
        return;
    }

    uint32 weatherID = 0;
    if(ToUInt(args[1], weatherID) != TO_INT_SUCCESS) {
        pPlayer->SendOnConsoleMessage("Weather requires 1 arguement (Weather ID)");
        return;
    }

    pPlayer->SendOnConsoleMessage("Setting Weather ID To " + ToString(weatherID) + "..");
    pWorld->SetCurrentWeather(weatherID);
    pWorld->SendCurrentWeatherToAll();
}
