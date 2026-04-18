#pragma once

#include "CommandBase.h"

class Maintenance : public CommandBase<Maintenance> {
public:
    static const CommandInfo& GetInfo();
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};