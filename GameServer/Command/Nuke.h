#pragma once

#include "CommandBase.h"

class Nuke : public CommandBase<Nuke> {
public:
    static const CommandInfo& GetInfo();
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};
