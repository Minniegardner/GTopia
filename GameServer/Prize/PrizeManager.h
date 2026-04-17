#pragma once

#include "Precompiled.h"
#include "Item/ItemUtils.h"

struct PrizeDrop
{
    uint16 itemID = ITEM_ID_BLANK;
    uint8 amount = 1;
};

class PrizeManager {
public:
    static PrizeManager* GetInstance()
    {
        static PrizeManager instance;
        return &instance;
    }

public:
    bool Init();

    PrizeDrop GetAwkwardUnicornDrop();
    PrizeDrop GetGBCDrop(bool potion = false);
    PrizeDrop GetSGBCDrop(bool potion = false);
    PrizeDrop GetChemStationDrop();

private:
    PrizeManager() = default;

    uint16 PickByOdds(const std::vector<std::pair<uint16, double>>& table) const;

private:
    bool m_initialized = false;

    std::vector<std::pair<uint16, double>> m_awkwardUnicornDrops;
    std::vector<std::pair<uint16, double>> m_gbcDrops;
    std::vector<std::pair<uint16, double>> m_gbcDropsPotion;
    std::vector<std::pair<uint16, double>> m_sgbcDrops;
    std::vector<std::pair<uint16, double>> m_sgbcDropsPotion;
    std::vector<std::pair<uint16, double>> m_chemStationDrops;
};

PrizeManager* GetPrizeManager();
