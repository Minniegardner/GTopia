#include "Input.h"
#include "../../../World/WorldManager.h"
#include "../../../Server/GameServer.h"
#include "Utils/StringUtils.h"
#include "Player/PlayModManager.h"

#include <cctype>

/**
 * fix here
 */

namespace {

constexpr const char* kLastChatStat = "LAST_CHAT_MS";
constexpr uint64 kChatIntervalMS = 500;
constexpr uint16 kIronMmmffItemID = 408;

ePlayModType GetMutePlayModType()
{
    static ePlayModType s_muteType = PLAYMOD_TYPE_NONE;
    if(s_muteType != PLAYMOD_TYPE_NONE) {
        return s_muteType;
    }

    for(uint32 i = 1; i <= 64; ++i) {
        PlayMod* pPlayMod = GetPlayModManager()->GetPlayMod((ePlayModType)i);
        if(!pPlayMod) {
            continue;
        }

        if(ToLower(pPlayMod->GetName()) == "mute") {
            s_muteType = (ePlayModType)i;
            return s_muteType;
        }
    }

    return PLAYMOD_TYPE_NONE;
}

bool HasMuteSpeechEffect(GamePlayer* pPlayer)
{
    if(!pPlayer) {
        return false;
    }

    const ePlayModType muteType = GetMutePlayModType();
    const bool hasMutePlayMod = (muteType != PLAYMOD_TYPE_NONE) && pPlayer->GetCharData().HasPlayMod(muteType);
    const bool hasIronMmmff = pPlayer->GetInventory().IsWearingItem(kIronMmmffItemID);
    return hasMutePlayMod || hasIronMmmff || pPlayer->IsMuted();
}

void ApplyMuteSpeechFilter(string& message)
{
    string filtered;
    filtered.reserve(message.size());

    for(size_t i = 0; i < message.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(message[i]);
        if(std::isspace(ch)) {
            filtered.push_back(static_cast<char>(ch));
            continue;
        }

        if(std::isupper(ch)) {
            filtered.push_back((i % 2 == 0) ? 'M' : 'F');
        }
        else {
            filtered.push_back((i % 2 == 0) ? 'm' : 'f');
        }
    }

    message.swap(filtered);
}

}

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

    const string lowerText = ToLower(text);
    const bool isMeMessage = (lowerText.rfind("/me ", 0) == 0);

    if(text[0] == '/' && !isMeMessage) {
        pPlayer->SendOnConsoleMessage("`o" + text);

        auto args = Split(text, ' ');
        GetGameServer()->ExecuteCommand(pPlayer, args);
        return;
    }

    const uint64 nowMS = Time::GetSystemTime();
    const uint64 lastChatMS = pPlayer->GetCustomStatValue(kLastChatStat);
    if(lastChatMS != 0 && nowMS - lastChatMS < kChatIntervalMS) {
        pPlayer->SendOnConsoleMessage(" `5** Warning: `wYou're sending messages way too fast!");
        return;
    }
    pPlayer->SetCustomStatValue(kLastChatStat, nowMS);

    if(!pPlayer->IsMuted() && pPlayer->GetMutedUntilMS() != 0) {
        pPlayer->ClearMute();
    }

    if(HasMuteSpeechEffect(pPlayer)) {
        ApplyMuteSpeechFilter(text);
    }

    char colorCode = pPlayer->GetRole()->GetChatColor();
    if(pPlayer->GetRole() && pPlayer->GetRole()->GetName() == "owner") {
        colorCode = '^';
    }

    if(isMeMessage) {
        text = text.substr(4);
        RemoveExtraWhiteSpaces(text);
        if(text.empty()) {
            return;
        }
    }

    string consoleText;
    if(isMeMessage) {
        consoleText = "`6<`w" + pPlayer->GetDisplayName() + " ";
        if(colorCode != 0) {
            consoleText += "`" + string(1, colorCode);
        }
        consoleText += text + "``6>";
    }
    else {
        consoleText = "<" + pPlayer->GetDisplayName() + "``> ";
        if(colorCode != 0) {
            consoleText += "`" + string(1, colorCode);
        }
        consoleText += text + "``";
    }

    pWorld->SendTalkBubbleAndConsoleToAll(consoleText, false, pPlayer);
}