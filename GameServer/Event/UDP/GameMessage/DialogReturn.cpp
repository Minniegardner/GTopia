#include "DialogReturn.h"
#include "IO/Log.h"

#include "../../../Player/Dialog/SignDialog.h"
#include "../../../Player/Dialog/TrashDialog.h"
#include "../../../Player/Dialog/DropDialog.h"
#include "../../../Player/Dialog/LockDialog.h"
#include "../../../Player/Dialog/DoorDialog.h"
#include "../../../Player/Dialog/MachineDialog.h"
#include "../../../Player/Dialog/ChemsynthDialog.h"
#include "../../../Player/Dialog/RenderWorldDialog.h"
#include "../../../Player/GamePlayer.h"
#include "../../../Server/GameServer.h"
#include "../../../Player/Dialog/RegisterDialog.h"
#include "../../../World/WorldManager.h"
#include "Utils/StringUtils.h"
#include "Utils/Timer.h"
#include "Utils/DialogBuilder.h"
#include "Item/ItemInfoManager.h"

namespace {

constexpr uint16 kTelephoneItemID = 3898;
constexpr const char* kDailyQuestClaimDayStat = "DailyQuestClaimDay";

string BuildDailyQuestStatKey(const char* prefix, uint32 epochDay, uint16 itemID)
{
    return string(prefix) + "_" + ToString(epochDay) + "_" + ToString(itemID);
}

string GetItemNameSafe(uint32 itemID)
{
    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(itemID);
    if(!pItem) {
        return "Item #" + ToString(itemID);
    }

    return pItem->name;
}

void ShowTelephoneMainMenu(GamePlayer* pPlayer)
{
    if(!pPlayer) {
        return;
    }

    DialogBuilder db;
    db.SetDefaultColor('o')
        ->AddLabelWithIcon("`wCrazy Jim's Quest Emporium``", kTelephoneItemID, true, true)
        ->AddLabel("HEEEEYYY there Growtopian! I'm Crazy Jim, and my quests are so crazy they're KERRRRAAAAZZY!!")
        ->AddSpacer()
        ->AddButton("GotoCrazyJimDailyQuest", "Daily Quest")
        ->EndDialog("TelephoneEdit", "", "Hang Up");

    pPlayer->SendOnDialogRequest(db.Get());
}

void ShowTelephoneQuestDialog(GamePlayer* pPlayer)
{
    if(!pPlayer) {
        return;
    }

    const uint32 currentEpochDay = (uint32)(Time::GetTimeSinceEpoch() / 86400ull);
    if(pPlayer->GetCustomStatValue(kDailyQuestClaimDayStat) == currentEpochDay) {
        DialogBuilder db;
        db.SetDefaultColor('o')
            ->AddLabelWithIcon("`wCrazy Jim's Quest Emporium``", kTelephoneItemID, true, true)
            ->AddLabel("You've already completed my Daily Quest for today! Call me back after midnight to hear about my next cravings.")
            ->AddButton("GotoCrazyJimMainMenu", "Back")
            ->EndDialog("TelephoneEdit", "", "Hang Up");

        pPlayer->SendOnDialogRequest(db.Get());
        return;
    }

    const TCPDailyQuestData& dq = GetMasterBroadway()->GetDailyQuestData();
    DialogBuilder db;
    db.SetDefaultColor('o')
        ->AddLabelWithIcon("`wCrazy Jim's Quest Emporium``", kTelephoneItemID, true, true)
        ->AddLabel("I guess some people call me Crazy Jim because I'm a bit of a hoarder. But I'm very particular about what I want! And today, what I want is this:")
        ->AddSpacer()
        ->AddLabelWithIcon("`2" + ToString(dq.questItemOneAmount) + " " + GetItemNameSafe(dq.questItemOneID) + "``", (uint16)dq.questItemOneID)
        ->AddLabel("and", true)
        ->AddLabelWithIcon("`2" + ToString(dq.questItemTwoAmount) + " " + GetItemNameSafe(dq.questItemTwoID) + "``", (uint16)dq.questItemTwoID)
        ->AddSpacer()
        ->AddLabel("You shove all that through the phone (it works, I've tried it), and I will hand you one of the rewards from my personal collection! But hurry, this offer is only good until midnight, and only one turn-in per person!")
        ->AddSpacer();

    const uint64 countOne = pPlayer->GetInventory().GetCountOfItem((uint16)dq.questItemOneID);
    const uint64 countTwo = pPlayer->GetInventory().GetCountOfItem((uint16)dq.questItemTwoID);
    db.AddLabel("You have " + ToString(countOne) + " " + GetItemNameSafe(dq.questItemOneID) + " and " + ToString(countTwo) + " " + GetItemNameSafe(dq.questItemTwoID) + ".");

    const uint64 collectOne = pPlayer->GetCustomStatValue(BuildDailyQuestStatKey("DQ_COLLECT", currentEpochDay, (uint16)dq.questItemOneID));
    const uint64 collectTwo = pPlayer->GetCustomStatValue(BuildDailyQuestStatKey("DQ_COLLECT", currentEpochDay, (uint16)dq.questItemTwoID));
    const uint64 breakOne = pPlayer->GetCustomStatValue(BuildDailyQuestStatKey("DQ_BREAK", currentEpochDay, (uint16)dq.questItemOneID));
    const uint64 breakTwo = pPlayer->GetCustomStatValue(BuildDailyQuestStatKey("DQ_BREAK", currentEpochDay, (uint16)dq.questItemTwoID));
    db.AddLabel("Today's tracked progress: collect " + ToString(collectOne) + "/" + ToString(dq.questItemOneAmount) + " + " + ToString(collectTwo) + "/" + ToString(dq.questItemTwoAmount) + ", break " + ToString(breakOne) + " + " + ToString(breakTwo) + ".");

    if(countOne >= dq.questItemOneAmount && countTwo >= dq.questItemTwoAmount) {
        db.AddButton("GotoCrazyJimTurnInDailyQuest", "`2Turn in quest items");
    }

    db.AddButton("GotoCrazyJimMainMenu", "Back")
        ->EndDialog("TelephoneEdit", "", "Hang Up");

    pPlayer->SendOnDialogRequest(db.Get());
}

void TurnInTelephoneQuest(GamePlayer* pPlayer)
{
    if(!pPlayer) {
        return;
    }

    const uint32 currentEpochDay = (uint32)(Time::GetTimeSinceEpoch() / 86400ull);
    if(pPlayer->GetCustomStatValue(kDailyQuestClaimDayStat) == currentEpochDay) {
        pPlayer->SendOnTalkBubble("You've already completed my Daily Quest for today!", true);
        return;
    }

    const TCPDailyQuestData& dq = GetMasterBroadway()->GetDailyQuestData();
    const uint64 haveOne = pPlayer->GetInventory().GetCountOfItem((uint16)dq.questItemOneID);
    const uint64 haveTwo = pPlayer->GetInventory().GetCountOfItem((uint16)dq.questItemTwoID);
    if(haveOne < dq.questItemOneAmount || haveTwo < dq.questItemTwoAmount) {
        pPlayer->SendOnTalkBubble("I don't have enough items to turn it in!", true);
        return;
    }

    const uint64 growtokenCount = pPlayer->GetInventory().GetCountOfItem(ITEM_ID_GROWTOKEN);
    if(growtokenCount >= 200) {
        pPlayer->ModifyInventoryItem(ITEM_ID_GROWTOKEN, -(int16)growtokenCount);
        pPlayer->ModifyInventoryItem(ITEM_ID_MEGA_GROWTOKEN, 1);
    }

    if(!pPlayer->GetInventory().HaveRoomForItem(ITEM_ID_GROWTOKEN, 1)) {
        pPlayer->SendOnTalkBubble("This seems far-fetched, but you've already reached the maximum amount of Growtokens.", true);
        return;
    }

    pPlayer->ModifyInventoryItem((uint16)dq.questItemOneID, -(int16)dq.questItemOneAmount);
    pPlayer->ModifyInventoryItem((uint16)dq.questItemTwoID, -(int16)dq.questItemTwoAmount);
    pPlayer->ModifyInventoryItem(ITEM_ID_GROWTOKEN, 1);
    pPlayer->SetCustomStatValue(kDailyQuestClaimDayStat, currentEpochDay);

    World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
    if(pWorld) {
        pWorld->SendParticleEffectToAll(pPlayer->GetWorldPos().x + 12.0f, pPlayer->GetWorldPos().y + 15.0f, 198);
        pWorld->PlaySFXForEveryone("audio/piano_nice.wav", 0);
    }

    pPlayer->SendOnTalkBubble("`wThanks, pardner! Have 1 `2Growtoken`w!", true);
    pPlayer->SendOnConsoleMessage("[`6You jammed " + ToString(dq.questItemOneAmount) + " " + GetItemNameSafe(dq.questItemOneID) + " and " + ToString(dq.questItemTwoAmount) + " " + GetItemNameSafe(dq.questItemTwoID) + " into the phone, and 1 `2Growtoken`` popped out!``]");
}

}

void DialogReturn::Execute(GamePlayer* pPlayer, ParsedTextPacket<8>& packet)
{
    if(!pPlayer) {
        return;
    }

    auto pDialogName = packet.Find(CompileTimeHashString("dialog_name"));
    if(!pDialogName || pDialogName->size > 50) {
        return;
    }

    uint32 hashedDialogName = HashString(pDialogName->value, pDialogName->size);

    switch(hashedDialogName) {
        case CompileTimeHashString("WrenchSelf"): {
            auto pButtonClicked = packet.Find(CompileTimeHashString("buttonClicked"));
            if(!pButtonClicked) {
                return;
            }

            string buttonClicked(pButtonClicked->value, pButtonClicked->size);
            if(buttonClicked == "PlayerInfo" || buttonClicked == "SocialProfile" || buttonClicked == "PlayerStats" ||
                buttonClicked == "Settings" || buttonClicked == "Titles" || buttonClicked == "Worlds")
            {
                pPlayer->SendWrenchSelf(buttonClicked);
            }
            break;
        }

        case CompileTimeHashString("WrenchOthers"): {
            auto pButtonClicked = packet.Find(CompileTimeHashString("buttonClicked"));
            auto pNetID = packet.Find(CompileTimeHashString("OtherNetID"));

            if(!pButtonClicked || !pNetID) {
                return;
            }

            int32 netID = 0;
            if(ToInt(string(pNetID->value, pNetID->size), netID) != TO_INT_SUCCESS) {
                return;
            }

            GamePlayer* pTarget = GetGameServer()->GetPlayerByNetID(netID);
            if(!pTarget || pTarget == pPlayer || pTarget->GetCurrentWorld() != pPlayer->GetCurrentWorld()) {
                pPlayer->SendOnConsoleMessage("`4Target is no longer valid.``");
                return;
            }

            string buttonClicked(pButtonClicked->value, pButtonClicked->size);
            if(buttonClicked == "Trade") {
                pPlayer->StartTrade(pTarget);
            }
            else if(buttonClicked == "sendpm") {
                pPlayer->SetLastWhisperUserID(pTarget->GetUserID());
                pPlayer->SetLastWhisperAtMS(Time::GetSystemTime());
                pPlayer->SendOnConsoleMessage("`oUse `/msg " + pTarget->GetRawName() + " <message>`` to send a private message.");
            }
            else if(buttonClicked == "show_clothes") {
                pPlayer->SendOnConsoleMessage(
                    "`oClothes for ``" + pTarget->GetDisplayName() +
                    "``: Hair `w" + ToString(pTarget->GetInventory().GetClothByPart(BODY_PART_HAIR)) +
                    "`` | Shirt `w" + ToString(pTarget->GetInventory().GetClothByPart(BODY_PART_SHIRT)) +
                    "`` | Pants `w" + ToString(pTarget->GetInventory().GetClothByPart(BODY_PART_PANT)) +
                    "`` | Shoes `w" + ToString(pTarget->GetInventory().GetClothByPart(BODY_PART_SHOE)) + "``"
                );
            }
            else if(buttonClicked == "Pull") {
                std::vector<string> cmdArgs = { "/pull", pTarget->GetRawName() };
                GetGameServer()->ExecuteCommand(pPlayer, cmdArgs);
            }
            else if(buttonClicked == "Kick") {
                std::vector<string> cmdArgs = { "/kick", pTarget->GetRawName() };
                GetGameServer()->ExecuteCommand(pPlayer, cmdArgs);
            }
            else if(buttonClicked == "Ban") {
                std::vector<string> cmdArgs = { "/ban", pTarget->GetRawName() };
                GetGameServer()->ExecuteCommand(pPlayer, cmdArgs);
            }
            else if(buttonClicked == "Add" || buttonClicked == "friend_add") {
                if(pPlayer->IsFriendWith(pTarget->GetUserID())) {
                    pPlayer->SendOnConsoleMessage("`oYou are already friends with ``" + pTarget->GetDisplayName() + "``.");
                }
                else if(!pPlayer->AcceptFriendRequestFrom(pTarget)) {
                    pPlayer->SendFriendRequestTo(pTarget);
                }
            }
            else if(buttonClicked == "Unfriend") {
                const bool removedMine = pPlayer->RemoveFriendUserID(pTarget->GetUserID());
                const bool removedOther = pTarget->RemoveFriendUserID(pPlayer->GetUserID());
                if(removedMine || removedOther) {
                    pPlayer->SendOnConsoleMessage("`oYou are no longer friends with ``" + pTarget->GetDisplayName() + "``.");
                }
            }
            else if(buttonClicked == "Ignore" || buttonClicked == "ignore_player") {
                if(pPlayer->IsIgnoring(pTarget->GetUserID())) {
                    pPlayer->RemoveIgnoredUserID(pTarget->GetUserID());
                    pPlayer->SendOnConsoleMessage("`oYou no longer ignore ``" + pTarget->GetDisplayName() + "``.");
                }
                else {
                    pPlayer->AddIgnoredUserID(pTarget->GetUserID());
                    pPlayer->SendOnConsoleMessage("`oYou now ignore ``" + pTarget->GetDisplayName() + "``.");
                }
            }
            else if(buttonClicked == "Report" || buttonClicked == "report_player") {
                pPlayer->SendOnConsoleMessage("`oReport queued for review on ``" + pTarget->GetDisplayName() + "``.");
                pPlayer->SendOnTalkBubble("`wThanks for the report, our team will review it.", true);
            }
            break;
        }

        case CompileTimeHashString("popup"): {
            auto pButtonClicked = packet.Find(CompileTimeHashString("buttonClicked"));
            auto pNetID = packet.Find(CompileTimeHashString("netID"));

            if(!pButtonClicked || !pNetID) {
                return;
            }

            int32 netID = 0;
            if(ToInt(string(pNetID->value, pNetID->size), netID) != TO_INT_SUCCESS) {
                return;
            }

            string buttonClicked(pButtonClicked->value, pButtonClicked->size);
            GamePlayer* pTarget = GetGameServer()->GetPlayerByNetID(netID);
            if(!pTarget || pTarget == pPlayer || pTarget->GetCurrentWorld() != pPlayer->GetCurrentWorld()) {
                pPlayer->SendOnConsoleMessage("`4Target is no longer valid.``");
                return;
            }

            if(buttonClicked == "trade" || buttonClicked == "Trade") {
                pPlayer->StartTrade(pTarget);
            }
            else if(buttonClicked == "pull" || buttonClicked == "Pull") {
                std::vector<string> cmdArgs = { "/pull", pTarget->GetRawName() };
                GetGameServer()->ExecuteCommand(pPlayer, cmdArgs);
            }
            else if(buttonClicked == "kick" || buttonClicked == "Kick") {
                std::vector<string> cmdArgs = { "/kick", pTarget->GetRawName() };
                GetGameServer()->ExecuteCommand(pPlayer, cmdArgs);
            }
            else if(buttonClicked == "ban" || buttonClicked == "Ban") {
                std::vector<string> cmdArgs = { "/ban", pTarget->GetRawName() };
                GetGameServer()->ExecuteCommand(pPlayer, cmdArgs);
            }
            else if(buttonClicked == "sendpm" || buttonClicked == "PM") {
                pPlayer->SetLastWhisperUserID(pTarget->GetUserID());
                pPlayer->SetLastWhisperAtMS(Time::GetSystemTime());
                pPlayer->SendOnConsoleMessage("`oUse `/msg " + pTarget->GetRawName() + " <message>`` to send a private message.");
            }
            else if(buttonClicked == "show_clothes" || buttonClicked == "Show Clothes") {
                pPlayer->SendOnConsoleMessage(
                    "`oClothes for ``" + pTarget->GetDisplayName() +
                    "``: Hair `w" + ToString(pTarget->GetInventory().GetClothByPart(BODY_PART_HAIR)) +
                    "`` | Shirt `w" + ToString(pTarget->GetInventory().GetClothByPart(BODY_PART_SHIRT)) +
                    "`` | Pants `w" + ToString(pTarget->GetInventory().GetClothByPart(BODY_PART_PANT)) +
                    "`` | Shoes `w" + ToString(pTarget->GetInventory().GetClothByPart(BODY_PART_SHOE)) + "``"
                );
            }
            else if(buttonClicked == "friend_add" || buttonClicked == "Add") {
                if(pPlayer->IsFriendWith(pTarget->GetUserID())) {
                    pPlayer->SendOnConsoleMessage("`oYou are already friends with ``" + pTarget->GetDisplayName() + "``.");
                }
                else if(!pPlayer->AcceptFriendRequestFrom(pTarget)) {
                    pPlayer->SendFriendRequestTo(pTarget);
                }
            }
            else if(buttonClicked == "Unfriend") {
                const bool removedMine = pPlayer->RemoveFriendUserID(pTarget->GetUserID());
                const bool removedOther = pTarget->RemoveFriendUserID(pPlayer->GetUserID());
                if(removedMine || removedOther) {
                    pPlayer->SendOnConsoleMessage("`oYou are no longer friends with ``" + pTarget->GetDisplayName() + "``.");
                }
            }
            else if(buttonClicked == "ignore_player" || buttonClicked == "Ignore") {
                if(pPlayer->IsIgnoring(pTarget->GetUserID())) {
                    pPlayer->RemoveIgnoredUserID(pTarget->GetUserID());
                    pPlayer->SendOnConsoleMessage("`oYou no longer ignore ``" + pTarget->GetDisplayName() + "``.");
                }
                else {
                    pPlayer->AddIgnoredUserID(pTarget->GetUserID());
                    pPlayer->SendOnConsoleMessage("`oYou now ignore ``" + pTarget->GetDisplayName() + "``.");
                }
            }
            else if(buttonClicked == "report_player" || buttonClicked == "Report") {
                pPlayer->SendOnConsoleMessage("`oReport sent for review: `w" + pTarget->GetDisplayName() + "``.");
                pPlayer->SendOnTalkBubble("`wThanks for the report, our team will review it.", true);
            }

            break;
        }

        case CompileTimeHashString("TradeModify"): {
            auto pButtonClicked = packet.Find(CompileTimeHashString("buttonClicked"));
            if(!pButtonClicked) {
                return;
            }

            string buttonClicked(pButtonClicked->value, pButtonClicked->size);
            if(buttonClicked != "OK" && buttonClicked != "Confirm") {
                return;
            }

            auto pStatus = packet.Find(CompileTimeHashString("status"));
            if(!pStatus) {
                pStatus = packet.Find(CompileTimeHashString("accepted"));
            }

            if(pStatus) {
                string statusStr(pStatus->value, pStatus->size);
                int32 status = 0;

                if(statusStr == "true") {
                    status = 1;
                }
                else if(statusStr == "false") {
                    status = 0;
                }
                else if(ToInt(statusStr, status) != TO_INT_SUCCESS) {
                    return;
                }

                pPlayer->AcceptOffer(status > 0);
                break;
            }

            auto pItemID = packet.Find(CompileTimeHashString("ItemID"));
            if(!pItemID) {
                pItemID = packet.Find(CompileTimeHashString("itemID"));
            }

            auto pAmount = packet.Find(CompileTimeHashString("Amount"));
            if(!pAmount) {
                pAmount = packet.Find(CompileTimeHashString("amount"));
            }

            if(!pAmount) {
                pAmount = packet.Find(CompileTimeHashString("count"));
            }

            if(!pItemID) {
                return;
            }

            uint32 itemID = 0;
            if(ToUInt(string(pItemID->value, pItemID->size), itemID) != TO_INT_SUCCESS || itemID > UINT16_MAX) {
                return;
            }

            uint32 amount = 0;
            if(pAmount) {
                if(ToUInt(string(pAmount->value, pAmount->size), amount) != TO_INT_SUCCESS) {
                    amount = 0;
                }
            }

            pPlayer->ModifyOffer((uint16)itemID, (uint16)amount);
            break;
        }

        case CompileTimeHashString("TradeConfirm"): {
            auto pButtonClicked = packet.Find(CompileTimeHashString("buttonClicked"));
            if(!pButtonClicked) {
                return;
            }

            string buttonClicked(pButtonClicked->value, pButtonClicked->size);
            if(buttonClicked == "Confirm" || buttonClicked == "OK") {
                pPlayer->ConfirmOffer();
            }
            else if(buttonClicked == "Cancel") {
                pPlayer->CancelTrade(false);
            }
            break;
        }

        case CompileTimeHashString("sign_edit"): {
            auto pTileX = packet.Find(CompileTimeHashString("tilex"));
            auto pTileY = packet.Find(CompileTimeHashString("tiley"));
            auto pSignText = packet.Find(CompileTimeHashString("sign_text"));

            if(!pTileX || !pTileY || !pSignText) {
                return;
            }

            // we need to int converter that supports non null term
            // idk if its good ways to convert it to a str
            // really we need it??

            int32 tileX = 0;
            if(ToInt(string(pTileX->value, pTileX->size), tileX) != TO_INT_SUCCESS) {
                return;
            }

            int32 tileY = 0;
            if(ToInt(string(pTileY->value, pTileY->size), tileY) != TO_INT_SUCCESS) {
                return;
            }

            SignDialog::Handle(pPlayer, string(pSignText->value, pSignText->size), tileX, tileY);
            break;
        }

        case CompileTimeHashString("trash_item"):
        case CompileTimeHashString("trash_item2"): {
            auto pItemID = packet.Find(CompileTimeHashString("itemID"));
            auto pCount = packet.Find(CompileTimeHashString("count"));

            if(!pItemID || !pCount) {
                return;
            }

            uint32 itemID = 0;
            if(ToUInt(string(pItemID->value, pItemID->size), itemID) != TO_INT_SUCCESS) {
                return;
            }

            int32 count = 0;
            if(ToInt(string(pCount->value, pCount->size), count) != TO_INT_SUCCESS) {
                return;
            }

            if(hashedDialogName == CompileTimeHashString("trash_item2")) {
                TrashDialog::HandleUntradeable(pPlayer, itemID, count);
            }
            else {
                TrashDialog::Handle(pPlayer, itemID, count);
            }
            break;
        }

        case CompileTimeHashString("drop_item"): {
            auto pItemID = packet.Find(CompileTimeHashString("itemID"));
            auto pCount = packet.Find(CompileTimeHashString("count"));

            if(!pItemID || !pCount) {
                return;
            }

            uint32 itemID = 0;
            if(ToUInt(string(pItemID->value, pItemID->size), itemID) != TO_INT_SUCCESS) {
                return;
            }

            int32 count = 0;
            if(ToInt(string(pCount->value, pCount->size), count) != TO_INT_SUCCESS) {
                return;
            }

            if(itemID > UINT16_MAX) {
                return;
            }

            DropDialog::Handle(pPlayer, (uint16)itemID, (int16)count);
            break;
        }

        case CompileTimeHashString("lock_edit"):
        case CompileTimeHashString("lock_remove_self"): {
            LockDialog::Handle(pPlayer, packet);
            break;
        }

        case CompileTimeHashString("door_edit"): {
            DoorDialog::Handle(pPlayer, packet);
            break;
        }

        case CompileTimeHashString("VendEdit"):
        case CompileTimeHashString("VendConfirm"):
        case CompileTimeHashString("Magplant"): {
            MachineDialog::Handle(pPlayer, packet);
            break;
        }

        case CompileTimeHashString("Chemsynth"): {
            ChemsynthDialog::Handle(pPlayer, packet);
            break;
        }

        case CompileTimeHashString("render_reply"): {
            RenderWorldDialog::Handle(pPlayer);
            break;
        }

        case CompileTimeHashString("TelephoneEdit"): {
            auto pButtonClicked = packet.Find(CompileTimeHashString("buttonClicked"));
            auto pNumber = packet.Find(CompileTimeHashString("Number"));

            string buttonClicked = pButtonClicked ? string(pButtonClicked->value, pButtonClicked->size) : "";
            int32 number = 0;
            if(pNumber) {
                ToInt(string(pNumber->value, pNumber->size), number);
            }

            if(buttonClicked == "GotoCrazyJimMainMenu" || number == 12345) {
                ShowTelephoneMainMenu(pPlayer);
            }
            else if(buttonClicked == "GotoCrazyJimDailyQuest") {
                ShowTelephoneQuestDialog(pPlayer);
            }
            else if(buttonClicked == "GotoCrazyJimTurnInDailyQuest") {
                TurnInTelephoneQuest(pPlayer);
            }
            break;
        }

        case CompileTimeHashString("command_mod_query"): {
            auto pButtonClicked = packet.Find(CompileTimeHashString("buttonClicked"));
            if(!pButtonClicked || pButtonClicked->size != 6 || string(pButtonClicked->value, pButtonClicked->size) != "Search") {
                return;
            }

            auto pTargetName = packet.Find(CompileTimeHashString("target_name"));
            if(!pTargetName || pTargetName->size == 0 || pTargetName->size > 24) {
                pPlayer->SendOnConsoleMessage("`4Invalid player name.``");
                return;
            }

            string query(pTargetName->value, pTargetName->size);
            RemoveExtraWhiteSpaces(query);

            std::vector<string> cmdArgs;
            cmdArgs.push_back("/mod");
            cmdArgs.push_back(query);
            GetGameServer()->ExecuteCommand(pPlayer, cmdArgs);
            break;
        }

        case CompileTimeHashString("command_mod_actions"): {
            auto pButtonClicked = packet.Find(CompileTimeHashString("buttonClicked"));
            if(!pButtonClicked || pButtonClicked->size == 0 || pButtonClicked->size > 40) {
                return;
            }

            string buttonClicked(pButtonClicked->value, pButtonClicked->size);

            auto onAction = buttonClicked.find('_');
            if(onAction == string::npos) {
                return;
            }

            string action = buttonClicked.substr(0, onAction);
            string netIDStr = buttonClicked.substr(onAction + 1);

            int32 netID = 0;
            if(ToInt(netIDStr, netID) != TO_INT_SUCCESS) {
                return;
            }

            GamePlayer* pTarget = GetGameServer()->GetPlayerByNetID(netID);
            if(!pTarget || pTarget == pPlayer || pTarget->GetCurrentWorld() != pPlayer->GetCurrentWorld()) {
                pPlayer->SendOnConsoleMessage("`4Target is no longer valid.``");
                return;
            }

            std::vector<string> cmdArgs;
            if(action == "pull") {
                cmdArgs.push_back("/pull");
                cmdArgs.push_back(pTarget->GetRawName());
            }
            else if(action == "kick") {
                cmdArgs.push_back("/kick");
                cmdArgs.push_back(pTarget->GetRawName());
            }
            else if(action == "ban") {
                cmdArgs.push_back("/ban");
                cmdArgs.push_back(pTarget->GetRawName());
            }
            else if(action == "warn") {
                string reason = "Please follow the rules.";
                auto pWarnReason = packet.Find(CompileTimeHashString("warn_reason"));
                if(pWarnReason && pWarnReason->size > 0 && pWarnReason->size <= 120) {
                    reason.assign(pWarnReason->value, pWarnReason->size);
                    RemoveExtraWhiteSpaces(reason);
                    if(reason.empty()) {
                        reason = "Please follow the rules.";
                    }
                }

                cmdArgs.push_back("/warn");
                cmdArgs.push_back(pTarget->GetRawName());
                cmdArgs.push_back(reason);
            }
            else if(action == "warpto") {
                cmdArgs.push_back("/warpto");
                cmdArgs.push_back(pTarget->GetRawName());
            }
            else {
                return;
            }

            GetGameServer()->ExecuteCommand(pPlayer, cmdArgs);
            break;
        }

        case CompileTimeHashString("find_item_dialog"): {
            auto pButtonClicked = packet.Find(CompileTimeHashString("buttonClicked"));
            if(!pButtonClicked || pButtonClicked->size != 4 || string(pButtonClicked->value, pButtonClicked->size) != "Find") {
                return;
            }

            auto pSearch = packet.Find(CompileTimeHashString("SearchString"));
            string query;

            if(pSearch && pSearch->size > 0 && pSearch->size <= 128) {
                query.assign(pSearch->value, pSearch->size);
                RemoveExtraWhiteSpaces(query);
            }

            std::vector<string> cmdArgs;
            cmdArgs.push_back("/find");
            if(!query.empty()) {
                cmdArgs.push_back(query);
            }

            GetGameServer()->ExecuteCommand(pPlayer, cmdArgs);
            break;
        }

        case CompileTimeHashString("growid_apply"): {
            RegisterDialog::Handle(pPlayer, packet);
            break;
        }

        case CompileTimeHashString("SocialPortal"): {
            auto pButtonClicked = packet.Find(CompileTimeHashString("buttonClicked"));
            if(!pButtonClicked) {
                return;
            }

            string buttonClicked(pButtonClicked->value, pButtonClicked->size);
            if(buttonClicked == "GotoFriendsMenu") {
                pPlayer->SendFriendMenu("GotoFriendsMenu");
            }
            break;
        }

        case CompileTimeHashString("FriendMenu"): {
            auto pButtonClicked = packet.Find(CompileTimeHashString("buttonClicked"));
            if(!pButtonClicked) {
                return;
            }

            string buttonClicked(pButtonClicked->value, pButtonClicked->size);
            if(buttonClicked == "GotoSocialPortal") {
                pPlayer->SendWrenchSelf("SocialProfile");
            }
            else if(buttonClicked == "FriendAll") {
                pPlayer->SendFriendMenu("FriendAll");
            }
            else if(buttonClicked == "SeeSent") {
                pPlayer->SendOnConsoleMessage("`oSent requests are shown in wrench interactions.``");
                pPlayer->SendFriendMenu("GotoFriendsMenu");
            }
            else if(buttonClicked == "SeeReceived") {
                pPlayer->SendOnConsoleMessage("`oReceived requests can be accepted from wrench by choosing Add as Friend.``");
                pPlayer->SendFriendMenu("GotoFriendsMenu");
            }
            break;
        }
    }
}