#include "Input.h"
#include "../../../World/WorldManager.h"
#include "../../../Server/GameServer.h"
#include "Utils/StringUtils.h"

void Input::Execute(GamePlayer* pPlayer, ParsedTextPacket<8>& packet)
{
    World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
    if(!pWorld) {
        return;
    }

    auto pText = packet.Find(CompileTimeHashString("text"));
    if(!pText) {
        return;
    }

    string text(pText->value, pText->size);
    if(text.size() > 120) {
        return;
    }

    RemoveExtraWhiteSpaces(text);
    if(text.empty() || text[0] ==  ' ') {
        return;
    }

    if(text[0] == '/') {
        auto args = Split(text, ' ');
        GetGameServer()->ExecuteCommand(pPlayer, args);
        return;
    }

    char colorCode = pPlayer->GetRole()->GetChatColor();

    string consoleText = "<" + pPlayer->GetDisplayName() + "``> ";
    if(colorCode != 0) {
        consoleText += "`" + string(1, colorCode);
    }
    consoleText += text + "``";

    pWorld->SendToAll([&](GamePlayer* pOther) { //umm is it really ok?
        pOther->SendOnConsoleMessage(consoleText);
    });
}