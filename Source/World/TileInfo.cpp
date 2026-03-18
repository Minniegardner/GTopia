#include "TileInfo.h"
#include "../Item/ItemInfoManager.h"
#include "../IO/Log.h"
#include "WorldTileManager.h"

TileInfo::TileInfo()
: m_pExtraData(nullptr), m_fg(0), m_bg(0), m_parent(0), m_flags(0), m_damage(0)
{
}

TileInfo::~TileInfo()
{
    SAFE_DELETE(m_pExtraData)
}

void TileInfo::Serialize(MemoryBuffer& memBuffer, bool write, bool database)
{
    memBuffer.ReadWrite(m_fg, write);
    memBuffer.ReadWrite(m_bg, write);
    memBuffer.ReadWrite(m_parent, write);
    memBuffer.ReadWrite(m_flags, write);

    if(HasFlag(TILE_FLAG_HAS_PARENT)) {
        memBuffer.Seek(memBuffer.GetOffset() + 2);
    }

    if(HasFlag(TILE_FLAG_HAS_EXTRA_DATA)) {
        ItemInfo* pItem = GetItemInfoManager()->GetItemByID(m_fg);
        if(!pItem) {
            return;
        }

        if(!write) {
            m_pExtraData = new TileExtra();
            if(m_pExtraData->Setup(pItem->type)) {
                m_pExtraData->Serialize(memBuffer, write, database, this);
            }
        }
        else {
            m_pExtraData->Serialize(memBuffer, write, database, this);
        }
    }
}

void TileInfo::SetFG(uint16 itemID, WorldTileManager* pTileMgr)
{
    if(!pTileMgr) {
        return;
    }

    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(itemID);
    if(!pItem) {
        return;
    }

    if(m_pExtraData) {
        SAFE_DELETE(m_pExtraData);
        RemoveFlag(TILE_FLAG_HAS_EXTRA_DATA);
    }

    if(itemID == ITEM_ID_BLANK) {
        m_lastDamageTime.Reset();
        m_damage = 0;

        if(m_fg == ITEM_ID_GUARDIAN_PINEAPPLE) {
            pTileMgr->SetGuardPineappleTile(nullptr);
        }
        // else if
        else {
            ItemInfo* pBrokenItem = GetItemInfoManager()->GetItemByID(m_fg);
            // really need to get it from here?
    
            if(pBrokenItem->IsMainDoor()) {
                pTileMgr->SetMainDoorTile(nullptr);
            }
        }

        m_fg = itemID;
        return;
    }

    if(pItem->id == ITEM_ID_GUARDIAN_PINEAPPLE) {
        pTileMgr->SetGuardPineappleTile(this);
    }

    if(TileExtra::HasExtra(pItem->type)) {
        SetFlag(TILE_FLAG_HAS_EXTRA_DATA);
        
        m_pExtraData = new TileExtra();
        m_pExtraData->Setup(pItem->type);
    }

    if(pItem->IsMainDoor()) {
        pTileMgr->SetMainDoorTile(this);
    }

    m_fg = itemID;
}

void TileInfo::SetBG(uint16 itemID)
{
    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(itemID);
    if(!pItem && !pItem->IsBackground() && itemID != ITEM_ID_BLANK) {
        return;
    }

    if(itemID == ITEM_ID_BLANK) {
        m_lastDamageTime.Reset();
        m_damage = 0;
    }

    m_bg = itemID;
}

void TileInfo::PunchTile(uint8 damage)
{
    uint16 itemToDamage = GetDisplayedItem();

    if(itemToDamage == ITEM_ID_BLANK) {
        LOGGER_LOG_WARN("Mate we are damaging nothingness?");
        return;
    }
    
    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(itemToDamage);
    if(!pItem) {
        return;
    }

    if(m_lastDamageTime.GetElapsedTime() >= pItem->restoreTime * 1000) {
        m_lastDamageTime.Reset();
        m_damage = 0;
    }

    if(m_damage + damage < pItem->hp) {
        m_damage += damage;
    }
    else {
        m_damage = pItem->hp;
    }

    m_lastDamageTime.Reset();

    if(
        pItem->type == ITEM_TYPE_RACE_FLAG || pItem->type == ITEM_TYPE_SWITCHEROO ||
        (pItem->type == ITEM_TYPE_DEADLY_IF_ON && pItem->id != ITEM_ID_STEAM_SPIKES)
    ) {
        ToggleFlag(TILE_FLAG_IS_ON);
    }
}

float TileInfo::GetHealthPercent()
{
    uint16 itemToDamage = GetDisplayedItem();

    if(itemToDamage == ITEM_ID_BLANK) {
        return 1.0f;
    }
    
    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(itemToDamage);
    if(!pItem) {
        return 1.0f;
    }

    if(m_lastDamageTime.GetElapsedTime() >= pItem->restoreTime * 1000) {
        m_lastDamageTime.Reset();
        m_damage = 0;

        return 1.0f;
    }
    
    return 1.0f - (m_damage / pItem->hp);
}

uint16 TileInfo::GetDisplayedItem()
{
    return m_fg != ITEM_ID_BLANK ? m_fg : m_bg;
}

uint32 TileInfo::GetMemEstimate()
{
    uint32 memSize = 0;
    memSize += sizeof(m_fg);
    memSize += sizeof(m_bg);
    memSize += sizeof(m_parent);
    memSize += sizeof(m_flags);

    if(HasFlag(TILE_FLAG_HAS_PARENT)) {
        memSize += 2;
    }

    /**
     * pre-calc size for extras?
     */
    return memSize;
}
