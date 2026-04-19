#pragma once

#include "CommandBase.h"

class ListRoles : public CommandBase<ListRoles> {
public:
    static const CommandInfo& GetInfo();
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};