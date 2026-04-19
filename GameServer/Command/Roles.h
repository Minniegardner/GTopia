#pragma once

#include "CommandBase.h"

class Roles : public CommandBase<Roles> {
public:
    static const CommandInfo& GetInfo();
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};