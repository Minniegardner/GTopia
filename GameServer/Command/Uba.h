#pragma once

#include "CommandBase.h"

class Uba : public CommandBase<Uba> {
public:
    static const CommandInfo& GetInfo();

public:
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};
