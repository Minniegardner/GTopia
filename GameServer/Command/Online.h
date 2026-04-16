#pragma once

#include "CommandBase.h"

class Online : public CommandBase<Online> {
public:
    static const CommandInfo& GetInfo();

public:
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};
