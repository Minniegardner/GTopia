#pragma once

#include "CommandBase.h"

class SummonAll : public CommandBase<SummonAll> {
public:
    static const CommandInfo& GetInfo();
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};