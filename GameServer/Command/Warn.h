#pragma once

#include "CommandBase.h"

class Warn : public CommandBase<Warn> {
public:
    static const CommandInfo& GetInfo();

public:
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};
