#pragma once

#include "../Precompiled.h"
#include "../Memory/MemoryBuffer.h"
#include "../Item/ItemUtils.h"

enum eTileExtraTypes 
{
    TILE_EXTRA_TYPE_NONE = 0,
    TILE_EXTRA_TYPE_DOOR = 1,
    TILE_EXTRA_TYPE_SIGN = 2,
    TILE_EXTRA_TYPE_LOCK = 3,
    TILE_EXTRA_TYPE_SEED = 4,
    TILE_EXTRA_TYPE_RACE_FLAG = 5,
    TILE_EXTRA_TYPE_MAILBOX = 6,
    TILE_EXTRA_TYPE_BULLETIN = 7,
    TILE_EXTRA_TYPE_COMPONENT = 8,
    TILE_EXTRA_TYPE_PROVIDER = 9,
    TILE_EXTRA_TYPE_LAB = 10,
    TILE_EXTRA_TYPE_HEART_MONITOR = 11,
    TILE_EXTRA_TYPE_DONATION_BOX = 12,
    TILE_EXTRA_TYPE_TOYBOX = 13,
    TILE_EXTRA_TYPE_MANNEQUIN = 14,
    TILE_EXTRA_TYPE_MAGIC_EGG = 15,
    TILE_EXTRA_TYPE_TEAM = 16,
    TILE_EXTRA_TYPE_GAME_GEN = 17,
    TILE_EXTRA_TYPE_XENONITE = 18,
    TILE_EXTRA_TYPE_DRESSUP = 19,
    TILE_EXTRA_TYPE_CRYSTAL = 20,
    TILE_EXTRA_TYPE_BURGLAR = 21,
    TILE_EXTRA_TYPE_SPOTLIGHT = 22,
    TILE_EXTRA_TYPE_DISPLAY_BLOCK = 23,
    TILE_EXTRA_TYPE_VENDING = 24,
    TILE_EXTRA_TYPE_FISH_TANK = 25,
    TILE_EXTRA_TYPE_SOLAR = 26,
    TILE_EXTRA_TYPE_FORGE = 27,
    TILE_EXTRA_TYPE_GIVING_TREE = 28,
    TILE_EXTRA_TYPE_GIVING_TREE_STUMP = 29,
    TILE_EXTRA_TYPE_STEAM_ORGAN = 30,
    TILE_EXTRA_TYPE_TAMAGOTCHI = 31,
    TILE_EXTRA_TYPE_SEWING = 32,
    TILE_EXTRA_TYPE_FLAG = 33,
    TILE_EXTRA_TYPE_LOBSTER_TRAP = 34,
    TILE_EXTRA_TYPE_ARTCANVAS = 35,
    TILE_EXTRA_TYPE_BATTLE_CAGE = 36,
    TILE_EXTRA_TYPE_PET_TRAINER = 37,
    TILE_EXTRA_TYPE_STEAM_ENGINE = 38,
    TILE_EXTRA_TYPE_LOCK_BOT = 39,
    TILE_EXTRA_TYPE_WEATHER_SPECIAL = 40,
    TILE_EXTRA_TYPE_SPIRIT_STORAGE = 41,
    TILE_EXTRA_TYPE_BEDROCK = 42,
    TILE_EXTRA_TYPE_DISPLAY_SHELF = 43,
    TILE_EXTRA_TYPE_VIP_DOOR = 44,
    TILE_EXTRA_TYPE_CHAL_TIMER = 45,
    TILE_EXTRA_TYPE_CHAL_FLAG = 46,
    TILE_EXTRA_TYPE_FISH_MOUNT = 47,
    TILE_EXTRA_TYPE_PORTRAIT = 48,
    TILE_EXTRA_TYPE_WEATHER_SPECIAL2 = 49,
    TILE_EXTRA_TYPE_FOSSIL_PREP = 50,
    TILE_EXTRA_TYPE_DNA_MACHINE = 51,
    TILE_EXTRA_TYPE_BLASTER = 52,
    TILE_EXTRA_TYPE_CHEMTANK = 53,
    TILE_EXTRA_TYPE_STORAGE = 54,
    TILE_EXTRA_TYPE_OVEN = 55,
    TILE_EXTRA_TYPE_SUPER_MUSIC = 56,
    TILE_EXTRA_TYPE_GEIGER_CHARGER = 57,
    TILE_EXTRA_TYPE_WEATHER_INFINITY = 58,
    TILE_EXTRA_TYPE_STATS_BLOCK = 59,
    TILE_EXTRA_TYPE_TESSERACT_MANIPULATOR = 60,
    TILE_EXTRA_TYPE_GAIA_HEART = 61,
    TILE_EXTRA_TYPE_MAGPLANT = 62,

    TILE_EXTRA_TYPE_SIZE
};

uint8 GetTileExtraType(uint8 itemType);
uint8 GetTileExtraTypeByItem(uint16 itemID, uint8 itemType);

class TileInfo;

class TileExtra {
public:
    explicit TileExtra(uint8 tileExtraType) : type(tileExtraType) {}
    virtual ~TileExtra() {};

    virtual void Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo* pTile, uint16 worldVersion) = 0;
    static TileExtra* Create(uint8 tileExtraType);

protected:
    virtual void Serialize(MemoryBuffer& memBuffer, bool write);

public:
    uint8 type = TILE_EXTRA_TYPE_NONE;
};

class TileExtra_Door : public TileExtra {
public:
    static constexpr uint8 TYPE = TILE_EXTRA_TYPE_DOOR;

    TileExtra_Door() : TileExtra(TYPE) {}

    string name;
    string text;
    string id;

protected:
    void Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo* pTile, uint16 worldVersion) override;
};

class TileExtra_Sign : public TileExtra {
public:
    static constexpr uint8 TYPE = TILE_EXTRA_TYPE_SIGN;

    TileExtra_Sign() : TileExtra(TYPE) {}

    string text;

protected:
    void Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo* pTile, uint16 worldVersion) override;
};

class TileExtra_Vending : public TileExtra {
public:
    static constexpr uint8 TYPE = TILE_EXTRA_TYPE_VENDING;

    TileExtra_Vending() : TileExtra(TYPE) {}

    uint16 itemID = ITEM_ID_BLANK;
    int32 price = 0;
    int32 stock = 0;
    int32 earnings = 0;

protected:
    void Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo* pTile, uint16 worldVersion) override;
};

class TileExtra_DisplayBlock : public TileExtra {
public:
    static constexpr uint8 TYPE = TILE_EXTRA_TYPE_DISPLAY_BLOCK;

    TileExtra_DisplayBlock() : TileExtra(TYPE) {}

    uint16 itemID = ITEM_ID_BLANK;

protected:
    void Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo* pTile, uint16 worldVersion) override;
};

class TileExtra_Crystal : public TileExtra {
public:
    static constexpr uint8 TYPE = TILE_EXTRA_TYPE_CRYSTAL;

    TileExtra_Crystal() : TileExtra(TYPE) {}

    uint32 red = 0;
    uint32 green = 0;
    uint32 blue = 0;
    uint32 white = 0;
    uint32 black = 0;

    uint32 GetTotal() const
    {
        return red + green + blue + white + black;
    }

protected:
    void Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo* pTile, uint16 worldVersion) override;
};

struct TileExtra_DonatedItem
{
    uint16 itemID = ITEM_ID_BLANK;
    uint8 amount = 0;
    uint32 userID = 0;
    string username;
    string comment;
    uint32 donateID = 0;
    uint64 donatedAt = 0;
};

class TileExtra_DonationBox : public TileExtra {
public:
    static constexpr uint8 TYPE = TILE_EXTRA_TYPE_DONATION_BOX;

    TileExtra_DonationBox() : TileExtra(TYPE) {}

    uint32 currentDonateID = 0;
    std::vector<TileExtra_DonatedItem> donatedItems;

protected:
    void Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo* pTile, uint16 worldVersion) override;
};

struct TileExtra_MannequinClothing
{
    uint16 hair = ITEM_ID_BLANK;
    uint16 shirt = ITEM_ID_BLANK;
    uint16 pants = ITEM_ID_BLANK;
    uint16 shoes = ITEM_ID_BLANK;
    uint16 face = ITEM_ID_BLANK;
    uint16 hand = ITEM_ID_BLANK;
    uint16 back = ITEM_ID_BLANK;
    uint16 hat = ITEM_ID_BLANK;
    uint16 necklace = ITEM_ID_BLANK;
};

class TileExtra_Mannequin : public TileExtra {
public:
    static constexpr uint8 TYPE = TILE_EXTRA_TYPE_MANNEQUIN;

    TileExtra_Mannequin() : TileExtra(TYPE) {}

    string label;
    uint8 hairColorA = 0;
    uint8 hairColorR = 0;
    uint8 hairColorG = 0;
    uint8 hairColorB = 0;
    uint8 unknown = 223;
    TileExtra_MannequinClothing clothing;

protected:
    void Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo* pTile, uint16 worldVersion) override;
};

class TileExtra_Magplant : public TileExtra {
public:
    static constexpr uint8 TYPE = TILE_EXTRA_TYPE_MAGPLANT;

    TileExtra_Magplant() : TileExtra(TYPE) {}

    uint16 itemID = ITEM_ID_BLANK;
    int32 itemCount = 0;
    int32 itemLimit = 0;
    bool magnetic = false;
    bool remote = false;

protected:
    void Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo* pTile, uint16 worldVersion) override;
};

enum class ChemsynthColor : int32
{
    NONE = 0,
    RED = ITEM_ID_CHEMICAL_R,
    YELLOW = ITEM_ID_CHEMICAL_Y,
    GREEN = ITEM_ID_CHEMICAL_G,
    BLUE = ITEM_ID_CHEMICAL_B,
    PINK = ITEM_ID_CHEMICAL_P
};

class TileExtra_Chemsynth : public TileExtra {
public:
    static constexpr uint8 TYPE = TILE_EXTRA_TYPE_CHEMTANK;

    TileExtra_Chemsynth() : TileExtra(TYPE) {}

    ChemsynthColor color = ChemsynthColor::NONE;
    ChemsynthColor targetColor = ChemsynthColor::NONE;

protected:
    void Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo* pTile, uint16 worldVersion) override;
};

enum eTileExtraLockFlag
{
    TILE_EXTRA_LOCK_FLAG_IGNORE_AIR = 1 << 0,
    TILE_EXTRA_LOCK_FLAG_AIR_ONLY = 1 << 1,
    TILE_EXTRA_LOCK_FLAG_RESTRICT_ADMINS = 1 << 2,
    TILE_EXTRA_LOCK_FLAG_BUILDING_ONLY = 1 << 3
};

class TileExtra_Lock : public TileExtra {
public:
    static constexpr uint8 TYPE = TILE_EXTRA_TYPE_LOCK;

    TileExtra_Lock() : TileExtra(TYPE) {}

    uint8 flags = 0;
    int32 ownerID = -1;
    std::vector<int32> accessList; // includes tempo
    int32 minEntryLevel = 0;
    int32 worldTimer = 0;

    void SetFlag(uint8 flag) { flags |= flag; }
    void RemoveFlag(uint8 flag) { flags &= ~flag; }
    bool HasFlag(uint8 flag) { return flags & flag; };

    int32 GetTempo()
    {
        for(auto& id : accessList) {
            if(id < 0) {
                return -id; // positive
            }
        }

        return -1;
    }

    void SetTempo(uint32 tempo)
    {
        for(auto& id : accessList) {
            if(id < 0) {
                id = -tempo;
                return;
            }
        }

        accessList.push_back(-tempo);
    }

    bool HasAccess(int32 userID)
    {
        if(ownerID == userID) {
            return true;
        }

        for(auto& id : accessList) {
            if(id == userID) {
                return true;
            }
        }

        return false;
    }

    bool IsAdmin(int32 userID)
    {
        for(auto& id : accessList) {
            if(id == userID && ownerID != userID) {
                return true;
            }
        }

        return false;
    }

protected:
    void Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo* pTile, uint16 worldVersion) override;
};

class TileExtra_Seed : public TileExtra {
public:
    static constexpr uint8 TYPE = TILE_EXTRA_TYPE_SEED;

    TileExtra_Seed() : TileExtra(TYPE) {}

    uint32 growTime = 0;
    uint8 fruitCount = 3;

protected:
    void Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo* pTile, uint16 worldVersion) override;
};

class TileExtra_SpiritStorage : public TileExtra {
public:
    static constexpr uint8 TYPE = TILE_EXTRA_TYPE_SPIRIT_STORAGE;

    TileExtra_SpiritStorage() : TileExtra(TYPE) {}

    int32 spiritCount = 0;

protected:
    void Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo* pTile, uint16 worldVersion) override;
};

/**
 * 
 * 
 * 
 * 
 */

class TileExtra_Component : public TileExtra {
public:
    static constexpr uint8 TYPE = TILE_EXTRA_TYPE_COMPONENT;

    TileExtra_Component() : TileExtra(TYPE) {}

    uint8 randValue = 0;

protected:
    void Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo* pTile, uint16 worldVersion) override;
};

class TileExtra_Provider : public TileExtra {
public:
    static constexpr uint8 TYPE = TILE_EXTRA_TYPE_PROVIDER;

    TileExtra_Provider() : TileExtra(TYPE) {}

    uint32 readyTime = 0;
    int32 itemID = 0;

protected:
    void Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo* pTile, uint16 worldVersion) override;
};

class TileExtra_Lab : public TileExtra {
public:
    static constexpr uint8 TYPE = TILE_EXTRA_TYPE_LAB;

    TileExtra_Lab() : TileExtra(TYPE) {}

    int32 userID = -1;
    uint8 achievementID = 0;

protected:
    void Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo* pTile, uint16 worldVersion) override;
};

class TileExtra_HeartMonitor : public TileExtra {
public:
    static constexpr uint8 TYPE = TILE_EXTRA_TYPE_HEART_MONITOR;

    TileExtra_HeartMonitor() : TileExtra(TYPE) {}

    int32 userID = -1;
    string playerDisplayName;

protected:
    void Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo* pTile, uint16 worldVersion) override;
};