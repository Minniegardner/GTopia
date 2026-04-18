#include "BlockActivate.h"

#include "Item/ItemInfoManager.h"
#include "Utils/StringUtils.h"
#include "Utils/DialogBuilder.h"
#include "../../../World/WorldPathfinding.h"
#include "../../../World/WorldManager.h"
#include "World/World.h"
#include "World/TileInfo.h"

namespace {

TileInfo* FindDoorByID(World* pWorld, const string& doorID)
{
    if(!pWorld || doorID.empty()) {
        return nullptr;
    }

    Vector2Int size = pWorld->GetTileManager()->GetSize();
    for(int32 y = 0; y < size.y; ++y) {
        for(int32 x = 0; x < size.x; ++x) {
            TileInfo* pTile = pWorld->GetTileManager()->GetTile(x, y);
            if(!pTile) {
                continue;
            }

            ItemInfo* pItem = GetItemInfoManager()->GetItemByID(pTile->GetDisplayedItem());
            if(pItem) {
                if(pItem->type == ITEM_TYPE_DOOR || pItem->type == ITEM_TYPE_USER_DOOR || pItem->type == ITEM_TYPE_PORTAL || pItem->type == ITEM_TYPE_SUNGATE || pItem->type == ITEM_TYPE_FRIENDS_ENTRANCE) {
                    TileExtra_Door* pDoor = pTile->GetExtra<TileExtra_Door>();
                    if(pDoor && ToUpper(pDoor->id) == doorID) {
                        return pTile;
                    }
                }
                else if(pItem->type == ITEM_TYPE_SIGN && (pItem->id == ITEM_ID_PATH_MARKER || pItem->id == ITEM_ID_OBJECTIVE_MARKER || pItem->id == ITEM_ID_CARNIVAL_LANDING)) {
                    TileExtra_Sign* pSign = pTile->GetExtra<TileExtra_Sign>();
                    if(pSign && ToUpper(pSign->text) == doorID) {
                        return pTile;
                    }
                }
            }
        }
    }

    return nullptr;
}

}

void BlockActivate::Execute(GamePlayer* pPlayer, World* pWorld, GameUpdatePacket* pPacket)
{
    if(!pPlayer || !pWorld || !pPacket) {
        return;
    }

    TileInfo* pTile = pWorld->GetTileManager()->GetTile(pPacket->tileX, pPacket->tileY);
    if(!pTile) {
        return;
    }

    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(pTile->GetDisplayedItem());
    if(!pItem) {
        return;
    }

    const Vector2Float currentPos = pPlayer->GetWorldPos();
    const Vector2Float targetPos = {
        static_cast<float>((pTile->GetPos().x * 32) + 2),
        static_cast<float>(pTile->GetPos().y * 32)
    };

    if(!WorldPathfinding::HasPath(pWorld, pPlayer, currentPos, targetPos)) {
        pPlayer->SendOnTalkBubble("Something is blocking the way, get closer.", true);
        pPlayer->SendFakePingReply();
        return;
    }

    switch(pItem->type) {
        case ITEM_TYPE_PORTAL:
        case ITEM_TYPE_DOOR:
        case ITEM_TYPE_USER_DOOR:
        case ITEM_TYPE_SUNGATE:
        case ITEM_TYPE_FRIENDS_ENTRANCE:
        {
            TileExtra_Door* pDoor = pTile->GetExtra<TileExtra_Door>();
            if(!pDoor) {
                return;
            }

            if(!pWorld->CanBreak(pPlayer, pTile) && !pTile->HasFlag(TILE_FLAG_IS_OPEN_TO_PUBLIC)) {
                pPlayer->PlaySFX("punch_locked.wav");
                pPlayer->SendOnTalkBubble("``The door is `4locked``!", true);
                return;
            }

            string doorDest = pDoor->text;
            RemoveAllSpaces(doorDest);
            doorDest = ToUpper(doorDest);

            auto splitPos = doorDest.find(':');
            if(splitPos == string::npos) {
                if(doorDest.empty()) {
                    TileInfo* pMainDoorTile = pWorld->GetTileManager()->GetKeyTile(KEY_TILE_MAIN_DOOR);
                    if(pMainDoorTile) {
                        pPlayer->SetWorldPos((float)pMainDoorTile->GetPos().x * 32.0f, (float)pMainDoorTile->GetPos().y * 32.0f);
                        pPlayer->SetRespawnPos((float)pMainDoorTile->GetPos().x * 32.0f, (float)pMainDoorTile->GetPos().y * 32.0f);
                        pPlayer->PlaySFX("door_open.wav", 200);
                        pPlayer->SendOnSetPos((float)pMainDoorTile->GetPos().x * 32.0f, (float)pMainDoorTile->GetPos().y * 32.0f);
                    }
                    return;
                }

                pPlayer->SetPendingDoorWarpID("");
                GetWorldManager()->PlayerJoinRequest(pPlayer, doorDest);
                return;
            }

            string worldName = doorDest.substr(0, splitPos);
            string doorID = doorDest.substr(splitPos + 1);
            if(!worldName.empty()) {
                pPlayer->SetPendingDoorWarpID(ToUpper(doorID));
                GetWorldManager()->PlayerJoinRequest(pPlayer, worldName);
                return;
            }

            if(!doorID.empty()) {
                TileInfo* pTargetDoor = FindDoorByID(pWorld, ToUpper(doorID));
                if(pTargetDoor) {
                    pPlayer->SetWorldPos((float)pTargetDoor->GetPos().x * 32.0f, (float)pTargetDoor->GetPos().y * 32.0f);
                    pPlayer->SetRespawnPos((float)pTargetDoor->GetPos().x * 32.0f, (float)pTargetDoor->GetPos().y * 32.0f);
                    pPlayer->PlaySFX("door_open.wav", 200);
                    pPlayer->SendOnSetPos((float)pTargetDoor->GetPos().x * 32.0f, (float)pTargetDoor->GetPos().y * 32.0f);
                    return;
                }
            }

            if(worldName.empty()) {
                TileInfo* pMainDoorTile = pWorld->GetTileManager()->GetKeyTile(KEY_TILE_MAIN_DOOR);
                if(pMainDoorTile) {
                    pPlayer->SetWorldPos((float)pMainDoorTile->GetPos().x * 32.0f, (float)pMainDoorTile->GetPos().y * 32.0f);
                    pPlayer->SetRespawnPos((float)pMainDoorTile->GetPos().x * 32.0f, (float)pMainDoorTile->GetPos().y * 32.0f);
                    pPlayer->PlaySFX("door_open.wav", 200);
                    pPlayer->SendOnSetPos((float)pMainDoorTile->GetPos().x * 32.0f, (float)pMainDoorTile->GetPos().y * 32.0f);
                    return;
                }
            }

            pPlayer->PlaySFX("door_open.wav", 200);
            pPlayer->SendOnSetPos((float)pTile->GetPos().x * 32.0f, (float)pTile->GetPos().y * 32.0f);
            return;
        }
        case ITEM_TYPE_CHECKPOINT:
        {
            pPlayer->SetRespawnPos(static_cast<float>((pTile->GetPos().x * 32) + 2), static_cast<float>(pTile->GetPos().y * 32));
            pPlayer->SendOnSetPos((float)pTile->GetPos().x * 32.0f, (float)pTile->GetPos().y * 32.0f);
            return;
        }
        case ITEM_TYPE_DONATION_BOX:
        {
            TileExtra_DonationBox* pDonation = pTile->GetExtra<TileExtra_DonationBox>();
            if(!pDonation) {
                return;
            }

            const bool hasAccess = pWorld->PlayerHasAccessOnTile(pPlayer, pTile);

            DialogBuilder db;
            db.SetDefaultColor('o')
                ->AddLabelWithIcon("`w" + string(pItem->name) + "``", pItem->id, true);

            if(hasAccess) {
                if(pDonation->donatedItems.empty()) {
                    db.AddLabel("The box is currently empty.");
                }
                else {
                    db.AddLabel("You have " + ToString(pDonation->donatedItems.size()) + " gifts waiting:");
                    for(const TileExtra_DonatedItem& donation : pDonation->donatedItems) {
                        ItemInfo* pDonatedItem = GetItemInfoManager()->GetItemByID(donation.itemID);
                        if(!pDonatedItem) {
                            continue;
                        }

                        string label = string(pDonatedItem->name) + " (`w" + ToString(donation.amount) + "``) from `w" + donation.username + "``";
                        if(!donation.comment.empty()) {
                            label += "`#- \"" + donation.comment + "\"``";
                        }

                        db.AddCheckBox("dn_" + ToString(donation.donateID), label, false);
                    }

                    db.AddButton("ClearAll", "`4Retrieve All``");
                }
            }
            else {
                if(pDonation->donatedItems.empty()) {
                    db.AddLabel("The box is currently empty.")
                        ->AddLabel("Would you like to leave a `9gift`` for the owner?");
                }
                else {
                    db.AddLabel("You see `w" + ToString(pDonation->donatedItems.size()) + "`` gifts in the box.")
                        ->AddLabel("Would you like to leave a `9gift`` for the owner?");
                }
            }

            db.AddItemPicker("SelectedItem", "`wGive Gift`` (Min rarity: `52``)", "Choose an item to donate")
                ->EmbedData("tilex", pTile->GetPos().x)
                ->EmbedData("tiley", pTile->GetPos().y)
                ->EmbedData("TileItemID", pTile->GetDisplayedItem())
                ->EndDialog("DonationEdit", "", "Cancel");

            pPlayer->SendOnDialogRequest(db.Get());
            return;
        }
        case ITEM_TYPE_BEDROCK:
        case ITEM_TYPE_NORMAL:
        default:
            return;
    }
}