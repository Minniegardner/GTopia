#pragma once

#include "CommandBase.h"

class Stats : public CommandBase<Stats> {
public:
    static const CommandInfo& GetInfo();

public:
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};
