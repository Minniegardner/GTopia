#include "CommandTargetUtils.h"

#include "Utils/StringUtils.h"
#include "../Server/GameServer.h"

bool ParseCommandTargetArg(
    const string& rawArg,
    bool allowUserID,
    CommandTargetSpec& outSpec,
    string& errorMessage)
{
    outSpec = CommandTargetSpec{};
    errorMessage.clear();

    string arg = rawArg;
    RemoveExtraWhiteSpaces(arg);
    if(arg.empty()) {
        errorMessage = "Target is required.";
        return false;
    }

    if(allowUserID && arg[0] == '#') {
        string uidText = arg.substr(1);
        uint32 userID = 0;
        if(uidText.empty() || ToUInt(uidText, userID) != TO_INT_SUCCESS || userID == 0) {
            errorMessage = "Invalid target format. Use #<UserID> or player name.";
            return false;
        }

        outSpec.byUserID = true;
        outSpec.userID = userID;
        outSpec.exactMatch = true;
        outSpec.query = ToString(userID);
        return true;
    }

    bool exactMatch = false;
    if(!arg.empty() && arg[0] == '/') {
        exactMatch = true;
        arg.erase(arg.begin());
    }

    if(arg.empty()) {
        errorMessage = "Target is required.";
        return false;
    }

    if(!exactMatch && arg.size() < 3) {
        errorMessage = "`6>> `4Oops: `6Enter at least the `#first three characters `6of the persons name.``";
        return false;
    }

    outSpec.byUserID = false;
    outSpec.userID = 0;
    outSpec.exactMatch = exactMatch;
    outSpec.query = arg;
    return true;
}

std::vector<GamePlayer*> ResolveLocalTargets(
    const CommandTargetSpec& spec,
    bool sameWorldOnly,
    uint32 worldID)
{
    if(spec.byUserID) {
        GamePlayer* pTarget = GetGameServer()->GetPlayerByUserID(spec.userID);
        if(!pTarget) {
            return {};
        }

        if(sameWorldOnly && pTarget->GetCurrentWorld() != worldID) {
            return {};
        }

        return { pTarget };
    }

    return GetGameServer()->FindPlayersByNamePrefix(spec.query, sameWorldOnly, worldID);
}

void SendAmbiguousLocalTargetList(
    GamePlayer* pPlayer,
    const string& query,
    const std::vector<GamePlayer*>& matches,
    const string& scopeLabel)
{
    if(!pPlayer) {
        return;
    }

    pPlayer->SendOnConsoleMessage("`oThere are multiple players in the " + scopeLabel + " starting with `w" + query + " `obe more specific.");

    const size_t limit = std::min<size_t>(matches.size(), 8);
    for(size_t i = 0; i < limit; ++i) {
        GamePlayer* pMatch = matches[i];
        if(!pMatch) {
            continue;
        }

        pPlayer->SendOnConsoleMessage("`o- `w" + pMatch->GetRawName() + "`` (`o#" + ToString(pMatch->GetUserID()) + "``)");
    }

    if(matches.size() > limit) {
        pPlayer->SendOnConsoleMessage("`o...and " + ToString((uint32)(matches.size() - limit)) + " more.");
    }
}
