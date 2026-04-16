#pragma once

#include "Precompiled.h"

struct StoreProductReward
{
    uint16 itemID = 0;
    uint8 amount = 0;
};

struct StoreProduct
{
    int32 tab = 0;
    string button = "";
    string name = "";
    string rttx = "";
    string description = "";
    int32 tex1 = 0;
    int32 tex2 = 0;
    int32 cost = 0;

    std::vector<StoreProductReward> rewards;
};

class StoreCatalog {
public:
    static bool EnsureLoaded();

    static const std::vector<StoreProduct>& GetProducts();
    static const StoreProduct* FindByButton(const string& button);

private:
    static bool LoadFromFile(const string& filePath);

private:
    static bool s_loaded;
    static std::vector<StoreProduct> s_products;
};
