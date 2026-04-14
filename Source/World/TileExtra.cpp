#include "TileExtra.h"
#include "TileInfo.h"
#include "Item/ItemUtils.h"
#include "Item/LockAlgorithm.h"
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
            return TILE_EXTRA_TYPE_DOOR;

        case ITEM_TYPE_SIGN:
            return TILE_EXTRA_TYPE_SIGN;

        case ITEM_TYPE_MAILBOX:
            return TILE_EXTRA_TYPE_MAILBOX;

        case ITEM_TYPE_BULLETIN:
            return TILE_EXTRA_TYPE_BULLETIN;

        case ITEM_TYPE_LOCK:
            return TILE_EXTRA_TYPE_LOCK;

        case ITEM_TYPE_SEED:
            return TILE_EXTRA_TYPE_SEED;

        case ITEM_TYPE_PROVIDER:
            return TILE_EXTRA_TYPE_PROVIDER;

        case ITEM_TYPE_COMPONENT:
            return TILE_EXTRA_TYPE_COMPONENT;

        case ITEM_TYPE_DONATION_BOX:
            return TILE_EXTRA_TYPE_DONATION_BOX;

        case ITEM_TYPE_MANNEQUIN:
            return TILE_EXTRA_TYPE_MANNEQUIN;

        case ITEM_TYPE_DISPLAY_BLOCK:
            return TILE_EXTRA_TYPE_DISPLAY_BLOCK;

        case ITEM_TYPE_FLAG:
            return TILE_EXTRA_TYPE_FLAG;

        case ITEM_TYPE_ARTCANVAS:
            return TILE_EXTRA_TYPE_ARTCANVAS;

        case ITEM_TYPE_SPIRIT_STORAGE:
            return TILE_EXTRA_TYPE_SPIRIT_STORAGE;

        case ITEM_TYPE_DISPLAY_SHELF:
            return TILE_EXTRA_TYPE_DISPLAY_SHELF;

        case ITEM_TYPE_WEATHER_SPECIAL:
            return TILE_EXTRA_TYPE_WEATHER_SPECIAL;

        case ITEM_TYPE_WEATHER_SPECIAL2:
            return TILE_EXTRA_TYPE_WEATHER_SPECIAL2;

        case ITEM_TYPE_SUPER_MUSIC:
            return TILE_EXTRA_TYPE_SUPER_MUSIC;

        case ITEM_TYPE_HEART_MONITOR:
            return TILE_EXTRA_TYPE_HEART_MONITOR;

        case ITEM_TYPE_VENDING:
            return TILE_EXTRA_TYPE_VENDING;

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

        case TILE_EXTRA_TYPE_MAILBOX:
            return new TileExtra_Mailbox();

        case TILE_EXTRA_TYPE_BULLETIN:
            return new TileExtra_Bulletin();

        case TILE_EXTRA_TYPE_VENDING:
            return new TileExtra_Vending();

        case TILE_EXTRA_TYPE_MAGPLANT:
            return new TileExtra_Magplant();

        case TILE_EXTRA_TYPE_CHEMTANK:
            return new TileExtra_Chemsynth();

        case TILE_EXTRA_TYPE_DONATION_BOX:
            return new TileExtra_DonationBox();

        case TILE_EXTRA_TYPE_MANNEQUIN:
            return new TileExtra_Mannequin();

        case TILE_EXTRA_TYPE_DISPLAY_BLOCK:
            return new TileExtra_DisplayBlock();

        case TILE_EXTRA_TYPE_DISPLAY_SHELF:
            return new TileExtra_DisplayShelf();

        case TILE_EXTRA_TYPE_WEATHER_SPECIAL:
            return new TileExtra_WeatherSpecial();

        case TILE_EXTRA_TYPE_WEATHER_SPECIAL2:
            return new TileExtra_WeatherSpecial2();

        case TILE_EXTRA_TYPE_WEATHER_INFINITY:
            return new TileExtra_WeatherInfinity();

        case TILE_EXTRA_TYPE_SUPER_MUSIC:
            return new TileExtra_SuperMusic();

        case TILE_EXTRA_TYPE_FLAG:
            return new TileExtra_Flag();

        case TILE_EXTRA_TYPE_ARTCANVAS:
            return new TileExtra_ArtCanvas();

        case TILE_EXTRA_TYPE_LOCK:
            return new TileExtra_Lock(); 

        case TILE_EXTRA_TYPE_SEED:
            return new TileExtra_Seed();

        case TILE_EXTRA_TYPE_COMPONENT:
            return new TileExtra_Component();

        case TILE_EXTRA_TYPE_PROVIDER:
            return new TileExtra_Provider();

        case TILE_EXTRA_TYPE_SPIRIT_STORAGE:
            return new TileExtra_SpiritStorage();

        case TILE_EXTRA_TYPE_HEART_MONITOR:
            return new TileExtra_HeartMonitor();

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

    string packetName = name;
    if(write && !database && packetName.empty()) {
        packetName = text;
    }

    if(write && !database) {
        memBuffer.ReadWriteString(packetName, true);
    }
    else {
        memBuffer.ReadWriteString(name, write);
    }

    if(database) {
        memBuffer.ReadWriteString(text, write);
        memBuffer.ReadWriteString(id, write);
    }

    uint8 doorFlags = 0;
    if(pTile && pTile->HasFlag(TILE_FLAG_IS_OPEN_TO_PUBLIC)) {
        doorFlags |= 1;
    }

    memBuffer.ReadWrite(doorFlags, write);

    if(database) {
        string password;
        memBuffer.ReadWriteString(password, write);
    }
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

    memBuffer.ReadWrite(itemID, write);
    memBuffer.ReadWrite(packetPrice, write);
}

void TileExtra_Magplant::Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo* pTile, uint16 worldVersion)
{
    TileExtra::Serialize(memBuffer, write);
    memBuffer.ReadWrite(itemID, write);
    memBuffer.ReadWrite(itemCount, write);

    uint8 magneticFlag = magnetic ? 1 : 0;
    uint8 remoteFlag = remote ? 1 : 0;
    memBuffer.ReadWrite(magneticFlag, write);
    memBuffer.ReadWrite(remoteFlag, write);
    memBuffer.ReadWrite(itemLimit, write);

    if(!write) {
        magnetic = magneticFlag != 0;
        remote = remoteFlag != 0;
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

void TileExtra_Lock::Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo *pTile, uint16 worldVersion)
{
    TileExtra::Serialize(memBuffer, write);

    if(!database) {
        memBuffer.ReadWrite(flags, write);
        memBuffer.ReadWrite(ownerID, write);

        uint32 accessSize = 0;
        if(write) {
            for(int32 id : accessList) {
                if(id >= 0) {
                    ++accessSize;
                }
            }
        }

        memBuffer.ReadWrite(accessSize, write);

        if(write) {
            for(int32 id : accessList) {
                if(id >= 0) {
                    memBuffer.ReadWrite(id, true);
                }
            }
        }
        else {
            accessList.clear();
            for(uint32 i = 0; i < accessSize; ++i) {
                int32 id = 0;
                memBuffer.ReadWrite(id, false);
                accessList.push_back(id);
            }
        }

        if(pTile && IsWorldLock(pTile->GetDisplayedItem())) {
            int32 tempo = GetTempo();
            memBuffer.ReadWrite(tempo, write);
            if(!write) {
                SetTempo((uint32)std::abs(tempo));
            }
        }

        memBuffer.ReadWrite(minEntryLevel, write);
        memBuffer.ReadWrite(worldTimer, write);
        return;
    }

    bool airOnly = HasFlag(TILE_EXTRA_LOCK_FLAG_AIR_ONLY);
    bool blocksOnly = HasFlag(TILE_EXTRA_LOCK_FLAG_BUILDING_ONLY);
    memBuffer.ReadWrite(airOnly, write);
    memBuffer.ReadWrite(blocksOnly, write);

    memBuffer.ReadWrite(flags, write);
    memBuffer.ReadWrite(ownerID, write);

    uint32 accessSize = (uint32)accessList.size();
    memBuffer.ReadWrite(accessSize, write);

    if(!write) {
        accessList.resize(accessSize);
    }

    if(accessSize > 0) {
        memBuffer.ReadWriteRaw(accessList.data(), accessSize * sizeof(int32), write);
    }

    if(pTile && IsWorldLock(pTile->GetDisplayedItem())) {
        int32 tempo = GetTempo();
        memBuffer.ReadWrite(tempo, write);
        if(!write) {
            SetTempo((uint32)std::abs(tempo));
        }
    }

    memBuffer.ReadWrite(minEntryLevel, write);
    memBuffer.ReadWrite(worldTimer, write);
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
        int64 unk0 = 0;
        int64 unk1 = 0;
        memBuffer.ReadWrite(unk0, write);
        memBuffer.ReadWrite(unk1, write);
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
        int64 unk0 = 0;
        int64 unk1 = 0;
        memBuffer.ReadWrite(unk0, write);
        memBuffer.ReadWrite(unk1, write);
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

void TileExtra_DonationBox::Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo *pTile, uint16 worldVersion)
{
    if(!database) {
        uint8 packetType = TILE_EXTRA_TYPE_MAGIC_EGG;
        memBuffer.ReadWrite(packetType, write);

        int32 zero = 0;
        memBuffer.ReadWrite(zero, write);
        return;
    }

    TileExtra::Serialize(memBuffer, write);

    uint32 count = (uint32)donatedItems.size();
    memBuffer.ReadWrite(count, write);

    if(!write) {
        donatedItems.resize(count);
    }

    for(uint32 i = 0; i < count; ++i) {
        memBuffer.ReadWrite(donatedItems[i].itemID, write);
        memBuffer.ReadWrite(donatedItems[i].amount, write);
        memBuffer.ReadWrite(donatedItems[i].userID, write);
        memBuffer.ReadWriteString(donatedItems[i].username, write);
        memBuffer.ReadWriteString(donatedItems[i].comment, write);
        memBuffer.ReadWrite(donatedItems[i].donateID, write);
        memBuffer.ReadWrite(donatedItems[i].donatedAt, write);
    }

    memBuffer.ReadWrite(currentDonateID, write);
}

void TileExtra_Mailbox::Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo *pTile, uint16 worldVersion)
{
    if(!database) {
        uint8 packetType = TILE_EXTRA_TYPE_MAGIC_EGG;
        memBuffer.ReadWrite(packetType, write);

        int32 zero = 0;
        memBuffer.ReadWrite(zero, write);
        return;
    }

    TileExtra::Serialize(memBuffer, write);

    uint32 count = (uint32)mailItems.size();
    memBuffer.ReadWrite(count, write);

    if(!write) {
        mailItems.resize(count);
    }

    for(uint32 i = 0; i < count; ++i) {
        memBuffer.ReadWrite(mailItems[i].userID, write);
        memBuffer.ReadWriteString(mailItems[i].username, write);
        memBuffer.ReadWriteString(mailItems[i].comment, write);
        memBuffer.ReadWrite(mailItems[i].mailID, write);
        memBuffer.ReadWrite(mailItems[i].postedAt, write);
    }

    memBuffer.ReadWrite(currentMailID, write);
    memBuffer.ReadWrite(isPublic, write);
    memBuffer.ReadWrite(hideNames, write);
}

void TileExtra_Mannequin::Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo *pTile, uint16 worldVersion)
{
    TileExtra::Serialize(memBuffer, write);

    memBuffer.ReadWriteString(label, write);
    memBuffer.ReadWrite(hairA, write);
    memBuffer.ReadWrite(hairR, write);
    memBuffer.ReadWrite(hairG, write);
    memBuffer.ReadWrite(hairB, write);
    memBuffer.ReadWrite(unk, write);

    memBuffer.ReadWrite(hair, write);
    memBuffer.ReadWrite(shirt, write);
    memBuffer.ReadWrite(pants, write);
    memBuffer.ReadWrite(shoes, write);
    memBuffer.ReadWrite(face, write);
    memBuffer.ReadWrite(hand, write);
    memBuffer.ReadWrite(back, write);
    memBuffer.ReadWrite(hat, write);
    memBuffer.ReadWrite(necklace, write);
}

void TileExtra_DisplayBlock::Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo *pTile, uint16 worldVersion)
{
    TileExtra::Serialize(memBuffer, write);
    memBuffer.ReadWrite(itemID, write);
}

void TileExtra_DisplayShelf::Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo *pTile, uint16 worldVersion)
{
    TileExtra::Serialize(memBuffer, write);

    // Keep Growtopia order for compatibility.
    memBuffer.ReadWrite(item2, write);
    memBuffer.ReadWrite(item1, write);
    memBuffer.ReadWrite(item3, write);
    memBuffer.ReadWrite(item4, write);
}

void TileExtra_WeatherSpecial::Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo *pTile, uint16 worldVersion)
{
    TileExtra::Serialize(memBuffer, write);
    memBuffer.ReadWrite(itemOrColor, write);
}

void TileExtra_WeatherSpecial2::Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo *pTile, uint16 worldVersion)
{
    TileExtra::Serialize(memBuffer, write);

    memBuffer.ReadWrite(itemID, write);
    memBuffer.ReadWrite(gravity, write);
    memBuffer.ReadWrite(stuffFlagsOrCycle, write);

    if(database) {
        memBuffer.ReadWrite(epochFlags, write);
    }
}

void TileExtra_WeatherInfinity::Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo *pTile, uint16 worldVersion)
{
    TileExtra::Serialize(memBuffer, write);

    memBuffer.ReadWrite(cycleTimer, write);

    uint32 count = (uint32)weathers.size();
    memBuffer.ReadWrite(count, write);

    if(!write) {
        weathers.resize(count);
    }

    for(uint32 i = 0; i < count; ++i) {
        memBuffer.ReadWrite(weathers[i], write);
    }
}

void TileExtra_SuperMusic::Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo *pTile, uint16 worldVersion)
{
    TileExtra::Serialize(memBuffer, write);
    memBuffer.ReadWriteString(notes, write);
    memBuffer.ReadWrite(volume, write);
}

void TileExtra_Flag::Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo *pTile, uint16 worldVersion)
{
    TileExtra::Serialize(memBuffer, write);
    memBuffer.ReadWriteString(country, write);
}

void TileExtra_ArtCanvas::Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo *pTile, uint16 worldVersion)
{
    TileExtra::Serialize(memBuffer, write);
    memBuffer.ReadWrite(itemID, write);
    memBuffer.ReadWriteString(label, write);
}
