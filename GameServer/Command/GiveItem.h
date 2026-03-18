#pragma once

#include "CommandBase.h"

class GiveItem : public CommandBase<GiveItem> {
public:
    static const CommandInfo& GetInfo();

public:
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};