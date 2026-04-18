#pragma once

#include "CommandBase.h"

class RepairMagplantWorlds : public CommandBase<RepairMagplantWorlds> {
public:
    static const CommandInfo& GetInfo();
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};
