#include "StoreCatalog.h"

#include "Utils/StringUtils.h"
#include "IO/Log.h"
#include "OS/OSUtils.h"
#include <fstream>

bool StoreCatalog::s_loaded = false;
std::vector<StoreProduct> StoreCatalog::s_products;

bool StoreCatalog::EnsureLoaded()
{
    if(s_loaded) {
        return true;
    }

    string primaryPath = GetProgramPath() + "/store.txt";
    string fallbackPath = GetProgramPath() + "/Configs/store.txt";

    if(!LoadFromFile(primaryPath)) {
        if(!LoadFromFile(fallbackPath)) {
            LOGGER_LOG_WARN("Failed to load store catalog from %s and %s", primaryPath.c_str(), fallbackPath.c_str());
            return false;
        }
    }

    s_loaded = true;
    LOGGER_LOG_INFO("Store catalog loaded with %d products", (int32)s_products.size());
    return true;
}

const std::vector<StoreProduct>& StoreCatalog::GetProducts()
{
    return s_products;
}

const StoreProduct* StoreCatalog::FindByButton(const string& button)
{
    for(const auto& product : s_products) {
        if(product.button == button) {
            return &product;
        }
    }

    return nullptr;
}

bool StoreCatalog::LoadFromFile(const string& filePath)
{
    std::ifstream file(filePath);
    if(!file.is_open()) {
        return false;
    }

    s_products.clear();

    for(string line; std::getline(file, line); ) {
        RemoveExtraWhiteSpaces(line);

        if(line.empty() || line[0] == '#') {
            continue;
        }

        std::vector<string> pipes = Split(line, '|');
        if(pipes.size() < 9) {
            continue;
        }

        StoreProduct product;
        product.tab = ToInt(pipes[0]);
        product.button = pipes[1];
        product.name = pipes[2];
        product.rttx = pipes[3];
        product.description = pipes[4];
        product.tex1 = ToInt(pipes[5]);
        product.tex2 = ToInt(pipes[6]);
        product.cost = ToInt(pipes[7]);

        std::vector<string> rewardList = Split(pipes[8], ',');
        for(const auto& reward : rewardList) {
            std::vector<string> parts = Split(reward, ':');
            if(parts.size() != 2) {
                continue;
            }

            int32 itemID = ToInt(parts[0]);
            int32 amount = ToInt(parts[1]);
            if(itemID <= 0 || amount <= 0) {
                continue;
            }

            if(amount > 255) {
                amount = 255;
            }

            product.rewards.push_back(StoreProductReward{ (uint16)itemID, (uint8)amount });
        }

        if(product.tab < 1 || product.tab > 5 || product.button.empty()) {
            continue;
        }

        s_products.push_back(std::move(product));
    }

    return !s_products.empty();
}
