#include "DialogReturn.h"
#include "IO/Log.h"

#include "../../../Player/Dialog/SignDialog.h"
#include "../../../Player/Dialog/TrashDialog.h"

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
        case CompileTimeHashString("sign_edit"): {
            auto pTileX = packet.Find(CompileTimeHashString("tilex"));
            auto pTileY = packet.Find(CompileTimeHashString("tiley"));
            auto pSignText = packet.Find(CompileTimeHashString("sign_text"));

            if(!pTileX || !pTileY || !pSignText) {
                return;
            }

            string tileX(pTileX->value, pTileX->size); // we need toint without null term
            string tileY(pTileY->value, pTileY->size);

            SignDialog::Handle(pPlayer, string(pSignText->value, pSignText->size), ToInt(tileX), ToInt(tileY));
            break;
        }

        case CompileTimeHashString("trash_item"):
        case CompileTimeHashString("trash_item2"): {
            auto pItemID = packet.Find(CompileTimeHashString("itemID"));
            auto pCount = packet.Find(CompileTimeHashString("count"));

            if(!pItemID || !pCount) {
                return;
            }

            string itemID(pItemID->value, pItemID->size); // we need toint without null term
            string count(pCount->value, pCount->size);

            if(hashedDialogName == CompileTimeHashString("trash_item2")) {
                TrashDialog::HandleUntradeable(pPlayer, ToUInt(itemID), ToInt(count));
            }
            else {
                TrashDialog::Handle(pPlayer, ToUInt(itemID), ToInt(count));
            }
            break;
        }
    }
}