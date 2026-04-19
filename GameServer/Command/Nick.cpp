#include "Nick.h"

#include "Utils/StringUtils.h"
#include "../World/WorldManager.h"

const CommandInfo& Nick::GetInfo()
{
    static CommandInfo info =
    {
        "/nick [nickname]",
        "Set or clear your nickname",
        ROLE_PERM_COMMAND_MOD,
        {
            CompileTimeHashString("nick")
        }
    };

    return info;
}

void Nick::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    if(args.size() == 1) {
        pPlayer->SetNickname("");
        pPlayer->SendOnConsoleMessage("Your nickname has been cleared.");
    }
    else {
        string nickname = JoinString(args, " ", 1);
        RemoveExtraWhiteSpaces(nickname);
        if(nickname.empty()) {
            pPlayer->SendOnConsoleMessage("Usage: " + GetInfo().usage);
            return;
        }

        if(nickname.size() > 24) {
            nickname.resize(24);
        }

        pPlayer->SetNickname(nickname);
        pPlayer->SendOnConsoleMessage("Your nickname has been set to " + nickname + "`o.");
    }

    if(pPlayer->GetCurrentWorld() != 0) {
        World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
        if(pWorld) {
            pWorld->SendNameChangeToAll(pPlayer);
            pWorld->SendSetCharPacketToAll(pPlayer);
        }
    }
}
