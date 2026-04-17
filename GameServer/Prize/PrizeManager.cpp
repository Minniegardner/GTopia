#include "PrizeManager.h"

#include "Math/Random.h"

namespace {

void AddOdds(std::vector<std::pair<uint16, double>>& table, uint16 itemID, double odds)
{
    if(itemID == ITEM_ID_BLANK || odds <= 0.0) {
        return;
    }

    table.emplace_back(itemID, odds);
}

void AddOddsFromRefStyle(std::vector<std::pair<uint16, double>>& table, uint16 itemID, double refOdds)
{
    if(refOdds <= 0.0) {
        return;
    }

    // Reference odds are inverse-probability style; convert to a positive weight.
    AddOdds(table, itemID, 1.0 / refOdds);
}

}

PrizeManager* GetPrizeManager()
{
    return PrizeManager::GetInstance();
}

bool PrizeManager::Init()
{
    if(m_initialized) {
        return true;
    }

    m_awkwardUnicornDrops.clear();
    m_gbcDrops.clear();
    m_gbcDropsPotion.clear();
    m_sgbcDrops.clear();
    m_sgbcDropsPotion.clear();

    AddOddsFromRefStyle(m_awkwardUnicornDrops, ITEM_ID_CLOUDS, 6);
    AddOddsFromRefStyle(m_awkwardUnicornDrops, ITEM_ID_DREAMSTONE_BLOCK, 6);
    AddOddsFromRefStyle(m_awkwardUnicornDrops, ITEM_ID_RAINBOW_BLOCK, 6);
    AddOddsFromRefStyle(m_awkwardUnicornDrops, ITEM_ID_ENCHANTED_SPATULA, 6);
    AddOddsFromRefStyle(m_awkwardUnicornDrops, ITEM_ID_HAPPY_UNICORN_BLOCK, 15);
    AddOddsFromRefStyle(m_awkwardUnicornDrops, ITEM_ID_ANGRY_UNICORN_BLOCK, 15);
    AddOddsFromRefStyle(m_awkwardUnicornDrops, ITEM_ID_UNICORN_JUMPER, 30);
    AddOddsFromRefStyle(m_awkwardUnicornDrops, ITEM_ID_HORSE_MASK, 30);
    AddOddsFromRefStyle(m_awkwardUnicornDrops, ITEM_ID_SCROLL_BULLETIN, 60);
    AddOddsFromRefStyle(m_awkwardUnicornDrops, ITEM_ID_VERY_BAD_UNICORN, 150);
    AddOddsFromRefStyle(m_awkwardUnicornDrops, ITEM_ID_UNICORN_HORN, 150);
    AddOddsFromRefStyle(m_awkwardUnicornDrops, ITEM_ID_TEDDY_BEAR, 250);
    AddOddsFromRefStyle(m_awkwardUnicornDrops, ITEM_ID_GIFT_OF_THE_UNICORN, 250);
    AddOddsFromRefStyle(m_awkwardUnicornDrops, ITEM_ID_RAINBOW_SCARF, 300);
    AddOddsFromRefStyle(m_awkwardUnicornDrops, ITEM_ID_TWINTAIL_HAIR, 300);

    // Partial but reference-aligned core GBC table.
    AddOddsFromRefStyle(m_gbcDrops, ITEM_ID_ANGEL_WINGS, 10000);
    AddOddsFromRefStyle(m_gbcDrops, ITEM_ID_BOOTY_CHEST, 200);
    AddOddsFromRefStyle(m_gbcDrops, ITEM_ID_CANDY_HEART, 200);
    AddOddsFromRefStyle(m_gbcDrops, ITEM_ID_CHOCOLATE_HEART, 375);
    AddOddsFromRefStyle(m_gbcDrops, ITEM_ID_DIAPER, 2000);
    AddOddsFromRefStyle(m_gbcDrops, ITEM_ID_GOLDEN_HEART_CRYSTAL, 200000);
    AddOddsFromRefStyle(m_gbcDrops, ITEM_ID_LOVE_CURTAIN, 8000);
    AddOddsFromRefStyle(m_gbcDrops, ITEM_ID_LOVESEAT, 140);
    AddOddsFromRefStyle(m_gbcDrops, ITEM_ID_PINK_MARBLE_ARCH, 6000);
    AddOddsFromRefStyle(m_gbcDrops, ITEM_ID_SOUR_LOLLIPOP, 200);
    AddOddsFromRefStyle(m_gbcDrops, ITEM_ID_TEENY_ANGEL_WINGS, 8500);
    AddOddsFromRefStyle(m_gbcDrops, ITEM_ID_VALENTINE_DRESS, 1000);

    AddOddsFromRefStyle(m_sgbcDrops, ITEM_ID_STAINED_GLASS_HEARTWINGS, 12000);
    AddOddsFromRefStyle(m_sgbcDrops, ITEM_ID_SCEPTER_OF_THE_HONOR_GUARD, 20900);

    m_gbcDropsPotion = m_gbcDrops;
    m_sgbcDropsPotion = m_sgbcDrops;

    for(auto& [itemID, weight] : m_gbcDropsPotion) {
        if(itemID == ITEM_ID_GOLDEN_HEART_CRYSTAL) {
            weight *= 2.0;
        }
    }

    for(auto& [itemID, weight] : m_sgbcDropsPotion) {
        if(itemID == ITEM_ID_GOLDEN_HEART_CRYSTAL) {
            weight *= 2.0;
        }
    }

    m_initialized = true;
    return true;
}

uint16 PrizeManager::PickByOdds(const std::vector<std::pair<uint16, double>>& table) const
{
    if(table.empty()) {
        return ITEM_ID_BLANK;
    }

    double totalWeight = 0.0;
    for(const auto& [_, weight] : table) {
        totalWeight += std::max(0.0, weight);
    }

    if(totalWeight <= 0.0) {
        return table.front().first;
    }

    const double roll = RandomRangeFloat(0.0f, static_cast<float>(totalWeight));
    double cursor = 0.0;
    for(const auto& [itemID, weight] : table) {
        cursor += std::max(0.0, weight);
        if(roll <= cursor) {
            return itemID;
        }
    }

    return table.back().first;
}

PrizeDrop PrizeManager::GetAwkwardUnicornDrop()
{
    Init();

    PrizeDrop out;
    out.itemID = PickByOdds(m_awkwardUnicornDrops);
    out.amount = 1;
    return out;
}

PrizeDrop PrizeManager::GetGBCDrop(bool potion)
{
    Init();

    PrizeDrop out;
    out.itemID = PickByOdds(potion ? m_gbcDropsPotion : m_gbcDrops);
    out.amount = 1;

    switch(out.itemID) {
        case ITEM_ID_LOVE_CURTAIN: out.amount = 2; break;
        case ITEM_ID_LOVESEAT: out.amount = 2; break;
        case ITEM_ID_PINK_MARBLE_ARCH: out.amount = 10; break;
        case ITEM_ID_SOUR_LOLLIPOP: out.amount = 10; break;
        default: break;
    }

    return out;
}

PrizeDrop PrizeManager::GetSGBCDrop(bool potion)
{
    Init();

    PrizeDrop out;
    out.itemID = PickByOdds(potion ? m_sgbcDropsPotion : m_sgbcDrops);
    out.amount = 1;
    return out;
}
