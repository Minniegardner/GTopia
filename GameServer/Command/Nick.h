#pragma once

#include "CommandBase.h"

class Nick : public CommandBase<Nick> {
public:
    static const CommandInfo& GetInfo();
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};
