#include "Input.h"
#include "../../../World/WorldManager.h"
#include "../../../Server/GameServer.h"
#include "Utils/StringUtils.h"

/**
 * fix here
 */

void Input::Execute(GamePlayer* pPlayer, ParsedTextPacket<8>& packet)
{
    if(!pPlayer) {
        return;
    }

    World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
    if(!pWorld) {
        return;
    }

    auto pText = packet.Find(CompileTimeHashString("text"));
    if(!pText) {
        return;
    }

    string text(pText->value, pText->size);
    
    RemoveExtraWhiteSpaces(text);
    if(text.empty()) {
        return;
    }

    if(text[0] == '/') {
        pPlayer->SendOnConsoleMessage("`o" + text);

        auto args = Split(text, ' ');
        GetGameServer()->ExecuteCommand(pPlayer, args);
        return;
    }

    if(pPlayer->IsMuted()) {
        const uint64 nowMS = Time::GetSystemTime();
        const uint64 mutedUntilMS = pPlayer->GetMutedUntilMS();
        uint64 secondsLeft = 1;

        if(mutedUntilMS > nowMS) {
            secondsLeft = (mutedUntilMS - nowMS) / 1000;
            if(secondsLeft == 0) {
                secondsLeft = 1;
            }
        }

        string reason = pPlayer->GetMuteReason();
        if(reason.empty()) {
            reason = "No reason provided";
        }

        pPlayer->SendOnConsoleMessage("`4You are muted for another `w" + ToString((int)secondsLeft) + "`4 seconds. Reason: `w" + reason + "``");
        return;
    }

    char colorCode = pPlayer->GetRole()->GetChatColor();

    string consoleText = "<" + pPlayer->GetDisplayName() + "``> ";
    if(colorCode != 0) {
        consoleText += "`" + string(1, colorCode);
    }
    consoleText += text + "``";

    pWorld->SendTalkBubbleAndConsoleToAll(consoleText, false, pPlayer);
}