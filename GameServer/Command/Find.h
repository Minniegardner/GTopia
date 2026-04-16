#pragma once

#include "CommandBase.h"

class Find : public CommandBase<Find> {
public:
    static const CommandInfo& GetInfo();

public:
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};
