#pragma once

#include "Precompiled.h"

class GamePlayer;

struct CommandTargetSpec {
    bool byUserID = false;
    uint32 userID = 0;
    bool exactMatch = false;
    string query;
};

bool ParseCommandTargetArg(
    const string& rawArg,
    bool allowUserID,
    CommandTargetSpec& outSpec,
    string& errorMessage);

std::vector<GamePlayer*> ResolveLocalTargets(
    const CommandTargetSpec& spec,
    bool sameWorldOnly = false,
    uint32 worldID = 0);

void SendAmbiguousLocalTargetList(
    GamePlayer* pPlayer,
    const string& query,
    const std::vector<GamePlayer*>& matches,
    const string& scopeLabel);
