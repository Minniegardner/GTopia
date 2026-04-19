#pragma once

#include "CommandBase.h"

class AddRole : public CommandBase<AddRole> {
public:
    static const CommandInfo& GetInfo();
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};