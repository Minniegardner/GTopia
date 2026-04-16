#pragma once

#include "CommandBase.h"

class SetGems : public CommandBase<SetGems> {
public:
    static const CommandInfo& GetInfo();

public:
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};
