#pragma once

#include "CommandBase.h"

class AddLevel : public CommandBase<AddLevel> {
public:
    static const CommandInfo& GetInfo();

public:
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};
