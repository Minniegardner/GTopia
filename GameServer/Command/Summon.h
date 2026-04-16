#pragma once

#include "CommandBase.h"

class Summon : public CommandBase<Summon> {
public:
    static const CommandInfo& GetInfo();

public:
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};
