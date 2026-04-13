#pragma once

#include "CommandBase.h"

class Ping : public CommandBase<Ping> {
public:
    static const CommandInfo& GetInfo();

public:
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};
