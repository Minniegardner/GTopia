#pragma once

#include "Precompiled.h"
#include "../Player/GamePlayer.h"
#include "Player/Role.h"

struct CommandInfo
{
    string usage = "";
    string desc = "";
    eRolePerm perm = ROLE_PERM_NONE;
    std::vector<uint32> aliases = {};
};

template<typename T>
class CommandBase {
public:
    static const CommandInfo& GetInfo()
    {
        return T::GetInfo();
    }

    static void Execute(GamePlayer* pPlayer, std::vector<string>& args) 
    {
        T::Execute(pPlayer, args);
    }
};