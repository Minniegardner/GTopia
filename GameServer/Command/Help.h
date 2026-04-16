#pragma once

#include "CommandBase.h"

class Help : public CommandBase<Help> {
public:
    static const CommandInfo& GetInfo();

public:
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};
