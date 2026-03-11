#include "PlayerInventory.h"
#include "../Item/ItemInfoManager.h"
#include "../IO/Log.h"

void InventoryItemInfo::Serialize(MemoryBuffer& memBuffer, bool write)
{
    memBuffer.ReadWrite(id, write);
    memBuffer.ReadWrite(count, write);
    memBuffer.ReadWrite(flags, write);
}

PlayerInventory::PlayerInventory()
: m_capacity(INVENTORY_DEFAULT_CAPACITY)
{
    for(uint8 i = 0; i < BODY_PART_SIZE; ++i) {
        m_clothes[i] = 0;
    }

    m_items.reserve(INVENTORY_DEFAULT_CAPACITY);

    m_items.emplace_back(InventoryItemInfo{ ITEM_ID_FIST, 1, 0 });
    m_items.emplace_back(InventoryItemInfo{ ITEM_ID_WRENCH, 1, 0 });

    m_quickSlots[0] = ITEM_ID_FIST;
}

void PlayerInventory::Serialize(MemoryBuffer& memBuffer, bool write, bool database)
{
    if(!database) {
        memBuffer.ReadWrite(m_version, write);
    }

    memBuffer.ReadWrite(m_capacity, write);

    uint16 invItemSize = m_items.size();
    if(database || m_version > 0) {
        memBuffer.ReadWrite(invItemSize, write);
    }
    else {
        uint8 tempInvSize = (uint8)invItemSize;
        memBuffer.ReadWrite(tempInvSize, write);

        if(!write) {
            invItemSize = tempInvSize;
        }
    }

    if(!write) {
        m_items.reserve(m_capacity);
    }

    if(m_version == 0 && !database && invItemSize > 255) {
        invItemSize = 250;
    }

    ItemInfoManager* pItemMgr = GetItemInfoManager();

    for(uint16 i = 0; i < invItemSize; ++i) {
        if(write) {
            m_items[i].Serialize(memBuffer, true);
        }
        else {
            InventoryItemInfo item;
            item.Serialize(memBuffer, write);
    
            if(IsIllegalItem(item.id) || item.id < 0) {
                LOGGER_LOG_WARN("Illegal item %d found in player inventory skipping adding", item.id);
                continue;
            }

            ItemInfo* pItem = pItemMgr->GetItemByID(item.id);
            if(!pItem) {
                continue;
            }

            if(pItem->type == ITEM_TYPE_CLOTHES && item.flags == 1) {
                m_clothes[pItem->bodyPart] = item.id;
            }
    
            m_items.push_back(std::move(item));
        }
    }
}

InventoryItemInfo* PlayerInventory::GetItemByID(uint16 itemID)
{
    for(auto& item : m_items) {
        if(item.id == itemID) {
            return &item;
        }
    }

    return nullptr;
}

uint8 PlayerInventory::AddItem(uint16 itemID, uint8 count, eInventoryErrors& errorCode)
{
    ItemInfo* pItemInfo = GetItemInfoManager()->GetItemByID(itemID);
    if(!pItemInfo) {
        return 0;
    }

    InventoryItemInfo* pSearchItem = GetItemByID(itemID);
    if(!pSearchItem) {
        if(m_items.size() + 1 > m_capacity) {
            return 0;
        }

        if(count > pItemInfo->maxCanHold) {
            count = pItemInfo->maxCanHold;
        }

        InventoryItemInfo item;
        item.id = itemID;
        item.count = count;
        item.flags = 0;

        m_items.push_back(std::move(item));
        return count;
    }

    if(pSearchItem->count + count > pItemInfo->maxCanHold) {
        return 0;
    }

    pSearchItem->count += count;
    return count;
}

uint8 PlayerInventory::RemoveItem(uint16 itemID, uint8 count)
{
    InventoryItemInfo* pItem = GetItemByID(itemID);
    if(!pItem) {
        return 0;
    }

    if(pItem->count - count <= 0) {
        return RemoveItem(itemID);
    }

    pItem->count -= count;
    return count;
}

uint8 PlayerInventory::RemoveItem(uint16 itemID)
{
    InventoryItemInfo* pItem = GetItemByID(itemID);
    if(!pItem) {
        return 0;
    }

    uint8 itemCount = pItem->count;
    if(pItem != &m_items.back()) {
        *pItem = std::move(m_items.back());
    }
    m_items.pop_back();

    RemoveFromQuickSlots(itemID);
    return itemCount;
}

void PlayerInventory::RemoveFromQuickSlots(uint16 itemID)
{
    for(uint8 i = 0; i < 4; ++i) {
        if(m_quickSlots[i] == itemID) {
            m_quickSlots[i] = 0;
        }
    }
}

uint32 PlayerInventory::GetMemEstimate(bool database)
{
    uint32 memEstimate = sizeof(m_capacity) + m_items.size() * sizeof(InventoryItemInfo);

    if(!database) {
        memEstimate += sizeof(m_version);
    }

    if(database || m_version > 0) {
        memEstimate += sizeof(uint16);
    }
    else {
        memEstimate += sizeof(uint8);
    }

    return memEstimate;
}

void PlayerInventory::SetVersion(uint32 protocol)
{
    /*if(protocol < 94) {
        m_version = 0;
    }
    else {
        m_version = 1;
    }*/

    m_version = 0;
}
