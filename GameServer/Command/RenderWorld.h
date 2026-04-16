#pragma once

#include "CommandBase.h"

class RenderWorld : public CommandBase<RenderWorld> {
public:
    static const CommandInfo& GetInfo();

public:
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};