#include "Time.h"

#include <ctime>

const CommandInfo& CmdTime::GetInfo()
{
    static CommandInfo info =
    {
        "/time",
        "Show server UTC time",
        ROLE_PERM_NONE,
        {
            CompileTimeHashString("time")
        }
    };

    return info;
}

void CmdTime::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    const std::time_t now = std::time(nullptr);
    std::tm utcTm {};
#ifdef _WIN32
    gmtime_s(&utcTm, &now);
#else
    gmtime_r(&now, &utcTm);
#endif

    char buffer[64] = { 0 };
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &utcTm);
    pPlayer->SendOnConsoleMessage("`$>> `oServer Time (`$UTC +0`o): `w" + string(buffer) + "``");
}
