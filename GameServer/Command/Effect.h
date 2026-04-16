#pragma once

#include "CommandBase.h"

class Effect : public CommandBase<Effect> {
public:
    static const CommandInfo& GetInfo();

public:
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};
