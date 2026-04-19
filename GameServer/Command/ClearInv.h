#pragma once

#include "CommandBase.h"

class ClearInv : public CommandBase<ClearInv> {
public:
    static const CommandInfo& GetInfo();
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};
