#include "Buy.h"

#include "../../../Store/StoreCatalog.h"
#include "Item/ItemInfoManager.h"
#include "Utils/StringUtils.h"
#include "Math/Random.h"
#include "IO/Log.h"

namespace {

bool CanReceiveRewards(GamePlayer* pPlayer, const std::vector<StoreProductReward>& rewards)
{
    if(!pPlayer) {
        return false;
    }

    PlayerInventory& inv = pPlayer->GetInventory();

    std::unordered_map<uint16, int32> simulatedCounts;
    uint32 usedTypes = inv.GetItemTypesCount();
    uint32 capacity = inv.GetCapacity();

    for(const auto& reward : rewards) {
        if(reward.itemID == 9412) {
            continue;
        }

        ItemInfo* pItem = GetItemInfoManager()->GetItemByID(reward.itemID);
        if(!pItem) {
            return false;
        }

        int32 current = 0;
        auto it = simulatedCounts.find(reward.itemID);
        if(it == simulatedCounts.end()) {
            current = (int32)inv.GetCountOfItem(reward.itemID);
            simulatedCounts.insert_or_assign(reward.itemID, current);
        }
        else {
            current = it->second;
        }

        if(current == 0) {
            if(usedTypes + 1 > capacity) {
                return false;
            }
            usedTypes += 1;
        }

        int32 next = current + reward.amount;
        if(next > pItem->maxCanHold) {
            return false;
        }

        simulatedCounts[reward.itemID] = next;
    }

    return true;
}

uint8 ResolveTabFromItem(const string& item)
{
    if(item == "locks") {
        return 1;
    }
    if(item == "itempack") {
        return 2;
    }
    if(item == "bigitems") {
        return 3;
    }
    if(item == "weather") {
        return 4;
    }
    if(item == "token") {
        return 5;
    }

    return 0;
}

void AppendRandomRewards(const string& button, std::vector<StoreProductReward>& rewards)
{
    if(button == "basic_splice") {
        static const uint16 ids[] = { 3567, 2793, 57, 13, 17, 21, 101, 381, 1139 };
        rewards.push_back({11, 10});
        for(uint8 i = 0; i < 10; ++i) {
            int32 idx = RandomRangeInt(0, (int32)(sizeof(ids) / sizeof(ids[0])) - 1);
            rewards.push_back({ids[idx], 1});
        }
        return;
    }

    std::vector<uint16> pool;
    const auto& items = GetItemInfoManager()->GetItems();

    if(button == "rare_seed") {
        for(const auto& item : items) {
            if(item.type == ITEM_TYPE_SEED && item.rarity >= 13 && item.rarity <= 60) {
                pool.push_back((uint16)item.id);
            }
        }

        for(uint8 i = 0; i < 5 && !pool.empty(); ++i) {
            int32 idx = RandomRangeInt(0, (int32)pool.size() - 1);
            rewards.push_back({pool[(uint32)idx], 1});
        }
        return;
    }

    if(button == "clothes_pack" || button == "rare_clothes_pack") {
        for(const auto& item : items) {
            if(item.type != ITEM_TYPE_CLOTHES) {
                continue;
            }

            if(button == "clothes_pack") {
                if(item.rarity <= 10) {
                    pool.push_back((uint16)item.id);
                }
            }
            else {
                if(item.rarity >= 11 && item.rarity <= 60) {
                    pool.push_back((uint16)item.id);
                }
            }
        }

        for(uint8 i = 0; i < 3 && !pool.empty(); ++i) {
            int32 idx = RandomRangeInt(0, (int32)pool.size() - 1);
            rewards.push_back({pool[(uint32)idx], 1});
        }
    }
}

} // namespace

void Buy::Execute(GamePlayer* pPlayer, ParsedTextPacket<8>& packet)
{
    if(!pPlayer || !pPlayer->HasState(PLAYER_STATE_IN_GAME)) {
        return;
    }

    auto pItem = packet.Find(CompileTimeHashString("item"));
    if(!pItem) {
        return;
    }

    string item(pItem->value, pItem->size);
    if(item == "main") {
        SendHome(pPlayer);
        return;
    }

    uint8 tab = ResolveTabFromItem(item);
    if(tab != 0) {
        auto pSelection = packet.Find(CompileTimeHashString("selection"));
        string selection = pSelection ? string(pSelection->value, pSelection->size) : "";
        SendTab(pPlayer, tab, selection);
        return;
    }

    HandlePurchase(pPlayer, item);
}

void Buy::SendHome(GamePlayer* pPlayer)
{
    if(!pPlayer) {
        return;
    }

    string req;
    req += "set_description_text|Welcome to the `2Growtopia Store``! Select the item you'd like more info on.`o `wThanks for being a supporter of Growtopia!\n";
    req += "enable_tabs|1\n";
    req += "add_tab_button|main_menu|Home|interface/large/btn_shop.rttex||1|0|0|0||||-1|-1|||0|0|CustomParams:|\n";
    req += "add_tab_button|locks_menu|Locks And Stuff|interface/large/btn_shop.rttex||0|1|0|0||||-1|-1|||0|0|CustomParams:|\n";
    req += "add_tab_button|itempack_menu|Item Packs|interface/large/btn_shop.rttex||0|3|0|0||||-1|-1|||0|0|CustomParams:|\n";
    req += "add_tab_button|bigitems_menu|Awesome Items|interface/large/btn_shop.rttex||0|4|0|0||||-1|-1|||0|0|CustomParams:|\n";
    req += "add_tab_button|weather_menu|Weather Machines|interface/large/btn_shop.rttex|Tired of the same sunny sky?  We offer alternatives within...|0|5|0|0||||-1|-1|||0|0|CustomParams:|\n";
    req += "add_tab_button|token_menu|Growtoken Items|interface/large/btn_shop.rttex||0|2|0|0||||-1|-1|||0|0|CustomParams:|\n";
    req += "add_banner|interface/large/gui_shop_featured_header.rttex|0|1|\n";
    req += "add_button|gems_chest|Chest o' Gems|interface/large/store_buttons/store_buttons.rttex|`2You Get:`` 280,000 Gems and 84 World Locks.|0|5|0||||-1|-1||-1|-1|Sample featured pack.|1||||||0|0|CustomParams:|\n";
    req += "add_button|redeem_code|Redeem Code|interface/large/store_buttons/store_buttons40.rttex|OPENDIALOG&showredeemcodewindow|1|5|0|0|||-1|-1||-1|-1||1||||||0|0|CustomParams:|\n";

    VariantVector data(2);
    data[0] = "OnStoreRequest";
    data[1] = req;
    pPlayer->SendCallFunctionPacket(data);
}

void Buy::SendTab(GamePlayer* pPlayer, uint8 tab, const string& selection)
{
    if(!pPlayer) {
        return;
    }

    StoreCatalog::EnsureLoaded();

    string req;
    switch(tab) {
        case 1:
            req += "set_description_text|`2Locks And Stuff!``  Select the item you'd like more info on, or BACK to go back.\n";
            break;
        case 2:
            req += "set_description_text|`2Item Packs!``  Select the item you'd like more info on, or BACK to go back.\n";
            break;
        case 3:
            req += "set_description_text|`2Awesome Items!``  Select the item you'd like more info on, or BACK to go back.\n";
            break;
        case 4:
            req += "set_description_text|`2Weather Machines!``  Select the item you'd like more info on, or BACK to go back.\n";
            break;
        case 5: {
            uint8 gtCount = pPlayer->GetInventory().GetCountOfItem(ITEM_ID_GROWTOKEN);
            req += "set_description_text|`2Spend your Growtokens!`` (You have `5" + ToString((int32)gtCount) + "``) You earn Growtokens from Crazy Jim and Sales-Man. Select the item you'd like more info on, or BACK to go back.\n";
            break;
        }
        default:
            return;
    }

    req += "enable_tabs|1\n";
    req += "add_tab_button|main_menu|Home|interface/large/btn_shop.rttex||0|0|0|0||||-1|-1|||0|0|CustomParams:|\n";
    req += "add_tab_button|locks_menu|Locks And Stuff|interface/large/btn_shop.rttex||" + string(tab == 1 ? "1" : "0") + "|1|0|0||||-1|-1|||0|0|CustomParams:|\n";
    req += "add_tab_button|itempack_menu|Item Packs|interface/large/btn_shop.rttex||" + string(tab == 2 ? "1" : "0") + "|3|0|0||||-1|-1|||0|0|CustomParams:|\n";
    req += "add_tab_button|bigitems_menu|Awesome Items|interface/large/btn_shop.rttex||" + string(tab == 3 ? "1" : "0") + "|4|0|0||||-1|-1|||0|0|CustomParams:|\n";
    req += "add_tab_button|weather_menu|Weather Machines|interface/large/btn_shop.rttex|Tired of the same sunny sky?  We offer alternatives within...|" + string(tab == 4 ? "1" : "0") + "|5|0|0||||-1|-1|||0|0|CustomParams:|\n";
    req += "add_tab_button|token_menu|Growtoken Items|interface/large/btn_shop.rttex||" + string(tab == 5 ? "1" : "0") + "|2|0|0||||-1|-1|||0|0|CustomParams:|\n";

    const auto& products = StoreCatalog::GetProducts();

    uint32 capacity = pPlayer->GetInventory().GetCapacity();
    uint32 upgrades = ((capacity - 16) / 10) + 1;
    int32 backpackCost = 100 * (int32)upgrades * (int32)upgrades - 200 * (int32)upgrades + 200;

    for(const auto& product : products) {
        if(product.tab != tab) {
            continue;
        }

        int32 cost = product.cost;
        if(product.button == "upgrade_backpack") {
            if(upgrades > 38) {
                continue;
            }
            cost = backpackCost;
        }

        req += "add_button|" + product.button + "|" + product.name + "|" + product.rttx + "|" + product.description + "|" +
            ToString(product.tex1) + "|" + ToString(product.tex2) + "|" + ToString(cost) + "|0|||-1|-1||-1|-1||1||||||0|0|CustomParams:|\n";
    }

    if(!selection.empty()) {
        req += "select_item|" + selection + "\n";
    }

    VariantVector data(2);
    data[0] = "OnStoreRequest";
    data[1] = req;
    pPlayer->SendCallFunctionPacket(data);
}

void Buy::HandlePurchase(GamePlayer* pPlayer, const string& button)
{
    if(!pPlayer) {
        return;
    }

    if(!StoreCatalog::EnsureLoaded()) {
        pPlayer->SendOnConsoleMessage("`4Store catalog is not loaded.``");
        return;
    }

    const StoreProduct* pProduct = StoreCatalog::FindByButton(button);
    if(!pProduct) {
        return;
    }

    int32 cost = pProduct->cost;
    bool tokenPurchase = (pProduct->tab == 5);
    int32 tokenCost = std::abs(cost);

    uint32 capacity = pPlayer->GetInventory().GetCapacity();
    uint32 upgrades = ((capacity - 16) / 10) + 1;
    if(button == "upgrade_backpack") {
        if(upgrades > 38) {
            pPlayer->SendOnConsoleMessage("`4Your backpack is already maxed out.``");
            return;
        }

        cost = 100 * (int32)upgrades * (int32)upgrades - 200 * (int32)upgrades + 200;
    }

    uint8 currentGrowtoken = pPlayer->GetInventory().GetCountOfItem(ITEM_ID_GROWTOKEN);

    if(tokenPurchase) {
        if(currentGrowtoken < tokenCost) {
            VariantVector result(2);
            result[0] = "OnStorePurchaseResult";
            result[1] = "You can't afford `0" + pProduct->name + "``!  You're `$" + ToString(tokenCost - (int32)currentGrowtoken) + "`` `2Growtokens`` short.";
            pPlayer->SendCallFunctionPacket(result);
            return;
        }
    }
    else {
        if(pPlayer->GetGems() < cost) {
            VariantVector result(2);
            result[0] = "OnStorePurchaseResult";
            result[1] = "You can't afford `0" + pProduct->name + "``!  You're `$" + ToString(cost - pPlayer->GetGems()) + "`` Gems short.";
            pPlayer->SendCallFunctionPacket(result);
            return;
        }
    }

    std::vector<StoreProductReward> rewards = pProduct->rewards;
    AppendRandomRewards(button, rewards);

    if(!CanReceiveRewards(pPlayer, rewards)) {
        VariantVector result(2);
        result[0] = "OnStorePurchaseResult";
        result[1] = "You need more inventory space to receive this purchase.";
        pPlayer->SendCallFunctionPacket(result);
        return;
    }

    string received = "";
    bool first = true;

    for(const auto& reward : rewards) {
        if(reward.itemID == 9412) {
            pPlayer->GetInventory().IncreaseCapacity(10);
            pPlayer->SendInventoryPacket();
            continue;
        }

        pPlayer->ModifyInventoryItem(reward.itemID, reward.amount);

        ItemInfo* pItem = GetItemInfoManager()->GetItemByID(reward.itemID);
        if(!pItem) {
            continue;
        }

        if(!first) {
            received += ", ";
        }
        first = false;

        received += pItem->name;
        if(reward.amount > 1) {
            received += " x" + ToString((int32)reward.amount);
        }
    }

    if(received.empty()) {
        received = "(No item)";
    }

    VariantVector result(2);
    result[0] = "OnStorePurchaseResult";

    if(tokenPurchase) {
        pPlayer->ModifyInventoryItem(ITEM_ID_GROWTOKEN, (int16)-tokenCost);

        int32 left = (int32)pPlayer->GetInventory().GetCountOfItem(ITEM_ID_GROWTOKEN);
        result[1] = "You've purchased `0" + pProduct->name + "`` for `$" + ToString(tokenCost) + "`` `2Growtokens``.\nYou have `$" + ToString(left) + "`` `2Growtokens`` left.\n\n`5Received: ```0" + received + "``";
    }
    else {
        if(!pPlayer->TrySpendGems(cost)) {
            return;
        }

        pPlayer->SendOnSetBux();
        result[1] = "You've purchased `0" + pProduct->name + "`` for `$" + ToString(cost) + "`` Gems.\nYou have `$" + ToString(pPlayer->GetGems()) + "`` Gems left.\n\n`5Received: ```0" + received + "``";
    }

    pPlayer->SendCallFunctionPacket(result);
}
