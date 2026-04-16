#include "WorldPathfinding.h"

#include "World.h"
#include "../Player/GamePlayer.h"
#include "Item/ItemInfoManager.h"

#include <cmath>
#include <list>
#include <vector>

namespace WorldPathfinding {

namespace {

struct PathNode {
    Vector2Int tilePos = { 0, 0 };
    int32 parentIndex = -1;
    bool visited = false;
    float localCost = INFINITY;
    float globalCost = INFINITY;
};

bool IsOutOfBounds(const Vector2Int& tilePos, const Vector2Int& worldSize)
{
    return tilePos.x < 0 || tilePos.y < 0 || tilePos.x >= worldSize.x || tilePos.y >= worldSize.y;
}

bool IsObstacle(World* pWorld, GamePlayer* pPlayer, const Vector2Int& tilePos)
{
    TileInfo* pTile = pWorld->GetTileManager()->GetTile(tilePos.x, tilePos.y);
    if(!pTile) {
        return true;
    }

    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(pTile->GetDisplayedItem());
    if(!pItem) {
        return true;
    }

    if(pItem->collisionType == COLLISION_SOLID) {
        return true;
    }

    if(!pWorld->CanPlace(pPlayer, pTile) && !pTile->HasFlag(TILE_FLAG_IS_OPEN_TO_PUBLIC)) {
        return true;
    }

    return false;
}

float Heuristic(const Vector2Int& a, const Vector2Int& b)
{
    const float xDist = static_cast<float>(a.x - b.x);
    const float yDist = static_cast<float>(a.y - b.y);
    return std::sqrt((xDist * xDist) + (yDist * yDist));
}

int32 ToIndex(const Vector2Int& tilePos, int32 width)
{
    return tilePos.x + (tilePos.y * width);
}

Vector2Int ToTilePos(const Vector2Float& pixelPos)
{
    return {
        static_cast<int32>((pixelPos.x + 10.0f) / 32.0f),
        static_cast<int32>((pixelPos.y + 15.0f) / 32.0f)
    };
}

}

bool HasPath(World* pWorld, GamePlayer* pPlayer, const Vector2Float& currentPos, const Vector2Float& futurePos)
{
    if(!pWorld || !pPlayer) {
        return false;
    }

    if(pPlayer->GetCharData().HasPlayMod(PLAYMOD_TYPE_GHOST)) {
        return true;
    }

    const Vector2Int worldSize = pWorld->GetTileManager()->GetSize();
    if(worldSize.x <= 0 || worldSize.y <= 0) {
        return false;
    }

    const Vector2Float worldPixels = {
        static_cast<float>(worldSize.x * 32),
        static_cast<float>(worldSize.y * 32)
    };

    if(currentPos.x < 0.0f || currentPos.x >= worldPixels.x || currentPos.y < 0.0f || currentPos.y >= worldPixels.y) {
        return false;
    }

    if(futurePos.x < 0.0f || futurePos.x >= worldPixels.x || futurePos.y < 0.0f || futurePos.y >= worldPixels.y) {
        return false;
    }

    const Vector2Int currentTile = ToTilePos(currentPos);
    const Vector2Int futureTile = ToTilePos(futurePos);

    if(IsOutOfBounds(currentTile, worldSize) || IsOutOfBounds(futureTile, worldSize)) {
        return false;
    }

    if(IsObstacle(pWorld, pPlayer, currentTile) || IsObstacle(pWorld, pPlayer, futureTile)) {
        return false;
    }

    const int32 worldArea = worldSize.x * worldSize.y;
    const int32 startIndex = ToIndex(currentTile, worldSize.x);
    const int32 endIndex = ToIndex(futureTile, worldSize.x);

    if(startIndex == endIndex) {
        return true;
    }

    std::vector<PathNode> nodes(static_cast<size_t>(worldArea));
    for(int32 i = 0; i < worldArea; ++i) {
        nodes[(size_t)i].tilePos = { i % worldSize.x, i / worldSize.x };
        nodes[(size_t)i].parentIndex = -1;
        nodes[(size_t)i].visited = false;
        nodes[(size_t)i].localCost = INFINITY;
        nodes[(size_t)i].globalCost = INFINITY;
    }

    nodes[(size_t)startIndex].localCost = 0.0f;
    nodes[(size_t)startIndex].globalCost = Heuristic(currentTile, futureTile);

    int32 currentIndex = startIndex;
    std::list<int32> untestedNodes;
    untestedNodes.emplace_back(currentIndex);

    auto TryRelax = [&](const Vector2Int& nextPos) {
        if(IsOutOfBounds(nextPos, worldSize)) {
            return;
        }

        const int32 neighborIndex = ToIndex(nextPos, worldSize.x);
        PathNode& neighbor = nodes[(size_t)neighborIndex];
        if(neighbor.visited) {
            return;
        }

        if(IsObstacle(pWorld, pPlayer, nextPos)) {
            return;
        }

        untestedNodes.emplace_back(neighborIndex);

        const float candidate = nodes[(size_t)currentIndex].localCost + Heuristic(nodes[(size_t)currentIndex].tilePos, neighbor.tilePos);
        if(candidate < neighbor.localCost) {
            neighbor.parentIndex = currentIndex;
            neighbor.localCost = candidate;
            neighbor.globalCost = candidate + Heuristic(neighbor.tilePos, futureTile);
        }
    };

    while(!untestedNodes.empty() && currentIndex != endIndex) {
        untestedNodes.sort([&](int32 lhs, int32 rhs) {
            return nodes[(size_t)lhs].globalCost < nodes[(size_t)rhs].globalCost;
        });

        while(!untestedNodes.empty() && nodes[(size_t)untestedNodes.front()].visited) {
            untestedNodes.pop_front();
        }

        if(untestedNodes.empty()) {
            break;
        }

        currentIndex = untestedNodes.front();
        nodes[(size_t)currentIndex].visited = true;

        const Vector2Int pos = nodes[(size_t)currentIndex].tilePos;
        TryRelax({ pos.x, pos.y + 1 });
        TryRelax({ pos.x + 1, pos.y });
        TryRelax({ pos.x, pos.y - 1 });
        TryRelax({ pos.x - 1, pos.y });
    }

    int32 trace = endIndex;
    while(trace >= 0 && trace < worldArea && nodes[(size_t)trace].parentIndex != -1) {
        const int32 parent = nodes[(size_t)trace].parentIndex;
        if(parent == trace) {
            return false;
        }

        trace = parent;
    }

    return trace == startIndex;
}

}
