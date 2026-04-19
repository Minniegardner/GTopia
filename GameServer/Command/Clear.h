#pragma once

#include "CommandBase.h"

class Clear : public CommandBase<Clear> {
public:
    static const CommandInfo& GetInfo();
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};
