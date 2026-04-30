#include "Reply.h"
#include "../Server/GameServer.h"
#include "Utils/StringUtils.h"

const CommandInfo& Reply::GetInfo()
{
    static CommandInfo info =
    {
        "/r <message>",
        "Reply to last private message",
        ROLE_PERM_NONE,
        {
            CompileTimeHashString("r")
        }
    };

    return info;
}

void Reply::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    if(args.size() < 2) {
        pPlayer->SendOnConsoleMessage("You can't send an empty message, that's too boring.");
        return;
    }

    const uint32 targetUserID = pPlayer->GetLastWhisperUserID();
    if(targetUserID == 0) {
        pPlayer->SendOnConsoleMessage("`6>> No one has messaged you yet. Use `$/msg <name> <message>`6.``");
        return;
    }

    const uint64 nowMS = Time::GetSystemTime();
    const uint64 lastWhisperAt = pPlayer->GetLastWhisperAtMS();
    if(lastWhisperAt == 0 || (nowMS > lastWhisperAt && (nowMS - lastWhisperAt) > 60000)) {
        pPlayer->SendOnConsoleMessage("`6>> We can't really track that person. Please use `$/msg `6again.``");
        return;
    }

    GamePlayer* pTarget = GetPlayerManager()->GetPlayerByUserID(targetUserID);
    if(!pTarget || pTarget == pPlayer) {
        pPlayer->SendOnConsoleMessage("`6>> We can't really track that person. Please use `$/msg `6again.``");
        return;
    }

    string message = JoinString(args, " ", 1);
    RemoveExtraWhiteSpaces(message);
    if(message.empty()) {
        pPlayer->SendOnConsoleMessage("You can't send an empty message, that's too boring.");
        return;
    }

    if(message.size() > 180) {
        message.resize(180);
    }

    pPlayer->SetLastWhisperAtMS(nowMS);
    pPlayer->SetLastWhisperUserID(pTarget->GetUserID());
    pTarget->SetLastWhisperAtMS(nowMS);
    pTarget->SetLastWhisperUserID(pPlayer->GetUserID());

    pPlayer->SendOnConsoleMessage("`o(Sent to `$" + pTarget->GetRawName() + "`o)");
    pTarget->SendOnConsoleMessage("`o(From `$" + pPlayer->GetRawName() + "`o): " + message);
}
