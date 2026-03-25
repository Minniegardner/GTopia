#include "Precompiled.h"
#include "Item/ItemInfoManager.h"
#include "Network/NetHTTP.h"
#include "IO/File.h"
#include "IO/Log.h"
#include "Utils/StringUtils.h"
#include "Utils/Timer.h"
#include <nlohmann/json.hpp>

/**
 * 
 * Some items wiki data might broken beacuse the wiki page is broken for some items
 * 
 */

struct ItemWikiData
{
    string description;
    string element;
    string seed1;
    string seed2;
};

bool ParseItemWiki(const string& data, ItemWikiData& out)
{
    auto TrimBraces = [&](const string& str) -> string
    {
        int32 pos = str.find("}}");
        if (pos != -1) {
            return str.substr(0, pos);
        }
        return "";
    };

    auto lines = Split(data, '\n');
    for(auto& line : lines) {
        if(line.empty()) {
            continue;
        }

        auto args = Split(line, '|');
        if(args.empty()) {
            continue;
        }

        if(args[0] == "{{Item") {
            if(args.size() > 2) {
                out.description = args[1];
                out.element = TrimBraces(args[2]);
            }
            else {
                out.description = TrimBraces(args[1]);
            }
        }

        if(args[0] == "{{RecipeSplice") {
            if(args.size() > 3) {
                out.seed1 = args[1];
                out.seed2 = TrimBraces(args[2]);
            }
            else {
                out.seed1 = args[1];
                out.seed2 = TrimBraces(args[2]);
            }
        }
    }

    return true;
}

std::unordered_map<string, ItemWikiData> parsedWikiItems;

bool ParseWiki(const string& data) 
{
    try
    {
        nlohmann::json json = nlohmann::json::parse(data);
        if(json.contains("query") && json["query"].contains("pages")) {
            string itemName;

            for(auto& [pageID, pageData] : json["query"]["pages"].items()) {
                if(pageData.contains("title")) {
                    itemName = pageData["title"].get<string>();
                }

                if(pageData.contains("revisions")) {
                    auto data = pageData["revisions"][0]["*"].get<string>();

                    if(itemName.empty()) {
                        continue;
                    }

                    ItemWikiData wikiData;
                    if(!ParseItemWiki(data, wikiData)) {
                        continue;
                    }

                    parsedWikiItems.insert_or_assign(itemName, wikiData);
                }
            }
        }
    }
    catch(const std::exception& e)
    {
        return false;
    }

    return false;
}

int main(int argc, char const* argv[])
{
    ItemInfoManager* pItemMgr = GetItemInfoManager();
    if(!pItemMgr->LoadByItemsDat(GetProgramPath() + "/items.dat")) {
        LOGGER_LOG_ERROR("Failed to load items.dat");
        return 0;
    }

    uint32 itemCount = pItemMgr->GetItemCount();
    uint32 startFrom = 0;
    if(argc > 1) {
        itemCount = ToUInt(argv[1]);
    }
    else if(argc > 2) {
        itemCount = ToUInt(argv[1]);
        startFrom = ToUInt(argv[2]);
    }

    File file;
    FileMode fMode = startFrom == 0 ? FILE_MODE_WRITE : FILE_MODE_APPEND;
    
    if(!file.Open(GetProgramPath() + "/items_generated.txt", fMode)) {
        LOGGER_LOG_ERROR("Failed to open items_generated.txt");
        return 0;
    }

    NetHTTP http;
    http.Init("https://growtopia.fandom.com");

    uint64 startTime = Time::GetSystemTime();

    LOGGER_LOG_INFO("Started getting item names for batch");
    std::vector<std::string> batchItemNames;
    std::string currentBatchNames;
    int32 currItemBatchSize = 0;
    
    for(int32 i = startFrom; i < itemCount + startFrom; ++i) {
        ItemInfo* pItem = pItemMgr->GetItemByID(i);
        if (!pItem) {
            LOGGER_LOG_ERROR("Not adding item %d for batching its NULL", i);
            continue;
        }

        if(pItem->id % 2 != 0) {
            continue;
        }
    
        if(currItemBatchSize > 0) {
            currentBatchNames += "|";
        }
        currentBatchNames += pItem->name;
        ++currItemBatchSize;
    
        if (currItemBatchSize == 49) {
            batchItemNames.push_back(currentBatchNames);
            currentBatchNames.clear();
            currItemBatchSize = 0;
        }
    }

    if(!currentBatchNames.empty()) {
        batchItemNames.push_back(currentBatchNames);
    }

    LOGGER_LOG_ERROR("Started fetching from wiki");
    for(int32 i = 0; i < batchItemNames.size(); ++i) {
        if(!http.Get("/api.php?action=query&prop=revisions&titles=" + batchItemNames[i] + "&rvprop=content&format=json")) {
            LOGGER_LOG_WARN("Failed to GET on %s", batchItemNames[i].c_str());
            continue;
        }

        if(http.GetStatus() != 200) {
            LOGGER_LOG_WARN("Got status code %d on %s", http.GetStatus(), batchItemNames[i].c_str());
            continue;
        }

        string body = http.GetBody();
        if(body.empty()) {
            LOGGER_LOG_WARN("Body is empty! %s", batchItemNames[i].c_str());
            continue;
        }

        ParseWiki(body);
        LOGGER_LOG_INFO("Parsed batch %d", i);
        SleepMS(10);
    }

    uint16 lastSeed1 = 0;
    uint16 lastSeed2 = 0;

    for(uint32 i = startFrom; i < itemCount; ++i) {
        ItemInfo* pItem = pItemMgr->GetItemByID(i);
        if(!pItem) {
            LOGGER_LOG_ERROR("Item %d not found skipping...?", i);
            continue;
        }

        //LOGGER_LOG_INFO("Processing item %d %s (%d/%d)", pItem->id, pItem->name.c_str(), i + 1, itemCount);

        string itemStr;
        if(pItem->name.find("null_item") != -1) {
            uint32 startID = pItem->id;
            uint32 endID = pItem->id + 1;

            while(i + 1 < itemCount) {
                ItemInfo* pNextItem = pItemMgr->GetItemByID(i + 1);
                if(!pNextItem || pNextItem->name.find("null_item") == -1) {
                    break;
                }

                ++i;
                endID = pNextItem->id;
            }

            itemStr += "make_null|" + ToString(startID) + "|" + ToString(endID) + "|\n\n";
            file.Write(itemStr.data(), itemStr.size());
            continue;
        }
        else if(pItem->type == ITEM_TYPE_CLOTHES) {
            itemStr += "add_cloth|";
            itemStr += ToString(pItem->id) + "|";
            itemStr += pItem->name + "|";
            itemStr += ItemMaterialToStr(pItem->material) + "|";
            itemStr += pItem->textureFile + "|";
            itemStr += ToString(pItem->textureX) + "," + ToString(pItem->textureY) + "|";
            itemStr += ItemVisualEffectToStr(pItem->visualEffect) + "|";
            itemStr += ItemStorageTypeToStr(pItem->storage) + "|";
            itemStr += ItemBodyPartToStr(pItem->bodyPart) + "|\n";
        }
        else if(pItem->type == ITEM_TYPE_SEED) {
            itemStr += "set_seed|";
            itemStr += ToString(lastSeed1) + "|";
            itemStr += ToString(lastSeed2) + "|";
            itemStr += ToString(pItem->seedBgColor.r) + "," + ToString(pItem->seedBgColor.g) + "," + ToString(pItem->seedBgColor.b) + "," + ToString(pItem->seedBgColor.a) + "|";
            itemStr += ToString(pItem->seedFgColor.r) + "," + ToString(pItem->seedFgColor.g) + "," + ToString(pItem->seedFgColor.b) + "," + ToString(pItem->seedFgColor.a) + "|\n";
            itemStr += "\n";
            
            file.Write(itemStr.data(), itemStr.size());
            continue;
        }
        else {
            itemStr += "add_item|";
            itemStr += ToString(pItem->id) + "|";
            itemStr += pItem->name + "|";
            itemStr += ItemTypeToStr(pItem->type) + "|";
            itemStr += ItemMaterialToStr(pItem->material) + "|";
            itemStr += pItem->textureFile + "|";
            itemStr += ToString(pItem->textureX) + "," + ToString(pItem->textureY) + "|";
            itemStr += ItemVisualEffectToStr(pItem->visualEffect) + "|";
            itemStr += ItemStorageTypeToStr(pItem->storage) + "|";
            itemStr += ItemCollisionTypeToStr(pItem->collisionType) + "|";
            itemStr += ToString(pItem->hp/6) + "|";
            itemStr += ToString(pItem->restoreTime) + "|\n";
        }

        auto it = parsedWikiItems.find(pItem->name);
        if(it == parsedWikiItems.end()) {
            LOGGER_LOG_ERROR("Failed to get wiki data for %d", pItem->id);
        }
        else {
            auto& wikiData = it->second;

            if(!wikiData.description.empty()) {
                itemStr += "description|" + wikiData.description + "|\n";
            }

            if(!wikiData.element.empty()) {
                itemStr += "set_element|" + ToUpper(wikiData.element) + "|\n";
            }

            if(!wikiData.seed1.empty() && !wikiData.seed2.empty()) {
                ItemInfo* pSeed1 = pItemMgr->GetItemByName(wikiData.seed1);
                ItemInfo* pSeed2 = pItemMgr->GetItemByName(wikiData.seed2);

                if(!pSeed1 || !pSeed2) {
                    LOGGER_LOG_ERROR("Seed not found for %d", pItem->id);
                    lastSeed1 = lastSeed2 = 0;
                }
                else {
                    lastSeed1 = pSeed1->id + 1;
                    lastSeed2 = pSeed2->id + 1;
                }
            }
            else {
                lastSeed1 = lastSeed2 = 0;
            }

        }

        if(pItem->flags != 0) {
            string itemFlagStr;

            for(uint8 i = 0; i < 16; ++i) {
                if(pItem->flags & (1 << i)) {
                    if(!itemFlagStr.empty()) {
                        itemFlagStr += ",";
                    }

                    itemFlagStr += ItemFlagToStr(1 << i);
                }
            }

            itemStr += "set_flags|" + itemFlagStr + "|\n";
        }

        if(!pItem->extraString.empty() || pItem->animMS != 200) {
            itemStr += "set_extra|" + pItem->extraString + "|" + ToString(pItem->animMS) + "|\n";
        }

        if(pItem->rarity == 999) {
            itemStr += "set_rarity|" + ToString(pItem->rarity) + "|\n";
        }

        file.Write(itemStr.data(), itemStr.size());
    }

    file.Close();

    LOGGER_LOG_INFO("Finished! took %dms", Time::GetSystemTime() - startTime);
}
