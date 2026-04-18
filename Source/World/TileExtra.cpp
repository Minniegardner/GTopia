#include "TileExtra.h"
#include "TileInfo.h"
#include "Item/ItemUtils.h"
#include "Item/ItemInfoManager.h"
#include "Utils/Timer.h"

namespace {

bool IsMagplantFamily(uint16 itemID)
{
    return itemID == ITEM_ID_MAGPLANT_5000 || itemID == ITEM_ID_UNSTABLE_TESSERACT || itemID == ITEM_ID_GAIAS_BEACON;
}

bool IsChemsynthTank(uint16 itemID, uint8 itemType)
{
    return itemID == ITEM_ID_CHEMSYNTH_TANK || itemType == ITEM_TYPE_CHEMTANK;
}

}

uint8 GetTileExtraType(uint8 itemType)
{
    switch(itemType) {
        case ITEM_TYPE_USER_DOOR: case ITEM_TYPE_DOOR:
        case ITEM_TYPE_PORTAL: case ITEM_TYPE_SUNGATE:
        case ITEM_TYPE_FRIENDS_ENTRANCE:
            return TILE_EXTRA_TYPE_DOOR;

        case ITEM_TYPE_SIGN:
            return TILE_EXTRA_TYPE_SIGN;

        case ITEM_TYPE_LOCK:
            return TILE_EXTRA_TYPE_LOCK;

        case ITEM_TYPE_SEED:
            return TILE_EXTRA_TYPE_SEED;

        case ITEM_TYPE_PROVIDER:
            return TILE_EXTRA_TYPE_PROVIDER;

        case ITEM_TYPE_LAB:
            return TILE_EXTRA_TYPE_LAB;

        case ITEM_TYPE_HEART_MONITOR:
            return TILE_EXTRA_TYPE_HEART_MONITOR;

        case ITEM_TYPE_COMPONENT:
            return TILE_EXTRA_TYPE_COMPONENT;

        case ITEM_TYPE_SPIRIT_STORAGE:
            return TILE_EXTRA_TYPE_SPIRIT_STORAGE;

        case ITEM_TYPE_VENDING:
            return TILE_EXTRA_TYPE_VENDING;

        case ITEM_TYPE_DISPLAY_BLOCK:
            return TILE_EXTRA_TYPE_DISPLAY_BLOCK;

        case ITEM_TYPE_CRYSTAL:
            return TILE_EXTRA_TYPE_CRYSTAL;

        case ITEM_TYPE_DONATION_BOX:
            return TILE_EXTRA_TYPE_DONATION_BOX;

        case ITEM_TYPE_MANNEQUIN:
            return TILE_EXTRA_TYPE_MANNEQUIN;

        case ITEM_TYPE_FOSSIL_PREP:
            return TILE_EXTRA_TYPE_FOSSIL_PREP;

        case ITEM_TYPE_GEIGERCHARGE:
            return TILE_EXTRA_TYPE_GEIGER_CHARGER;

        default:
            return TILE_EXTRA_TYPE_NONE;
    }
}

uint8 GetTileExtraTypeByItem(uint16 itemID, uint8 itemType)
{
    if(IsMagplantFamily(itemID)) {
        return TILE_EXTRA_TYPE_MAGPLANT;
    }

    if(IsChemsynthTank(itemID, itemType)) {
        return TILE_EXTRA_TYPE_CHEMTANK;
    }

    return GetTileExtraType(itemType);
}

TileExtra* TileExtra::Create(uint8 tileExtraType)
{
    switch(tileExtraType) {
        case TILE_EXTRA_TYPE_DOOR:
            return new TileExtra_Door();

        case TILE_EXTRA_TYPE_SIGN:
            return new TileExtra_Sign();

        case TILE_EXTRA_TYPE_VENDING:
            return new TileExtra_Vending();

        case TILE_EXTRA_TYPE_DISPLAY_BLOCK:
            return new TileExtra_DisplayBlock();

        case TILE_EXTRA_TYPE_CRYSTAL:
            return new TileExtra_Crystal();

        case TILE_EXTRA_TYPE_DONATION_BOX:
            return new TileExtra_DonationBox();

        case TILE_EXTRA_TYPE_MANNEQUIN:
            return new TileExtra_Mannequin();

        case TILE_EXTRA_TYPE_MAGPLANT:
            return new TileExtra_Magplant();

        case TILE_EXTRA_TYPE_CHEMTANK:
            return new TileExtra_Chemsynth();

        case TILE_EXTRA_TYPE_FOSSIL_PREP:
            return new TileExtra_FossilPrep();

        case TILE_EXTRA_TYPE_GEIGER_CHARGER:
            return new TileExtra_GeigerCharger();

        case TILE_EXTRA_TYPE_LOCK:
            return new TileExtra_Lock(); 

        case TILE_EXTRA_TYPE_SEED:
            return new TileExtra_Seed();

        case TILE_EXTRA_TYPE_COMPONENT:
            return new TileExtra_Component();

        case TILE_EXTRA_TYPE_PROVIDER:
            return new TileExtra_Provider();

        case TILE_EXTRA_TYPE_LAB:
            return new TileExtra_Lab();

        case TILE_EXTRA_TYPE_HEART_MONITOR:
            return new TileExtra_HeartMonitor();

        case TILE_EXTRA_TYPE_SPIRIT_STORAGE:
            return new TileExtra_SpiritStorage();

        default:
            return nullptr;
    }
}

void TileExtra::Serialize(MemoryBuffer& memBuffer, bool write)
{
    memBuffer.ReadWrite(type, write);
}

void TileExtra_Door::Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo* pTile, uint16 worldVersion)
{
    TileExtra::Serialize(memBuffer, write);
    memBuffer.ReadWriteString(name, write);

    if(database) {
        memBuffer.ReadWriteString(text, write);
        memBuffer.ReadWriteString(id, write);
    }

    int8 unk = 0;
    memBuffer.ReadWrite(unk, write);
}

void TileExtra_Sign::Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo* pTile, uint16 worldVersion)
{
    TileExtra::Serialize(memBuffer, write);

    memBuffer.ReadWriteString(text, write);

    int32 unk = -1; // something with owner union but eh
    memBuffer.ReadWrite(unk, write);
}

void TileExtra_Vending::Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo* pTile, uint16 worldVersion)
{
    TileExtra::Serialize(memBuffer, write);

    if(database) {
        memBuffer.ReadWrite(itemID, write);
        memBuffer.ReadWrite(price, write);
        memBuffer.ReadWrite(stock, write);
        memBuffer.ReadWrite(earnings, write);
        return;
    }

    int32 packetPrice = price;
    if((packetPrice < 0 && stock < std::abs(packetPrice)) || stock < 1) {
        packetPrice = 0;
    }

    int32 packetItemID = (int32)itemID;
    memBuffer.ReadWrite(packetItemID, write);
    memBuffer.ReadWrite(packetPrice, write);

    if(!write) {
        if(packetItemID < 0 || packetItemID > UINT16_MAX) {
            itemID = ITEM_ID_BLANK;
        }
        else {
            itemID = (uint16)packetItemID;
        }
    }
}

void TileExtra_DisplayBlock::Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo* pTile, uint16 worldVersion)
{
    TileExtra::Serialize(memBuffer, write);

    int32 packetItemID = (int32)itemID;
    memBuffer.ReadWrite(packetItemID, write);

    if(!write) {
        if(packetItemID < 0 || packetItemID > UINT16_MAX) {
            itemID = ITEM_ID_BLANK;
        }
        else {
            itemID = (uint16)packetItemID;
        }
    }
}

void TileExtra_Crystal::Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo* pTile, uint16 worldVersion)
{
    TileExtra::Serialize(memBuffer, write);

    if(database) {
        memBuffer.ReadWrite(red, write);
        memBuffer.ReadWrite(green, write);
        memBuffer.ReadWrite(blue, write);
        memBuffer.ReadWrite(white, write);
        memBuffer.ReadWrite(black, write);
        return;
    }

    if(write) {
        uint8 count = (uint8)std::min<uint32>(255, GetTotal() + (black * 2));
        uint8 unk = 0;
        memBuffer.ReadWrite(count, true);
        memBuffer.ReadWrite(unk, true);

        for(uint32 i = 0; i < red; ++i) {
            uint8 value = 0x31;
            memBuffer.ReadWrite(value, true);
        }

        for(uint32 i = 0; i < green; ++i) {
            uint8 value = 0x32;
            memBuffer.ReadWrite(value, true);
        }

        for(uint32 i = 0; i < blue; ++i) {
            uint8 value = 0x33;
            memBuffer.ReadWrite(value, true);
        }

        for(uint32 i = 0; i < white; ++i) {
            uint8 value = 0x34;
            memBuffer.ReadWrite(value, true);
        }

        for(uint32 i = 0; i < black; ++i) {
            uint8 value = 0x34;
            memBuffer.ReadWrite(value, true);
        }

        for(uint32 i = 0; i < black; ++i) {
            uint8 value = 0x35;
            memBuffer.ReadWrite(value, true);
            memBuffer.ReadWrite(value, true);
        }
        return;
    }

    uint8 count = 0;
    memBuffer.ReadWrite(count, false);

    uint8 unk = 0;
    memBuffer.ReadWrite(unk, false);

    red = 0;
    green = 0;
    blue = 0;
    white = 0;
    black = 0;

    for(uint8 i = 0; i < count; ++i) {
        uint8 value = 0;
        memBuffer.ReadWrite(value, false);

        if(value == 0x31) {
            ++red;
        }
        else if(value == 0x32) {
            ++green;
        }
        else if(value == 0x33) {
            ++blue;
        }
        else if(value == 0x34) {
            ++white;
        }
        else if(value == 0x35) {
            ++black;
            if(i + 1 < count) {
                uint8 extra = 0;
                memBuffer.ReadWrite(extra, false);
                ++i;
            }
        }
    }
}

void TileExtra_DonationBox::Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo* pTile, uint16 worldVersion)
{
    if(database) {
        TileExtra::Serialize(memBuffer, write);

        uint32 donatedItemCount = (uint32)donatedItems.size();
        memBuffer.ReadWrite(donatedItemCount, write);

        if(!write) {
            donatedItems.clear();
            donatedItems.resize(donatedItemCount);
        }

        for(uint32 i = 0; i < donatedItemCount; ++i) {
            TileExtra_DonatedItem& entry = donatedItems[i];
            memBuffer.ReadWrite(entry.itemID, write);
            memBuffer.ReadWrite(entry.amount, write);
            memBuffer.ReadWrite(entry.userID, write);
            memBuffer.ReadWriteString(entry.username, write);
            memBuffer.ReadWriteString(entry.comment, write);
            memBuffer.ReadWrite(entry.donateID, write);
            memBuffer.ReadWrite(entry.donatedAt, write);
        }

        memBuffer.ReadWrite(currentDonateID, write);
        return;
    }

    uint8 packetType = TILE_EXTRA_TYPE_MAGIC_EGG;
    memBuffer.ReadWrite(packetType, write);

    uint32 zero = 0;
    memBuffer.ReadWrite(zero, write);
}

void TileExtra_Mannequin::Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo* pTile, uint16 worldVersion)
{
    TileExtra::Serialize(memBuffer, write);

    memBuffer.ReadWriteString(label, write);
    memBuffer.ReadWrite(hairColorA, write);
    memBuffer.ReadWrite(hairColorR, write);
    memBuffer.ReadWrite(hairColorG, write);
    memBuffer.ReadWrite(hairColorB, write);
    memBuffer.ReadWrite(unknown, write);

    if(database) {
        memBuffer.ReadWrite(clothing.hair, write);
        memBuffer.ReadWrite(clothing.shirt, write);
        memBuffer.ReadWrite(clothing.pants, write);
        memBuffer.ReadWrite(clothing.shoes, write);
        memBuffer.ReadWrite(clothing.face, write);
        memBuffer.ReadWrite(clothing.hand, write);
        memBuffer.ReadWrite(clothing.back, write);
        memBuffer.ReadWrite(clothing.hat, write);
        memBuffer.ReadWrite(clothing.necklace, write);
        return;
    }

    if(write) {
        memBuffer.ReadWrite(clothing.hat, true);
        memBuffer.ReadWrite(clothing.shirt, true);
        memBuffer.ReadWrite(clothing.pants, true);
        memBuffer.ReadWrite(clothing.shoes, true);
        memBuffer.ReadWrite(clothing.face, true);
        memBuffer.ReadWrite(clothing.hand, true);
        memBuffer.ReadWrite(clothing.back, true);
        memBuffer.ReadWrite(clothing.hair, true);
        memBuffer.ReadWrite(clothing.necklace, true);
        return;
    }

    memBuffer.ReadWrite(clothing.hat, false);
    memBuffer.ReadWrite(clothing.shirt, false);
    memBuffer.ReadWrite(clothing.pants, false);
    memBuffer.ReadWrite(clothing.shoes, false);
    memBuffer.ReadWrite(clothing.face, false);
    memBuffer.ReadWrite(clothing.hand, false);
    memBuffer.ReadWrite(clothing.back, false);
    memBuffer.ReadWrite(clothing.hair, false);
    memBuffer.ReadWrite(clothing.necklace, false);
}

void TileExtra_Magplant::Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo* pTile, uint16 worldVersion)
{
    TileExtra::Serialize(memBuffer, write);
    uint8 magneticFlag = magnetic ? 1 : 0;
    uint8 remoteFlag = remote ? 1 : 0;

    // Keep the same field order as GrowtopiaV1 to avoid packet/world decode mismatch.
    memBuffer.ReadWrite(itemID, write);
    memBuffer.ReadWrite(itemCount, write);
    memBuffer.ReadWrite(magneticFlag, write);
    memBuffer.ReadWrite(remoteFlag, write);
    memBuffer.ReadWrite(itemLimit, write);

    if(!write) {
        const uint16 parsedItemID = itemID;
        const int32 parsedItemCount = itemCount;
        const int32 parsedItemLimit = itemLimit;
        const uint8 parsedMagneticFlag = magneticFlag;
        const uint8 parsedRemoteFlag = remoteFlag;

        auto IsDecodedStateValid = [&](uint16 inItemID, int32 inItemCount, int32 inItemLimit, uint8 inMagnetic, uint8 inRemote) -> bool {
            if(inMagnetic > 1 || inRemote > 1) {
                return false;
            }

            int32 expectedItemLimit = 5000;
            if(pTile) {
                const uint16 machineID = pTile->GetFG();
                if(machineID == ITEM_ID_UNSTABLE_TESSERACT || machineID == ITEM_ID_GAIAS_BEACON) {
                    expectedItemLimit = 1500;
                }
            }

            if(inItemLimit < 0 || inItemLimit > expectedItemLimit) {
                return false;
            }

            if(inItemCount < 0 || inItemCount > inItemLimit) {
                return false;
            }

            if(inItemID != ITEM_ID_BLANK && !GetItemInfoManager()->GetItemByID(inItemID)) {
                return false;
            }

            return true;
        };

        if(!IsDecodedStateValid(parsedItemID, parsedItemCount, parsedItemLimit, parsedMagneticFlag, parsedRemoteFlag)) {
            const uint32 parsedCountU = (uint32)parsedItemCount;
            const uint32 parsedLimitU = (uint32)parsedItemLimit;

            const int32 legacyItemLimit = (int32)(((uint32)parsedItemID & 0xFFFFu) | ((parsedCountU & 0xFFFFu) << 16));
            const uint8 legacyRemote = (uint8)((parsedCountU >> 16) & 0xFFu);
            const uint8 legacyMagnetic = (uint8)((parsedCountU >> 24) & 0xFFu);
            const int32 legacyItemCount = (int32)(
                ((uint32)parsedMagneticFlag) |
                ((uint32)parsedRemoteFlag << 8) |
                ((parsedLimitU & 0xFFu) << 16) |
                (((parsedLimitU >> 8) & 0xFFu) << 24)
            );
            const uint16 legacyItemID = (uint16)((parsedLimitU >> 16) & 0xFFFFu);

            if(IsDecodedStateValid(legacyItemID, legacyItemCount, legacyItemLimit, legacyMagnetic, legacyRemote)) {
                itemID = legacyItemID;
                itemCount = legacyItemCount;
                itemLimit = legacyItemLimit;
                magneticFlag = legacyMagnetic;
                remoteFlag = legacyRemote;
            }
        }

        if(itemLimit < 0) {
            itemLimit = 0;
        }

        if(itemCount < 0) {
            itemCount = 0;
        }

        if(itemLimit > 0 && itemCount > itemLimit) {
            itemCount = itemLimit;
        }

        if(itemID != ITEM_ID_BLANK && !GetItemInfoManager()->GetItemByID(itemID)) {
            itemID = ITEM_ID_BLANK;
            itemCount = 0;
        }

        remote = remoteFlag != 0;
        magnetic = magneticFlag != 0;
    }
}

void TileExtra_Chemsynth::Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo* pTile, uint16 worldVersion)
{
    TileExtra::Serialize(memBuffer, write);

    int32 currentColor = static_cast<int32>(color);
    int32 currentTargetColor = static_cast<int32>(targetColor);
    memBuffer.ReadWrite(currentColor, write);
    memBuffer.ReadWrite(currentTargetColor, write);

    if(!write) {
        color = static_cast<ChemsynthColor>(currentColor);
        targetColor = static_cast<ChemsynthColor>(currentTargetColor);
    }
}

void TileExtra_FossilPrep::Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo* pTile, uint16 worldVersion)
{
    TileExtra::Serialize(memBuffer, write);
    memBuffer.ReadWrite(uses, write);
}

void TileExtra_GeigerCharger::Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo* pTile, uint16 worldVersion)
{
    TileExtra::Serialize(memBuffer, write);
    memBuffer.ReadWrite(charges, write);
}

void TileExtra_Lock::Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo *pTile, uint16 worldVersion)
{
    TileExtra::Serialize(memBuffer, write);

    memBuffer.ReadWrite(flags, write);
    memBuffer.ReadWrite(ownerID, write);

    uint32 accessSize = accessList.size();
    memBuffer.ReadWrite(accessSize, write);

    if(!write) {
        accessList.resize(accessSize);
    }

    if(accessSize > 0) {
        memBuffer.ReadWriteRaw(accessList.data(), accessSize * sizeof(int32), write);
    }

    if(worldVersion > 11) {
        memBuffer.ReadWrite(minEntryLevel, write);
    }

    if(worldVersion > 12) {
        memBuffer.ReadWrite(worldTimer, write);
    }

    if(worldVersion > 13) {
        memBuffer.ReadWrite(guildID, write);

        if(write && !database) {
            uint16 clientFG = (uint16)GetItemInfoManager()->GetClientIDByItemID(guildFG);
            uint16 clientBG = (uint16)GetItemInfoManager()->GetClientIDByItemID(guildBG);
            memBuffer.ReadWrite(clientFG, true);
            memBuffer.ReadWrite(clientBG, true);
        }
        else {
            memBuffer.ReadWrite(guildFG, write);
            memBuffer.ReadWrite(guildBG, write);
        }

        memBuffer.ReadWrite(guildLevel, write);
        memBuffer.ReadWrite(guildFlags, write);

        if(write) {
            uint8 padding[3] = {0, 0, 0};
            memBuffer.ReadWriteRaw(padding, sizeof(padding), true);
        }
        else {
            uint8 padding[3];
            memBuffer.ReadWriteRaw(padding, sizeof(padding), false);
        }
    }
}

void TileExtra_Seed::Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo *pTile, uint16 worldVersion)
{
    TileExtra::Serialize(memBuffer, write);
    if(write && !database) {
        uint32 elapsedSec = 0;
        uint64 nowMS = Time::GetSystemTime();

        if(growTime > 0 && nowMS > growTime) {
            elapsedSec = (uint32)((nowMS - growTime) / 1000);
        }

        memBuffer.ReadWrite(elapsedSec, true);
    }
    else {
        memBuffer.ReadWrite(growTime, write);
    }

    memBuffer.ReadWrite(fruitCount, write);
}

void TileExtra_Component::Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo *pTile, uint16 worldVersion)
{
    TileExtra::Serialize(memBuffer, write);
    memBuffer.ReadWrite(randValue, write);
}

void TileExtra_Provider::Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo *pTile, uint16 worldVersion)
{
    TileExtra::Serialize(memBuffer, write);
    memBuffer.ReadWrite(readyTime, write);

    uint16 fgItem = pTile->GetFG();
    if(fgItem == ITEM_ID_WINTERFEST_CALENDAR_2017) {
        memBuffer.ReadWrite(itemID, write);
    }

    if(!database && (fgItem == ITEM_ID_WINTERFEST_CALENDAR_2018 || fgItem == ITEM_ID_WINTERFEST_CALENDAR_2019)) {
        memBuffer.ReadWrite(itemID, write);
    }

    if(!database && fgItem == ITEM_ID_BUILDING_BLOCKS_MACHINE) {
        // long
        // long
    }

    if(worldVersion > 9 &&
        (
            fgItem == ITEM_ID_KEENAN_GTS_AWESOME_ITEM_O_MATIC ||
            fgItem == ITEM_ID_VENOMSTS_AWESOME_ITEM_O_MATIC ||
            fgItem == ITEM_ID_LINKTRADER_GTS_AWESOME_ITEM_O_MATIC ||
            fgItem == ITEM_ID_TERYS_AWESOME_ITEM_O_MATIC ||
            fgItem == ITEM_ID_EVETS_GTS_AWESOME_ITEM_O_MATIC ||
            fgItem == ITEM_ID_BENBARRAGES_AWESOME_ITEM_O_MATIC ||
            fgItem == ITEM_ID_MRSONGO_GTS_AWESOME_ITEM_O_MATIC ||
            fgItem == ITEM_ID_OLDGERTIES_AWESOME_ITEM_O_MATIC ||
            fgItem == ITEM_ID_MACPROOF_GTS_AWESOME_ITEM_O_MATIC
        ))
    {
        // long
        // long
        // same as building blocks machine
    }
}

void TileExtra_SpiritStorage::Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo *pTile, uint16 worldVersion)
{
    TileExtra::Serialize(memBuffer, write);
    memBuffer.ReadWrite(spiritCount, write);
}

void TileExtra_Lab::Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo *pTile, uint16 worldVersion)
{
    TileExtra::Serialize(memBuffer, write);
    memBuffer.ReadWrite(userID, write);
    memBuffer.ReadWrite(achievementID, write);
}

void TileExtra_HeartMonitor::Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo *pTile, uint16 worldVersion)
{
    TileExtra::Serialize(memBuffer, write);
    memBuffer.ReadWrite(userID, write);
    memBuffer.ReadWriteString(playerDisplayName, write);
}
