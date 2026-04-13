#pragma once

#include "CommandBase.h"

class Pull : public CommandBase<Pull> {
public:
    static const CommandInfo& GetInfo();

public:
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};
