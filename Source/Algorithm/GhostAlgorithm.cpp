#include "GhostAlgorithm.h"

#include "../Item/ItemUtils.h"
#include "../IO/Log.h"
#include "../Packet/GamePacket.h"
#include "../Packet/NetPacket.h"
#include "../Player/PlayMod.h"
#include "../Utils/Timer.h"
#include "../World/TileInfo.h"
#include "../../GameServer/Context.h"
#include "../../GameServer/Player/GamePlayer.h"
#include "../../GameServer/Server/GameServer.h"
#include "../../GameServer/World/World.h"

#include <array>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace {

constexpr uint64 GHOST_TICK_INTERVAL_MS = 300;
constexpr uint64 GHOST_HAUNTED_CHECK_MS = 60ull * 60ull * 1000ull;
constexpr uint64 GHOST_JAR_PULL_MS = 3000;
constexpr uint64 GHOST_JAR_RESOLVE_MS = 7000;
constexpr uint8 GHOST_MAX_NETWORK_ID = 255;

enum eGhostNpcType : uint8 {
    GHOST_NPC_TYPE_GHOST = 1,
    GHOST_NPC_TYPE_GHOST_JAR = 2,
    GHOST_NPC_TYPE_YELLOW_GHOST = 4,
    GHOST_NPC_TYPE_SHARK_GHOST = 6,
    GHOST_NPC_TYPE_WINTER_GHOST = 7,
    GHOST_NPC_TYPE_BOSS_GHOST = 11,
    GHOST_NPC_TYPE_YELLOW_CRAZY_GHOST = 12
};

enum eGhostNpcAction : uint8 {
    GHOST_NPC_ACTION_REMOVE = 1,
    GHOST_NPC_ACTION_ADD = 2,
    GHOST_NPC_ACTION_MOVE = 3
};

struct GhostEntity {
    uint8 networkID = 0;
    uint8 npcType = GHOST_NPC_TYPE_GHOST;
    bool isJar = false;

    Vector2Float pos = { 0.0f, 0.0f };
    Vector2Float lastPos = { 0.0f, 0.0f };
    Vector2Float destPos = { 0.0f, 0.0f };

    float speed = 0.0f;
    uint64 lastMoveMS = 0;
    uint64 spawnMS = 0;
    uint32 placedByUserID = 0;
};

struct GhostWorldState {
    uint64 lastTickMS = 0;
    uint64 lastHauntedCheckMS = 0;
    std::unordered_map<uint8, GhostEntity> entities;
};

std::unordered_map<uint32, GhostWorldState> s_worldStates;

bool IsGhostDebugEnabled()
{
    Context* pContext = GetContext();
    if(!pContext) {
        return false;
    }

    GameConfig* pConfig = pContext->GetGameConfig();
    return pConfig && pConfig->debug;
}

GhostWorldState& GetState(World* pWorld)
{
    return s_worldStates[pWorld->GetID()];
}

bool IsGhostNpcType(uint8 type)
{
    switch(type) {
        case GHOST_NPC_TYPE_GHOST:
        case GHOST_NPC_TYPE_YELLOW_GHOST:
        case GHOST_NPC_TYPE_SHARK_GHOST:
        case GHOST_NPC_TYPE_WINTER_GHOST:
        case GHOST_NPC_TYPE_BOSS_GHOST:
        case GHOST_NPC_TYPE_YELLOW_CRAZY_GHOST:
            return true;
        default:
            return false;
    }
}

uint8 RollGhostNpcType()
{
    const uint32 roll = (uint32)(rand() % 100);
    if(roll < 70) {
        return GHOST_NPC_TYPE_GHOST;
    }
    if(roll < 80) {
        return GHOST_NPC_TYPE_YELLOW_GHOST;
    }
    if(roll < 87) {
        return GHOST_NPC_TYPE_SHARK_GHOST;
    }
    if(roll < 94) {
        return GHOST_NPC_TYPE_WINTER_GHOST;
    }
    if(roll < 98) {
        return GHOST_NPC_TYPE_YELLOW_CRAZY_GHOST;
    }
    return GHOST_NPC_TYPE_BOSS_GHOST;
}

uint8 ResolveSpawnNpcType(uint8 requestedType)
{
    if(IsGhostNpcType(requestedType)) {
        return requestedType;
    }

    return RollGhostNpcType();
}

Vector2Float ClampWorldPos(World* pWorld, const Vector2Float& inPos)
{
    const Vector2Int size = pWorld->GetTileManager()->GetSize();

    Vector2Float outPos = inPos;
    outPos.x = std::max(1.0f, std::min(outPos.x, (float)(size.x * 32)));
    outPos.y = std::max(1.0f, std::min(outPos.y, (float)(size.y * 32)));
    return outPos;
}

uint64 GetTravelMS(const GhostEntity& entity)
{
    const float dx = entity.destPos.x - entity.lastPos.x;
    const float dy = entity.destPos.y - entity.lastPos.y;
    const float dist = std::sqrt((dx * dx) + (dy * dy));
    const float speed = std::max(1.0f, entity.speed);
    return (uint64)((dist / speed) * 1000.0f);
}

Vector2Float GetGhostLerpPos(const GhostEntity& entity, uint64 nowMS)
{
    const uint64 travelMS = GetTravelMS(entity);
    if(travelMS == 0) {
        return entity.destPos;
    }

    const float elapsed = (float)(nowMS - entity.lastMoveMS);
    float t = elapsed / (float)travelMS;
    if(t < 0.0f) {
        t = 0.0f;
    }
    else if(t > 1.0f) {
        t = 1.0f;
    }

    return {
        entity.lastPos.x + (entity.destPos.x - entity.lastPos.x) * t,
        entity.lastPos.y + (entity.destPos.y - entity.lastPos.y) * t
    };
}

uint8 AllocateNetworkID(const GhostWorldState& state)
{
    std::array<bool, 256> used{};
    for(const auto& [networkID, entity] : state.entities) {
        (void)entity;
        used[networkID] = true;
    }

    for(uint16 id = 1; id <= GHOST_MAX_NETWORK_ID; ++id) {
        if(!used[id]) {
            return (uint8)id;
        }
    }

    return 0;
}

void SendGhostNpcPacket(World* pWorld, const GhostEntity& entity, uint8 action, const Vector2Float& fromPos, const Vector2Float& toPos, float speed)
{
    if(!pWorld) {
        return;
    }

    GameUpdatePacket packet;
    packet.type = NET_GAME_PACKET_NPC;
    packet.field0 = entity.isJar ? GHOST_NPC_TYPE_GHOST_JAR : entity.npcType;
    packet.field1 = entity.networkID;
    packet.field2 = action;
    packet.posX = fromPos.x;
    packet.posY = fromPos.y;
    packet.field9 = toPos.x;
    packet.field10 = toPos.y;
    packet.field11 = speed;

    pWorld->SendGamePacketToAll(&packet);
}

void SendGhostMove(World* pWorld, GhostEntity& entity, const Vector2Float& newFrom, const Vector2Float& newDest, float newSpeed, uint64 nowMS)
{
    entity.pos = newFrom;
    entity.lastPos = newFrom;
    entity.destPos = newDest;
    entity.speed = newSpeed;
    entity.lastMoveMS = nowMS;

    SendGhostNpcPacket(pWorld, entity, GHOST_NPC_ACTION_MOVE, entity.lastPos, entity.destPos, entity.speed);
}

void RemoveEntity(World* pWorld, GhostWorldState& state, uint8 networkID)
{
    auto it = state.entities.find(networkID);
    if(it == state.entities.end()) {
        return;
    }

    SendGhostNpcPacket(pWorld, it->second, GHOST_NPC_ACTION_REMOVE, it->second.pos, it->second.pos, 0.0f);
    state.entities.erase(it);
}

bool AnyGhostLeft(const GhostWorldState& state)
{
    for(const auto& [_, entity] : state.entities) {
        if(!entity.isJar && IsGhostNpcType(entity.npcType)) {
            return true;
        }
    }

    return false;
}

void RefreshHauntedFlag(World* pWorld, const GhostWorldState& state)
{
    if(!pWorld) {
        return;
    }

    pWorld->SetWorldFlag(WORLD_FLAG_HAUNTED, AnyGhostLeft(state));
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

void SpawnHauntedGhost(World* pWorld, GhostWorldState& state)
{
    if(!pWorld) {
        return;
    }

    const Vector2Int size = pWorld->GetTileManager()->GetSize();
    if(size.x <= 0 || size.y <= 0) {
        return;
    }

    const uint8 networkID = AllocateNetworkID(state);
    if(networkID == 0) {
        return;
    }

    Vector2Float spawnPos;
    spawnPos.x = (float)(64 + (rand() % std::max<int32>(1, (size.x * 32) - 128)));
    spawnPos.y = (float)(64 + (rand() % std::max<int32>(1, (size.y * 32) - 128)));
    spawnPos = ClampWorldPos(pWorld, spawnPos);

    const uint64 nowMS = Time::GetSystemTime();

    GhostEntity entity;
    entity.networkID = networkID;
    entity.npcType = RollGhostNpcType();
    entity.isJar = false;
    entity.pos = spawnPos;
    entity.lastPos = spawnPos;
    entity.destPos = spawnPos;
    entity.speed = 23.0f;
    entity.lastMoveMS = nowMS;
    entity.spawnMS = nowMS;

    state.entities.insert_or_assign(entity.networkID, entity);
    pWorld->SetWorldFlag(WORLD_FLAG_HAUNTED, true);

    SendGhostNpcPacket(pWorld, entity, GHOST_NPC_ACTION_ADD, entity.pos, entity.pos, entity.speed);
}

void PullGhostsTowardLine(World* pWorld, GhostWorldState& state, const Vector2Float& startPos, const Vector2Float& endPos, float threshold, float ratio, float speed, uint64 nowMS)
{
    if(!pWorld) {
        return;
    }

    const float lineLength = std::sqrt(
        std::pow(endPos.x - startPos.x, 2.0f) +
        std::pow(endPos.y - startPos.y, 2.0f)
    );

    for(auto& [_, entity] : state.entities) {
        if(entity.isJar || !IsGhostNpcType(entity.npcType)) {
            continue;
        }

        const Vector2Float currentPos = GetGhostLerpPos(entity, nowMS);
        const float d1 = std::sqrt(std::pow(currentPos.x - startPos.x, 2.0f) + std::pow(currentPos.y - startPos.y, 2.0f));
        const float d2 = std::sqrt(std::pow(currentPos.x - endPos.x, 2.0f) + std::pow(currentPos.y - endPos.y, 2.0f));
        const float distance = std::abs((d1 + d2) - lineLength);

        if(distance >= threshold) {
            continue;
        }

        Vector2Float newPos = {
            currentPos.x + ratio * (startPos.x - currentPos.x),
            currentPos.y + ratio * (startPos.y - currentPos.y)
        };

        newPos = ClampWorldPos(pWorld, newPos);
        SendGhostMove(pWorld, entity, currentPos, newPos, speed, nowMS);
    }
}

void PullGhostsTowardJar(World* pWorld, GhostWorldState& state, const Vector2Float& jarPos, uint64 nowMS)
{
    if(!pWorld) {
        return;
    }

    const Vector2Float jarPosTop = { jarPos.x, jarPos.y - (4.0f * 32.0f) };
    const float lineLength = std::sqrt(
        std::pow((jarPosTop.x + 16.0f) - jarPos.x, 2.0f) +
        std::pow((jarPosTop.y + 16.0f) - jarPos.y, 2.0f)
    );

    for(auto& [_, entity] : state.entities) {
        if(entity.isJar || !IsGhostNpcType(entity.npcType)) {
            continue;
        }

        const Vector2Float currentPos = GetGhostLerpPos(entity, nowMS);
        const float d1 = std::sqrt(std::pow(currentPos.x - jarPos.x, 2.0f) + std::pow(currentPos.y - jarPos.y, 2.0f));
        const float d2 = std::sqrt(std::pow(currentPos.x - jarPosTop.x, 2.0f) + std::pow(currentPos.y - jarPosTop.y, 2.0f));
        const float distance = std::abs((d1 + d2) - lineLength);
        if(distance >= 40.0f) {
            continue;
        }

        SendGhostMove(pWorld, entity, currentPos, jarPos, 23.0f, nowMS);
    }
}

void ResolveJar(World* pWorld, GhostWorldState& state, const GhostEntity& jar)
{
    if(!pWorld) {
        return;
    }

    uint8 capturedGhostID = 0;

    for(const auto& [networkID, entity] : state.entities) {
        if(entity.isJar || !IsGhostNpcType(entity.npcType)) {
            continue;
        }

        const Vector2Float ghostPos = GetGhostLerpPos(entity, Time::GetSystemTime());
        const float xDist = std::abs(ghostPos.x - jar.pos.x);
        const float yDist = std::abs(ghostPos.y - jar.pos.y);
        if(xDist < 32.0f && yDist < 64.0f) {
            capturedGhostID = networkID;
            break;
        }
    }

    if(capturedGhostID == 0) {
        if(IsGhostDebugEnabled()) {
            LOGGER_LOG_DEBUG("GhostDebug: jar netID=%u in world %u resolved without capture", (uint32)jar.networkID, pWorld->GetID());
        }
        return;
    }

    RemoveEntity(pWorld, state, capturedGhostID);
    if(IsGhostDebugEnabled()) {
        LOGGER_LOG_DEBUG("GhostDebug: jar netID=%u captured ghost netID=%u in world %u",
            (uint32)jar.networkID, (uint32)capturedGhostID, pWorld->GetID());
    }
    pWorld->SendParticleEffectToAll(jar.pos.x, jar.pos.y, 44, 0, 0);
    pWorld->PlaySFXForEveryone("terraform.wav", 0);

    GamePlayer* pPlacer = GetGameServer()->GetPlayerByUserID(jar.placedByUserID);
    if(!pPlacer) {
        RefreshHauntedFlag(pWorld, state);
        return;
    }

    pPlacer->IncreaseStat("CAPTURED_GHOSTS");
    pPlacer->SendOnTalkBubble("`3I caught a ghost!", true);
    pPlacer->SendOnConsoleMessage("`3I caught a ghost!");

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

    RefreshHauntedFlag(pWorld, state);
}

void ClearWorldGhostsInternal(World* pWorld, GhostWorldState& state)
{
    if(!pWorld) {
        return;
    }

    std::vector<uint8> toRemove;
    toRemove.reserve(state.entities.size());

    for(const auto& [networkID, entity] : state.entities) {
        (void)entity;
        toRemove.push_back(networkID);
    }

    for(uint8 networkID : toRemove) {
        RemoveEntity(pWorld, state, networkID);
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
        if(IsGhostDebugEnabled()) {
            LOGGER_LOG_DEBUG("GhostDebug: place jar denied at %d,%d in world %u because tile is not blank (fg=%u)",
                pTile->GetPos().x, pTile->GetPos().y, pWorld->GetID(), (uint32)pTile->GetDisplayedItem());
        }
        return false;
    }

    GhostWorldState& state = GetState(pWorld);

    const uint8 networkID = AllocateNetworkID(state);
    if(networkID == 0) {
        pPlayer->SendOnConsoleMessage("`4Too many ghosts/jars in this world.");
        if(IsGhostDebugEnabled()) {
            LOGGER_LOG_DEBUG("GhostDebug: place jar denied in world %u due to NPC network-id exhaustion", pWorld->GetID());
        }
        return false;
    }

    const uint64 nowMS = Time::GetSystemTime();

    GhostEntity jar;
    jar.networkID = networkID;
    jar.npcType = GHOST_NPC_TYPE_GHOST_JAR;
    jar.isJar = true;
    jar.pos = { (pTile->GetPos().x * 32.0f) + 16.0f, (pTile->GetPos().y * 32.0f) + 10.0f };
    jar.lastPos = jar.pos;
    jar.destPos = jar.pos;
    jar.speed = 0.0f;
    jar.lastMoveMS = nowMS;
    jar.spawnMS = nowMS;
    jar.placedByUserID = pPlayer->GetUserID();

    state.entities.insert_or_assign(jar.networkID, jar);
    SendGhostNpcPacket(pWorld, jar, GHOST_NPC_ACTION_ADD, jar.pos, jar.pos, 0.0f);

    if(IsGhostDebugEnabled()) {
        LOGGER_LOG_DEBUG("GhostDebug: placed jar netID=%u world=%u byUser=%u at %.1f,%.1f",
            (uint32)jar.networkID, pWorld->GetID(), jar.placedByUserID, jar.pos.x, jar.pos.y);
    }

    const uint8 gemsToDrop = (uint8)(rand() % 7);
    if(gemsToDrop > 0) {
        WorldObject gemDrop;
        gemDrop.itemID = ITEM_ID_GEMS;
        gemDrop.count = gemsToDrop;
        pWorld->DropObject(pTile, gemDrop, true);
    }

    return true;
}

bool SpawnGhostAt(World* pWorld, const Vector2Float& worldPos, uint8 requestedType)
{
    if(!pWorld) {
        return false;
    }

    GhostWorldState& state = GetState(pWorld);

    const uint8 networkID = AllocateNetworkID(state);
    if(networkID == 0) {
        return false;
    }

    const Vector2Float clampedPos = ClampWorldPos(pWorld, worldPos);
    const uint64 nowMS = Time::GetSystemTime();

    GhostEntity ghost;
    ghost.networkID = networkID;
    ghost.npcType = ResolveSpawnNpcType(requestedType);
    ghost.isJar = false;
    ghost.pos = clampedPos;
    ghost.lastPos = clampedPos;
    ghost.destPos = clampedPos;
    ghost.speed = 23.0f;
    ghost.lastMoveMS = nowMS;
    ghost.spawnMS = nowMS;

    state.entities.insert_or_assign(ghost.networkID, ghost);
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
    return SpawnGhostAt(pWorld, { pos.x + 16.0f, pos.y + 16.0f }, 0);
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

    PullGhostsTowardLine(pWorld, state, playerPos, beamTilePos, 20.0f, 0.6f, 40.0f, Time::GetSystemTime());
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

    const GhostWorldState& state = GetState(pWorld);
    return AnyGhostLeft(state);
}

void UpdateWorldGhosts(World* pWorld)
{
    if(!pWorld) {
        return;
    }

    GhostWorldState& state = GetState(pWorld);
    const uint64 nowMS = Time::GetSystemTime();

    if(state.lastTickMS != 0 && nowMS - state.lastTickMS < GHOST_TICK_INTERVAL_MS) {
        return;
    }
    state.lastTickMS = nowMS;

    if(state.lastHauntedCheckMS == 0 || nowMS - state.lastHauntedCheckMS >= GHOST_HAUNTED_CHECK_MS) {
        state.lastHauntedCheckMS = nowMS;

        if(pWorld->GetPlayerCount() >= 1 && (rand() % 40) == 0) {
            size_t ghostCount = 0;
            for(const auto& [_, entity] : state.entities) {
                if(!entity.isJar && IsGhostNpcType(entity.npcType)) {
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

    std::vector<uint8> jarsToRemove;
    jarsToRemove.reserve(8);

    for(auto& [networkID, entity] : state.entities) {
        if(entity.isJar) {
            const uint64 elapsed = nowMS - entity.spawnMS;

            if(elapsed >= GHOST_JAR_RESOLVE_MS) {
                ResolveJar(pWorld, state, entity);
                jarsToRemove.push_back(networkID);
                continue;
            }

            if(elapsed >= GHOST_JAR_PULL_MS) {
                PullGhostsTowardJar(pWorld, state, entity.pos, nowMS);
            }

            continue;
        }

        if(!IsGhostNpcType(entity.npcType)) {
            continue;
        }

        const Vector2Float currentPos = GetGhostLerpPos(entity, nowMS);
        entity.pos = currentPos;

        const uint64 travelMS = GetTravelMS(entity);
        if(travelMS == 0 || nowMS - entity.lastMoveMS >= travelMS) {
            entity.lastPos = currentPos;
            entity.lastMoveMS = nowMS;
            entity.speed = (float)(18 + (rand() % 39));

            Vector2Float newPos = {
                currentPos.x + (float)((rand() % (6 * 32 + 1)) - (3 * 32)),
                currentPos.y + (float)((rand() % (6 * 32 + 1)) - (3 * 32))
            };
            entity.destPos = ClampWorldPos(pWorld, newPos);

            SendGhostNpcPacket(pWorld, entity, GHOST_NPC_ACTION_MOVE, entity.lastPos, entity.destPos, entity.speed);
            continue;
        }

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

    for(uint8 networkID : jarsToRemove) {
        RemoveEntity(pWorld, state, networkID);
    }

    RefreshHauntedFlag(pWorld, state);
}

void SyncWorldGhostsToPlayer(World* pWorld, GamePlayer* pPlayer)
{
    if(!pWorld || !pPlayer || pPlayer->GetCurrentWorld() != pWorld->GetID()) {
        return;
    }

    GhostWorldState& state = GetState(pWorld);
    if(state.entities.empty()) {
        return;
    }

    const uint64 nowMS = Time::GetSystemTime();

    std::vector<uint8> staleJars;
    for(const auto& [networkID, entity] : state.entities) {
        if(entity.isJar && nowMS - entity.spawnMS >= GHOST_JAR_RESOLVE_MS) {
            staleJars.push_back(networkID);
        }
    }

    for(uint8 networkID : staleJars) {
        RemoveEntity(pWorld, state, networkID);
    }

    for(const auto& [_, entity] : state.entities) {
        const Vector2Float pos = GetGhostLerpPos(entity, nowMS);

        GameUpdatePacket packet;
        packet.type = NET_GAME_PACKET_NPC;
        packet.field0 = entity.isJar ? GHOST_NPC_TYPE_GHOST_JAR : entity.npcType;
        packet.field1 = entity.networkID;
        packet.field2 = GHOST_NPC_ACTION_ADD;
        packet.posX = pos.x;
        packet.posY = pos.y;

        SendENetPacketRaw(NET_MESSAGE_GAME_PACKET, &packet, sizeof(GameUpdatePacket), nullptr, pPlayer->GetPeer());

        if(entity.isJar) {
            continue;
        }

        const uint64 travelMS = GetTravelMS(entity);
        if(travelMS == 0 || nowMS - entity.lastMoveMS >= travelMS) {
            continue;
        }

        GameUpdatePacket movePacket;
        movePacket.type = NET_GAME_PACKET_NPC;
        movePacket.field0 = entity.npcType;
        movePacket.field1 = entity.networkID;
        movePacket.field2 = GHOST_NPC_ACTION_MOVE;
        movePacket.posX = entity.lastPos.x;
        movePacket.posY = entity.lastPos.y;
        movePacket.field9 = entity.destPos.x;
        movePacket.field10 = entity.destPos.y;
        movePacket.field11 = entity.speed;

        SendENetPacketRaw(NET_MESSAGE_GAME_PACKET, &movePacket, sizeof(GameUpdatePacket), nullptr, pPlayer->GetPeer());
    }

    RefreshHauntedFlag(pWorld, state);
}

}
