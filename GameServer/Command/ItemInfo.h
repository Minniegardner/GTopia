#pragma once

#include "CommandBase.h"

class ItemInfoCmd : public CommandBase<ItemInfoCmd> {
public:
    static const CommandInfo& GetInfo();

public:
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};
