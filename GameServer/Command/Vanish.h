#pragma once

#include "CommandBase.h"

class Vanish : public CommandBase<Vanish> {
public:
    static const CommandInfo& GetInfo();
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};
