#include "Suspend.h"
#include "Utils/StringUtils.h"
#include "../Server/GameServer.h"
#include "../Server/MasterBroadway.h"

const CommandInfo& Suspend::GetInfo()
{
    static CommandInfo info =
    {
        "/suspend <player_prefix> <minutes|off> [reason]",
        "Temporarily mute or unmute a player",
        ROLE_PERM_COMMAND_WARN,
        {
            CompileTimeHashString("suspend"),
            CompileTimeHashString("mute"),
            CompileTimeHashString("unmute")
        }
    };

    return info;
}

void Suspend::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    if(args.size() < 3) {
        pPlayer->SendOnConsoleMessage("`oUsage: " + GetInfo().usage);
        return;
    }

    string query = args[1];
    bool exactMatch = false;
    if(!query.empty() && query[0] == '/') {
        exactMatch = true;
        query.erase(query.begin());
    }

    if(query.size() < 3 && !exactMatch) {
        pPlayer->SendOnConsoleMessage("`4Oops:`` You need at least the first three characters of the person's name.");
        return;
    }

    const string durationArg = ToLower(args[2]);
    const bool unmuteCommand = (durationArg == "off" || durationArg == "0" || durationArg == "unmute");

    string reason = "Please follow the rules.";
    int32 minutes = 0;

    if(!unmuteCommand) {
        if(ToInt(durationArg, minutes) != TO_INT_SUCCESS || minutes <= 0) {
            pPlayer->SendOnConsoleMessage("`4Invalid duration.`` Use minutes or `woff``.");
            return;
        }

        if(minutes > 10080) {
            minutes = 10080;
        }

        if(args.size() >= 4) {
            reason = JoinString(args, " ", 3);
            RemoveExtraWhiteSpaces(reason);
            if(reason.empty()) {
                reason = "Please follow the rules.";
            }

            if(reason.size() > 120) {
                reason.resize(120);
            }
        }
    }

    auto matches = GetGameServer()->FindPlayersByNamePrefix(query, false, 0);
    if(matches.empty()) {
        const uint64 payloadMinutes = unmuteCommand ? 0 : (uint64)minutes;

        GetMasterBroadway()->SendCrossServerActionRequest(
            unmuteCommand ? TCP_CROSS_ACTION_UNSUSPEND : TCP_CROSS_ACTION_SUSPEND,
            pPlayer->GetUserID(),
            pPlayer->GetRawName(),
            query,
            exactMatch,
            reason,
            payloadMinutes);
        return;
    }

    if(!exactMatch && matches.size() > 1) {
        pPlayer->SendOnConsoleMessage("`oThere are multiple players starting with `w" + query + "`o, be more specific.");
        return;
    }

    GamePlayer* pTarget = matches[0];
    if(!pTarget || pTarget == pPlayer) {
        pPlayer->SendOnConsoleMessage("Nope, you can't use that on yourself.");
        return;
    }

    if(durationArg == "off" || durationArg == "0" || durationArg == "unmute") {
        pTarget->ClearMute();
        pTarget->SendOnConsoleMessage("`oYour mute was removed by ``" + pPlayer->GetDisplayName() + "``.");
        pPlayer->SendOnConsoleMessage("`oUnmuted ``" + pTarget->GetDisplayName() + "``.");
        return;
    }

    const uint64 nowMS = Time::GetSystemTime();
    const uint64 muteDurationMS = (uint64)minutes * 60ull * 1000ull;

    pTarget->SetMutedUntilMS(nowMS + muteDurationMS, reason);
    pTarget->SendOnConsoleMessage("`4You have been muted for `w" + ToString(minutes) + "`4 minute(s) by ``" + pPlayer->GetDisplayName() + "``. Reason: `w" + reason + "``");
    pPlayer->SendOnConsoleMessage("`oMuted ``" + pTarget->GetDisplayName() + "`` for `w" + ToString(minutes) + "`` minute(s).");
}
