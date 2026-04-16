#include "ItemActivateRequest.h"
#include "Item/ItemInfoManager.h"
#include "Utils/DialogBuilder.h"

bool ConvertItem(uint16 convertThis, uint16 toThis) 
{
    ItemInfo* pConvItem = GetItemInfoManager()->GetItemByID(convertThis);
    if(!pConvItem) {
        return false;
    }

    ItemInfo* pTargetItem = GetItemInfoManager()->GetItemByID(toThis);
    if(!pTargetItem) {
        return false;
    }

    return true;
}

void ItemActivateRequest::Execute(GamePlayer* pPlayer, World* pWorld, GameUpdatePacket* pPacket)
{
    if(!pPlayer || !pWorld || !pPacket) {
        return;
    }

    if(!pPlayer->CanActivateItemNow()) {
        return;
    }

    pPlayer->ResetItemActiveTime();

    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(pPacket->itemID);
    if(!pItem) {
        LOGGER_LOG_WARN("Player %s (ID: %d) tried to activate item %d", pPlayer->GetRawName(), pPlayer->GetUserID(), pPacket->itemID);
        return;
    }

    if(pItem->HasFlag(ITEM_FLAG_MOD) && !pPlayer->GetRole()->HasPerm(ROLE_PERM_USE_ITEM_TYPE_MOD)) {
        LOGGER_LOG_WARN("Player %s (ID: %d) tried to use mod flagged item %d", pPlayer->GetRawName(), pPlayer->GetUserID(), pPacket->itemID);
        return;
    }

    if(pItem->type == ITEM_TYPE_CLOTHES) {
        pPlayer->ToggleCloth(pPacket->itemID);
        return;
    }

    if(pItem->id == ITEM_ID_TELEPHONE) {
        DialogBuilder db;
        db.SetDefaultColor('o')
            ->AddLabelWithIcon("`wTelephone``", ITEM_ID_TELEPHONE, true, true)
            ->AddLabel("Dial a number to call somebody in Growtopia. Phone numbers have 5 digits, like 12345 (try it - you'd be crazy not to!). Most numbers are not in service!")
            ->AddTextInput("Number", "Phone #", "", 5)
            ->EndDialog("TelephoneEdit", "Dial", "Hang Up");
        pPlayer->SendOnDialogRequest(db.Get());
        return;
    }
}