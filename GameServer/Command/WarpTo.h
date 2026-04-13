#pragma once

#include "CommandBase.h"

class WarpTo : public CommandBase<WarpTo> {
public:
    static const CommandInfo& GetInfo();

public:
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};
