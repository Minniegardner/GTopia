#include "TileChangeRequest.h"
#include "Item/ItemInfoManager.h"
#include "Algorithm/ChemsynthAlgorithm.h"
#include "Algorithm/GhostAlgorithm.h"
#include "Prize/PrizeManager.h"
#include "Utils/DialogBuilder.h"
#include "../../../Component/FossilComponent.h"
#include "../../../Component/GeigerComponent.h"
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

bool IsGauntletItem(uint16 itemID)
{
    switch(itemID) {
        case ITEM_ID_GAUNTLET_OF_ELEMENTS_TIER_I:
        case ITEM_ID_GAUNTLET_OF_ELEMENTS_TIER_II:
        case ITEM_ID_GAUNTLET_OF_ELEMENTS_TIER_III:
        case ITEM_ID_GAUNTLET_OF_ELEMENTS_TIER_IV:
        case ITEM_ID_GAUNTLET_OF_ELEMENTS_TIER_V:
        case ITEM_ID_EXQUISITE_GAUNTLET_OF_ELEMENTS:
            return true;
        default:
            return false;
    }
}

std::vector<int32> GetGauntletElements(uint16 itemID)
{
    switch(itemID) {
        case ITEM_ID_GAUNTLET_OF_ELEMENTS_TIER_I: return { 0 };
        case ITEM_ID_GAUNTLET_OF_ELEMENTS_TIER_II: return { 0, 1 };
        case ITEM_ID_GAUNTLET_OF_ELEMENTS_TIER_III: return { 0, 1, 2 };
        case ITEM_ID_GAUNTLET_OF_ELEMENTS_TIER_IV: return { 0, 1, 2 };
        case ITEM_ID_GAUNTLET_OF_ELEMENTS_TIER_V: return { 0, 1, 2, 3 };
        case ITEM_ID_EXQUISITE_GAUNTLET_OF_ELEMENTS: return { 0, 1, 2, 3 };
        default: return {};
    }
}

bool IsGauntletTileSkipped(ItemInfo* pTileItem)
{
    if(!pTileItem) {
        return true;
    }

    return
        pTileItem->id == ITEM_ID_BLANK ||
        pTileItem->type == ITEM_TYPE_SEED ||
        pTileItem->type == ITEM_TYPE_LOCK ||
        pTileItem->id == ITEM_ID_MAIN_DOOR ||
        pTileItem->id == ITEM_ID_BEDROCK ||
        pTileItem->HasFlag(ITEM_FLAG_UNTRADEABLE) ||
        pTileItem->HasFlag(ITEM_FLAG_MOD) ||
        pTileItem->element == ITEM_ELEMENT_NONE;
}

bool TryHandleGauntlet(GamePlayer* pPlayer, World* pWorld, TileInfo* pTile, ItemInfo* pItem)
{
    if(!pPlayer || !pWorld || !pTile || !pItem || !IsGauntletItem(pItem->id)) {
        return false;
    }

    if(!pWorld->CanBreak(pPlayer, pTile)) {
        pPlayer->SendOnTalkBubble("With great power, comes great responsibility! Your power needs to be used on your own tiles.", true);
        return true;
    }

    ItemInfo* pTileItem = GetItemInfoManager()->GetItemByID(pTile->GetDisplayedItem());
    if(IsGauntletTileSkipped(pTileItem)) {
        return true;
    }

    std::vector<int32> elements = GetGauntletElements(pItem->id);
    if(elements.empty()) {
        return true;
    }

    if(std::find(elements.begin(), elements.end(), (int32)pTileItem->element) == elements.end()) {
        if(pTileItem->element < ITEM_ELEMENT_EARTH || pTileItem->element > ITEM_ELEMENT_WATER) {
            pPlayer->SendOnTalkBubble("This element cannot be moved by any gauntlet.", true);
        }
        else {
            static const char* kElementNames[] = { "Earth", "Wind", "Fire", "Water" };
            string gauntletElements;
            for(size_t i = 0; i < elements.size(); ++i) {
                if(i > 0) {
                    gauntletElements += ", ";
                }

                gauntletElements += kElementNames[elements[i]];
            }

            pPlayer->SendOnTalkBubble("This gauntlet can only move " + gauntletElements + " elemental blocks.", true);
        }

        return true;
    }

    const Vector2Int tilePos = pTile->GetPos();
    const int32 selectedIndex = tilePos.x + (tilePos.y * pWorld->GetTileManager()->GetSize().x);

    if(pPlayer->GetGauntletSwapIndex(0) == -1) {
        if(pWorld->GetPlayerOnTile(tilePos)) {
            pPlayer->SendOnTalkBubble("There is a player standing over one of the tiles.", true);
            return true;
        }

        pPlayer->SetGauntletSwapIndex(0, selectedIndex);
        pPlayer->SetGauntletSwapIndex(1, -1);
        pPlayer->SetGauntletAvailableSwap({});

        const bool isModerator = pPlayer->GetRole() && pPlayer->GetRole()->HasPerm(ROLE_PERM_COMMAND_MOD);
        const int32 range = isModerator ? 6 : 2;
        const int32 size = range * 2 + 1;
        const int32 center = range;
        const int32 worldWidth = pWorld->GetTileManager()->GetSize().x;
        const int32 worldHeight = pWorld->GetTileManager()->GetSize().y;

        std::vector<int32> availableSwap;
        availableSwap.reserve((size * size) - 1);

        for(int32 y = 0; y < size; ++y) {
            for(int32 x = 0; x < size; ++x) {
                if(x == center && y == center) {
                    continue;
                }

                const int32 targetX = tilePos.x - center + x;
                const int32 targetY = tilePos.y - center + y;
                if(targetX < 0 || targetY < 0 || targetX >= worldWidth || targetY >= worldHeight) {
                    continue;
                }

                TileInfo* pCandidateTile = pWorld->GetTileManager()->GetTile(targetX, targetY);
                if(!pCandidateTile) {
                    continue;
                }

                ItemInfo* pCandidateItem = GetItemInfoManager()->GetItemByID(pCandidateTile->GetDisplayedItem());
                if(IsGauntletTileSkipped(pCandidateItem)) {
                    continue;
                }

                if(std::find(elements.begin(), elements.end(), (int32)pCandidateItem->element) == elements.end()) {
                    continue;
                }

                if(pWorld->GetPlayerOnTile({ targetX, targetY })) {
                    continue;
                }

                availableSwap.push_back(targetX + (targetY * worldWidth));
            }
        }

        pPlayer->SetGauntletAvailableSwap(availableSwap);
        return true;
    }

    pPlayer->SetGauntletSwapIndex(1, selectedIndex);
    const std::vector<int32>& availableSwap = pPlayer->GetGauntletAvailableSwap();
    if(std::find(availableSwap.begin(), availableSwap.end(), selectedIndex) == availableSwap.end()) {
        pPlayer->SendOnTalkBubble("The selected tile is not valid to swap.", true);
        return true;
    }

    const int32 firstIndex = pPlayer->GetGauntletSwapIndex(0);
    TileInfo* pFirstTile = pWorld->GetTileManager()->GetTile(firstIndex);
    TileInfo* pSecondTile = pWorld->GetTileManager()->GetTile(selectedIndex);
    if(!pFirstTile || !pSecondTile) {
        pPlayer->ResetGauntletSwapState();
        return true;
    }

    pWorld->GetTileManager()->ModifyKeyTile(pFirstTile, true);
    pWorld->GetTileManager()->ModifyKeyTile(pSecondTile, true);
    pFirstTile->SwapContent(*pSecondTile);
    pWorld->GetTileManager()->ModifyKeyTile(pFirstTile, false);
    pWorld->GetTileManager()->ModifyKeyTile(pSecondTile, false);

    pWorld->PlaySFXForEveryone("blorb.wav", 0);
    pWorld->SendTileUpdate(pFirstTile);
    pWorld->SendTileUpdate(pSecondTile);

    pPlayer->ResetGauntletSwapState();
    return true;
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

bool IsWorldFlagMachineItem(uint16 itemID)
{
    switch(itemID) {
        case ITEM_ID_SIGNAL_JAMMER:
        case ITEM_ID_PUNCH_JAMMER:
        case ITEM_ID_ZOMBIE_JAMMER:
        case ITEM_ID_BALLOON_JAMMER:
        case ITEM_ID_ANTIGRAVITY_GENERATOR:
        case ITEM_ID_HYPERTECH_ANTIGRAVITY_FIELD:
            return true;
        default:
            return false;
    }
}

bool HasWorldFlagMachineAlreadyPlaced(World* pWorld, uint16 itemID)
{
    if(!pWorld || !IsWorldFlagMachineItem(itemID)) {
        return false;
    }

    WorldTileManager* pTileManager = pWorld->GetTileManager();
    if(!pTileManager) {
        return false;
    }

    Vector2Int size = pTileManager->GetSize();
    for(int32 y = 0; y < size.y; ++y) {
        for(int32 x = 0; x < size.x; ++x) {
            TileInfo* pTile = pTileManager->GetTile(x, y);
            if(pTile && pTile->GetFG() == itemID) {
                return true;
            }
        }
    }

    return false;
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

bool IsLuckyInLovePotionActive(GamePlayer* pPlayer)
{
    if(!pPlayer) {
        return false;
    }

    return pPlayer->GetCharData().HasPlayMod(PLAYMOD_TYPE_LUCKY_IN_LOVE);
}

bool IsWeatherMachineType(uint8 itemType)
{
    return
        itemType == ITEM_TYPE_WEATHER_MACHINE ||
        itemType == ITEM_TYPE_WEATHER_SPECIAL ||
        itemType == ITEM_TYPE_WEATHER_SPECIAL2;
}

bool ShouldCancelBreakBecauseContainerIsNotEmpty(TileInfo* pTile, ItemInfo* pTileItem, string& outBubbleMessage)
{
    if(!pTile || !pTileItem) {
        return false;
    }

    if(pTileItem->type == ITEM_TYPE_DONATION_BOX) {
        if(TileExtra_DonationBox* pDonation = pTile->GetExtra<TileExtra_DonationBox>()) {
            if(!pDonation->donatedItems.empty()) {
                outBubbleMessage = "Remove what's in there first!";
                return true;
            }
        }
    }

    if(TileExtra_Magplant* pMagplant = pTile->GetExtra<TileExtra_Magplant>()) {
        if(pMagplant->itemCount > 0) {
            outBubbleMessage = "That machine still has items in it!";
            return true;
        }
    }

    if(pTileItem->type == ITEM_TYPE_DISPLAY_BLOCK) {
        if(TileExtra_DisplayBlock* pDisplay = pTile->GetExtra<TileExtra_DisplayBlock>()) {
            if(pDisplay->itemID != ITEM_ID_BLANK) {
                outBubbleMessage = "Remove what's in there first!";
                return true;
            }
        }
    }

    if(pTileItem->type == ITEM_TYPE_VENDING) {
        if(TileExtra_Vending* pVending = pTile->GetExtra<TileExtra_Vending>()) {
            if(pVending->itemID != ITEM_ID_BLANK && pVending->stock > 0) {
                outBubbleMessage = "Remove what's in there first!";
                return true;
            }
        }
    }

    return false;
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

    uint32 cooldownSec = pTileItem->growTime;
    if(cooldownSec == 0) {
        cooldownSec = 1800;
    }

    const uint64 cooldownMS64 = (uint64)cooldownSec * 1000ull;
    const uint32 cooldownMS = cooldownMS64 > UINT32_MAX ? UINT32_MAX : (uint32)cooldownMS64;
    const uint32 nowMS = (uint32)Time::GetSystemTime();

    // Keep provider cooldown in elapsed-time form (last harvest timestamp), which is robust to uint32 tick wrap.
    if(!pWorld->PlayerHasAccessOnTile(pPlayer, pTile) && pProvider->readyTime != 0) {
        const uint32 elapsedMS = nowMS - pProvider->readyTime;
        if(elapsedMS < cooldownMS) {
            return true;
        }
    }

    pProvider->readyTime = nowMS;

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

        case ITEM_ID_TACKLE_BOX: {
            SpawnPrizeDrop(pWorld, pTile, GetPrizeManager()->GetTackleBoxDrop());
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

bool TryHandleSpecialTilePlace(GamePlayer* pPlayer, World* pWorld, TileInfo* pTile, ItemInfo* pPlaceItem, ItemInfo* pTileItem)
{
    if(!pPlayer || !pWorld || !pTile || !pPlaceItem || !pTileItem) {
        return false;
    }

    if(pTileItem->type == ITEM_TYPE_DISPLAY_BLOCK) {
        TileExtra_DisplayBlock* pDisplay = pTile->GetExtra<TileExtra_DisplayBlock>();
        if(!pDisplay) {
            return false;
        }

        if(pDisplay->itemID != ITEM_ID_BLANK) {
            pPlayer->SendOnTalkBubble("Remove what's in there first.", true);
            return true;
        }

        if(pPlaceItem->type == ITEM_TYPE_DISPLAY_BLOCK || pPlaceItem->HasFlag(ITEM_FLAG_MOD) || pPlaceItem->HasFlag(ITEM_FLAG_UNTRADEABLE)) {
            pPlayer->SendOnTalkBubble("You can't put that there.", true);
            return true;
        }

        pDisplay->itemID = pPlaceItem->id;
        pPlayer->ModifyInventoryItem(pPlaceItem->id, -1);

        const Vector2Int tilePos = pTile->GetPos();
        pWorld->SendParticleEffectToAll((tilePos.x * 32.0f) + 16.0f, (tilePos.y * 32.0f) + 16.0f, 4, 1, 0);
        pWorld->PlaySFXForEveryone("blorb.wav", 0);
        pWorld->SendTileUpdate(pTile);
        return true;
    }

    if(pTileItem->type == ITEM_TYPE_CRYSTAL) {
        if(pPlaceItem->type != ITEM_TYPE_CRYSTAL) {
            return false;
        }

        TileExtra_Crystal* pCrystal = pTile->GetExtra<TileExtra_Crystal>();
        if(!pCrystal) {
            return false;
        }

        if(pCrystal->GetTotal() >= 5) {
            pPlayer->SendOnTalkBubble("The crystals have reached their highest vibration!", true);
            return true;
        }

        switch(pPlaceItem->id) {
            case ITEM_ID_RED_CRYSTAL: ++pCrystal->red; break;
            case ITEM_ID_BLUE_CRYSTAL: ++pCrystal->blue; break;
            case ITEM_ID_GREEN_CRYSTAL: ++pCrystal->green; break;
            case ITEM_ID_WHITE_CRYSTAL: ++pCrystal->white; break;
            case ITEM_ID_BLACK_CRYSTAL: ++pCrystal->black; break;
            default: return true;
        }

        pPlayer->ModifyInventoryItem(pPlaceItem->id, -1);
        pWorld->SendTileUpdate(pTile);
        return true;
    }

    if(pTileItem->type == ITEM_TYPE_DONATION_BOX) {
        TileExtra_DonationBox* pDonation = pTile->GetExtra<TileExtra_DonationBox>();
        if(!pDonation) {
            return false;
        }

        if(pDonation->donatedItems.size() >= 20) {
            pPlayer->SendOnTalkBubble("The donation box is already full!", true);
            return true;
        }

        int32 donatedByPlayer = 0;
        for(const TileExtra_DonatedItem& item : pDonation->donatedItems) {
            if(item.userID == pPlayer->GetUserID()) {
                ++donatedByPlayer;
            }
        }

        if(donatedByPlayer >= 3 && !pWorld->PlayerHasAccessOnTile(pPlayer, pTile)) {
            pPlayer->SendOnTalkBubble("You already donated items 3 times! Try again later.", true);
            return true;
        }

        if(pPlaceItem->rarity < 2) {
            pPlayer->SendOnTalkBubble("This box only accepts items of rarity 2 and above.", true);
            return true;
        }

        if(pPlaceItem->id == ITEM_ID_FIST || pPlaceItem->id == ITEM_ID_WRENCH) {
            pPlayer->SendOnTalkBubble("You can't put that there.", true);
            return true;
        }

        DialogBuilder db;
        db.SetDefaultColor('o')
            ->AddLabelWithIcon("`w" + string(pPlaceItem->name) + "``", pPlaceItem->id, true)
            ->AddLabel("How many to put in the box as a gift? (Note: You will `4LOSE`` the items you give!)")
            ->AddTextInput("Amount", "Count:", ToString(pPlayer->GetInventory().GetCountOfItem(pPlaceItem->id)), 3)
            ->AddTextInput("Comment", "Optional Note:", "", 128)
            ->EmbedData("tilex", pTile->GetPos().x)
            ->EmbedData("tiley", pTile->GetPos().y)
            ->EmbedData("TileItemID", pTile->GetDisplayedItem())
            ->EmbedData("ItemIDToDonate", pPlaceItem->id)
            ->AddButton("Give", "`4Give the item(s)``")
            ->EndDialog("DonationEdit", "", "Cancel");

        pPlayer->SendOnDialogRequest(db.Get());
        return true;
    }

    if(pTileItem->type == ITEM_TYPE_MANNEQUIN && pPlaceItem->type == ITEM_TYPE_CLOTHES) {
        TileExtra_Mannequin* pMannequin = pTile->GetExtra<TileExtra_Mannequin>();
        if(!pMannequin) {
            return false;
        }

        bool alreadyWearing = false;
        switch(pPlaceItem->bodyPart) {
            case BODY_PART_BACK: alreadyWearing = pMannequin->clothing.back == pPlaceItem->id; break;
            case BODY_PART_HAND: alreadyWearing = pMannequin->clothing.hand == pPlaceItem->id; break;
            case BODY_PART_HAT: alreadyWearing = pMannequin->clothing.hat == pPlaceItem->id; break;
            case BODY_PART_FACEITEM: alreadyWearing = pMannequin->clothing.face == pPlaceItem->id; break;
            case BODY_PART_CHESTITEM: alreadyWearing = pMannequin->clothing.necklace == pPlaceItem->id; break;
            case BODY_PART_SHIRT: alreadyWearing = pMannequin->clothing.shirt == pPlaceItem->id; break;
            case BODY_PART_PANT: alreadyWearing = pMannequin->clothing.pants == pPlaceItem->id; break;
            case BODY_PART_SHOE: alreadyWearing = pMannequin->clothing.shoes == pPlaceItem->id; break;
            case BODY_PART_HAIR: alreadyWearing = pMannequin->clothing.hair == pPlaceItem->id; break;
            default: break;
        }

        if(alreadyWearing) {
            return true;
        }

        DialogBuilder db;
        db.SetDefaultColor('o')
            ->AddLabelWithIcon("`w" + string(pPlaceItem->name) + "``", pPlaceItem->id, true)
            ->AddLabel("Do you really want to put your " + string(pPlaceItem->name) + " on the Mannequin?")
            ->EmbedData("tilex", pTile->GetPos().x)
            ->EmbedData("tiley", pTile->GetPos().y)
            ->EmbedData("TileItemID", pTile->GetDisplayedItem())
            ->EmbedData("ItemID", pPlaceItem->id)
            ->EndDialog("MannequinEdit", "Yes", "No");

        pPlayer->SendOnDialogRequest(db.Get());
        return true;
    }

    return false;
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
        const string cachedName = GetGameServer()->GetPlayerNameByUserID((uint32)pLockExtra->ownerID);
        if(cachedName.empty()) {
            return "DeletedUser";
        }

        return cachedName;
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
        case ITEM_ID_BALLOON_JAMMER: worldFlag = WORLD_FLAG_BALLOON_JAMMED; break;
        case ITEM_ID_ANTIGRAVITY_GENERATOR:
        case ITEM_ID_HYPERTECH_ANTIGRAVITY_FIELD: worldFlag = WORLD_FLAG_ANTI_GRAVITY; break;
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
        case ITEM_ID_BALLOON_JAMMER: pWorld->SetWorldFlag(WORLD_FLAG_BALLOON_JAMMED, false); break;
        case ITEM_ID_ANTIGRAVITY_GENERATOR:
        case ITEM_ID_HYPERTECH_ANTIGRAVITY_FIELD: pWorld->SetWorldFlag(WORLD_FLAG_ANTI_GRAVITY, false); break;
        case ITEM_ID_GUARDIAN_PINEAPPLE: pWorld->SetWorldFlag(WORLD_FLAG_GUARDIAN_PINEAPPLE, false); break;
        default: break;
    }
}

}

void TileChangeRequest::OnPunchedLock(GamePlayer* pPlayer, TileInfo* pTile)
{

}

void TileChangeRequest::HandleConsumable(GamePlayer* pPlayer, World* pWorld, GameUpdatePacket* pPacket)
{
    if(!pPlayer || !pWorld || !pPacket) {
        return;
    }

    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(pPacket->itemID);
    if(!pItem) {
        return;
    }

    TileInfo* pTile = pWorld->GetTileManager()->GetTile(pPacket->tileX, pPacket->tileY);
    if(!pTile) {
        return;
    }

    const Vector2Int tilePos = pTile->GetPos();
    static constexpr const char* kBirthCertCooldownStat = "BirthCertLastUseMS";

    switch(pItem->id) {
        case ITEM_ID_FOSSIL_BRUSH: {
            if(!FossilComponent::TryHandlePrepAction(pPlayer, pWorld, pTile)) {
                pPlayer->SendOnTalkBubble("`wUse this on a Fossil Prep Station with an untouched Fossil on the tile.", true);
            }
            return;
        }

        case ITEM_ID_GROW_SPRAY_FERTILIZER:
        case ITEM_ID_DELUXE_GROW_SPRAY:
        case ITEM_ID_ULTRA_GROW_SPRAY: {
            if(pTile->GetDisplayedItem() == ITEM_ID_BLANK) {
                return;
            }

            ItemInfo* pTileItem = GetItemInfoManager()->GetItemByID(pTile->GetDisplayedItem());
            if(!pTileItem || pTileItem->type != ITEM_TYPE_SEED) {
                return;
            }

            TileExtra_Seed* pSeedData = pTile->GetExtra<TileExtra_Seed>();
            if(!pSeedData) {
                return;
            }

            uint64 growSprayMS = pItem->id == ITEM_ID_DELUXE_GROW_SPRAY ? 24ull * 60ull * 60ull * 1000ull : 60ull * 60ull * 1000ull;
            if(pItem->id == ITEM_ID_ULTRA_GROW_SPRAY) {
                growSprayMS = 24ull * 60ull * 60ull * 1000ull;
            }
            const uint64 nowMS = Time::GetSystemTime();
            if(pSeedData->growTime > 0 && nowMS >= pSeedData->growTime && (nowMS - pSeedData->growTime) >= ((uint64)pTileItem->growTime * 1000ull)) {
                pPlayer->SendOnTalkBubble("That tree is already grown!", true);
                return;
            }

            if(growSprayMS > 0) {
                if(pSeedData->growTime > growSprayMS) {
                    pSeedData->growTime -= (uint32)growSprayMS;
                }
                else {
                    pSeedData->growTime = 0;
                }
            }

            pPlayer->ModifyInventoryItem(pItem->id, -1);
            pPlayer->SendOnPlayPositioned("audio/spray.wav");
            const bool isOneDaySpray = pItem->id == ITEM_ID_DELUXE_GROW_SPRAY || pItem->id == ITEM_ID_ULTRA_GROW_SPRAY;
            pPlayer->SendOnTalkBubble("``" + string(pTileItem->name) + "`` Tree aged by " + string(isOneDaySpray ? "24 hours``" : "1 hour``"), true);
            pWorld->SendTileUpdate(pTile);
            return;
        }

        case ITEM_ID_DOOR_MOVER: {
            if(!pWorld->IsPlayerWorldOwner(pPlayer)) {
                pPlayer->SendOnTalkBubble("`4You can only use this item on your own worlds with a World Lock!", true);
                return;
            }

            TileInfo* pMainDoor = pWorld->GetTileManager()->GetKeyTile(KEY_TILE_MAIN_DOOR);
            if(!pMainDoor) {
                pPlayer->SendOnTalkBubble("`4You need to have a main door first!", true);
                return;
            }

            if(pTile->GetDisplayedItem() != ITEM_ID_BLANK) {
                pPlayer->SendOnTalkBubble("`4You can only move your Main Door to empty spaces!", true);
                return;
            }

            TileInfo* pTileBelow = pWorld->GetTileManager()->GetTile(tilePos.x, tilePos.y + 1);
            if(!pTileBelow || (pTileBelow->GetDisplayedItem() != ITEM_ID_BLANK && pTileBelow->GetDisplayedItem() != ITEM_ID_BEDROCK)) {
                pPlayer->SendOnTalkBubble("`4The tile below has to be empty or bedrock!", true);
                return;
            }

            const Vector2Int oldDoorPos = pMainDoor->GetPos();
            TileInfo* pOldBelow = pWorld->GetTileManager()->GetTile(oldDoorPos.x, oldDoorPos.y + 1);

            pMainDoor->SetFG(ITEM_ID_BLANK, pWorld->GetTileManager());
            if(pOldBelow && pOldBelow->GetDisplayedItem() == ITEM_ID_BEDROCK) {
                pOldBelow->SetFG(ITEM_ID_BLANK, pWorld->GetTileManager());
            }

            if(pTileBelow->GetDisplayedItem() == ITEM_ID_BLANK) {
                pTileBelow->SetFG(ITEM_ID_BEDROCK, pWorld->GetTileManager());
            }

            pTile->SetFG(ITEM_ID_MAIN_DOOR, pWorld->GetTileManager());

            pPlayer->ModifyInventoryItem(ITEM_ID_DOOR_MOVER, -1);
            pPlayer->SendOnTalkBubble("`wYou moved the door!", true);

            pWorld->SendTileUpdate(pMainDoor);
            if(pOldBelow) {
                pWorld->SendTileUpdate(pOldBelow);
            }
            pWorld->SendTileUpdate(pTileBelow);
            pWorld->SendTileUpdate(pTile);
            return;
        }

        case ITEM_ID_BIRTH_CERTIFICATE: {
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

        case ITEM_ID_WATER_BUCKET: {
            pTile->ToggleFlag(TILE_FLAG_IS_WET);
            pPlayer->ModifyInventoryItem(pItem->id, -1);
            pPlayer->SendOnPlayPositioned("audio/spray.wav");
            pWorld->SendTileUpdate(pTile);
            return;
        }

        case ITEM_ID_GHOST_JAR: {
            if(!GhostAlgorithm::PlaceGhostJar(pPlayer, pWorld, pTile)) {
                return;
            }

            pPlayer->ModifyInventoryItem(ITEM_ID_GHOST_JAR, -1);
            return;
        }

        case ITEM_ID_GHOST_BE_GONE: {
            if(!GhostAlgorithm::HasWorldGhosts(pWorld)) {
                pPlayer->SendOnTalkBubble("There are no ghosts in this world.", true);
                return;
            }

            DialogBuilder db;
            db.SetDefaultColor('o')
                ->AddLabelWithIcon("`wRemove Ghost``", ITEM_ID_GHOST_BE_GONE, true, true)
                ->AddLabel("Wanna remove ghost? It's gonna cost you `515 World Locks `ofor our services.")
                ->AddButton("GhostBeGone", "Yes remove ghost.")
                ->EndDialog("GhostBeGone", "", "Nevermind");

            pPlayer->SendOnDialogRequest(db.Get());
            return;
        }

        case ITEM_ID_BLOCK_GLUE: {
            pTile->ToggleFlag(TILE_FLAG_GLUED);
            pPlayer->ModifyInventoryItem(pItem->id, -1);
            pPlayer->SendOnPlayPositioned("audio/spray.wav");
            pWorld->SendTileUpdate(pTile);
            return;
        }

        case ITEM_ID_SNOWBALL: {
            if(pTile->GetDisplayedItem() == ITEM_ID_MAIN_DOOR) {
                pPlayer->SendOnTalkBubble("You can't use that there.", true);
                pPlayer->PlaySFX("cant_place_tile.wav");
                return;
            }

            GamePlayer* pVictim = pWorld->GetPlayerOnTile(tilePos);
            if(!pVictim) {
                return;
            }

            pVictim->AddPlayMod(PLAYMOD_TYPE_FROZEN, true);
            pVictim->SendOnTalkBubble("`#YUMMY!", false);
            pPlayer->ModifyInventoryItem(pItem->id, -1);
            pPlayer->SendOnPlayPositioned("audio/freeze.wav");
            return;
        }

        case ITEM_ID_ELDRITCH_FLAME: {
            std::vector<TileInfo*> changedTiles;
            for(int32 i = 0; i < 10; ++i) {
                int32 offsetX = (rand() % 7) - 3;
                int32 offsetY = (rand() % 7) - 3;
                TileInfo* pFireTile = pWorld->GetTileManager()->GetTile(tilePos.x + offsetX, tilePos.y + offsetY);
                if(!pFireTile || pFireTile->HasFlag(TILE_FLAG_ON_FIRE)) {
                    continue;
                }

                pFireTile->SetFlag(TILE_FLAG_ON_FIRE);
                changedTiles.push_back(pFireTile);
            }

            if(changedTiles.empty()) {
                return;
            }

            pPlayer->ModifyInventoryItem(pItem->id, -1);
            pPlayer->SendOnPlayPositioned("audio/spray.wav");
            for(TileInfo* pFireTile : changedTiles) {
                pWorld->SendTileUpdate(pFireTile);
            }
            return;
        }

        default:
            return;
    }

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

    if(pPacket->itemID != ITEM_ID_FIST && pPlayer->GetInventory().GetCountOfItem(pItem->id) == 0) {
        pPlayer->SendFakePingReply();
        return;
    }

    if(pItem->type == ITEM_TYPE_CONSUMABLE && !isChemsynthTool) {
        HandleConsumable(pPlayer, pWorld, pPacket);
        pPlayer->IncreaseStat("CONSUMABLES_USED");
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

    if(TryHandleGauntlet(pPlayer, pWorld, pTile, pItem)) {
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

    if(pPacket->itemID != ITEM_ID_FIST && TryHandleSpecialTilePlace(pPlayer, pWorld, pTile, pItem, pTileItem)) {
        return;
    }

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

        if(IsWorldFlagMachineItem(pItem->id) && HasWorldFlagMachineAlreadyPlaced(pWorld, pItem->id)) {
            pPlayer->SendFakePingReply();
            pPlayer->SendOnTalkBubble("Only one of these can be placed in a world.", true);
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
        const uint16 handItem = pPlayer->GetInventory().GetClothByPart(BODY_PART_HAND);

        if(handItem == ITEM_ID_NEUTRON_GUN) {
            const Vector2Float tilePosFloat = { tilePos.x * 32.0f, tilePos.y * 32.0f };
            GhostAlgorithm::PullGhostsTowardPlayer(pWorld, pPlayer->GetWorldPos(), tilePosFloat);
            return;
        }

        if(handItem == ITEM_ID_FIRE_HOSE) {
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
            if(pTileItem && pTileItem->id == ITEM_ID_SCIENCE_STATION) {
                SpawnPrizeDrop(pWorld, pTile, GetPrizeManager()->GetChemStationDrop());
                return;
            }

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
            FossilComponent::OnFossilRockPunched(pPlayer, pWorld, pTile, pTileItem);
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
                if(IsWeatherMachineType(pTileItem->type) && pTileItem->weatherID != 200) {
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

            string breakCancelMessage;
            if(ShouldCancelBreakBecauseContainerIsNotEmpty(pTile, pTileItem, breakCancelMessage)) {
                pPlayer->SendOnTalkBubble(breakCancelMessage, true);
                return;
            }

            if(pTileItem->HasFlag(ITEM_FLAG_DROPLESS) && !pPlayer->GetInventory().HaveRoomForItem(pTileItem->id, 1)) {
                pPlayer->SendOnTalkBubble("I don't have enough inventory space to pick that up!", true);
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
            
            if(IsWeatherMachineType(pTileItem->type)) {
                if(pWorld->GetCurrentWeather() == pTileItem->weatherID) {
                    pWorld->SetCurrentWeather(pWorld->GetDefaultWeather());
                }

                pTile->RemoveFlag(TILE_FLAG_IS_ON);
                pWorld->SendCurrentWeatherToAll();
            }

            if(!pTileItem->HasFlag(ITEM_FLAG_DROPLESS)) {
                if(pTileItem->id == ITEM_ID_GOLDEN_BOOTY_CHEST) {
                    SpawnPrizeDrop(pWorld, pTile, GetPrizeManager()->GetGBCDrop(IsLuckyInLovePotionActive(pPlayer)));
                }
                else if(pTileItem->id == ITEM_ID_SUPER_GOLDEN_BOOTY_CHEST) {
                    SpawnPrizeDrop(pWorld, pTile, GetPrizeManager()->GetSGBCDrop(IsLuckyInLovePotionActive(pPlayer)));
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

                GeigerComponent::TrySpawnRadioactiveDrop(pPlayer, pWorld, pTile, pTileItem);
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