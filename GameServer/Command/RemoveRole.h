#pragma once

#include "CommandBase.h"

class RemoveRole : public CommandBase<RemoveRole> {
public:
    static const CommandInfo& GetInfo();
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};