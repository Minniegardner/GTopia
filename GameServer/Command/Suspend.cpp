#include "Suspend.h"
#include "Utils/StringUtils.h"
#include "../Server/GameServer.h"

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
        pPlayer->SendOnConsoleMessage("`6>> `4Oops: `6Enter at least the `#first three characters `6of the persons name.``");
        return;
    }

    auto matches = GetGameServer()->FindPlayersByNamePrefix(query, false, 0);
    if(matches.empty()) {
        pPlayer->SendOnConsoleMessage("`6>> No one online who has a name starting with `$" + query + "`6.``");
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

    const string durationArg = ToLower(args[2]);
    if(durationArg == "off" || durationArg == "0" || durationArg == "unmute") {
        pTarget->ClearMute();
        pTarget->SendOnConsoleMessage("`oYour mute was removed by ``" + pPlayer->GetDisplayName() + "``.");
        pPlayer->SendOnConsoleMessage("`oUnmuted ``" + pTarget->GetDisplayName() + "``.");
        return;
    }

    int32 minutes = 0;
    if(ToInt(durationArg, minutes) != TO_INT_SUCCESS || minutes <= 0) {
        pPlayer->SendOnConsoleMessage("`4Invalid duration.`` Use minutes or `woff``.");
        return;
    }

    if(minutes > 10080) {
        minutes = 10080;
    }

    string reason = "Please follow the rules.";
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

    const uint64 nowMS = Time::GetSystemTime();
    const uint64 muteDurationMS = (uint64)minutes * 60ull * 1000ull;

    pTarget->SetMutedUntilMS(nowMS + muteDurationMS, reason);
    pTarget->SendOnConsoleMessage("`4You have been muted for `w" + ToString(minutes) + "`4 minute(s) by ``" + pPlayer->GetDisplayName() + "``. Reason: `w" + reason + "``");
    pPlayer->SendOnConsoleMessage("`oMuted ``" + pTarget->GetDisplayName() + "`` for `w" + ToString(minutes) + "`` minute(s).");
}
