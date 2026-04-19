#pragma once

#include "CommandBase.h"

class GetRoles : public CommandBase<GetRoles> {
public:
    static const CommandInfo& GetInfo();
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};