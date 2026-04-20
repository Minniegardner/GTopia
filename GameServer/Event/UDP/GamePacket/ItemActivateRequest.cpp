#include "ItemActivateRequest.h"
#include "Item/ItemInfoManager.h"
#include "Utils/DialogBuilder.h"

namespace {

static constexpr const char* kBirthCertCooldownStat = "BirthCertLastUseMS";

}

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

    if(pItem->id == ITEM_ID_BIRTH_CERTIFICATE) {
        if(!pPlayer->HasGrowID()) {
            pPlayer->SendOnTalkBubble("`4You need a GrowID first!", true);
            return;
        }

        const uint64 nowMS = Time::GetSystemTime();
        const uint64 lastUseMS = pPlayer->GetCustomStatValue(kBirthCertCooldownStat);
        const uint64 cooldownMS = 60ull * 24ull * 60ull * 60ull * 1000ull;
        if(lastUseMS > 0 && nowMS < lastUseMS + cooldownMS) {
            const uint64 remainingDays = ((lastUseMS + cooldownMS) - nowMS + 86399999ull) / 86400000ull;
            pPlayer->SendOnTalkBubble("`4You can use another Birth Certificate in " + ToString(remainingDays) + " days.", true);
            return;
        }

        DialogBuilder db;
        db.SetDefaultColor('o')
            ->AddLabelWithIcon("`wChange your GrowID``", ITEM_ID_BIRTH_CERTIFICATE, true)
            ->AddSpacer()
            ->AddLabel("This will change your GrowID `4permanently``. You can't change it again for `460 days``.")
            ->AddLabel("Your `wBirth Certificate`` will be consumed if you press `5Change It``.")
            ->AddLabel("Choose an appropriate name or `6we will change it for you!``")
            ->AddTextInput("NewName", "Enter your new name:", "", 32)
            ->EndDialog("BirthCertificate", "Change it!", "Cancel");

        pPlayer->SendOnDialogRequest(db.Get());
        return;
    }
}