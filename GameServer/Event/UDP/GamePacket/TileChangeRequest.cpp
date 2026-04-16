#include "TileChangeRequest.h"
#include "Item/ItemInfoManager.h"
#include "Algorithm/ChemsynthAlgorithm.h"
#include "Prize/PrizeManager.h"
#include "../../../Player/Dialog/PlayerDialog.h"
#include "../../../Server/GameServer.h"
#include "../../../Server/MasterBroadway.h"

namespace {

uint8 RollRarityGemBonus(int16 rarity)
{
    int32 safeRarity = std::max<int32>(0, rarity);
    if(safeRarity <= 7) {
        return 0;
    }

    int32 gemsToGive = ((safeRarity / 4) * 3) / 4;
    if(safeRarity > 30) {
        gemsToGive = safeRarity / 4;
    }

    if(gemsToGive <= 0) {
        return 0;
    }

    return (uint8)(rand() % (gemsToGive + 1));
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

const ItemInfo* FindSplicedSeedResult(uint16 firstSeedID, uint16 secondSeedID)
{
    const auto& items = GetItemInfoManager()->GetItems();
    for(const ItemInfo& candidate : items) {
        if(candidate.type != ITEM_TYPE_SEED) {
            continue;
        }

        if((candidate.seed1 == firstSeedID && candidate.seed2 == secondSeedID) ||
            (candidate.seed1 == secondSeedID && candidate.seed2 == firstSeedID))
        {
            return &candidate;
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

string BuildDailyQuestStatKey(const char* prefix, uint32 epochDay, uint16 itemID)
{
    return string(prefix) + "_" + ToString(epochDay) + "_" + ToString(itemID);
}

void TrackDailyQuestBreakProgress(GamePlayer* pPlayer, uint16 itemID, uint64 amount)
{
    if(!pPlayer || amount == 0) {
        return;
    }

    const TCPDailyQuestData& dq = GetMasterBroadway()->GetDailyQuestData();
    if(itemID != dq.questItemOneID && itemID != dq.questItemTwoID) {
        return;
    }

    const uint32 epochDay = (uint32)(Time::GetTimeSinceEpoch() / 86400ull);
    pPlayer->IncreaseStat(BuildDailyQuestStatKey("DQ_BREAK", epochDay, itemID), amount);
}

void SpawnPrizeDrop(World* pWorld, TileInfo* pTile, const PrizeDrop& prize)
{
    if(!pWorld || !pTile || prize.itemID == ITEM_ID_BLANK || prize.amount == 0) {
        return;
    }

    WorldObject obj;
    obj.itemID = prize.itemID;
    obj.count = prize.amount;
    pWorld->DropObject(pTile, obj, true);
}

bool TryHarvestProvider(GamePlayer* pPlayer, World* pWorld, TileInfo* pTile, ItemInfo* pTileItem)
{
    if(!pPlayer || !pWorld || !pTile || !pTileItem || pTileItem->type != ITEM_TYPE_PROVIDER) {
        return false;
    }

    TileExtra_Provider* pProvider = pTile->GetExtra<TileExtra_Provider>();
    if(!pProvider) {
        return false;
    }

    const uint64 nowMS = Time::GetSystemTime();
    if(pProvider->readyTime != 0 && nowMS < pProvider->readyTime) {
        return true;
    }

    uint32 cooldownSec = pTileItem->growTime;
    if(cooldownSec == 0) {
        cooldownSec = 1800;
    }
    pProvider->readyTime = (uint32)(nowMS + (uint64)cooldownSec * 1000ull);

    switch(pTileItem->id) {
        case ITEM_ID_ATM_MACHINE: {
            int32 gemsToDrop = (rand() % 70) == 0 ? (1 + (rand() % 150)) : (1 + (rand() % 50));
            DropGems(pWorld, pTile, (uint8)std::min<int32>(255, gemsToDrop));
            break;
        }

        case ITEM_ID_AWKWARD_FRIENDLY_UNICORN: {
            SpawnPrizeDrop(pWorld, pTile, GetPrizeManager()->GetAwkwardUnicornDrop());
            break;
        }

        case ITEM_ID_COW:
        case ITEM_ID_BUFFALO: {
            WorldObject obj;
            obj.itemID = ITEM_ID_MILK;
            obj.count = (uint8)(1 + (rand() % 2));
            pWorld->DropObject(pTile, obj, true);
            pPlayer->IncreaseStat("COW_HARVESTED");
            break;
        }

        default:
            return false;
    }

    const Vector2Int pos = pTile->GetPos();
    pWorld->SendParticleEffectToAll(pos.x * 32.0f + 12.0f, pos.y * 32.0f + 15.0f, 46);
    pWorld->SendTileUpdate(pTile);
    return true;
}

void SpawnFarmableDrops(World* pWorld, TileInfo* pTile, ItemInfo* pTileItem)
{
    if(!pWorld || !pTile || !pTileItem) {
        return;
    }

    int32 blockDrop = 0;
    int32 seedDrop = 0;
    int32 gemDrop = 0;
    bool luckyHit = false;

    if(pTileItem->rarity == 999) {
        return;
    }

    if((rand() % 2) == 0 || luckyHit) {
        blockDrop = pTileItem->HasFlag(ITEM_FLAG_SEEDLESS) ? 0 : 1;

        if(!luckyHit) {
            if((rand() % 7) == 0) {
                return;
            }

            if(!pTileItem->HasFlag(ITEM_FLAG_PERMANENT)) {
                seedDrop = 1;
            }

            blockDrop = 0;
        }
    }

    if(!pTileItem->HasFlag(ITEM_FLAG_SEEDLESS) && !pTileItem->HasFlag(ITEM_FLAG_DROPLESS)) {
        if((rand() % 20) == 0) {
            gemDrop = 1;
        }

        gemDrop += RollRarityGemBonus(pTileItem->rarity);

        if(luckyHit) {
            if((rand() % 100) >= 90) {
                gemDrop *= 5;
            }

            luckyHit = false;
        }
    }

    if(seedDrop > 0) {
        WorldObject seedObj;
        seedObj.itemID = pTileItem->id + 1;
        seedObj.count = (uint8)seedDrop;
        pWorld->DropObject(pTile, seedObj, true);
    }

    if(blockDrop > 0) {
        WorldObject blockObj;
        blockObj.itemID = pTileItem->id;
        blockObj.count = (uint8)blockDrop;
        pWorld->DropObject(pTile, blockObj, true);
    }

    if(gemDrop > 0) {
        DropGems(pWorld, pTile, (uint8)std::min<int32>(255, gemDrop));
    }
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

bool ToggleWorldFlagMachine(World* pWorld, TileInfo* pTile, uint16 itemID)
{
    if(!pWorld || !pTile) {
        return false;
    }

    uint32 worldFlag = 0;
    switch(itemID) {
        case ITEM_ID_SIGNAL_JAMMER: worldFlag = WORLD_FLAG_JAMMED; break;
        case ITEM_ID_PUNCH_JAMMER: worldFlag = WORLD_FLAG_PUNCH_JAMMER; break;
        case ITEM_ID_ZOMBIE_JAMMER: worldFlag = WORLD_FLAG_ZOMBIE_JAMMER; break;
        case ITEM_ID_ANTIGRAVITY_GENERATOR: worldFlag = WORLD_FLAG_ANTI_GRAVITY; break;
        case ITEM_ID_GUARDIAN_PINEAPPLE: worldFlag = WORLD_FLAG_GUARDIAN_PINEAPPLE; break;
        default: return false;
    }

    pTile->ToggleFlag(TILE_FLAG_IS_ON);
    pWorld->SetWorldFlag(worldFlag, pTile->HasFlag(TILE_FLAG_IS_ON));
    return true;
}

void ClearWorldFlagMachine(World* pWorld, uint16 itemID)
{
    if(!pWorld) {
        return;
    }

    switch(itemID) {
        case ITEM_ID_SIGNAL_JAMMER: pWorld->SetWorldFlag(WORLD_FLAG_JAMMED, false); break;
        case ITEM_ID_PUNCH_JAMMER: pWorld->SetWorldFlag(WORLD_FLAG_PUNCH_JAMMER, false); break;
        case ITEM_ID_ZOMBIE_JAMMER: pWorld->SetWorldFlag(WORLD_FLAG_ZOMBIE_JAMMER, false); break;
        case ITEM_ID_ANTIGRAVITY_GENERATOR: pWorld->SetWorldFlag(WORLD_FLAG_ANTI_GRAVITY, false); break;
        case ITEM_ID_GUARDIAN_PINEAPPLE: pWorld->SetWorldFlag(WORLD_FLAG_GUARDIAN_PINEAPPLE, false); break;
        default: break;
    }
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

    const bool isChemsynthTool =
        pItem->id == ITEM_ID_CHEMSYNTH_SOLVENT ||
        pItem->id == ITEM_ID_CHEMSYNTH_CATALYST ||
        pItem->id == ITEM_ID_CHEMSYNTH_REPLICATOR ||
        pItem->id == ITEM_ID_CHEMSYNTH_CENTRIFUGE ||
        pItem->id == ITEM_ID_CHEMSYNTH_STIRRER;

    if(pItem->type == ITEM_TYPE_CLOTHES) {

        return;
    }

    TileInfo* pTile = pWorld->GetTileManager()->GetTile(pPacket->tileX, pPacket->tileY);
    if(!pTile) {
        LOGGER_LOG_DEBUG("Unable to get tile %d %d for tile change request", pPacket->tileX, pPacket->tileY);
        return;
    }

    if(pItem->type == ITEM_TYPE_CONSUMABLE && !isChemsynthTool) {
        pPlayer->IncreaseStat("CONSUMABLES_USED");
        return;
    }

    if(pPacket->itemID != ITEM_ID_FIST && pPlayer->GetInventory().GetCountOfItem(pItem->id) == 0) {
        pPlayer->SendFakePingReply();
        return;
    }

    if(isChemsynthTool) {
        if(!pWorld->CanBreak(pPlayer, pTile)) {
            pPlayer->SendFakePingReply();
            pPlayer->PlaySFX("punch_locked.wav");
            pPlayer->SendOnTalkBubble("`wThat area is owned by " + GetOwnerDisplayName(pWorld, pTile), true);
            return;
        }

        ChemsynthAlgorithm::UseTool(pPlayer, pWorld, pTile, pItem->id);
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
            const ItemInfo* pSeedOnTile = GetItemInfoManager()->GetItemByID(pTile->GetFG());
            if(!pSeedOnTile || pSeedOnTile->type != ITEM_TYPE_SEED) {
                pPlayer->SendFakePingReply();
                return;
            }

            const ItemInfo* pSplicedSeed = FindSplicedSeedResult(pItem->id, pSeedOnTile->id);
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
            if(pTileItem && pTileItem->id == ITEM_ID_STEAM_CRANK) {
                pTile->ToggleFlag(TILE_FLAG_IS_ON);
                pWorld->SendTileUpdate(tilePos.x, tilePos.y);
                return;
            }

            if(pTileItem && ToggleWorldFlagMachine(pWorld, pTile, pTileItem->id)) {
                pWorld->SendTileUpdate(tilePos.x, tilePos.y);
                return;
            }

            if(TryHarvestProvider(pPlayer, pWorld, pTile, pTileItem)) {
                return;
            }

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

            ClearWorldFlagMachine(pWorld, pTileItem->id);
            
            if(pTileItem->type == ITEM_TYPE_WEATHER_MACHINE) {
                pWorld->SetCurrentWeather(pTileItem->weatherID);
                pTile->RemoveFlag(TILE_FLAG_IS_ON);
                pWorld->SendCurrentWeatherToAll();
            }

            if(!pTileItem->HasFlag(ITEM_FLAG_DROPLESS)) {
                if(pTileItem->id == ITEM_ID_GOLDEN_BOOTY_CHEST) {
                    SpawnPrizeDrop(pWorld, pTile, GetPrizeManager()->GetGBCDrop(false));
                }
                else if(pTileItem->id == ITEM_ID_SUPER_GOLDEN_BOOTY_CHEST) {
                    SpawnPrizeDrop(pWorld, pTile, GetPrizeManager()->GetSGBCDrop(false));
                }

                if(pTileItem->type == ITEM_TYPE_SEED) {
                    uint8 seedCount = (uint8)(1 + (rand() % 2));
                    if(pTile->HasFlag(TILE_FLAG_WILL_SPAWN_SEEDS_TOO)) {
                        seedCount = std::max<uint8>(seedCount, 2);
                    }

                    WorldObject seedDrop;
                    seedDrop.itemID = pTileItem->id;
                    seedDrop.count = seedCount;
                    pWorld->DropObject(pTile, seedDrop, true);

                    const uint8 treeGemDrop = RollRarityGemBonus(pTileItem->rarity);
                    if(treeGemDrop > 0) {
                        DropGems(pWorld, pTile, treeGemDrop);
                    }
                }
                else if(!pTileItem->HasFlag(ITEM_FLAG_SEEDLESS)) {
                    SpawnFarmableDrops(pWorld, pTile, pTileItem);
                }
            }
    
            pWorld->SendTileApplyDamage(tilePos.x, tilePos.y, (int32)pPlayer->GetCharData().GetPunchDamage(), pPlayer->GetNetID());

            if(pTileItem->type != ITEM_TYPE_LOCK) {
                if(pTileItem->type == ITEM_TYPE_SEED) {
                    pPlayer->AddXP(1);
                }
                else {
                    const uint32 xp = 1 + (uint32)(std::max<int32>(0, pTileItem->rarity) / 15);
                    pPlayer->AddXP(xp);
                }

                TrackDailyQuestBreakProgress(pPlayer, pTileItem->id, 1);
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