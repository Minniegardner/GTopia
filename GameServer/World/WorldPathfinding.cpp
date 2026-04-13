#include "WorldPathfinding.h"

#include "World.h"
#include "../Player/GamePlayer.h"
#include "Item/ItemInfoManager.h"

#include <algorithm>
#include <limits>

namespace WorldPathfinding {

namespace {

struct PathNode {
    int32 parent = -1;
    float localCost = std::numeric_limits<float>::infinity();
    float globalCost = std::numeric_limits<float>::infinity();
    bool visited = false;
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
    const float dx = static_cast<float>(a.x - b.x);
    const float dy = static_cast<float>(a.y - b.y);
    return std::sqrt((dx * dx) + (dy * dy));
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

    const Vector2Int worldSize = pWorld->GetTileManager()->GetSize();
    if(worldSize.x <= 0 || worldSize.y <= 0) {
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

    nodes[startIndex].localCost = 0.0f;
    nodes[startIndex].globalCost = Heuristic(currentTile, futureTile);

    std::vector<int32> openList;
    openList.push_back(startIndex);

    const int8 neighbors[4][2] = {
        { 0, 1 },
        { 1, 0 },
        { 0, -1 },
        { -1, 0 }
    };

    while(!openList.empty()) {
        std::sort(openList.begin(), openList.end(), [&](int32 lhs, int32 rhs) {
            return nodes[lhs].globalCost < nodes[rhs].globalCost;
        });

        while(!openList.empty() && nodes[openList.front()].visited) {
            openList.erase(openList.begin());
        }

        if(openList.empty()) {
            break;
        }

        const int32 currentIndex = openList.front();
        openList.erase(openList.begin());

        if(currentIndex == endIndex) {
            break;
        }

        nodes[currentIndex].visited = true;

        const Vector2Int currentTilePos = {
            currentIndex % worldSize.x,
            currentIndex / worldSize.x
        };

        for(const auto& delta : neighbors) {
            const Vector2Int neighborTilePos = {
                currentTilePos.x + delta[0],
                currentTilePos.y + delta[1]
            };

            if(IsOutOfBounds(neighborTilePos, worldSize)) {
                continue;
            }

            if(IsObstacle(pWorld, pPlayer, neighborTilePos)) {
                continue;
            }

            const int32 neighborIndex = ToIndex(neighborTilePos, worldSize.x);
            if(nodes[neighborIndex].visited) {
                continue;
            }

            openList.push_back(neighborIndex);

            const float localCost = nodes[currentIndex].localCost + Heuristic(currentTilePos, neighborTilePos);
            if(localCost < nodes[neighborIndex].localCost) {
                nodes[neighborIndex].parent = currentIndex;
                nodes[neighborIndex].localCost = localCost;
                nodes[neighborIndex].globalCost = localCost + Heuristic(neighborTilePos, futureTile);
            }
        }
    }

    if(endIndex < 0 || endIndex >= worldArea) {
        return false;
    }

    int32 trace = endIndex;
    while(nodes[trace].parent != -1) {
        if(nodes[trace].parent == trace) {
            return false;
        }

        trace = nodes[trace].parent;
        if(trace < 0 || trace >= worldArea) {
            return false;
        }
    }

    return trace == startIndex;
}

}
