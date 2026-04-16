#include "MenuClickInfo.h"

#include "Item/ItemInfoManager.h"
#include "Utils/DialogBuilder.h"
#include "Utils/StringUtils.h"

void MenuClickInfo::Execute(GamePlayer* pPlayer, ParsedTextPacket<8>& packet)
{
    if(!pPlayer || !pPlayer->HasState(PLAYER_STATE_IN_GAME)) {
        return;
    }

    auto pItem = packet.Find(CompileTimeHashString("itemID"));
    if(!pItem) {
        return;
    }

    uint32 itemID = 0;
    if(ToUInt(string(pItem->value, pItem->size), itemID) != TO_INT_SUCCESS) {
        return;
    }

    ItemInfo* pInfo = GetItemInfoManager()->GetItemByID((uint16)itemID);
    if(!pInfo) {
        return;
    }

    DialogBuilder db;
    db.SetDefaultColor('o');
    db.AddLabelWithIcon("`wAbout " + pInfo->name + "``", pInfo->id, true);
    db.AddSpacer();
    db.AddLabel(pInfo->description);
    db.AddSpacer();
    db.AddLabel("`oRarity: `w" + ToString((int32)pInfo->rarity) + "``");
    db.AddQuickExit();
    db.EndDialog("", "", "OK");

    pPlayer->SendOnDialogRequest(db.Get());
}