#pragma once

#include "CommandBase.h"

class TogglePlayMod : public CommandBase<TogglePlayMod> {
public:
    static const CommandInfo& GetInfo();

public:
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};