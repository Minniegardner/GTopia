#include "TileChangeRequest.h"
#include "Item/ItemInfoManager.h"
#include "../../../Player/Dialog/PlayerDialog.h"
#include "../../../Server/GameServer.h"

namespace {

uint8 GetRarityToGem(int16 rarity)
{
    if(rarity >= 87) return 22;
    if(rarity >= 68) return 18;
    if(rarity >= 53) return 14;
    if(rarity >= 41) return 11;
    if(rarity >= 36) return 10;
    if(rarity >= 32) return 9;
    if(rarity >= 24) return 5;
    return 1;
}

string GetTreeNameFromSeedName(const string& seedName)
{
    static const string suffix = " Seed";
    if(seedName.length() > suffix.length()) {
        size_t start = seedName.length() - suffix.length();
        if(seedName.compare(start, suffix.length(), suffix) == 0) {
            return seedName.substr(0, start);
        }
    }
    return seedName;
}

ItemInfo* FindSplicedSeedResult(uint16 firstSeedID, uint16 secondSeedID)
{
    const auto& items = GetItemInfoManager()->GetItems();
    for(ItemInfo* pCandidate : items) {
        if(!pCandidate || pCandidate->type != ITEM_TYPE_SEED) {
            continue;
        }

        if((pCandidate->seed1 == firstSeedID && pCandidate->seed2 == secondSeedID) ||
            (pCandidate->seed1 == secondSeedID && pCandidate->seed2 == firstSeedID))
        {
            return pCandidate;
        }
    }

    return nullptr;
}

void DropGems(World* pWorld, TileInfo* pTile, uint8 amount)
{
    if(!pWorld || !pTile || amount == 0) {
        return;
    }

    WorldObject gemDrop;
    gemDrop.itemID = ITEM_ID_GEMS;
    gemDrop.count = amount;
    pWorld->DropObject(pTile, gemDrop, true);
}

string GetOwnerDisplayName(World* pWorld, TileInfo* pTile)
{
    if(!pWorld || !pTile) {
        return "DeletedUser";
    }

    TileInfo* pLockTile = nullptr;

    if(pTile->GetParent() != 0) {
        pLockTile = pWorld->GetTileManager()->GetTile(pTile->GetParent());
    }
    else {
        pLockTile = pWorld->GetTileManager()->GetKeyTile(KEY_TILE_WORLD_LOCK);
    }

    if(!pLockTile) {
        return "DeletedUser";
    }

    TileExtra_Lock* pLockExtra = pLockTile->GetExtra<TileExtra_Lock>();
    if(!pLockExtra) {
        return "DeletedUser";
    }

    GamePlayer* pOwner = GetGameServer()->GetPlayerByUserID((uint32)pLockExtra->ownerID);
    if(!pOwner) {
        return "DeletedUser";
    }

    return pOwner->GetDisplayName();
}

}

void TileChangeRequest::OnPunchedLock(GamePlayer* pPlayer, TileInfo* pTile)
{

}

void TileChangeRequest::HandleConsumable(GamePlayer *pPlayer, World *pWorld, GameUpdatePacket *pPacket)
{

}

void TileChangeRequest::Execute(GamePlayer* pPlayer, World* pWorld, GameUpdatePacket* pPacket)
{
    if(!pPlayer || !pWorld || !pPacket) {
        return;
    }

    /*if(pPacket->itemID != ITEM_ID_FIST) {
        
    }*/

    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(pPacket->itemID);
    if(!pItem) {
        return;
    }

    if(pItem->type == ITEM_TYPE_CLOTHES) {

        return;
    }

    TileInfo* pTile = pWorld->GetTileManager()->GetTile(pPacket->tileX, pPacket->tileY);
    if(!pTile) {
        LOGGER_LOG_DEBUG("Unable to get tile %d %d for tile change request", pPacket->tileX, pPacket->tileY);
        return;
    }

    if(pItem->type == ITEM_TYPE_CONSUMABLE) {
        pPlayer->IncreaseStat("CONSUMABLES_USED");
        return;
    }

    if(pPacket->itemID != ITEM_ID_FIST && pPlayer->GetInventory().GetCountOfItem(pItem->id) == 0) {
        pPlayer->SendFakePingReply();
        return;
    }

    const bool isPunch = pPacket->itemID == ITEM_ID_FIST;
    const bool canInteract = isPunch ? pWorld->CanBreak(pPlayer, pTile) : pWorld->CanPlace(pPlayer, pTile);
    if(!canInteract) {
        pPlayer->SendFakePingReply();
        pPlayer->PlaySFX("punch_locked.wav");
        pPlayer->SendOnTalkBubble("`wThat area is owned by " + GetOwnerDisplayName(pWorld, pTile), true);
        return;
    }

    Vector2Int tilePos = pTile->GetPos();

    if(pItem->type == ITEM_TYPE_WRENCH) {
        PlayerDialog::Handle(pPlayer, pTile);
        return;
    }

    if(
        pTile->GetDisplayedItem() == ITEM_ID_BLANK && pPacket->itemID == ITEM_ID_FIST
        && pTile->HasFlag(TILE_FLAG_ON_FIRE)
    ) {
        pPlayer->SendFakePingReply();
        return;
    }
   

    ItemInfo* pTileItem = GetItemInfoManager()->GetItemByID(pTile->GetDisplayedItem());

    if(pPacket->itemID != ITEM_ID_FIST) {
        bool allowSeedSplice = false;
        if(pItem->type == ITEM_TYPE_SEED && pTile->GetFG() != ITEM_ID_BLANK) {
            ItemInfo* pSeedOnTile = GetItemInfoManager()->GetItemByID(pTile->GetFG());
            allowSeedSplice = pSeedOnTile && pSeedOnTile->type == ITEM_TYPE_SEED;
        }

        if(
            ((!pTileItem->IsBackground() && pTile->GetFG() != ITEM_ID_BLANK) && !allowSeedSplice) ||
            (pTileItem->IsBackground() && pTile->GetBG() != ITEM_ID_BLANK)
        ) {
            return;
        }
    }

    if(pTileItem->HasFlag(ITEM_FLAG_MOD) && !pPlayer->GetRole()->HasPerm(ROLE_PERM_USE_ITEM_TYPE_MOD)) {
        pPlayer->SendFakePingReply();

        if(pPacket->itemID == ITEM_ID_FIST) {
            if(IsMainDoor(pTileItem->id)) {
                pPlayer->SendOnTalkBubble("`w(stand over and punch to use)", true);
            }
            else {
                pPlayer->SendOnTalkBubble("`wIt's too strong to break.", true);
            }
        }
        else if(pItem->IsBackground()) {
            pPlayer->SendOnTalkBubble("`wCan't put anything behind that!", true);
        }

        pPlayer->PlaySFX("cant_break_tile.wav");
        return;
    }

    if(pPacket->itemID != ITEM_ID_FIST) {
        if(pPacket->itemID == ITEM_ID_GUARDIAN_PINEAPPLE && pWorld->GetTileManager()->GetKeyTile(KEY_TILE_GUARD_PINEAPPLE)) {
            pPlayer->SendFakePingReply();
            pPlayer->SendOnTalkBubble("This world already has a Guardian Pineapple somewhere on it, installing two would be dangerous!", true);
            return;
        }

        if(pItem->type == ITEM_TYPE_LOCK) {
            pWorld->OnAddLock(pPlayer, pTile, pItem->id);
            return;
        }

        if(pItem->type == ITEM_TYPE_SEED && pTile->GetFG() != ITEM_ID_BLANK) {
            ItemInfo* pSeedOnTile = GetItemInfoManager()->GetItemByID(pTile->GetFG());
            if(!pSeedOnTile || pSeedOnTile->type != ITEM_TYPE_SEED) {
                pPlayer->SendFakePingReply();
                return;
            }

            ItemInfo* pSplicedSeed = FindSplicedSeedResult(pItem->id, pSeedOnTile->id);
            if(!pSplicedSeed) {
                pPlayer->SendFakePingReply();
                return;
            }

            pPlayer->ModifyInventoryItem(pItem->id, -1);

            pTile->SetFG(pSplicedSeed->id, pWorld->GetTileManager());
            pTile->SetFlag(TILE_FLAG_IS_SEEDLING);
            pTile->SetFlag(TILE_FLAG_WAS_SPLICED);
            if((rand() % 5) == 0) {
                pTile->SetFlag(TILE_FLAG_WILL_SPAWN_SEEDS_TOO);
            }
            else {
                pTile->RemoveFlag(TILE_FLAG_WILL_SPAWN_SEEDS_TOO);
            }

            TileExtra_Seed* pSeedExtra = pTile->GetExtra<TileExtra_Seed>();
            if(pSeedExtra) {
                pSeedExtra->growTime = (uint32)Time::GetSystemTime();
                pSeedExtra->fruitCount = (uint8)(2 + (rand() % 11));
            }

            string treeName = GetTreeNameFromSeedName(pSplicedSeed->name);
            string msg = "`w" + pItem->name + "`` and `w" + pSeedOnTile->name + "`` have been spliced to make a `$" + treeName + " Tree``!";
            pPlayer->SendOnTalkBubble(msg, true);

            pWorld->SendTileUpdate(tilePos.x, tilePos.y);
            return;
        }

        pPlayer->ModifyInventoryItem(pItem->id, -1);
    }

    if(pPacket->itemID == ITEM_ID_FIST) {
        if(pPlayer->GetInventory().GetClothByPart(BODY_PART_HAND) == ITEM_ID_FIRE_HOSE) {
            if(pTile->HasFlag(TILE_FLAG_ON_FIRE)) {
                pTile->RemoveFlag(TILE_FLAG_ON_FIRE);
                pWorld->SendParticleEffectToAll(tilePos.x * 32, tilePos.y * 32, 149);
                pWorld->SendTileUpdate(tilePos.x, tilePos.y);
                return;
            }

            pPlayer->SendFakePingReply();
            return;
        }
        else {
            uint8 punchDamage = (uint8)pPlayer->GetCharData().GetPunchDamage();
            if(pTileItem->type == ITEM_TYPE_SEED) {
                TileExtra_Seed* pSeedExtra = pTile->GetExtra<TileExtra_Seed>();
                if(pSeedExtra && pTileItem->growTime > 0) {
                    uint64 elapsedMS = Time::GetSystemTime() - pSeedExtra->growTime;
                    uint64 elapsedSec = elapsedMS / 1000;
                    if(elapsedSec >= pTileItem->growTime) {
                        WorldObject fruitDrop;
                        fruitDrop.itemID = pTileItem->id > 0 ? (pTileItem->id - 1) : pTileItem->id;
                        fruitDrop.count = pSeedExtra->fruitCount >= 2 ? pSeedExtra->fruitCount : (uint8)(2 + (rand() % 11));
                        pWorld->DropObject(pTile, fruitDrop, true);

                        punchDamage = 255;
                    }
                }
            }

            pTile->PunchTile(punchDamage);

            float tileHealth = pTile->GetHealthPercent();
            if(tileHealth > 0) {
                if(pTileItem->type == ITEM_TYPE_WEATHER_MACHINE && pTileItem->weatherID != 200) {
                    if(pTile->HasFlag(TILE_FLAG_IS_ON)) {
                        if(pWorld->GetCurrentWeather() != pTileItem->weatherID) {
                            pWorld->SetCurrentWeather(pTileItem->weatherID);
                        }
                        else {
                            pWorld->SetCurrentWeather(pWorld->GetDefaultWeather());
                        }
                    }
                    else {
                        if(pWorld->GetCurrentWeather() == pTileItem->weatherID) {
                            pWorld->SetCurrentWeather(pWorld->GetDefaultWeather());
                        }
                        else {
                            pWorld->SetCurrentWeather(pTileItem->weatherID);
                        }
                    }
    
                    pTile->ToggleFlag(TILE_FLAG_IS_ON);
                    pWorld->SendCurrentWeatherToAll();
                }
    
                pWorld->SendTileApplyDamage(tilePos.x, tilePos.y, (int32)pPlayer->GetCharData().GetPunchDamage(), pPlayer->GetNetID());
                return;
            }

            if(pWorld->GetTileManager()->GetKeyTile(KEY_TILE_GUARD_PINEAPPLE) && !pPlayer->GetInventory().HaveRoomForItem(pTileItem->id, 1)) {
                pPlayer->SendFakePingReply();
                pPlayer->SendOnTalkBubble("I better not break that, I have no room to pick it up!", true);
                return;
            }
    
            if(pTileItem->HasFlag(ITEM_FLAG_AUTOPICKUP)) {
                pPlayer->GetInventory().AddItem(pTileItem->id, 1, pPlayer);
            }
    
            if(pTileItem->type == ITEM_TYPE_LOCK) {
                pWorld->OnRemoveLock(pPlayer, pTile);
            }
            
            if(pTileItem->type == ITEM_TYPE_WEATHER_MACHINE) {
                pWorld->SetCurrentWeather(pTileItem->weatherID);
                pTile->RemoveFlag(TILE_FLAG_IS_ON);
                pWorld->SendCurrentWeatherToAll();
            }

            if(!pTileItem->HasFlag(ITEM_FLAG_DROPLESS)) {
                if(pTileItem->type == ITEM_TYPE_SEED) {
                    uint8 seedCount = (uint8)(1 + (rand() % 2));
                    if(pTile->HasFlag(TILE_FLAG_WILL_SPAWN_SEEDS_TOO)) {
                        seedCount = std::max<uint8>(seedCount, 2);
                    }

                    WorldObject seedDrop;
                    seedDrop.itemID = pTileItem->id;
                    seedDrop.count = seedCount;
                    pWorld->DropObject(pTile, seedDrop, true);
                }
                else if(!pTileItem->HasFlag(ITEM_FLAG_SEEDLESS)) {
                    uint8 rarityToGem = GetRarityToGem(pTileItem->rarity);
                    bool farmable = rarityToGem > 1;

                    if((rand() % (farmable ? 2 : 5)) == 0) {
                        DropGems(pWorld, pTile, (uint8)(1 + (rand() % rarityToGem)));
                    }

                    if((rand() % (farmable ? 3 : 5)) == 0) {
                        WorldObject seedDrop;
                        seedDrop.itemID = pTileItem->id + 1;
                        seedDrop.count = 1;
                        pWorld->DropObject(pTile, seedDrop, true);
                    }
                    else if((rand() % (farmable ? 5 : 9)) == 0) {
                        WorldObject blockDrop;
                        blockDrop.itemID = pTileItem->id;
                        blockDrop.count = 1;
                        pWorld->DropObject(pTile, blockDrop, true);
                    }
                }
            }
    
            pWorld->SendTileApplyDamage(tilePos.x, tilePos.y, (int32)pPlayer->GetCharData().GetPunchDamage(), pPlayer->GetNetID());

            if(pTileItem->type != ITEM_TYPE_LOCK) {
                pPlayer->AddXP(1);
            }

            if(pTileItem->type == ITEM_TYPE_SEED) {
                pPlayer->IncreaseStat("TREES_HARVESTED");
            }
            else {
                pPlayer->IncreaseStat("BLOCKS_DESTROYED");
            }
        }
    }

    if(pPacket->itemID != ITEM_ID_FIST) {
        if(pItem->type == ITEM_TYPE_LOCK) {
            pPlayer->IncreaseStat("LOCKS_PLACED");
        }
        else if(pItem->type != ITEM_TYPE_SEED) {
            pPlayer->IncreaseStat("BLOCKS_PLACED");
        }
    }

    pWorld->HandleTilePackets(pPacket);
    pWorld->SendTileUpdate(tilePos.x, tilePos.y);
}