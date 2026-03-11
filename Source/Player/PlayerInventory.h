#pragma once

#include "../Precompiled.h"
#include "../Item/ItemUtils.h"
#include "../Memory/MemoryBuffer.h"

#define INVENTORY_DEFAULT_CAPACITY 16

struct InventoryItemInfo
{
    uint16 id = 0;
    uint8 count = 0;
    uint8 flags = 0;

    void Serialize(MemoryBuffer& memBuffer, bool write);
};

enum eInventoryErrors
{
    INVENTORY_ERROR_UNKNOWN,
    INVENTORY_ERROR_CAPACITY,
    INVENTORY_ERROR_MAX_HOLD
};

class PlayerInventory {
public:
    PlayerInventory();

public:
    void Serialize(MemoryBuffer& memBuffer, bool write, bool database);
    InventoryItemInfo* GetItemByID(uint16 itemID);

    uint8 AddItem(uint16 itemID, uint8 count, eInventoryErrors& errorCode);
    uint8 RemoveItem(uint16 itemID, uint8 count);
    uint8 RemoveItem(uint16 itemID);

    void RemoveFromQuickSlots(uint16 itemID);

    uint32 GetMemEstimate(bool database);
    void SetVersion(uint32 protocol);

private:
    uint8 m_version;
    uint32 m_capacity;

    std::vector<InventoryItemInfo> m_items;
    uint16 m_clothes[BODY_PART_SIZE];
    uint16 m_quickSlots[4];
};