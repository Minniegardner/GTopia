#pragma once

#include "CommandBase.h"

class Weather : public CommandBase<Weather> {
public:
    static const CommandInfo& GetInfo();

public:
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};
