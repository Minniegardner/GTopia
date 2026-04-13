#pragma once

#include "CommandBase.h"

class Kick : public CommandBase<Kick> {
public:
    static const CommandInfo& GetInfo();

public:
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};
