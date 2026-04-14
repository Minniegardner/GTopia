#pragma once

#include "CommandBase.h"

class SuperBroadcast : public CommandBase<SuperBroadcast> {
public:
    static const CommandInfo& GetInfo();

public:
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};
