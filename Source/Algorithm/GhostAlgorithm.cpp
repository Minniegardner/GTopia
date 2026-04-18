#include "GhostAlgorithm.h"

#include "../Item/ItemUtils.h"
#include "../Packet/GamePacket.h"
#include "../Player/PlayMod.h"
#include "../Utils/Timer.h"
#include "../World/TileInfo.h"
#include "../../GameServer/Player/GamePlayer.h"
#include "../../GameServer/Server/GameServer.h"
#include "../../GameServer/World/World.h"

#include <cmath>
#include <unordered_map>

namespace {

enum eGhostNpcType : uint8 {
    GHOST_NPC_TYPE_NORMAL = 1,
    GHOST_NPC_TYPE_JAR = 2
};

enum eGhostNpcAction : uint8 {
    GHOST_NPC_ACTION_REMOVE = 1,
    GHOST_NPC_ACTION_ADD = 2,
    GHOST_NPC_ACTION_MOVE = 3
};

struct GhostEntity {
    uint32 id = 0;
    bool jar = false;
    Vector2Float pos = {0.0f, 0.0f};
    Vector2Float lastPos = {0.0f, 0.0f};
    Vector2Float destPos = {0.0f, 0.0f};
    float speed = 0.0f;
    uint64 lastMoveMS = 0;
    uint64 spawnMS = 0;
    uint32 placedByUserID = 0;
};

struct GhostWorldState {
    uint32 nextEntityID = 1;
    uint64 lastTickMS = 0;
    uint64 lastHauntedCheckMS = 0;
    std::unordered_map<uint32, GhostEntity> entities;
};

std::unordered_map<uint32, GhostWorldState> s_worldStates;

GhostWorldState& GetState(World* pWorld)
{
    return s_worldStates[pWorld->GetID()];
}

Vector2Float ClampWorldPos(World* pWorld, const Vector2Float& inPos)
{
    const Vector2Int size = pWorld->GetTileManager()->GetSize();

    Vector2Float outPos = inPos;
    outPos.x = std::max(1.0f, std::min(outPos.x, (float)(size.x * 32)));
    outPos.y = std::max(1.0f, std::min(outPos.y, (float)(size.y * 32)));
    return outPos;
}

Vector2Float GetGhostLerpPos(const GhostEntity& entity, uint64 nowMS)
{
    const float dx = entity.destPos.x - entity.lastPos.x;
    const float dy = entity.destPos.y - entity.lastPos.y;
    const float dist = std::sqrt((dx * dx) + (dy * dy));
    const float speed = std::max(1.0f, entity.speed);
    const float totalMS = (dist / speed) * 1000.0f;
    if(totalMS <= 0.0f) {
        return entity.destPos;
    }

    const float elapsed = (float)(nowMS - entity.lastMoveMS);
    float t = elapsed / totalMS;
    if(t < 0.0f) {
        t = 0.0f;
    }
    else if(t > 1.0f) {
        t = 1.0f;
    }

    return {
        entity.lastPos.x + (t * dx),
        entity.lastPos.y + (t * dy)
    };
}

void SendGhostNpcPacket(World* pWorld, const GhostEntity& entity, uint8 action, const Vector2Float& fromPos, const Vector2Float& toPos, float speed)
{
    if(!pWorld) {
        return;
    }

    GameUpdatePacket packet;
    packet.type = NET_GAME_PACKET_NPC;
    packet.field0 = action;
    packet.field1 = entity.jar ? GHOST_NPC_TYPE_JAR : GHOST_NPC_TYPE_NORMAL;
    packet.netID = (int32)entity.id;
    packet.posX = fromPos.x;
    packet.posY = fromPos.y;
    packet.field9 = toPos.x;
    packet.field10 = toPos.y;
    packet.field11 = speed;

    pWorld->SendGamePacketToAll(&packet);
}

void SpawnHauntedGhost(World* pWorld, GhostWorldState& state)
{
    if(!pWorld) {
        return;
    }

    const Vector2Int size = pWorld->GetTileManager()->GetSize();
    if(size.x <= 0 || size.y <= 0) {
        return;
    }

    Vector2Float pos;
    pos.x = (float)(64 + (rand() % std::max<int32>(1, (size.x * 32) - 128)));
    pos.y = (float)(64 + (rand() % std::max<int32>(1, (size.y * 32) - 128)));

    GhostEntity entity;
    entity.id = state.nextEntityID++;
    entity.jar = false;
    entity.pos = ClampWorldPos(pWorld, pos);
    entity.lastPos = entity.pos;
    entity.destPos = entity.pos;
    entity.speed = 23.0f;
    entity.lastMoveMS = Time::GetSystemTime();
    entity.spawnMS = entity.lastMoveMS;

    state.entities.insert_or_assign(entity.id, entity);
    pWorld->SetWorldFlag(WORLD_FLAG_HAUNTED, true);

    SendGhostNpcPacket(pWorld, entity, GHOST_NPC_ACTION_ADD, entity.pos, entity.pos, entity.speed);
}

bool AnyGhostLeft(const GhostWorldState& state)
{
    for(const auto& [_, entity] : state.entities) {
        if(!entity.jar) {
            return true;
        }
    }

    return false;
}

bool PointInsideGhostHitbox(const Vector2Float& point, const Vector2Float& ghostPos)
{
    return
        point.x > ghostPos.x - 8.0f && point.x < ghostPos.x + 10.0f &&
        point.y > ghostPos.y - 2.0f && point.y < ghostPos.y + 16.0f;
}

bool PlayerTouchesGhost(GamePlayer* pPlayer, const Vector2Float& ghostPos)
{
    if(!pPlayer) {
        return false;
    }

    const Vector2Float pos = pPlayer->GetWorldPos();
    const Vector2Float topLeft = { pos.x, pos.y };
    const Vector2Float topRight = { pos.x + 20.0f, pos.y };
    const Vector2Float bottomLeft = { pos.x, pos.y + 30.0f };
    const Vector2Float bottomRight = { pos.x + 20.0f, pos.y + 30.0f };

    return
        PointInsideGhostHitbox(topLeft, ghostPos) ||
        PointInsideGhostHitbox(topRight, ghostPos) ||
        PointInsideGhostHitbox(bottomLeft, ghostPos) ||
        PointInsideGhostHitbox(bottomRight, ghostPos);
}

void TrySlimePlayer(World* pWorld, GamePlayer* pPlayer)
{
    if(!pWorld || !pPlayer) {
        return;
    }

    if(pPlayer->GetCharData().HasPlayMod(PLAYMOD_TYPE_SLIMED)) {
        return;
    }

    pPlayer->AddPlayMod(PLAYMOD_TYPE_SLIMED);

    if((rand() % 100) < 50) {
        pPlayer->SendOnTalkBubble("I've been slimed!", false);
    }
    else {
        pPlayer->SendOnTalkBubble("`2AYIEE! A ghost!", false);
    }

    const Vector2Float pos = pPlayer->GetWorldPos();
    pWorld->SendParticleEffectToAll(pos.x + 12.0f, pos.y + 15.0f, 195, 0, 0);
}

void RemoveEntity(World* pWorld, GhostWorldState& state, uint32 entityID)
{
    auto it = state.entities.find(entityID);
    if(it == state.entities.end()) {
        return;
    }

    SendGhostNpcPacket(pWorld, it->second, GHOST_NPC_ACTION_REMOVE, it->second.pos, it->second.pos, 0.0f);
    state.entities.erase(it);
}

void HandleGhostJarCapture(World* pWorld, GhostWorldState& state, const GhostEntity& jarEntity)
{
    if(!pWorld) {
        return;
    }

    GamePlayer* pPlacer = GetGameServer()->GetPlayerByUserID(jarEntity.placedByUserID);
    if(!pPlacer || pPlacer->GetCurrentWorld() != pWorld->GetID()) {
        return;
    }

    uint32 ghostToCaptureID = 0;
    for(const auto& [entityID, entity] : state.entities) {
        if(entity.jar) {
            continue;
        }

        const Vector2Float ghostPos = GetGhostLerpPos(entity, Time::GetSystemTime());
        const float xDist = std::abs(ghostPos.x - jarEntity.pos.x);
        const float yDist = std::abs(ghostPos.y - jarEntity.pos.y);
        if(xDist < 32.0f && yDist < 64.0f) {
            ghostToCaptureID = entityID;
            break;
        }
    }

    if(ghostToCaptureID == 0) {
        return;
    }

    RemoveEntity(pWorld, state, ghostToCaptureID);

    if(!AnyGhostLeft(state)) {
        pWorld->SetWorldFlag(WORLD_FLAG_HAUNTED, false);
    }

    pPlacer->IncreaseStat("CAPTURED_GHOSTS");
    pPlacer->SendOnTalkBubble("`3I caught a ghost!", true);
    pPlacer->SendOnConsoleMessage("`3I caught a ghost!");

    pWorld->SendParticleEffectToAll(jarEntity.pos.x, jarEntity.pos.y, 44, 0, 0);

    if(!pPlacer->GetInventory().HaveRoomForItem(ITEM_ID_GHOST_IN_A_JAR, 1)) {
        WorldObject obj;
        obj.itemID = ITEM_ID_GHOST_IN_A_JAR;
        obj.count = 1;
        obj.pos = pPlacer->GetWorldPos();
        pWorld->DropObject(obj);
    }
    else {
        pPlacer->ModifyInventoryItem(ITEM_ID_GHOST_IN_A_JAR, 1);
    }
}

void ClearWorldGhostsInternal(World* pWorld, GhostWorldState& state)
{
    if(!pWorld) {
        return;
    }

    std::vector<uint32> entityIDs;
    entityIDs.reserve(state.entities.size());
    for(const auto& [entityID, entity] : state.entities) {
        (void)entity;
        entityIDs.push_back(entityID);
    }

    for(uint32 entityID : entityIDs) {
        RemoveEntity(pWorld, state, entityID);
    }

    pWorld->SetWorldFlag(WORLD_FLAG_HAUNTED, false);
}

}

namespace GhostAlgorithm {

bool PlaceGhostJar(GamePlayer* pPlayer, World* pWorld, TileInfo* pTile)
{
    if(!pPlayer || !pWorld || !pTile) {
        return false;
    }

    if(pTile->GetDisplayedItem() != ITEM_ID_BLANK) {
        return false;
    }

    GhostWorldState& state = GetState(pWorld);

    GhostEntity jarEntity;
    jarEntity.id = state.nextEntityID++;
    jarEntity.jar = true;
    jarEntity.pos = { (pTile->GetPos().x * 32.0f) + 16.0f, (pTile->GetPos().y * 32.0f) + 10.0f };
    jarEntity.lastPos = jarEntity.pos;
    jarEntity.destPos = jarEntity.pos;
    jarEntity.speed = 23.0f;
    jarEntity.lastMoveMS = Time::GetSystemTime();
    jarEntity.spawnMS = jarEntity.lastMoveMS;
    jarEntity.placedByUserID = pPlayer->GetUserID();

    state.entities.insert_or_assign(jarEntity.id, jarEntity);
    SendGhostNpcPacket(pWorld, jarEntity, GHOST_NPC_ACTION_ADD, jarEntity.pos, jarEntity.pos, jarEntity.speed);

    const uint8 gemsToDrop = (uint8)(rand() % 7);
    if(gemsToDrop > 0) {
        WorldObject gemDrop;
        gemDrop.itemID = ITEM_ID_GEMS;
        gemDrop.count = gemsToDrop;
        pWorld->DropObject(pTile, gemDrop, true);
    }

    return true;
}

bool SpawnGhostAt(World* pWorld, const Vector2Float& worldPos)
{
    if(!pWorld) {
        return false;
    }

    GhostWorldState& state = GetState(pWorld);

    GhostEntity ghost;
    ghost.id = state.nextEntityID++;
    ghost.jar = false;
    ghost.pos = ClampWorldPos(pWorld, worldPos);
    ghost.lastPos = ghost.pos;
    ghost.destPos = ghost.pos;
    ghost.speed = 23.0f;
    ghost.lastMoveMS = Time::GetSystemTime();
    ghost.spawnMS = ghost.lastMoveMS;

    state.entities.insert_or_assign(ghost.id, ghost);
    pWorld->SetWorldFlag(WORLD_FLAG_HAUNTED, true);

    SendGhostNpcPacket(pWorld, ghost, GHOST_NPC_ACTION_ADD, ghost.pos, ghost.pos, ghost.speed);
    return true;
}

bool SpawnGhostNearPlayer(GamePlayer* pPlayer, World* pWorld)
{
    if(!pPlayer || !pWorld) {
        return false;
    }

    const Vector2Float pos = pPlayer->GetWorldPos();
    return SpawnGhostAt(pWorld, { pos.x + 16.0f, pos.y + 16.0f });
}

void PullGhostsTowardPlayer(World* pWorld, const Vector2Float& playerPos, const Vector2Float& beamTilePos)
{
    if(!pWorld) {
        return;
    }

    GhostWorldState& state = GetState(pWorld);
    if(state.entities.empty()) {
        return;
    }

    const uint64 nowMS = Time::GetSystemTime();
    const float lineLength = std::sqrt(
        std::pow((beamTilePos.x + 16.0f) - playerPos.x, 2.0f) +
        std::pow((beamTilePos.y + 16.0f) - playerPos.y, 2.0f)
    );

    for(auto& [entityID, entity] : state.entities) {
        (void)entityID;
        if(entity.jar) {
            continue;
        }

        const Vector2Float currentPos = GetGhostLerpPos(entity, nowMS);
        const float d1 = std::sqrt(std::pow(currentPos.x - playerPos.x, 2.0f) + std::pow(currentPos.y - playerPos.y, 2.0f));
        const float d2 = std::sqrt(std::pow(currentPos.x - beamTilePos.x, 2.0f) + std::pow(currentPos.y - beamTilePos.y, 2.0f));
        const float distance = std::abs((d1 + d2) - lineLength);

        if(distance >= 20.0f) {
            continue;
        }

        const float ratio = 0.6f;
        Vector2Float newPos = {
            currentPos.x + ratio * (playerPos.x - currentPos.x),
            currentPos.y + ratio * (playerPos.y - currentPos.y)
        };
        newPos = ClampWorldPos(pWorld, newPos);

        entity.pos = currentPos;
        entity.lastPos = currentPos;
        entity.destPos = newPos;
        entity.lastMoveMS = nowMS;
        entity.speed = 40.0f;

        SendGhostNpcPacket(pWorld, entity, GHOST_NPC_ACTION_MOVE, entity.lastPos, entity.destPos, entity.speed);
    }
}

void ClearWorldGhosts(World* pWorld)
{
    if(!pWorld) {
        return;
    }

    GhostWorldState& state = GetState(pWorld);
    ClearWorldGhostsInternal(pWorld, state);
}

void DestroyWorldState(uint32 worldID)
{
    s_worldStates.erase(worldID);
}

bool HasWorldGhosts(World* pWorld)
{
    if(!pWorld) {
        return false;
    }

    GhostWorldState& state = GetState(pWorld);
    for(const auto& [_, entity] : state.entities) {
        if(!entity.jar) {
            return true;
        }
    }

    return false;
}

void UpdateWorldGhosts(World* pWorld)
{
    if(!pWorld) {
        return;
    }

    GhostWorldState& state = GetState(pWorld);
    const uint64 nowMS = Time::GetSystemTime();

    if(state.lastTickMS != 0 && nowMS - state.lastTickMS < 300) {
        return;
    }
    state.lastTickMS = nowMS;

    if(state.lastHauntedCheckMS == 0 || nowMS - state.lastHauntedCheckMS >= (60ull * 60ull * 1000ull)) {
        state.lastHauntedCheckMS = nowMS;

        if(pWorld->GetPlayerCount() >= 1 && (rand() % 40) == 0) {
            size_t ghostCount = 0;
            for(const auto& [_, entity] : state.entities) {
                if(!entity.jar) {
                    ++ghostCount;
                }
            }

            if(state.entities.size() < 50 && ghostCount < 25) {
                SpawnHauntedGhost(pWorld, state);
            }
        }
    }

    if(state.entities.empty()) {
        if(pWorld->HasWorldFlag(WORLD_FLAG_HAUNTED)) {
            pWorld->SetWorldFlag(WORLD_FLAG_HAUNTED, false);
        }
        return;
    }

    std::vector<uint32> toRemove;
    toRemove.reserve(8);

    for(auto& [entityID, entity] : state.entities) {
        if(!entity.jar) {
            const Vector2Float currentPos = GetGhostLerpPos(entity, nowMS);
            entity.pos = currentPos;

            const float dx = entity.destPos.x - entity.lastPos.x;
            const float dy = entity.destPos.y - entity.lastPos.y;
            const float dist = std::sqrt((dx * dx) + (dy * dy));
            const float speed = std::max(1.0f, entity.speed);
            const uint64 travelMS = (uint64)((dist / speed) * 1000.0f);

            if(travelMS == 0 || nowMS - entity.lastMoveMS >= travelMS) {
                entity.lastPos = currentPos;
                entity.lastMoveMS = nowMS;
                entity.speed = (float)(18 + (rand() % 39));

                Vector2Float nextPos = {
                    currentPos.x + (float)((rand() % (6 * 32 + 1)) - (3 * 32)),
                    currentPos.y + (float)((rand() % (6 * 32 + 1)) - (3 * 32))
                };
                entity.destPos = ClampWorldPos(pWorld, nextPos);

                SendGhostNpcPacket(pWorld, entity, GHOST_NPC_ACTION_MOVE, entity.lastPos, entity.destPos, entity.speed);
            }
            else {
                const auto& players = pWorld->GetPlayers();
                for(GamePlayer* pPlayer : players) {
                    if(!pPlayer) {
                        continue;
                    }

                    if(PlayerTouchesGhost(pPlayer, currentPos)) {
                        TrySlimePlayer(pWorld, pPlayer);
                    }
                }
            }
        }
        else {
            const uint64 elapsed = nowMS - entity.spawnMS;
            if(elapsed >= 7000) {
                HandleGhostJarCapture(pWorld, state, entity);
                toRemove.push_back(entityID);
                continue;
            }

            if(elapsed >= 3000) {
                const Vector2Float jarPos = entity.pos;
                const Vector2Float jarTop = { jarPos.x, jarPos.y - (4.0f * 32.0f) };

                for(auto& [ghostID, ghostEntity] : state.entities) {
                    if(ghostEntity.jar) {
                        continue;
                    }

                    const Vector2Float currentGhostPos = GetGhostLerpPos(ghostEntity, nowMS);
                    const float lineLength = std::sqrt(std::pow((jarTop.x + 16.0f) - jarPos.x, 2.0f) + std::pow((jarTop.y + 16.0f) - jarPos.y, 2.0f));
                    const float d1 = std::sqrt(std::pow(currentGhostPos.x - jarPos.x, 2.0f) + std::pow(currentGhostPos.y - jarPos.y, 2.0f));
                    const float d2 = std::sqrt(std::pow(currentGhostPos.x - jarTop.x, 2.0f) + std::pow(currentGhostPos.y - jarTop.y, 2.0f));
                    const float distance = std::abs((d1 + d2) - lineLength);

                    if(distance < 40.0f) {
                        ghostEntity.lastPos = currentGhostPos;
                        ghostEntity.destPos = jarPos;
                        ghostEntity.lastMoveMS = nowMS;
                        ghostEntity.speed = 23.0f;
                        ghostEntity.pos = currentGhostPos;

                        SendGhostNpcPacket(pWorld, ghostEntity, GHOST_NPC_ACTION_MOVE, ghostEntity.lastPos, ghostEntity.destPos, ghostEntity.speed);
                    }
                }
            }
        }
    }

    for(uint32 entityID : toRemove) {
        RemoveEntity(pWorld, state, entityID);
    }

    if(!AnyGhostLeft(state)) {
        pWorld->SetWorldFlag(WORLD_FLAG_HAUNTED, false);
    }
}

}
