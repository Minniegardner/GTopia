#include "TileInfo.h"
#include "../Item/ItemInfoManager.h"
#include "../IO/Log.h"
#include "WorldTileManager.h"
#include "WorldInfo.h"
#include "Utils/Timer.h"

namespace {

void InitializeMagplantExtra(TileExtra_Magplant* pMagplant, uint16 itemID)
{
    if(!pMagplant) {
        return;
    }

    pMagplant->itemID = ITEM_ID_BLANK;
    pMagplant->itemCount = 0;
    pMagplant->magnetic = false;
    pMagplant->remote = false;

    if(itemID == ITEM_ID_MAGPLANT_5000) {
        pMagplant->itemLimit = 5000;
    }
    else if(itemID == ITEM_ID_UNSTABLE_TESSERACT || itemID == ITEM_ID_GAIAS_BEACON) {
        pMagplant->itemLimit = 1500;
    }
    else {
        pMagplant->itemLimit = 0;
    }
}

void InitializeCrystalExtra(TileExtra_Crystal* pCrystal, uint16 itemID)
{
    if(!pCrystal) {
        return;
    }

    pCrystal->red = 0;
    pCrystal->green = 0;
    pCrystal->blue = 0;
    pCrystal->white = 0;
    pCrystal->black = 0;

    switch(itemID) {
        case ITEM_ID_RED_CRYSTAL: pCrystal->red = 1; break;
        case ITEM_ID_GREEN_CRYSTAL: pCrystal->green = 1; break;
        case ITEM_ID_BLUE_CRYSTAL: pCrystal->blue = 1; break;
        case ITEM_ID_WHITE_CRYSTAL: pCrystal->white = 1; break;
        case ITEM_ID_BLACK_CRYSTAL: pCrystal->black = 1; break;
        default: break;
    }
}

void InitializeMannequinExtra(TileExtra_Mannequin* pMannequin)
{
    if(!pMannequin) {
        return;
    }

    pMannequin->label.clear();
    pMannequin->hairColorA = 0;
    pMannequin->hairColorR = 0;
    pMannequin->hairColorG = 0;
    pMannequin->hairColorB = 0;
    pMannequin->unknown = 223;
    pMannequin->clothing = TileExtra_MannequinClothing{};
}

void InitializeDonationBoxExtra(TileExtra_DonationBox* pDonation)
{
    if(!pDonation) {
        return;
    }

    pDonation->currentDonateID = 0;
    pDonation->donatedItems.clear();
}

}

TileInfo::TileInfo()
: m_pExtraData(nullptr), m_fg(0), m_bg(0), m_parent(0), m_flags(0), m_damage(0)
{
}

TileInfo::~TileInfo()
{
    SAFE_DELETE(m_pExtraData)
}

void TileInfo::Serialize(MemoryBuffer& memBuffer, bool write, bool database, WorldInfo* pWorld)
{
    memBuffer.ReadWrite(m_fg, write);
    memBuffer.ReadWrite(m_bg, write);
    memBuffer.ReadWrite(m_parent, write);
    memBuffer.ReadWrite(m_flags, write);

    if(HasFlag(TILE_FLAG_HAS_PARENT)) {
        memBuffer.ReadWrite(m_parent, write); // ye its like that
    }

    if(HasFlag(TILE_FLAG_HAS_EXTRA_DATA)) {
        if(write) {
            if(!m_pExtraData) {
                ItemInfo* pItem = GetItemInfoManager()->GetItemByID(m_fg);
                if(!pItem) {
                    LOGGER_LOG_ERROR("Tile flagged with extra data but extra data is NULL? fg:%d", m_fg);
                    return;
                }

                uint8 tileExtraType = GetTileExtraTypeByItem(m_fg, pItem->type);
                if(tileExtraType == TILE_EXTRA_TYPE_NONE) {
                    LOGGER_LOG_ERROR("Tile flagged with extra data but type is invalid? fg:%d type:%d", m_fg, pItem->type);
                    return;
                }

                m_pExtraData = TileExtra::Create(tileExtraType);
                if(TileExtra_Magplant* pMagplant = GetExtra<TileExtra_Magplant>()) {
                    InitializeMagplantExtra(pMagplant, m_fg);
                }
            }
            
            m_pExtraData->Serialize(memBuffer, true, database, this, pWorld->GetWorldVersion());
        }
        else {
            ItemInfo* pItem = GetItemInfoManager()->GetItemByID(m_fg);
            if(!pItem) {
                return;
            }

            uint8 tileExtraType = GetTileExtraTypeByItem(m_fg, pItem->type);
            if(tileExtraType != TILE_EXTRA_TYPE_NONE) {
                m_pExtraData = TileExtra::Create(tileExtraType);

                if(m_pExtraData) {
                    m_pExtraData->Serialize(memBuffer, false, database, this, pWorld->GetWorldVersion());
                }
            }
        }
    }
}

void TileInfo::SwapContent(TileInfo& other)
{
    std::swap(m_fg, other.m_fg);
    std::swap(m_bg, other.m_bg);
    std::swap(m_parent, other.m_parent);
    std::swap(m_flags, other.m_flags);
    std::swap(m_damage, other.m_damage);
    std::swap(m_lastDamageTime, other.m_lastDamageTime);
    std::swap(m_pExtraData, other.m_pExtraData);
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

        pTileMgr->ModifyKeyTile(this, true);
        m_fg = itemID;
        return;
    }

    uint8 tileExtraType = GetTileExtraTypeByItem(itemID, pItem->type);
    if(tileExtraType != TILE_EXTRA_TYPE_NONE) {
        SetFlag(TILE_FLAG_HAS_EXTRA_DATA);

        m_pExtraData = TileExtra::Create(tileExtraType);

        if(TileExtra_Chemsynth* pChemsynth = GetExtra<TileExtra_Chemsynth>()) {
            pChemsynth->color = ChemsynthColor::NONE;
            pChemsynth->targetColor = ChemsynthColor::NONE;
        }

        if(TileExtra_Lab* pLab = GetExtra<TileExtra_Lab>()) {
            pLab->userID = 0;
            pLab->achievementID = 0;
        }

        if(TileExtra_HeartMonitor* pHeartMonitor = GetExtra<TileExtra_HeartMonitor>()) {
            pHeartMonitor->userID = 0;
            pHeartMonitor->playerDisplayName.clear();
        }

        if(TileExtra_Magplant* pMagplant = GetExtra<TileExtra_Magplant>()) {
            InitializeMagplantExtra(pMagplant, itemID);
        }

        if(TileExtra_Crystal* pCrystal = GetExtra<TileExtra_Crystal>()) {
            InitializeCrystalExtra(pCrystal, itemID);
        }

        if(TileExtra_Mannequin* pMannequin = GetExtra<TileExtra_Mannequin>()) {
            InitializeMannequinExtra(pMannequin);
        }

        if(TileExtra_DonationBox* pDonation = GetExtra<TileExtra_DonationBox>()) {
            InitializeDonationBoxExtra(pDonation);
        }

        if(TileExtra_Provider* pProvider = GetExtra<TileExtra_Provider>()) {
            pProvider->readyTime = (uint32)Time::GetSystemTime();
        }
    }

    m_fg = itemID;
    pTileMgr->ModifyKeyTile(this, false);
}

void TileInfo::SetBG(uint16 itemID)
{
    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(itemID);
    if(!pItem) {
        return;
    }

    if(!pItem->IsBackground() && itemID != ITEM_ID_BLANK) {
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