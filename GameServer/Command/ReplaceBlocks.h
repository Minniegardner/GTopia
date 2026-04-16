#pragma once

#include "CommandBase.h"

class ReplaceBlocks : public CommandBase<ReplaceBlocks> {
public:
    static const CommandInfo& GetInfo();

public:
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};
