#include "DialogReturn.h"
#include "IO/Log.h"

#include "../../../Player/Dialog/SignDialog.h"
#include "../../../Player/Dialog/TrashDialog.h"
#include "../../../Player/Dialog/DropDialog.h"
#include "../../../Player/Dialog/LockDialog.h"
#include "../../../Player/Dialog/RenderWorldDialog.h"
#include "../../../Player/GamePlayer.h"
#include "../../../Server/GameServer.h"
#include "Utils/StringUtils.h"

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
            if(buttonClicked == "trade") {
                GamePlayer* pTarget = GetGameServer()->GetPlayerByNetID(netID);
                if(pTarget && pTarget != pPlayer && pTarget->GetCurrentWorld() == pPlayer->GetCurrentWorld()) {
                    pPlayer->StartTrade(pTarget);
                }
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