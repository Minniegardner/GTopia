#include "Wrench.h"

#include "../../../Server/GameServer.h"
#include "Item/ItemInfoManager.h"
#include "Utils/StringUtils.h"
#include "Utils/DialogBuilder.h"

void Wrench::Execute(GamePlayer* pPlayer, ParsedTextPacket<8>& packet)
{
    if(!pPlayer || !pPlayer->HasState(PLAYER_STATE_IN_GAME)) {
        return;
    }

    auto pNetIDField = packet.Find(CompileTimeHashString("netid"));
    if(!pNetIDField) {
        pNetIDField = packet.Find(CompileTimeHashString("netID"));
    }

    if(!pNetIDField) {
        return;
    }

    int32 targetNetID = 0;
    if(ToInt(string(pNetIDField->value, pNetIDField->size), targetNetID) != TO_INT_SUCCESS) {
        return;
    }

    GamePlayer* pTarget = GetGameServer()->GetPlayerByNetID(targetNetID);
    if(!pTarget) {
        return;
    }

    if(pTarget->GetCurrentWorld() == 0 || pTarget->GetCurrentWorld() != pPlayer->GetCurrentWorld()) {
        return;
    }

    DialogBuilder db;
    db.SetDefaultColor('o')
    ->EmbedData("netID", pTarget->GetNetID());

    if(pTarget->GetUserID() == pPlayer->GetUserID()) {
        string achievementList;
        for(const auto& achievement : pTarget->GetAchievements()) {
            if(!achievementList.empty()) {
                achievementList += ", ";
            }
            achievementList += achievement;
        }

        db.AddLabelWithIcon("`w" + pTarget->GetDisplayName() + "``", ITEM_ID_WRENCH, true)
        ->AddTextBox("`oYou have `w" + ToString(pTarget->GetInventory().GetCapacity()) + "`` backpack slots.``")
        ->AddTextBox("`oCurrent world ID: `w" + ToString(pTarget->GetCurrentWorld()) + "````")
        ->AddTextBox("`oAchievements: `w" + ToString((int)pTarget->GetAchievements().size()) + "``")
        ->AddTextBox(achievementList.empty() ? "`oNo achievements unlocked yet.``" : "`oUnlocked: `w" + achievementList + "``")
        ->AddSpacer()
        ->EndDialog("popup", "", "Continue");
    }
    else {
        db.AddLabelWithIcon("`w" + pTarget->GetDisplayName() + "``", ITEM_ID_WRENCH, true)
        ->AddButton("trade", "`wTrade``")
        ->AddButton("sendpm", "`wSend Message``")
        ->AddButton("friend_add", "`wAdd as friend``")
        ->AddButton("show_clothes", "`wView worn clothes``")
        ->AddButton("ignore_player", "`wIgnore Player``")
        ->AddButton("report_player", "`wReport Player``")
        ->AddSpacer()
        ->EndDialog("popup", "", "Continue");
    }

    pPlayer->SendOnDialogRequest(db.Get());
}