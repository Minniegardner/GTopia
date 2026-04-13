#pragma once

#include "CommandBase.h"

class RandomPull : public CommandBase<RandomPull> {
public:
    static const CommandInfo& GetInfo();

public:
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};
