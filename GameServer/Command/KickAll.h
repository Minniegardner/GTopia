#pragma once

#include "CommandBase.h"

class KickAll : public CommandBase<KickAll> {
public:
    static const CommandInfo& GetInfo();

public:
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};
