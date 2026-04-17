#pragma once

#include "CommandBase.h"

class SpawnGhost : public CommandBase<SpawnGhost> {
public:
    static const CommandInfo& GetInfo();

public:
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};
