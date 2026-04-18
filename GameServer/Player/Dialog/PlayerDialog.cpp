#include "PlayerDialog.h"
#include "../GamePlayer.h"
#include "Item/ItemInfoManager.h"
#include "World/TileInfo.h"
#include "Utils/DialogBuilder.h"
#include "../../Server/MasterBroadway.h"
#include "../../World/WorldManager.h"

#include "SignDialog.h"
#include "PathMarkerDialog.h"
#include "LockDialog.h"
#include "DoorDialog.h"
#include "MachineDialog.h"
#include "ChemsynthDialog.h"
#include "../../Component/FossilComponent.h"
#include "../../Component/GeigerComponent.h"

namespace {

constexpr uint16 kTelephoneItemID = 3898;

void ShowTelephoneDialog(GamePlayer* pPlayer, TileInfo* pTile)
{
    if(!pPlayer || !pTile) {
        return;
    }

    DialogBuilder db;
    db.SetDefaultColor('o')
        ->AddLabelWithIcon("`wTelephone``", kTelephoneItemID, true, true)
        ->AddLabel("Dial a number to call somebody in Growtopia. Phone numbers have 5 digits, like 12345 (try it - you'd be crazy not to!). Most numbers are not in service!")
        ->AddTextInput("Number", "Phone #", "", 5)
        ->EmbedData("tilex", pTile->GetPos().x)
        ->EmbedData("tiley", pTile->GetPos().y)
        ->EmbedData("TileItemID", pTile->GetDisplayedItem())
        ->EndDialog("TelephoneEdit", "Dial", "Hang Up");

    pPlayer->SendOnDialogRequest(db.Get());
}

void ShowDonationBoxDialog(GamePlayer* pPlayer, TileInfo* pTile)
{
    if(!pPlayer || !pTile) {
        return;
    }

    World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
    if(!pWorld) {
        return;
    }

    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(pTile->GetDisplayedItem());
    if(!pItem) {
        return;
    }

    TileExtra_DonationBox* pDonation = pTile->GetExtra<TileExtra_DonationBox>();
    if(!pDonation) {
        return;
    }

    const bool hasAccess = pWorld->PlayerHasAccessOnTile(pPlayer, pTile);
    const size_t donatedCount = pDonation->donatedItems.size();

    DialogBuilder db;
    db.SetDefaultColor('o')
        ->AddLabelWithIcon("`w" + string(pItem->name) + "``", pItem->id, true)
        ->EmbedData("tilex", pTile->GetPos().x)
        ->EmbedData("tiley", pTile->GetPos().y)
        ->EmbedData("TileItemID", pTile->GetDisplayedItem());

    if(hasAccess) {
        if(donatedCount < 1) {
            db.AddLabel("The box is currently empty.");
        }
        else {
            db.AddLabel("You have " + ToString(donatedCount) + " gifts waiting:");
            db.AddSpacer();

            for(const TileExtra_DonatedItem& donated : pDonation->donatedItems) {
                ItemInfo* pDonatedItem = GetItemInfoManager()->GetItemByID(donated.itemID);
                const string itemName = pDonatedItem ? string(pDonatedItem->name) : ("Item #" + ToString(donated.itemID));
                const string checkboxKey = "_dn" + ToString(donated.donateID);

                if(donated.comment.empty()) {
                    db.AddCheckBox(checkboxKey, itemName + " (`w" + ToString((uint32)donated.amount) + "``) from `w" + donated.username + "``", false);
                }
                else {
                    db.AddCheckBox(checkboxKey, itemName + " (`w" + ToString((uint32)donated.amount) + "``) from `w" + donated.username + "```#- \"" + donated.comment + "\"``", false);
                }
            }

            db.AddSpacer()
                ->AddButton("ClearSelected", "`4Retrieve Selected``")
                ->AddButton("ClearAll", "`4Retrieve All``")
                ->AddSpacer();
        }

        if(donatedCount >= 20) {
            db.AddLabel("This box already has `w20`` gifts in it, can't add more until you clear them.");
        }
        else {
            db.AddItemPicker("SelectedItem", "`wGive Gift`` (Min rarity: `52``)", "Choose an item to donate");
        }
    }
    else {
        if(donatedCount < 1) {
            db.AddLabel("The box is currently empty.");
        }
        else {
            db.AddLabel("You see `w" + ToString(donatedCount) + "`` gifts in the box.");
        }

        db.AddLabel("Would you like to leave a `9gift`` for the owner?");

        if(donatedCount >= 20) {
            db.AddLabel("This box already has `w20`` gifts in it, can't add more until you clear them.");
        }
        else {
            db.AddSpacer()
                ->AddItemPicker("SelectedItem", "`wGive Gift`` (Min rarity: `52``)", "Choose an item to donate");
        }
    }

    db.EndDialog("DonationEdit", "", "Cancel");
    pPlayer->SendOnDialogRequest(db.Get());
}

}

void PlayerDialog::Handle(GamePlayer* pPlayer, TileInfo* pTile)
{
    if(!pTile) {
        return;
    }

    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(pTile->GetDisplayedItem());
    if(!pItem) {
        return;
    }

    if(pItem->type == ITEM_TYPE_SIGN) {
        if(
            pItem->id == ITEM_ID_OBJECTIVE_MARKER ||
            pItem->id == ITEM_ID_PATH_MARKER ||
            pItem->id == ITEM_ID_CARNIVAL_LANDING
        ) {
            PathMarkerDialog::Request(pPlayer, pTile);
        }
        else {
            SignDialog::Request(pPlayer, pTile);
        }
        return;
    }

    if(pItem->id == kTelephoneItemID) {
        ShowTelephoneDialog(pPlayer, pTile);
        return;
    }

    if(pItem->type == ITEM_TYPE_LOCK) {
        LockDialog::Request(pPlayer, pTile);
        return;
    }

    if(pItem->type == ITEM_TYPE_DOOR || pItem->type == ITEM_TYPE_USER_DOOR || pItem->type == ITEM_TYPE_PORTAL || pItem->type == ITEM_TYPE_SUNGATE || pItem->type == ITEM_TYPE_FRIENDS_ENTRANCE) {
        DoorDialog::Request(pPlayer, pTile);
        return;
    }

    if(pItem->type == ITEM_TYPE_VENDING || pItem->id == ITEM_ID_MAGPLANT_5000 || pItem->id == ITEM_ID_UNSTABLE_TESSERACT || pItem->id == ITEM_ID_GAIAS_BEACON) {
        MachineDialog::Request(pPlayer, pTile);
        return;
    }

    if(pItem->type == ITEM_TYPE_DONATION_BOX) {
        ShowDonationBoxDialog(pPlayer, pTile);
        return;
    }

    if(
        pItem->type == ITEM_TYPE_CHEMSYNTH || pItem->type == ITEM_TYPE_CHEMTANK ||
        pItem->id == ITEM_ID_CHEMSYNTH_PROCESSOR || pItem->id == ITEM_ID_CHEMSYNTH_TANK
    ) {
        ChemsynthDialog::Request(pPlayer, pTile);
        return;
    }

    if(pItem->type == ITEM_TYPE_FOSSIL_PREP || pItem->id == ITEM_ID_FOSSIL_PREP_STATION) {
        FossilComponent::RequestPrepDialog(pPlayer, pTile);
        return;
    }

    if(pItem->type == ITEM_TYPE_GEIGERCHARGE || pItem->id == ITEM_ID_GEIGER_CHARGER) {
        GeigerComponent::RequestChargerDialog(pPlayer, pTile);
        return;
    }
}
