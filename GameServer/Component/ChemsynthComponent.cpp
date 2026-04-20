#include "ChemsynthComponent.h"

#include "Algorithm/ChemsynthAlgorithm.h"
#include "Utils/Timer.h"
#include "World/TileInfo.h"
#include "../World/World.h"
#include "../World/WorldManager.h"

namespace {
constexpr uint64 kChemsynthTickIntervalMS = 1500;

bool s_enabled = true;
Timer s_lastTick;
}

void ChemsynthComponent::SetEnabled(bool enabled)
{
    s_enabled = enabled;
    if(enabled) {
        s_lastTick.Reset();
    }
}

void ChemsynthComponent::OnTick()
{
    if(!s_enabled || s_lastTick.GetElapsedTime() < kChemsynthTickIntervalMS) {
        return;
    }

    s_lastTick.Reset();

    WorldManager* pWorldManager = GetWorldManager();
    if(!pWorldManager) {
        return;
    }

    pWorldManager->ForEachWorld([](World* pWorld) {
        if(!pWorld || pWorld->IsWaitingForClose() || pWorld->GetPlayerCount() < 1) {
            return;
        }

        WorldTileManager* pTileManager = pWorld->GetTileManager();
        if(!pTileManager) {
            return;
        }

        const Vector2Int size = pTileManager->GetSize();
        for(int32 y = 0; y < size.y; ++y) {
            for(int32 x = 0; x < size.x; ++x) {
                TileInfo* pTile = pTileManager->GetTile(x, y);
                if(!pTile || pTile->GetDisplayedItem() != ITEM_ID_CHEMSYNTH_PROCESSOR || !pTile->HasFlag(TILE_FLAG_IS_ON)) {
                    continue;
                }

                ChemsynthAlgorithm::MoveActiveTank(pWorld, pTile);
            }
        }
    });
}
