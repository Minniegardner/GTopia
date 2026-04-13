#include "DialogReturn.h"
#include "IO/Log.h"

#include "../../../Player/Dialog/SignDialog.h"
#include "../../../Player/Dialog/TrashDialog.h"
#include "../../../Player/Dialog/DropDialog.h"
#include "../../../Player/Dialog/LockDialog.h"
#include "../../../Player/Dialog/DoorDialog.h"
#include "../../../Player/Dialog/MachineDialog.h"
#include "../../../Player/Dialog/RenderWorldDialog.h"
#include "../../../Player/GamePlayer.h"
#include "../../../Server/GameServer.h"
#include "Utils/StringUtils.h"
#include "Utils/Timer.h"

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
                const bool addedMine = pPlayer->AddFriendUserID(pTarget->GetUserID());
                const bool addedOther = pTarget->AddFriendUserID(pPlayer->GetUserID());

                if(addedMine || addedOther) {
                    pPlayer->SendOnConsoleMessage("`oFriend request accepted with ``" + pTarget->GetDisplayName() + "``.");
                    pTarget->SendOnConsoleMessage("`oYou are now friends with ``" + pPlayer->GetDisplayName() + "``.");
                }
                else {
                    pPlayer->SendOnConsoleMessage("`oYou are already friends with ``" + pTarget->GetDisplayName() + "``.");
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
                const bool addedMine = pPlayer->AddFriendUserID(pTarget->GetUserID());
                const bool addedOther = pTarget->AddFriendUserID(pPlayer->GetUserID());

                if(addedMine || addedOther) {
                    pPlayer->SendOnConsoleMessage("`oFriend request accepted with ``" + pTarget->GetDisplayName() + "``.");
                    pTarget->SendOnConsoleMessage("`oYou are now friends with ``" + pPlayer->GetDisplayName() + "``.");
                }
                else {
                    pPlayer->SendOnConsoleMessage("`oYou are already friends with ``" + pTarget->GetDisplayName() + "``.");
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

        case CompileTimeHashString("render_reply"): {
            RenderWorldDialog::Handle(pPlayer);
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
    }
}