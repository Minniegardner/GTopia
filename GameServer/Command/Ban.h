#pragma once

#include "CommandBase.h"

class Ban : public CommandBase<Ban> {
public:
    static const CommandInfo& GetInfo();

public:
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};
