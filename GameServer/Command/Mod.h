#pragma once

#include "CommandBase.h"

class Mod : public CommandBase<Mod> {
public:
    static const CommandInfo& GetInfo();

public:
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};
