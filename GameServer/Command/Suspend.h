#pragma once

#include "CommandBase.h"

class Suspend : public CommandBase<Suspend> {
public:
    static const CommandInfo& GetInfo();

public:
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};
