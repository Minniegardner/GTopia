#include "ChemsynthAlgorithm.h"

#include "Item/ItemInfoManager.h"
#include "World/TileInfo.h"
#include "World/World.h"
#include "World/WorldTileManager.h"
#include "Utils/Timer.h"
#include "Math/Random.h"
#include "Player/GamePlayer.h"

#include <algorithm>
#include <unordered_map>

namespace {

bool IsChemsynthProcessorItem(uint16 itemID)
{
    return itemID == ITEM_ID_CHEMSYNTH_PROCESSOR;
}

bool IsChemsynthTankItem(uint16 itemID)
{
    return itemID == ITEM_ID_CHEMSYNTH_TANK;
}

bool IsChemsynthToolItem(uint16 itemID)
{
    switch(itemID) {
        case ITEM_ID_CHEMSYNTH_SOLVENT:
        case ITEM_ID_CHEMSYNTH_CATALYST:
        case ITEM_ID_CHEMSYNTH_REPLICATOR:
        case ITEM_ID_CHEMSYNTH_CENTRIFUGE:
        case ITEM_ID_CHEMSYNTH_STIRRER:
            return true;
        default:
            return false;
    }
}

bool GetTankTiles(World* pWorld, TileInfo* pProcessorTile, std::vector<TileInfo*>& outTiles)
{
    outTiles.clear();

    if(!pWorld || !pProcessorTile || !IsChemsynthProcessorItem(pProcessorTile->GetDisplayedItem())) {
        return false;
    }

    const Vector2Int pos = pProcessorTile->GetPos();
    for(int32 i = 0; i < 10; ++i) {
        TileInfo* pTank = pWorld->GetTileManager()->GetTile(pos.x + 1 + i, pos.y);
        if(!pTank || !IsChemsynthTankItem(pTank->GetDisplayedItem())) {
            outTiles.clear();
            return false;
        }

        outTiles.push_back(pTank);
    }

    return true;
}

TileExtra_Chemsynth* GetTankData(TileInfo* pTile)
{
    return pTile ? pTile->GetExtra<TileExtra_Chemsynth>() : nullptr;
}

void UpdateTiles(World* pWorld, std::vector<TileInfo*>& tiles)
{
    if(!pWorld || tiles.empty()) {
        return;
    }

    pWorld->SendTileUpdateMultiple(tiles);
}

void SendChemSynthRewardFX(World* pWorld, GamePlayer* pPlayer)
{
    if(!pWorld || !pPlayer) {
        return;
    }

    pWorld->SendParticleEffectToAll(pPlayer->GetWorldPos().x + 16.0f, pPlayer->GetWorldPos().y + 16.0f, 5, 1.0f, 0);
    pWorld->PlaySFXForEveryone("audio/piano_nice.wav", 0);
}

}

namespace ChemsynthAlgorithm {

ChemsynthColor GetNextColor(ChemsynthColor color)
{
    switch(color) {
        case ChemsynthColor::RED: return ChemsynthColor::YELLOW;
        case ChemsynthColor::YELLOW: return ChemsynthColor::GREEN;
        case ChemsynthColor::GREEN: return ChemsynthColor::BLUE;
        case ChemsynthColor::BLUE: return ChemsynthColor::PINK;
        case ChemsynthColor::PINK: return ChemsynthColor::RED;
        default: return ChemsynthColor::RED;
    }
}

ChemsynthColor GetRandomColor()
{
    switch(RandomRangeInt(0, 4)) {
        case 0: return ChemsynthColor::RED;
        case 1: return ChemsynthColor::YELLOW;
        case 2: return ChemsynthColor::GREEN;
        case 3: return ChemsynthColor::BLUE;
        case 4: return ChemsynthColor::PINK;
        default: return ChemsynthColor::RED;
    }
}

uint16 ColorToChemicalID(ChemsynthColor color)
{
    switch(color) {
        case ChemsynthColor::RED: return ITEM_ID_CHEMICAL_R;
        case ChemsynthColor::YELLOW: return ITEM_ID_CHEMICAL_Y;
        case ChemsynthColor::GREEN: return ITEM_ID_CHEMICAL_G;
        case ChemsynthColor::BLUE: return ITEM_ID_CHEMICAL_B;
        case ChemsynthColor::PINK: return ITEM_ID_CHEMICAL_P;
        default: return ITEM_ID_BLANK;
    }
}

void SetTankActive(TileInfo* pTile, bool enable)
{
    if(!pTile) {
        return;
    }

    if(enable) {
        pTile->SetFlag(TILE_FLAG_IS_ON);
        pTile->SetFlag(TILE_FLAG_IS_OPEN_TO_PUBLIC);
    }
    else {
        pTile->RemoveFlag(TILE_FLAG_IS_ON);
        pTile->RemoveFlag(TILE_FLAG_IS_OPEN_TO_PUBLIC);
    }
}

bool IsTankActive(TileInfo* pTile)
{
    return pTile && IsChemsynthTankItem(pTile->GetDisplayedItem()) && pTile->HasFlag(TILE_FLAG_IS_ON) && pTile->HasFlag(TILE_FLAG_IS_OPEN_TO_PUBLIC);
}

void CancelChemsynth(World* pWorld, TileInfo* pProcessorTile)
{
    if(!pWorld || !pProcessorTile || !IsChemsynthProcessorItem(pProcessorTile->GetDisplayedItem())) {
        return;
    }

    std::vector<TileInfo*> tiles;
    tiles.push_back(pProcessorTile);
    SetTankActive(pProcessorTile, false);

    const Vector2Int pos = pProcessorTile->GetPos();
    for(int32 i = 0; i < 10; ++i) {
        TileInfo* pTank = pWorld->GetTileManager()->GetTile(pos.x + 1 + i, pos.y);
        if(!pTank || !IsChemsynthTankItem(pTank->GetDisplayedItem())) {
            continue;
        }

        SetTankActive(pTank, false);
        if(TileExtra_Chemsynth* pData = GetTankData(pTank)) {
            pData->color = ChemsynthColor::NONE;
            pData->targetColor = ChemsynthColor::NONE;
        }
        tiles.push_back(pTank);
    }

    UpdateTiles(pWorld, tiles);
}

void MoveActiveTank(World* pWorld, TileInfo* pProcessorTile)
{
    if(!pWorld || !pProcessorTile || !IsChemsynthProcessorItem(pProcessorTile->GetDisplayedItem())) {
        return;
    }

    if(!pProcessorTile->HasFlag(TILE_FLAG_IS_ON) && !pProcessorTile->HasFlag(TILE_FLAG_IS_OPEN_TO_PUBLIC)) {
        return;
    }

    std::vector<TileInfo*> tanks;
    if(!GetTankTiles(pWorld, pProcessorTile, tanks)) {
        CancelChemsynth(pWorld, pProcessorTile);
        return;
    }

    std::vector<TileInfo*> tiles;
    bool foundActive = false;

    for(size_t i = 0; i < tanks.size(); ++i) {
        TileInfo* pTank = tanks[i];
        if(!IsTankActive(pTank)) {
            continue;
        }

        SetTankActive(pTank, false);
        tiles.push_back(pTank);
        foundActive = true;

        TileInfo* pNextTank = nullptr;
        if(i + 1 < tanks.size()) {
            pNextTank = tanks[i + 1];
        }
        else {
            pNextTank = tanks.front();
        }

        if(pNextTank) {
            SetTankActive(pNextTank, true);
            tiles.push_back(pNextTank);
        }

        if(i + 1 < tanks.size()) {
            ++i;
        }
    }

    if(!foundActive && !tanks.empty()) {
        SetTankActive(tanks.front(), true);
        tiles.push_back(tanks.front());
    }

    UpdateTiles(pWorld, tiles);
}

uint32 GetPercentFinished(World* pWorld, TileInfo* pProcessorTile)
{
    if(!pWorld || !pProcessorTile || !IsChemsynthProcessorItem(pProcessorTile->GetDisplayedItem())) {
        return 999;
    }

    std::vector<TileInfo*> tanks;
    if(!GetTankTiles(pWorld, pProcessorTile, tanks)) {
        return 999;
    }

    uint32 percent = 0;
    for(TileInfo* pTank : tanks) {
        TileExtra_Chemsynth* pData = GetTankData(pTank);
        if(!pData) {
            return 999;
        }

        if(pData->color == pData->targetColor) {
            percent += 10;
        }
    }

    return percent;
}

bool IsChemsynthReady(World* pWorld, TileInfo* pProcessorTile)
{
    if(!pWorld || !pProcessorTile || !IsChemsynthProcessorItem(pProcessorTile->GetDisplayedItem())) {
        return false;
    }

    std::vector<TileInfo*> tanks;
    if(!GetTankTiles(pWorld, pProcessorTile, tanks)) {
        return false;
    }

    for(TileInfo* pTank : tanks) {
        if(!GetTankData(pTank)) {
            return false;
        }
    }

    return true;
}

std::pair<int32, int32> GetTankRange(World* pWorld, TileInfo* pTankTile)
{
    if(!pWorld || !pTankTile || !IsChemsynthTankItem(pTankTile->GetDisplayedItem())) {
        const Vector2Int pos = pTankTile ? pTankTile->GetPos() : Vector2Int{0, 0};
        return {pos.x, pos.x};
    }

    const Vector2Int pos = pTankTile->GetPos();
    int32 start = pos.x;
    for(int32 i = 0; i < 10; ++i) {
        TileInfo* pCandidate = pWorld->GetTileManager()->GetTile((pos.x - 1) - i, pos.y);
        if(!pCandidate) {
            break;
        }

        if(IsChemsynthProcessorItem(pCandidate->GetDisplayedItem())) {
            start = pCandidate->GetPos().x + 1;
            break;
        }
    }

    return {start, start + 9};
}

TileInfo* FindProcessorTile(World* pWorld, TileInfo* pTankTile)
{
    if(!pWorld || !pTankTile || !IsChemsynthTankItem(pTankTile->GetDisplayedItem())) {
        return nullptr;
    }

    const Vector2Int pos = pTankTile->GetPos();
    for(int32 i = 0; i < 10; ++i) {
        TileInfo* pCandidate = pWorld->GetTileManager()->GetTile((pos.x - 1) - i, pos.y);
        if(!pCandidate) {
            break;
        }

        if(IsChemsynthProcessorItem(pCandidate->GetDisplayedItem())) {
            return pCandidate;
        }
    }

    return nullptr;
}

void CheckProgressAndReward(GamePlayer* pPlayer, World* pWorld, TileInfo* pTankTile)
{
    if(!pPlayer || !pWorld || !pTankTile || !IsChemsynthTankItem(pTankTile->GetDisplayedItem())) {
        return;
    }

    TileInfo* pProcessorTile = FindProcessorTile(pWorld, pTankTile);
    if(!pProcessorTile) {
        return;
    }

    if(GetPercentFinished(pWorld, pProcessorTile) != 100) {
        return;
    }

    CancelChemsynth(pWorld, pProcessorTile);
    pPlayer->ModifyInventoryItem(ITEM_ID_SYNTHETIC_CHEMICAL, 1);
    pPlayer->SendOnTalkBubble("`2Chemical Synthesis complete!``\nI created `91 Synthetic Chemical``!", true);
    pPlayer->SendOnConsoleMessage("`2Chemical Synthesis complete!``\nI created `91 Synthetic Chemical``!");
    SendChemSynthRewardFX(pWorld, pPlayer);
    pPlayer->IncreaseStat("SYNTHETIC_CHEMICALS_MADE_CHEMSYNTH", 1);
}

void UseTool(GamePlayer* pPlayer, World* pWorld, TileInfo* pTankTile, uint16 itemID)
{
    if(!pPlayer || !pWorld || !pTankTile || !IsChemsynthTankItem(pTankTile->GetDisplayedItem()) || !IsChemsynthToolItem(itemID)) {
        return;
    }

    if(!IsTankActive(pTankTile)) {
        pPlayer->SendOnTalkBubble("Only usable on the selected Chemsynth Tank!", true);
        pPlayer->SendOnConsoleMessage("Only usable on the selected Chemsynth Tank!");
        return;
    }

    TileExtra_Chemsynth* pClickedData = GetTankData(pTankTile);
    if(!pClickedData) {
        pPlayer->SendOnTalkBubble("Chemsynth tank data is invalid.", true);
        return;
    }

    TileInfo* pProcessorTile = FindProcessorTile(pWorld, pTankTile);
    if(!pProcessorTile) {
        pPlayer->SendOnTalkBubble("Chemsynth processor not found for this tank line.", true);
        pPlayer->SendOnConsoleMessage("Chemsynth processor not found for this tank line.");
        return;
    }

    uint16 chemicalID = ColorToChemicalID(pClickedData->color);
    if(chemicalID == ITEM_ID_BLANK || pPlayer->GetInventory().GetCountOfItem(chemicalID) < 1) {
        pPlayer->SendOnTalkBubble("You do not have the required Chemical for this tank color.", true);
        pPlayer->SendOnConsoleMessage("You do not have the required Chemical for this tank color.");
        return;
    }

    std::pair<int32, int32> range = GetTankRange(pWorld, pTankTile);
    const int32 clickedIndex = std::clamp<int32>(pTankTile->GetPos().x - range.first, 0, 9);

    std::vector<TileInfo*> tanks;
    if(!GetTankTiles(pWorld, pProcessorTile, tanks)) {
        pPlayer->SendOnTalkBubble("Chemsynth setup is invalid.", true);
        pPlayer->SendOnConsoleMessage("Chemsynth setup is invalid.");
        return;
    }

    pPlayer->ModifyInventoryItem(chemicalID, -1);

    std::vector<TileInfo*> tiles;

    switch(itemID) {
        case ITEM_ID_CHEMSYNTH_SOLVENT: {
            for(int32 i = clickedIndex; i < 9; ++i) {
                TileInfo* pCurrent = tanks[i];
                TileInfo* pNext = tanks[i + 1];
                if(!pCurrent || !pNext) {
                    continue;
                }

                TileExtra_Chemsynth* pCurrentData = GetTankData(pCurrent);
                TileExtra_Chemsynth* pNextData = GetTankData(pNext);
                if(!pCurrentData || !pNextData) {
                    continue;
                }

                pCurrentData->color = pNextData->color;
                tiles.push_back(pCurrent);
            }

            if(TileExtra_Chemsynth* pLastData = GetTankData(tanks[9])) {
                pLastData->color = GetRandomColor();
                tiles.push_back(tanks[9]);
            }
            break;
        }

        case ITEM_ID_CHEMSYNTH_CATALYST: {
            int32 leftIndex = clickedIndex;
            int32 rightIndex = clickedIndex;

            for(int32 i = clickedIndex; i >= 0; --i) {
                TileExtra_Chemsynth* pData = GetTankData(tanks[i]);
                if(!pData || pData->color != pClickedData->color) {
                    break;
                }

                leftIndex = i;
            }

            for(int32 i = clickedIndex; i < 10; ++i) {
                TileExtra_Chemsynth* pData = GetTankData(tanks[i]);
                if(!pData || pData->color != pClickedData->color) {
                    break;
                }

                rightIndex = i;
            }

            for(int32 i = leftIndex; i <= rightIndex; ++i) {
                TileExtra_Chemsynth* pData = GetTankData(tanks[i]);
                if(!pData) {
                    continue;
                }

                pData->color = GetNextColor(pData->color);
                tiles.push_back(tanks[i]);
            }
            break;
        }

        case ITEM_ID_CHEMSYNTH_STIRRER: {
            ChemsynthColor clickedColor = pClickedData->color;
            ChemsynthColor leftColor = ChemsynthColor::NONE;
            ChemsynthColor rightColor = ChemsynthColor::NONE;

            TileInfo* pLeft = clickedIndex > 0 ? tanks[clickedIndex - 1] : nullptr;
            TileInfo* pRight = clickedIndex < 9 ? tanks[clickedIndex + 1] : nullptr;

            if(TileExtra_Chemsynth* pLeftData = GetTankData(pLeft)) {
                leftColor = pLeftData->color;
            }
            else {
                leftColor = GetRandomColor();
            }

            if(TileExtra_Chemsynth* pRightData = GetTankData(pRight)) {
                rightColor = pRightData->color;
            }
            else {
                rightColor = GetRandomColor();
            }

            if(TileExtra_Chemsynth* pLeftData = GetTankData(pLeft)) {
                pLeftData->color = rightColor;
                tiles.push_back(pLeft);
            }

            if(TileExtra_Chemsynth* pRightData = GetTankData(pRight)) {
                pRightData->color = leftColor;
                tiles.push_back(pRight);
            }

            pClickedData->color = clickedColor;
            tiles.push_back(pTankTile);
            break;
        }

        case ITEM_ID_CHEMSYNTH_CENTRIFUGE: {
            int32 leftDistance = clickedIndex;
            int32 rightDistance = 9 - clickedIndex;
            int32 maxTilesToFlip = std::min(leftDistance, rightDistance);

            if(maxTilesToFlip == 0) {
                break;
            }

            std::vector<ChemsynthColor> colors;
            for(int32 i = maxTilesToFlip; i > 0; --i) {
                TileExtra_Chemsynth* pData = GetTankData(tanks[clickedIndex - i]);
                if(!pData) {
                    continue;
                }

                colors.push_back(pData->color);
            }

            colors.push_back(pClickedData->color);

            for(int32 i = 1; i <= maxTilesToFlip; ++i) {
                TileExtra_Chemsynth* pData = GetTankData(tanks[clickedIndex + i]);
                if(!pData) {
                    continue;
                }

                colors.push_back(pData->color);
            }

            int32 colorIndex = 0;
            for(int32 i = clickedIndex - maxTilesToFlip; i <= clickedIndex + maxTilesToFlip; ++i) {
                TileExtra_Chemsynth* pData = GetTankData(tanks[i]);
                if(!pData) {
                    continue;
                }

                pData->color = colors[colorIndex++];
                tiles.push_back(tanks[i]);
            }
            break;
        }

        case ITEM_ID_CHEMSYNTH_REPLICATOR:
        default:
            break;
    }

    if(!tiles.empty()) {
        UpdateTiles(pWorld, tiles);
    }

    CheckProgressAndReward(pPlayer, pWorld, pTankTile);
}

void StartChemsynth(GamePlayer* pPlayer, World* pWorld, TileInfo* pProcessorTile)
{
    if(!pPlayer || !pWorld || !pProcessorTile || !IsChemsynthProcessorItem(pProcessorTile->GetDisplayedItem())) {
        return;
    }

    if(pProcessorTile->HasFlag(TILE_FLAG_IS_OPEN_TO_PUBLIC)) {
        pPlayer->SendOnTalkBubble("Chemical synthesis is already running.", true);
        pPlayer->SendOnConsoleMessage("Chemical synthesis is already running.");
        return;
    }

    std::vector<TileInfo*> tiles;
    std::vector<TileInfo*> tanks;
    if(!GetTankTiles(pWorld, pProcessorTile, tanks)) {
        pPlayer->SendOnTalkBubble("Chemsynth setup is invalid. Place exactly 10 tanks to the right of the processor.", true);
        pPlayer->SendOnConsoleMessage("Chemsynth setup is invalid. Place exactly 10 tanks to the right of the processor.");
        CancelChemsynth(pWorld, pProcessorTile);
        return;
    }

    if(pPlayer->GetInventory().GetCountOfItem(ITEM_ID_CHEMICAL_B) < 5 ||
        pPlayer->GetInventory().GetCountOfItem(ITEM_ID_CHEMICAL_R) < 5 ||
        pPlayer->GetInventory().GetCountOfItem(ITEM_ID_CHEMICAL_Y) < 5 ||
        pPlayer->GetInventory().GetCountOfItem(ITEM_ID_CHEMICAL_G) < 5 ||
        pPlayer->GetInventory().GetCountOfItem(ITEM_ID_CHEMICAL_P) < 5)
    {
        pPlayer->SendOnTalkBubble("`4Error: You need 5 of each Chemical to start, and we recommend at least 30 of each, as every Chemsynth tool uses up 1 chemical of the selected color.``", true);
        CancelChemsynth(pWorld, pProcessorTile);
        return;
    }

    pProcessorTile->SetFlag(TILE_FLAG_IS_ON);
    pProcessorTile->SetFlag(TILE_FLAG_IS_OPEN_TO_PUBLIC);
    tiles.push_back(pProcessorTile);

    for(size_t i = 0; i < tanks.size(); ++i) {
        TileInfo* pTank = tanks[i];
        tiles.push_back(pTank);

        SetTankActive(pTank, i == 0 || i == 5);

        if(TileExtra_Chemsynth* pData = GetTankData(pTank)) {
            pData->color = GetRandomColor();
            pData->targetColor = GetRandomColor();
        }
    }

    pPlayer->ModifyInventoryItem(ITEM_ID_CHEMICAL_B, -5);
    pPlayer->ModifyInventoryItem(ITEM_ID_CHEMICAL_R, -5);
    pPlayer->ModifyInventoryItem(ITEM_ID_CHEMICAL_Y, -5);
    pPlayer->ModifyInventoryItem(ITEM_ID_CHEMICAL_G, -5);
    pPlayer->ModifyInventoryItem(ITEM_ID_CHEMICAL_P, -5);
    pPlayer->SendOnTalkBubble("`2Chemical synthesis started!``", true);
    pPlayer->SendOnConsoleMessage("You used up `w5 Chemical R, 5 Chemical Y, 5 Chemical G, 5 Chemical B, and 5 Chemical P`` to begin Chemical Synthesis!");

    UpdateTiles(pWorld, tiles);
}

void UpdateWorldChemsynth(World* pWorld)
{
    if(!pWorld) {
        return;
    }

    static std::unordered_map<uint32, uint64> lastUpdateByWorld;

    const uint32 worldID = pWorld->GetID();
    uint64 nowMS = Time::GetSystemTime();
    uint64& lastUpdateMS = lastUpdateByWorld[worldID];
    if(nowMS - lastUpdateMS < 1500) {
        return;
    }

    lastUpdateMS = nowMS;

    WorldTileManager* pTileMgr = pWorld->GetTileManager();
    if(!pTileMgr) {
        return;
    }

    Vector2Int size = pTileMgr->GetSize();
    for(int32 y = 0; y < size.y; ++y) {
        for(int32 x = 0; x < size.x; ++x) {
            TileInfo* pTile = pTileMgr->GetTile(x, y);
            if(!pTile || !IsChemsynthProcessorItem(pTile->GetDisplayedItem()) || !pTile->HasFlag(TILE_FLAG_IS_OPEN_TO_PUBLIC)) {
                continue;
            }

            MoveActiveTank(pWorld, pTile);
        }
    }
}

}