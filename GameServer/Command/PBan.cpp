#include "PBan.h"
#include "Utils/StringUtils.h"

const CommandInfo& PBan::GetInfo()
{
    static CommandInfo info =
    {
        "/pban <target|#userid> <seconds|-1> <reason>",
        "Suspend an account (online or offline)",
        ROLE_PERM_COMMAND_BAN,
        {
            CompileTimeHashString("pban")
        }
    };

    return info;
}

void PBan::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    if(args.size() < 4) {
        pPlayer->SendOnConsoleMessage("`4Oops:`` Usage: /pban <target|#userid> <seconds|-1> <reason>");
        return;
    }

    const string target = args[1];
    const bool isUserIDTarget = !target.empty() && target[0] == '#';
    if(!isUserIDTarget && target.size() < 3) {
        pPlayer->SendOnConsoleMessage("`4Oops:`` You need at least the first three characters of the person's name, or use #userid.");
        return;
    }

    int32 durationSec = 0;
    if(ToInt(args[2], durationSec) != TO_INT_SUCCESS || durationSec == 0 || durationSec < -1) {
        pPlayer->SendOnConsoleMessage("`4Oops:`` Duration must be positive seconds, or `w-1`` for permanent.");
        return;
    }

    string reason = JoinString(args, " ", 3);
    RemoveExtraWhiteSpaces(reason);
    if(reason.empty()) {
        pPlayer->SendOnConsoleMessage("`4Oops:`` Usage: /pban <target|#userid> <seconds|-1> <reason>");
        return;
    }

    if(reason.size() > 140) {
        reason.resize(140);
    }

    pPlayer->BeginPBanRequest(target, durationSec, reason);
}
