#include "Vanish.h"

const CommandInfo& Vanish::GetInfo()
{
    static CommandInfo info =
    {
        "/vanish",
        "Toggle vanish mode",
        ROLE_PERM_COMMAND_GHOST,
        {
            CompileTimeHashString("vanish")
        }
    };

    return info;
}

void Vanish::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    if(pPlayer->GetCharData().HasPlayMod(PLAYMOD_TYPE_STEALTH)) {
        pPlayer->RemovePlayMod(PLAYMOD_TYPE_STEALTH);
    }
    else {
        pPlayer->AddPlayMod(PLAYMOD_TYPE_STEALTH);
    }
}
