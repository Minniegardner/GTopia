#pragma once

#include "CommandBase.h"

class GrowIDCmd : public CommandBase<GrowIDCmd> {
public:
    static const CommandInfo& GetInfo();

public:
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};
