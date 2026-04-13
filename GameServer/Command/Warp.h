#pragma once

#include "CommandBase.h"

class Warp : public CommandBase<Warp> {
public:
    static const CommandInfo& GetInfo();

public:
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};
