#include "World.h"
#include "IO/Log.h"
#include "../Player/GamePlayer.h"
#include "Packet/NetPacket.h"
#include "Item/ItemInfoManager.h"
#include "Math/Rect.h"
#include "Math/Math.h"
#include "Utils/StringUtils.h"
#include "../Server/MasterBroadway.h"
#include "../Server/GameServer.h"
#include <array>

#include "IO/File.h"

namespace {

enum eSteamDirection {
    STEAM_DIR_NONE,
    STEAM_DIR_UP,
    STEAM_DIR_RIGHT,
    STEAM_DIR_DOWN,
    STEAM_DIR_LEFT
};

bool IsReverseDirection(eSteamDirection lastDir, eSteamDirection nextDir)
{
    return
        (lastDir == STEAM_DIR_UP && nextDir == STEAM_DIR_DOWN) ||
        (lastDir == STEAM_DIR_DOWN && nextDir == STEAM_DIR_UP) ||
        (lastDir == STEAM_DIR_LEFT && nextDir == STEAM_DIR_RIGHT) ||
        (lastDir == STEAM_DIR_RIGHT && nextDir == STEAM_DIR_LEFT);
}

TileInfo* GetTileByDirection(World* pWorld, const Vector2Int& pos, eSteamDirection dir)
{
    if(!pWorld) {
        return nullptr;
    }

    switch(dir) {
        case STEAM_DIR_UP: return pWorld->GetTileManager()->GetTile(pos.x, pos.y - 1);
        case STEAM_DIR_RIGHT: return pWorld->GetTileManager()->GetTile(pos.x + 1, pos.y);
        case STEAM_DIR_DOWN: return pWorld->GetTileManager()->GetTile(pos.x, pos.y + 1);
        case STEAM_DIR_LEFT: return pWorld->GetTileManager()->GetTile(pos.x - 1, pos.y);
        default: return nullptr;
    }
}

bool IsSteamConductor(TileInfo* pTile)
{
    if(!pTile) {
        return false;
    }

    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(pTile->GetDisplayedItem());
    return pItem && pItem->type == ITEM_TYPE_STEAMPUNK;
}

bool IsItemSucker(uint16 itemID)
{
    return itemID == ITEM_ID_MAGPLANT_5000 || itemID == ITEM_ID_UNSTABLE_TESSERACT || itemID == ITEM_ID_GAIAS_BEACON;
}

bool IsSuckerItemCompatible(uint16 machineID, uint16 itemID)
{
    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(itemID);
    if(!pItem) {
        return false;
    }

    if(machineID == ITEM_ID_UNSTABLE_TESSERACT && pItem->type == ITEM_TYPE_SEED) {
        return false;
    }

    if(machineID == ITEM_ID_GAIAS_BEACON && pItem->type != ITEM_TYPE_SEED) {
        return false;
    }

    return true;
}

string GetWorldInfoSuffix(World* pWorld)
{
    if(!pWorld) {
        return "";
    }

    std::vector<string> flags;
    if(pWorld->HasWorldFlag(WORLD_FLAG_JAMMED)) {
        flags.push_back("JAMMED");
    }

    if(pWorld->HasWorldFlag(WORLD_FLAG_PUNCH_JAMMER)) {
        flags.push_back("PUNCH JAMMER");
    }

    if(pWorld->HasWorldFlag(WORLD_FLAG_ZOMBIE_JAMMER)) {
        flags.push_back("ZOMBIE JAMMER");
    }

    if(pWorld->HasWorldFlag(WORLD_FLAG_ANTI_GRAVITY)) {
        flags.push_back("ANTI-GRAVITY");
    }

    if(flags.empty()) {
        return "";
    }

    string out = " `6(";
    for(size_t i = 0; i < flags.size(); ++i) {
        if(i > 0) {
            out += ", ";
        }

        out += flags[i];
    }

    out += ")``";
    return out;
}

bool TrySuckerCheck(World* pWorld, WorldObject& obj)
{
    if(!pWorld || obj.itemID == ITEM_ID_GEMS || obj.count == 0) {
        return false;
    }

    const Vector2Int size = pWorld->GetTileManager()->GetSize();
    for(int32 y = 0; y < size.y; ++y) {
        for(int32 x = 0; x < size.x; ++x) {
            TileInfo* pTile = pWorld->GetTileManager()->GetTile(x, y);
            if(!pTile || !IsItemSucker(pTile->GetDisplayedItem())) {
                continue;
            }

            TileExtra_Magplant* pData = pTile->GetExtra<TileExtra_Magplant>();
            if(!pData || !pData->magnetic) {
                continue;
            }

            if(pData->itemLimit <= 0) {
                continue;
            }

            if(pData->itemCount < 0) {
                pData->itemCount = 0;
            }

            const uint16 machineID = pTile->GetDisplayedItem();
            if(!IsSuckerItemCompatible(machineID, obj.itemID)) {
                continue;
            }

            if(pData->itemID != obj.itemID) {
                continue;
            }

            if((pData->itemCount + (int32)obj.count) > pData->itemLimit) {
                continue;
            }

            pData->itemCount += (int32)obj.count;

            const Vector2Int tilePos = pTile->GetPos();
            pWorld->SendParticleEffectToAll((tilePos.x * 32.0f) + 16.0f, (tilePos.y * 32.0f) + 16.0f, 44, 0, 0);
            pWorld->SendTileUpdate(pTile);
            return true;
        }
    }

    return false;
}

TileInfo* FindDoorByIDInWorld(World* pWorld, const string& doorID)
{
    if(!pWorld || doorID.empty()) {
        return nullptr;
    }

    const Vector2Int size = pWorld->GetTileManager()->GetSize();
    for(int32 y = 0; y < size.y; ++y) {
        for(int32 x = 0; x < size.x; ++x) {
            TileInfo* pTile = pWorld->GetTileManager()->GetTile(x, y);
            if(!pTile) {
                continue;
            }

            ItemInfo* pItem = GetItemInfoManager()->GetItemByID(pTile->GetDisplayedItem());
            if(!pItem || (pItem->type != ITEM_TYPE_DOOR && pItem->type != ITEM_TYPE_USER_DOOR && pItem->type != ITEM_TYPE_PORTAL && pItem->type != ITEM_TYPE_SUNGATE)) {
                continue;
            }

            TileExtra_Door* pDoor = pTile->GetExtra<TileExtra_Door>();
            if(pDoor && ToUpper(pDoor->id) == doorID) {
                return pTile;
            }
        }
    }

    return nullptr;
}

bool ActivateFunctionalSteamTile(World* pWorld, TileInfo* pTile, uint32 connectorCount)
{
    if(!pWorld || !pTile) {
        return false;
    }

    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(pTile->GetDisplayedItem());
    if(!pItem) {
        return false;
    }

    switch(pItem->id) {
        case ITEM_ID_STEAM_VENT:
        case ITEM_ID_SPIRIT_STORAGE_UNIT:
        case ITEM_ID_STEAM_DOOR: {
            pWorld->QueueSteamActivation(pTile, connectorCount * 115);
            return true;
        }
        default: return false;
    }
}

}

bool World::SuckerCheck(WorldObject& obj)
{
    return TrySuckerCheck(this, obj);
}

GamePlayer* World::GetPlayerOnTile(const Vector2Int& tilePos)
{
    GamePlayer* pBestPlayer = nullptr;

    for(GamePlayer* pWorldPlayer : m_players) {
        if(!pWorldPlayer) {
            continue;
        }

        const Vector2Float worldPos = pWorldPlayer->GetWorldPos();
        const Vector2Int playerTile((int32)(worldPos.x / 32.0f), (int32)(worldPos.y / 32.0f));
        if(playerTile != tilePos) {
            continue;
        }

        if(!pBestPlayer || pWorldPlayer->GetNetID() > pBestPlayer->GetNetID()) {
            pBestPlayer = pWorldPlayer;
        }
    }

    return pBestPlayer;
}

World::World()
: m_worldID(0)
{
}

World::~World()
{
}

bool World::PlayerJoinWorld(GamePlayer* pPlayer)
{
    if(!pPlayer) {
        return false;
    }

    // Check if player is banned from this world
    if(IsPlayerBanned(pPlayer->GetUserID())) {
        pPlayer->SendOnConsoleMessage("`4Oh no! ``You've been banned from that world! Try again later after ban wears off.");
        return false;
    }

    TileInfo* pWorldLockTile = GetTileManager()->GetKeyTile(KEY_TILE_WORLD_LOCK);
    if(pWorldLockTile) {
        TileExtra_Lock* pLockExtra = pWorldLockTile->GetExtra<TileExtra_Lock>();
        if(pLockExtra && pLockExtra->minEntryLevel > 1) {
            if((int32)pPlayer->GetLevel() < pLockExtra->minEntryLevel) {
                pPlayer->SendOnConsoleMessage("`4You must be level " + ToString(pLockExtra->minEntryLevel) + " to enter this world!``");
                return false;
            }
        }
    }

    pPlayer->SetJoinWorld(false);
    pPlayer->SetCurrentWorld(m_worldID);
    pPlayer->SetMagplantPos({ -1, -1 });
    m_players.push_back(pPlayer);

    if(m_players.size() >= 20) {
        pPlayer->GiveAchievement("Social Butterfly (Classic)");
    }

    TileInfo* pMainDoorTile = GetTileManager()->GetKeyTile(KEY_TILE_MAIN_DOOR);
    TileInfo* pSpawnDoorTile = pMainDoorTile;

    string pendingDoorID = ToUpper(pPlayer->ConsumePendingDoorWarpID());
    if(!pendingDoorID.empty()) {
        TileInfo* pTargetDoor = FindDoorByIDInWorld(this, pendingDoorID);
        if(pTargetDoor) {
            pSpawnDoorTile = pTargetDoor;
        }
    }

    if(!pSpawnDoorTile) {
        pPlayer->SetWorldPos(0, 0);
        pPlayer->SetRespawnPos(0, 0);
    }
    else {
        Vector2Int spawnPos = pSpawnDoorTile->GetPos();
        pPlayer->SetWorldPos(spawnPos.x * 32, spawnPos.y * 32);
        pPlayer->SetRespawnPos(spawnPos.x * 32, spawnPos.y * 32);
    }

    MemoryBuffer memSize;
    Serialize(memSize, true, false);
    
    uint32 worldMemSize = memSize.GetOffset();
    uint8* pWorldData = new uint8[worldMemSize];

    MemoryBuffer memBuffer(pWorldData, worldMemSize);
    Serialize(memBuffer, true, false);

    GameUpdatePacket packet;
    packet.type = NET_GAME_PACKET_SEND_MAP_DATA;
    packet.netID = -1;
    packet.flags |= NET_GAME_PACKET_FLAGS_EXTENDED;
    packet.extraDataSize = worldMemSize;
    SendENetPacketRaw(NET_MESSAGE_GAME_PACKET, &packet, sizeof(GameUpdatePacket), pWorldData, pPlayer->GetPeer());
    SAFE_DELETE_ARRAY(pWorldData);

    pPlayer->SendOnSpawn(pPlayer->GetSpawnData(true));
    pPlayer->SendOnSetClothing();
    pPlayer->SendCharacterState();

    for(auto& pWorldPlayer : m_players) {
        if(pWorldPlayer && pWorldPlayer != pPlayer) {
            pPlayer->SendOnSpawn(pWorldPlayer->GetSpawnData(false));
            pPlayer->SendOnSetClothing(pWorldPlayer);
            pPlayer->SendCharacterState(pWorldPlayer);

            pWorldPlayer->SendOnSpawn(pPlayer->GetSpawnData(false));
            pWorldPlayer->SendOnSetClothing(pPlayer);
            pWorldPlayer->SendCharacterState(pPlayer);
        }
    }

    const int32 otherPlayersHere = std::max<int32>(0, static_cast<int32>(m_players.size()) - 1);
    const string worldInfoSuffix = GetWorldInfoSuffix(this);
    const string worldLabel = worldInfoSuffix.empty() ? ("`w" + GetWorlName() + "``") : ("`w" + GetWorlName() + worldInfoSuffix);
    pPlayer->SendOnConsoleMessage(
        "World " + worldLabel + " entered. There are `w" + ToString(otherPlayersHere) +
        "`` other people here, `w" + ToString(GetMasterBroadway()->GetGlobalOnlineCount()) + "`` online."
    );

    TileInfo* pLockTile = GetTileManager()->GetKeyTile(KEY_TILE_WORLD_LOCK);
    TileExtra_Lock* pLockExtra = pLockTile ? pLockTile->GetExtra<TileExtra_Lock>() : nullptr;

    if(pLockExtra && pLockExtra->ownerID > 0) {
        string ownerUsername = "DeletedUser";

        GamePlayer* pOwner = GetGameServer()->GetPlayerByUserID((uint32)pLockExtra->ownerID);
        if(pOwner) {
            ownerUsername = pOwner->GetRawName();
        }

        const bool hasAccess = pLockExtra->ownerID == (int32)pPlayer->GetUserID();
        pPlayer->SendOnConsoleMessage(
            "`5[`w" + GetWorlName() + "`o World Locked by `o" + ownerUsername + "`o" +
            (hasAccess ? " (`2ACCESS GRANTED`o)" : "") +
            "`5]"
        );
    }

    return true;
}

void World::PlayerLeaverWorld(GamePlayer* pPlayer)
{
    if(!pPlayer) {
        return;
    }

    int32 playerIdx = -1;

    for(uint16 i = 0; i < m_players.size(); ++i) {
        GamePlayer* pWorldPlayer = m_players[i];

        pWorldPlayer->SendOnRemove(pPlayer->GetNetID());

        if(pWorldPlayer == pPlayer) {
            playerIdx = i;
        }
    }

    if(playerIdx != -1) {
        m_players[playerIdx] = m_players.back();
        m_players.pop_back();

        pPlayer->SetCurrentWorld(0);
        pPlayer->SetMagplantPos({ -1, -1 });
    }

    if(m_players.empty()) {
        m_worldOfflineTime.Reset();
    }
}

void World::SendSkinColorUpdateToAll(GamePlayer* pPlayer)
{
    if(!pPlayer) {
        return;
    }

    uint32 skinColor = pPlayer->GetCharData().GetSkinColor(true);
    for(auto& pWorldPlayer: m_players) {
        if(pWorldPlayer) {
            pWorldPlayer->SendOnChangeSkin(skinColor, pPlayer);
        }
    }
}

void World::SendTalkBubbleAndConsoleToAll(const string& message, bool stackBubble, GamePlayer* pPlayer)
{
    for(auto& pWorldPlayer : m_players) {
        if(pWorldPlayer) {
            pWorldPlayer->SendOnConsoleMessage(message);
            pWorldPlayer->SendOnTalkBubble(message, stackBubble, pPlayer);
        }
    }
}

void World::SendConsoleMessageToAll(const string& message)
{
    for(auto& pWorldPlayer : m_players) {
        if(pWorldPlayer) {
            pWorldPlayer->SendOnConsoleMessage(message);
        }
    }
}

void World::SendNameChangeToAll(GamePlayer* pPlayer)
{
    if(!pPlayer) {
        return;
    }

    string playerName = pPlayer->GetDisplayName();
    for(auto& pWorldPlayer : m_players) {
        if(pWorldPlayer) {
            pWorldPlayer->SendOnNameChanged(playerName, pPlayer);
        }
    }
}

void World::SendSetCharPacketToAll(GamePlayer* pPlayer)
{
    if(!pPlayer) {
        return;
    }

    for(auto& pWorldPlayer : m_players) {
        if(pWorldPlayer) {
            pWorldPlayer->SendCharacterState(pPlayer);
        }
    }
}

void World::SendClothUpdateToAll(GamePlayer* pPlayer)
{
    if(!pPlayer) {
        return;
    }

    for(auto& pWorldPlayer : m_players) {
        if(pWorldPlayer) {
            pWorldPlayer->SendOnSetClothing(pPlayer);
        }
    }
}

void World::SendParticleEffectToAll(float coordX, float coordY, uint32 particleType, float particleSize, int32 delay)
{
    GameUpdatePacket packet;
    packet.type = NET_GAME_PACKET_SEND_PARTICLE_EFFECT;
    packet.posX = coordX;
    packet.posY = coordY;
    packet.particleEffectType = (float)particleType;
    packet.particleEffectSize = particleSize;
    packet.delay = delay;

    SendGamePacketToAll(&packet);
}

void World::SendTileUpdate(TileInfo* pTile)
{
    if(!pTile) {
        return;
    }

    Vector2Int vTilePos = pTile->GetPos();
    SendTileUpdate(vTilePos.x, vTilePos.y);
}

void World::SendTileUpdate(uint16 tileX, uint16 tileY)
{
    TileInfo* pTile = GetTileManager()->GetTile(tileX, tileY);
    if(!pTile) {
        return;
    }

    GameUpdatePacket packet;
    packet.type = NET_GAME_PACKET_SEND_TILE_UPDATE_DATA;
    packet.tileX = tileX;
    packet.tileY = tileY;
    packet.flags |= NET_GAME_PACKET_FLAGS_EXTENDED;

    MemoryBuffer memSizeBuf;
    pTile->Serialize(memSizeBuf, true, false, this);

    uint32 memSize = memSizeBuf.GetOffset();
    packet.extraDataSize = memSize;

    uint8* pTileData = new uint8[memSize];
    MemoryBuffer memBuffer(pTileData, memSize);
    pTile->Serialize(memBuffer, true, false, this);

    SendGamePacketToAll(&packet, nullptr, pTileData);

    SAFE_DELETE_ARRAY(pTileData);
}

void World::SendTileUpdateMultiple(const std::vector<TileInfo*>& tiles)
{
    if(tiles.empty()) {
        return;
    }

    MemoryBuffer memSizeBuf;
    for(auto& pTile : tiles) {
        pTile->Serialize(memSizeBuf, true, false, this);
    }

    uint32 memSize = tiles.size() * 2 * sizeof(int32) + memSizeBuf.GetOffset() + sizeof(int32);
    uint8* pData = new uint8[memSize];

    MemoryBuffer memBuffer(pData, memSize);
    for(auto& pTile : tiles) {
        Vector2Int vTilePos = pTile->GetPos();
        memBuffer.Write((int32)vTilePos.x);
        memBuffer.Write((int32)vTilePos.y);

        pTile->Serialize(memBuffer, true, false, this);
    }

    int32 endMarker = -1;
    memBuffer.Write(endMarker);

    GameUpdatePacket packet;
    packet.type = NET_GAME_PACKET_SEND_TILE_UPDATE_DATA_MULTIPLE;
    packet.flags |= NET_GAME_PACKET_FLAGS_EXTENDED;
    packet.extraDataSize = memSize;
    packet.tileX = -1;
    packet.tileY = -1;
    
    SendGamePacketToAll(&packet, nullptr, pData);
    SAFE_DELETE_ARRAY(pData);
}

void World::SendTileApplyDamage(uint16 tileX, uint16 tileY, int32 damage, int32 netID)
{
    GameUpdatePacket packet;
    packet.type = NET_GAME_PACKET_TILE_APPLY_DAMAGE;
    packet.tileX = tileX;
    packet.tileY = tileY;
    packet.tileDamage = damage;
    packet.netID = netID;

    SendGamePacketToAll(&packet);
}

void World::SendLockPacketToAll(int32 userID, int32 lockID, std::vector<TileInfo*>& tiles, TileInfo* pLockTile)
{
    if(!pLockTile) {
        return;
    }

    GameUpdatePacket packet;
    packet.type = NET_GAME_PACKET_SEND_LOCK;
    packet.flags |= NET_GAME_PACKET_FLAGS_EXTENDED;
    packet.tileX = pLockTile->GetPos().x;
    packet.tileY = pLockTile->GetPos().y;
    packet.itemID = lockID;
    packet.userID = userID;

    HandleTilePackets(&packet);

    uint8* pData = nullptr;

    if(!tiles.empty()) {
        packet.tileCount = tiles.size();
        packet.extraDataSize = tiles.size() * sizeof(uint16);
    
        uint32 memSize = packet.extraDataSize;
        pData = new uint8[memSize];
    
        MemoryBuffer memBuffer(pData, memSize);
        uint32 worldWidth = GetTileManager()->GetSize().x;
    
        for(auto& pTile : tiles) {
            if(!pTile) {
                continue;
            }
    
            Vector2Int vTilePos = pTile->GetPos();
            uint16 index = vTilePos.x + vTilePos.y * worldWidth;
            memBuffer.Write(index);
        }
    }

    SendGamePacketToAll(&packet, nullptr, pData);
    SAFE_DELETE_ARRAY(pData);
}

void World::PlaySFXForEveryone(const string& fileName, int32 delay)
{
    for(auto& pWorldPlayer : m_players) {
        if(pWorldPlayer) {
            pWorldPlayer->PlaySFX(fileName, delay);
        }
    }
}

void World::SendGamePacketToAll(GameUpdatePacket* pPacket, GamePlayer* pExceptMe, uint8* pExtraData)
{
    for(auto& pWorldPlayer : m_players) {
        if(pWorldPlayer) {
            if(pExceptMe && (pWorldPlayer == pExceptMe)) {
                continue;
            }

            SendENetPacketRaw(NET_MESSAGE_GAME_PACKET, pPacket, sizeof(GameUpdatePacket), pExtraData, pWorldPlayer->GetPeer());
        }
    }
}

void World::SendCurrentWeatherToAll()
{
    for(auto& pWorldPlayer : m_players) {
        if(pWorldPlayer) {
            pWorldPlayer->SendOnSetCurrentWeather(GetCurrentWeather());
        }
    }
}

void World::HandleTilePackets(GameUpdatePacket* pGamePacket)
{
    if(!pGamePacket) {
        return;
    }

    switch(pGamePacket->type) {
        case NET_GAME_PACKET_SEND_LOCK: {
            TileInfo* pTile = GetTileManager()->GetTile(pGamePacket->tileX, pGamePacket->tileY);
            if(!pTile) {
                return;
            }

            pTile->SetFG(pGamePacket->itemID, GetTileManager());

            TileExtra_Lock* pTileExtra = pTile->GetExtra<TileExtra_Lock>();
            if(!pTileExtra) {
                return;
            }

            pTileExtra->ownerID = pGamePacket->userID;
            SendTileUpdate(pGamePacket->tileX, pGamePacket->tileY);
            break;
        }

        case NET_GAME_PACKET_TILE_CHANGE_REQUEST: {
            TileInfo* pTile = GetTileManager()->GetTile(pGamePacket->tileX, pGamePacket->tileY);
            if(!pTile) {
                return;
            }
    
            ItemInfo* pItem = GetItemInfoManager()->GetItemByID(pGamePacket->itemID);
            if(!pItem) {
                return;
            }

            if(pItem->IsBackground()) {
                pTile->SetBG(pItem->id);
            }
            else if(pItem->type == ITEM_TYPE_SEED) {
                pTile->SetFG(pItem->id, GetTileManager());
                pTile->SetFlag(TILE_FLAG_IS_SEEDLING);

                TileExtra_Seed* pSeedExtra = pTile->GetExtra<TileExtra_Seed>();
                if(pSeedExtra) {
                    pSeedExtra->growTime = (uint32)Time::GetSystemTime();
                    pSeedExtra->fruitCount = (uint8)(2 + (rand() % 11));
                }
            }
            else if(pItem->type == ITEM_TYPE_FIST) {
                if(pTile->GetFG() != ITEM_ID_BLANK) {
                    pTile->SetFG(ITEM_ID_BLANK, GetTileManager());
                }
                else {
                    pTile->SetBG(ITEM_ID_BLANK);
                }
            }
            else {
                if(pItem->IsBackground()) {
                    pTile->SetBG(pItem->id);
                }
                else {
                    pTile->SetFG(pItem->id, GetTileManager());
                }
            }

            if(pItem->IsBackground()) {
                if(pTile->HasFlag(TILE_FLAG_HAS_EXTRA_DATA)) {
                    if(pGamePacket->HasFlag(NET_GAME_PACKET_FLAGS_FACINGLEFT)) {
                        pTile->SetFlag(TILE_FLAG_FLIPPED_X);
                    }
                    else {
                        pTile->RemoveFlag(TILE_FLAG_FLIPPED_X);
                    }
                }
            }
            else {
                /**
                 * 
                 * seed check
                 * 
                 */

                if(pItem->HasFlag(ITEM_FLAG_FLIPPABLE)) {
                    if(pGamePacket->HasFlag(NET_GAME_PACKET_FLAGS_FACINGLEFT)) {
                        pTile->SetFlag(TILE_FLAG_FLIPPED_X);
                    }
                    else {
                        pTile->RemoveFlag(TILE_FLAG_FLIPPED_X);
                    }
                }
            }
            break;
        }
    }
}

bool World::PlayerHasAccessOnTile(GamePlayer* pPlayer, TileInfo* pTile)
{
    if(!pPlayer || !pTile) {
        return false;
    }

    if(pTile->HasFlag(TILE_FLAG_IS_OPEN_TO_PUBLIC)) {
        return true;
    }

    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(pTile->GetDisplayedItem());
    if(pItem->HasFlag(ITEM_FLAG_PUBLIC)) {
        return true;
    }

    TileInfo* pWorldLockTile = GetTileManager()->GetKeyTile(KEY_TILE_WORLD_LOCK);
    if(pWorldLockTile) {
        TileExtra_Lock* pWLExtra = pWorldLockTile->GetExtra<TileExtra_Lock>();
        if(!pWLExtra) {
            return false;
        }

        return pWLExtra->HasAccess(pPlayer->GetUserID());
    }

    if(pTile->GetParent() == 0) {
        return true;
    }

    TileInfo* pMainLockTile = GetTileManager()->GetTile(pTile->GetParent());
    if(!pMainLockTile) {
        return false;
    }

    TileExtra_Lock* pMainLockExtra = pMainLockTile->GetExtra<TileExtra_Lock>();
    if(!pMainLockExtra) {
        return false;
    }

    if(pMainLockExtra->ownerID == (int32)pPlayer->GetUserID()) {
        return true;
    }

    return pMainLockExtra->HasAccess(pPlayer->GetUserID());
}

void World::OnAddLock(GamePlayer* pPlayer, TileInfo* pTile, uint16 lockID)
{
    if(!pPlayer || !pTile) {
        return;
    }

    if(pPlayer->GetInventory().GetCountOfItem(lockID) == 0) {
        return;
    }

    if(pTile->GetFG() != ITEM_ID_BLANK) {
        pPlayer->SendOnTalkBubble("Use a lock on blank tile next to the things you want to lock.", true);
        return;
    }

    if(IsWorldLock(lockID) && GetTileManager()->GetKeyTile(KEY_TILE_WORLD_LOCK)) {
        pPlayer->SendOnTalkBubble("Only one `$World Lock`` can be placed in a world, you'd have to remove the other one first.", true);
        return;
    }

    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(lockID);
    if(!pItem) {
        return;
    }

    std::vector<TileInfo*> lockedTiles;

    if(IsAreaLock(lockID)) {
        bool lockSuccsess = GetTileManager()->ApplyLockTiles(pTile, GetMaxTilesToLock(lockID), false, lockedTiles, false);
        if(!lockSuccsess) {
            pPlayer->SendOnTalkBubble("Something went wrong, unable to place lock in here", true);
            return;
        }
    }

    if(pTile->HasFlag(TILE_FLAG_PAINTED_RED | TILE_FLAG_PAINTED_GREEN | TILE_FLAG_PAINTED_BLUE)) {
        pTile->RemoveFlag(TILE_FLAG_PAINTED_RED | TILE_FLAG_PAINTED_GREEN | TILE_FLAG_PAINTED_BLUE);
    }
    pTile->RemoveFlag(TILE_FLAG_IS_OPEN_TO_PUBLIC);

    SendLockPacketToAll((int32)pPlayer->GetUserID(), (int32)lockID, lockedTiles, pTile);
    pPlayer->ModifyInventoryItem(lockID, -1);

    if(IsWorldLock(lockID)) {
        string playerName = pPlayer->GetDisplayName();

        SendTalkBubbleAndConsoleToAll("`5[`w" + GetWorlName() +  " has been `$World Locked`` by " + playerName + "`5]``", true, pPlayer);
        LOGGER_LOG_INFO("World %s has been locked by %d", GetWorlName().c_str(), pPlayer->GetUserID());

        SendNameChangeToAll(pPlayer);
    }
    else {
        pPlayer->SendOnTalkBubble("Area locked.", true);
        pPlayer->SendOnPlayPositioned("use_lock.wav");
    }
}

void World::OnRemoveLock(GamePlayer* pPlayer, TileInfo* pTile)
{
    if(!pPlayer || !pTile) {
        return;
    }

    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(pTile->GetDisplayedItem());
    if(!pItem || pItem->type != ITEM_TYPE_LOCK) {
        return;
    }

    if(IsWorldLock(pItem->id)) {
        SendConsoleMessageToAll("`5[```w" + GetWorlName() + "`` has had its `$World Lock`` removed!`5]``");
        LOGGER_LOG_INFO("Removed world lock in %s (%d) by %d", GetWorlName().c_str(), GetID(), pPlayer->GetUserID());
    }
    else {
        std::vector<TileInfo*> unlockedTiles = GetTileManager()->RemoveTileParentsLockedBy(pTile);
        SendTileUpdateMultiple(unlockedTiles);
    }
}

bool World::IsPlayerWorldOwner(GamePlayer* pPlayer)
{
    if(!pPlayer) {
        return false;
    }

    TileInfo* pLockTile = GetTileManager()->GetKeyTile(KEY_TILE_WORLD_LOCK);
    if(!pLockTile) {
        return false;
    }

    TileExtra_Lock* pTileExtra = pLockTile->GetExtra<TileExtra_Lock>();
    if(!pTileExtra) {
        return false;
    }

    if(pTileExtra->ownerID == pPlayer->GetUserID()) {
        return true;
    }

    return false;
}

bool World::IsPlayerWorldAdmin(GamePlayer* pPlayer)
{
    if(!pPlayer) {
        return false;
    }

    TileInfo* pLockTile = GetTileManager()->GetKeyTile(KEY_TILE_WORLD_LOCK);
    if(!pLockTile) {
        return false;
    }

    TileExtra_Lock* pTileExtra = pLockTile->GetExtra<TileExtra_Lock>();
    if(!pTileExtra) {
        return false;
    }

    if(pTileExtra->IsAdmin(pPlayer->GetUserID())) {
        return true;
    }

    return false;
}

namespace {

bool HasKickBanPullException(GamePlayer* pPlayer)
{
    if(!pPlayer) {
        return false;
    }

    Role* pRole = pPlayer->GetRole();
    return pRole && pRole->HasPerm(ROLE_PERM_COMMAND_MOD);
}

TileExtra_Lock* GetTileLockContext(World* pWorld, TileInfo* pTile, TileInfo** ppLockTile = nullptr)
{
    if(ppLockTile) {
        *ppLockTile = nullptr;
    }

    if(!pWorld || !pTile) {
        return nullptr;
    }

    if(pTile->GetParent() != 0) {
        TileInfo* pParentLock = pWorld->GetTileManager()->GetTile(pTile->GetParent());
        if(ppLockTile) {
            *ppLockTile = pParentLock;
        }

        return pParentLock ? pParentLock->GetExtra<TileExtra_Lock>() : nullptr;
    }

    TileInfo* pWorldLock = pWorld->GetTileManager()->GetKeyTile(KEY_TILE_WORLD_LOCK);
    if(ppLockTile) {
        *ppLockTile = pWorldLock;
    }

    return pWorldLock ? pWorldLock->GetExtra<TileExtra_Lock>() : nullptr;
}

}

bool World::CanKick(GamePlayer* pTarget, GamePlayer* pInvoker)
{
    if(!pTarget || !pInvoker) {
        return false;
    }

    TileInfo* pTargetTile = GetTileManager()->GetTile((int32)(pTarget->GetWorldPos().x / 32.0f), (int32)(pTarget->GetWorldPos().y / 32.0f));
    TileInfo* pInvokerTile = GetTileManager()->GetTile((int32)(pInvoker->GetWorldPos().x / 32.0f), (int32)(pInvoker->GetWorldPos().y / 32.0f));
    if(!pTargetTile || !pInvokerTile) {
        return false;
    }

    const bool targetException = HasKickBanPullException(pTarget);
    const bool invokerException = HasKickBanPullException(pInvoker);

    if(targetException && !invokerException) {
        return false;
    }

    if(invokerException) {
        return pTarget->GetUserID() != pInvoker->GetUserID();
    }

    if(pTarget->GetUserID() == pInvoker->GetUserID()) {
        return false;
    }

    TileExtra_Lock* pTargetLock = GetTileLockContext(this, pTargetTile);
    TileExtra_Lock* pInvokerLock = GetTileLockContext(this, pInvokerTile);

    const int32 targetOwnerID = pTargetLock ? pTargetLock->ownerID : 0;
    const int32 invokerTileOwnerID = pInvokerLock ? pInvokerLock->ownerID : 0;
    const bool invokerHasTargetAccess = pTargetLock && pTargetLock->HasAccess((int32)pInvoker->GetUserID());

    if(!invokerHasTargetAccess && targetOwnerID != (int32)pInvoker->GetUserID() && invokerTileOwnerID != (int32)pInvoker->GetUserID()) {
        return false;
    }

    if(targetOwnerID == (int32)pTarget->GetUserID() || invokerTileOwnerID == (int32)pTarget->GetUserID()) {
        return false;
    }

    if(targetOwnerID == 0 || invokerTileOwnerID == 0) {
        return false;
    }

    return true;
}

bool World::CanPull(GamePlayer* pTarget, GamePlayer* pInvoker)
{
    if(!pTarget || !pInvoker) {
        return false;
    }

    TileInfo* pTargetTile = GetTileManager()->GetTile((int32)(pTarget->GetWorldPos().x / 32.0f), (int32)(pTarget->GetWorldPos().y / 32.0f));
    TileInfo* pInvokerTile = GetTileManager()->GetTile((int32)(pInvoker->GetWorldPos().x / 32.0f), (int32)(pInvoker->GetWorldPos().y / 32.0f));
    if(!pTargetTile || !pInvokerTile) {
        return false;
    }

    const bool targetException = HasKickBanPullException(pTarget);
    const bool invokerException = HasKickBanPullException(pInvoker);

    if(targetException && !invokerException) {
        return false;
    }

    if(invokerException) {
        return pTarget->GetUserID() != pInvoker->GetUserID();
    }

    if(pTarget->GetUserID() == pInvoker->GetUserID()) {
        return false;
    }

    TileExtra_Lock* pTargetLock = GetTileLockContext(this, pTargetTile);
    TileExtra_Lock* pInvokerLock = GetTileLockContext(this, pInvokerTile);

    const int32 targetOwnerID = pTargetLock ? pTargetLock->ownerID : 0;
    const int32 invokerTileOwnerID = pInvokerLock ? pInvokerLock->ownerID : 0;
    const bool invokerHasTargetAccess = pTargetLock && pTargetLock->HasAccess((int32)pInvoker->GetUserID());
    const bool invokerHasInvokerTileAccess = pInvokerLock && pInvokerLock->HasAccess((int32)pInvoker->GetUserID());

    if((!invokerHasTargetAccess || !invokerHasInvokerTileAccess) && targetOwnerID != (int32)pInvoker->GetUserID() && invokerTileOwnerID != (int32)pInvoker->GetUserID()) {
        return false;
    }

    if(targetOwnerID == (int32)pTarget->GetUserID() || invokerTileOwnerID == (int32)pTarget->GetUserID()) {
        return false;
    }

    if(targetOwnerID == 0 || invokerTileOwnerID == 0) {
        return false;
    }

    return true;
}

bool World::CanBan(GamePlayer* pTarget, GamePlayer* pInvoker)
{
    if(!pTarget || !pInvoker) {
        return false;
    }

    const bool targetException = HasKickBanPullException(pTarget);
    const bool invokerException = HasKickBanPullException(pInvoker);

    if(targetException && !invokerException) {
        return false;
    }

    if(invokerException) {
        return pTarget->GetUserID() != pInvoker->GetUserID();
    }

    if(pTarget->GetUserID() == pInvoker->GetUserID()) {
        return false;
    }

    TileInfo* pWorldLockTile = GetTileManager()->GetKeyTile(KEY_TILE_WORLD_LOCK);
    TileExtra_Lock* pWorldLock = pWorldLockTile ? pWorldLockTile->GetExtra<TileExtra_Lock>() : nullptr;

    const int32 worldOwnerID = pWorldLock ? pWorldLock->ownerID : 0;
    if(worldOwnerID == 0 || worldOwnerID == (int32)pTarget->GetUserID()) {
        return false;
    }

    if(!pWorldLock->HasAccess((int32)pInvoker->GetUserID()) && worldOwnerID != (int32)pInvoker->GetUserID()) {
        return false;
    }

    if(pWorldLock->HasAccess((int32)pTarget->GetUserID())) {
        return false;
    }

    return true;
}

void World::PullPlayer(GamePlayer* pTarget, GamePlayer* pInvoker)
{
    if(!pTarget || !pInvoker) {
        return;
    }

    const Vector2Float invokerPos = pInvoker->GetWorldPos();
    pTarget->SetWorldPos(invokerPos.x, invokerPos.y);
    pTarget->SetRespawnPos(invokerPos.x, invokerPos.y);
    pTarget->SendPositionToWorldPlayers();
    pTarget->SendOnTextOverlay("You were pulled by " + pInvoker->GetDisplayName());

    SendConsoleMessageToAll(pInvoker->GetDisplayName() + " `5pulls `w" + pTarget->GetDisplayName() + "`o!");
    for(GamePlayer* pWorldPlayer : m_players) {
        if(pWorldPlayer) {
            pWorldPlayer->SendOnPlayPositioned("audio/object_spawn.wav", pTarget);
        }
    }
}

void World::KickPlayer(GamePlayer* pTarget, GamePlayer* pInvoker)
{
    if(!pTarget || !pInvoker) {
        return;
    }

    pTarget->SendOnConsoleMessage("`4You were kicked by ``" + pInvoker->GetDisplayName() + "``.");
    SendConsoleMessageToAll(pInvoker->GetDisplayName() + " `4kicks `w" + pTarget->GetDisplayName() + "`o!");
    ForceLeavePlayer(pTarget);
}

void World::ForceLeavePlayer(GamePlayer* pTarget)
{
    if(!pTarget) {
        return;
    }

    PlayerLeaverWorld(pTarget);
    pTarget->SendOnRequestWorldSelectMenu("");
}

bool World::CanPlace(GamePlayer* pPlayer, TileInfo* pTile)
{
    if(!pPlayer || !pTile) {
        return false;
    }

    if(IsPlayerWorldOwner(pPlayer)) {
        return true;
    }

    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(pTile->GetDisplayedItem());
    if(!pItem) {
        return false;
    }

    if(pItem->type == ITEM_TYPE_LOCK) {
        return false;
    }

    if(pItem->HasFlag(ITEM_FLAG_PUBLIC) || pTile->HasFlag(TILE_FLAG_IS_OPEN_TO_PUBLIC)) {
        return true;
    }

    if(pTile->GetParent() == 0) {
        TileInfo* pWorldLockTile = GetTileManager()->GetKeyTile(KEY_TILE_WORLD_LOCK);
        if(!pWorldLockTile) {
            return true;
        }

        TileExtra_Lock* pWLExtra = pWorldLockTile->GetExtra<TileExtra_Lock>();
        if(!pWLExtra) {
            return false;
        }

        if(pWLExtra->HasAccess(pPlayer->GetUserID())) {
            return true;
        }

        return pWorldLockTile->HasFlag(TILE_FLAG_IS_OPEN_TO_PUBLIC);
    }

    TileInfo* pMainLockTile = GetTileManager()->GetTile(pTile->GetParent());
    if(!pMainLockTile) {
        return false;
    }

    TileExtra_Lock* pMainLockExtra = pMainLockTile->GetExtra<TileExtra_Lock>();
    if(!pMainLockExtra) {
        return false;
    }

    if(pMainLockExtra->ownerID == (int32)pPlayer->GetUserID()) {
        return true;
    }

    ItemInfo* pMainLockItem = GetItemInfoManager()->GetItemByID(pMainLockTile->GetFG());
    const bool isBuilderLock = pMainLockItem && pMainLockItem->id == ITEM_ID_BUILDERS_LOCK;

    if(pMainLockExtra->HasAccess(pPlayer->GetUserID())) {
        if(isBuilderLock && pMainLockExtra->HasFlag(TILE_EXTRA_LOCK_FLAG_RESTRICT_ADMINS)) {
            return pMainLockExtra->HasFlag(TILE_EXTRA_LOCK_FLAG_BUILDING_ONLY);
        }

        return true;
    }

    if(isBuilderLock && pMainLockTile->HasFlag(TILE_FLAG_IS_OPEN_TO_PUBLIC)) {
        return pMainLockExtra->HasFlag(TILE_EXTRA_LOCK_FLAG_BUILDING_ONLY);
    }

    if(pMainLockTile->HasFlag(TILE_FLAG_IS_OPEN_TO_PUBLIC)) {
        return true;
    }

    return false;
}

bool World::CanBreak(GamePlayer* pPlayer, TileInfo* pTile)
{
    if(!pPlayer || !pTile) {
        return false;
    }

    if(IsPlayerWorldOwner(pPlayer)) {
        return true;
    }

    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(pTile->GetDisplayedItem());
    if(!pItem) {
        return false;
    }

    if(pItem->type == ITEM_TYPE_LOCK) {
        return false;
    }

    if(pItem->HasFlag(ITEM_FLAG_PUBLIC) || pTile->HasFlag(TILE_FLAG_IS_OPEN_TO_PUBLIC)) {
        return true;
    }

    if(pTile->GetParent() == 0) {
        TileInfo* pWorldLockTile = GetTileManager()->GetKeyTile(KEY_TILE_WORLD_LOCK);
        if(!pWorldLockTile) {
            return true;
        }

        TileExtra_Lock* pWLExtra = pWorldLockTile->GetExtra<TileExtra_Lock>();
        if(!pWLExtra) {
            return false;
        }

        if(pWLExtra->HasAccess(pPlayer->GetUserID())) {
            return true;
        }

        return pWorldLockTile->HasFlag(TILE_FLAG_IS_OPEN_TO_PUBLIC);
    }

    TileInfo* pMainLockTile = GetTileManager()->GetTile(pTile->GetParent());
    if(!pMainLockTile) {
        return false;
    }

    TileExtra_Lock* pMainLockExtra = pMainLockTile->GetExtra<TileExtra_Lock>();
    if(!pMainLockExtra) {
        return false;
    }

    if(pMainLockExtra->ownerID == (int32)pPlayer->GetUserID()) {
        return true;
    }

    ItemInfo* pMainLockItem = GetItemInfoManager()->GetItemByID(pMainLockTile->GetFG());
    const bool isBuilderLock = pMainLockItem && pMainLockItem->id == ITEM_ID_BUILDERS_LOCK;

    if(pMainLockExtra->HasAccess(pPlayer->GetUserID())) {
        if(isBuilderLock && pMainLockExtra->HasFlag(TILE_EXTRA_LOCK_FLAG_RESTRICT_ADMINS)) {
            return !pMainLockExtra->HasFlag(TILE_EXTRA_LOCK_FLAG_BUILDING_ONLY);
        }

        return true;
    }

    if(isBuilderLock && pMainLockTile->HasFlag(TILE_FLAG_IS_OPEN_TO_PUBLIC)) {
        return !pMainLockExtra->HasFlag(TILE_EXTRA_LOCK_FLAG_BUILDING_ONLY);
    }

    if(pMainLockTile->HasFlag(TILE_FLAG_IS_OPEN_TO_PUBLIC)) {
        return true;
    }

    return false;
}

void World::DropObject(TileInfo* pTile, WorldObject& obj, bool merge)
{
    if(!pTile) {
        return;
    }

    Vector2Int vTilePos = pTile->GetPos();
    Vector2Float vBasePos = Vector2Float(vTilePos.x, vTilePos.y) * 32;
    vBasePos.x += 16.0f;
    vBasePos.y += 16.0f;

    obj.pos += vBasePos;

    if(SuckerCheck(obj)) {
        return;
    }

    if(merge) {
        RectFloat tileRect(vTilePos.x * 32, vTilePos.y * 32, (vTilePos.x + 1) * 32, (vTilePos.y + 1) * 32);
        auto objsInRect = GetObjectManager()->GetObjectsInRectByItemID(tileRect, obj.itemID);
    
        if(!objsInRect.empty()) {
            objsInRect.push_back(&obj);

            ItemInfo* pItem = GetItemInfoManager()->GetItemByID(obj.itemID);
            if(!pItem) {
                return;
            }

            WorldObject* pBaseObj = nullptr;
    
            for(auto& pObj : objsInRect) {
                if(!pObj || pObj->HasFlag(OBJECT_FLAG_NO_STACK)) {
                    continue;
                }
    
                if(!pBaseObj) {
                    pBaseObj = pObj;
                    continue;
                }
    
                if(pBaseObj == pObj) {
                    continue;
                }
    
                if(pBaseObj->count >= pItem->maxCanHold) {
                    pBaseObj = pObj;
                    continue;
                }
    
                uint16 transfer = Min(pItem->maxCanHold - pBaseObj->count, pObj->count);
        
                if(transfer > 0) {
                    pBaseObj->count += transfer;
                    pBaseObj->SetFlag(OBJECT_FLAG_DIRTY);
    
                    pObj->count -= transfer;
                    pObj->SetFlag(OBJECT_FLAG_DIRTY);
                }
    
                if(pBaseObj->count >= pItem->maxCanHold) {
                    pBaseObj = pObj;
                }
            }

            for(auto& pObj : objsInRect) {
                if(!pObj) {
                    continue;
                }

                if(pObj->objectID == 0 && pObj->count > 0) {
                    pObj->pos = vBasePos;
                    DropObject(*pObj);
                }
                else if(pObj->count == 0) {
                    RemoveObject(pObj->objectID);
                    continue;
                }
                else if(pObj->HasFlag(OBJECT_FLAG_DIRTY)) {
                    pObj->RemoveFlag(OBJECT_FLAG_DIRTY);
                    ModifyObject(*pObj);
                }
            }
        }
        else {
            DropObject(obj);
        }

        return;
    }

    DropObject(obj);
}

void World::DropObject(const WorldObject& obj)
{
    GameUpdatePacket packet;
    packet.type = NET_GAME_PACKET_ITEM_CHANGE_OBJECT;
    packet.itemID = obj.itemID;
    packet.posX = obj.pos.x;
    packet.posY = obj.pos.y;
    packet.worldObjectCount = obj.count;
    packet.worldObjectFlags = obj.flags;
    packet.worldObjectType = -1;

    GetObjectManager()->HandleObjectPackets(&packet);
    SendGamePacketToAll(&packet);
}

void World::RemoveObject(uint32 objectID)
{
    GameUpdatePacket packet;
    packet.type = NET_GAME_PACKET_ITEM_CHANGE_OBJECT;
    packet.worldObjectID = objectID;
    packet.worldObjectType = -2;
    packet.field4 = -1;

    GetObjectManager()->HandleObjectPackets(&packet);
    SendGamePacketToAll(&packet);
}

void World::ModifyObject(const WorldObject& obj)
{
    GameUpdatePacket packet;
    packet.type = NET_GAME_PACKET_ITEM_CHANGE_OBJECT;
    packet.itemID = obj.itemID;
    packet.posX = obj.pos.x;
    packet.posY = obj.pos.y;
    packet.worldObjectCount = obj.count;
    packet.worldObjectFlags = obj.flags;
    packet.worldObjectType = -3;
    packet.worldObjectID = obj.objectID;

    GetObjectManager()->HandleObjectPackets(&packet);
    SendGamePacketToAll(&packet);
}

void World::SendSteamPacket(uint8 mode, const Vector2Int& tilePos)
{
    GameUpdatePacket packet;
    packet.type = NET_GAME_PACKET_STEAM;
    packet.tileX = tilePos.x;
    packet.tileY = tilePos.y;
    packet.netID = -1;
    packet.field2 = mode;
    packet.particleEffectType = static_cast<float>(mode);

    SendGamePacketToAll(&packet);
}

void World::QueueSteamActivation(TileInfo* pTile, uint32 delayMS)
{
    if(!pTile) {
        return;
    }

    SteamActivationEntry entry;
    entry.tilePos = pTile->GetPos();
    entry.activateAtMS = Time::GetSystemTime() + delayMS;

    m_steamActivationQueue.push_back(entry);
}

void World::UpdateSteamActivations()
{
    if(m_steamActivationQueue.empty()) {
        return;
    }

    const uint64 nowMS = Time::GetSystemTime();
    if(nowMS - m_lastSteamActivationTick < 200) {
        return;
    }

    m_lastSteamActivationTick = nowMS;

    int32 processed = 0;
    auto it = m_steamActivationQueue.begin();
    while(it != m_steamActivationQueue.end() && processed < 64) {
        const Vector2Int tilePos = it->tilePos;

        TileInfo* pTile = GetTileManager()->GetTile(tilePos.x, tilePos.y);
        if(!pTile) {
            it = m_steamActivationQueue.erase(it);
            continue;
        }

        if(nowMS < it->activateAtMS) {
            ++it;
            continue;
        }

        ItemInfo* pItem = GetItemInfoManager()->GetItemByID(pTile->GetDisplayedItem());
        if(pItem) {
            switch(pItem->id) {
                case ITEM_ID_SPIRIT_STORAGE_UNIT: {
                    RectFloat tileRect(tilePos.x * 32.0f, tilePos.y * 32.0f, (tilePos.x + 1) * 32.0f, (tilePos.y + 1) * 32.0f);
                    auto jars = GetObjectManager()->GetObjectsInRectByItemID(tileRect, ITEM_ID_GHOST_IN_A_JAR);

                    int32 spiritCollected = 0;
                    for(auto* pObj : jars) {
                        if(!pObj) {
                            continue;
                        }

                        spiritCollected += pObj->count;
                        RemoveObject(pObj->objectID);
                    }

                    TileExtra_SpiritStorage* pSpiritData = pTile->GetExtra<TileExtra_SpiritStorage>();
                    if(pSpiritData) {
                        pSpiritData->spiritCount += spiritCollected;

                        if(pSpiritData->spiritCount > 40) {
                            SendSteamPacket(22, tilePos);
                            pTile->SetFG(ITEM_ID_DESTROYED_SPIRIT_STORAGE_UNIT, GetTileManager());
                        }
                        else {
                            SendSteamPacket(20, tilePos);
                        }

                        SendTileUpdate(pTile);
                    }
                    else {
                        SendSteamPacket(20, tilePos);
                    }
                    break;
                }

                case ITEM_ID_STEAM_VENT: {
                    SendSteamPacket(3, tilePos);
                    break;
                }

                case ITEM_ID_STEAM_DOOR: {
                    pTile->ToggleFlag(TILE_FLAG_IS_ON);
                    SendSteamPacket(pTile->HasFlag(TILE_FLAG_IS_ON) ? 4 : 5, tilePos);
                    SendTileUpdate(pTile);
                    break;
                }
            }
        }

        it = m_steamActivationQueue.erase(it);
        ++processed;
    }
}

void World::TriggerSteamPulse(TileInfo* pTile)
{
    if(!pTile) {
        return;
    }

    SendSteamPacket(0, pTile->GetPos());

    eSteamDirection pulseDir = STEAM_DIR_NONE;
    eSteamDirection lastDir = STEAM_DIR_NONE;

    Vector2Int currentPos = pTile->GetPos();
    uint8 maxSteamConductors = 49;
    int32 redirectsSinceFound = 0;
    bool cutPulse = false;
    bool noLookup = false;

    while(maxSteamConductors > 0 && !cutPulse) {
        if(redirectsSinceFound >= 4) {
            break;
        }

        TileInfo* upTile = GetTileManager()->GetTile(currentPos.x, currentPos.y - 1);
        TileInfo* rightTile = GetTileManager()->GetTile(currentPos.x + 1, currentPos.y);
        TileInfo* downTile = GetTileManager()->GetTile(currentPos.x, currentPos.y + 1);
        TileInfo* leftTile = GetTileManager()->GetTile(currentPos.x - 1, currentPos.y);

        if(upTile && (pulseDir == STEAM_DIR_UP || pulseDir == STEAM_DIR_NONE)) {
            if(IsSteamConductor(upTile) && lastDir != STEAM_DIR_DOWN) {
                --maxSteamConductors;
                currentPos.y -= 1;
                lastDir = STEAM_DIR_UP;
                redirectsSinceFound = 0;
                goto found;
            }
            else {
                pulseDir = STEAM_DIR_RIGHT;
                redirectsSinceFound += 1;
                continue;
            }
        }

        if(rightTile && pulseDir == STEAM_DIR_RIGHT) {
            if(IsSteamConductor(rightTile) && lastDir != STEAM_DIR_LEFT) {
                --maxSteamConductors;
                currentPos.x += 1;
                lastDir = STEAM_DIR_RIGHT;
                redirectsSinceFound = 0;
                goto found;
            }
            else {
                pulseDir = STEAM_DIR_DOWN;
                redirectsSinceFound += 1;
                continue;
            }
        }

        if(downTile && pulseDir == STEAM_DIR_DOWN) {
            if(IsSteamConductor(downTile) && lastDir != STEAM_DIR_UP) {
                --maxSteamConductors;
                currentPos.y += 1;
                lastDir = STEAM_DIR_DOWN;
                redirectsSinceFound = 0;
                goto found;
            }
            else {
                pulseDir = STEAM_DIR_LEFT;
                redirectsSinceFound += 1;
                continue;
            }
        }

        if(leftTile && pulseDir == STEAM_DIR_LEFT) {
            if(IsSteamConductor(leftTile) && lastDir != STEAM_DIR_RIGHT) {
                --maxSteamConductors;
                currentPos.x -= 1;
                lastDir = STEAM_DIR_LEFT;
                redirectsSinceFound = 0;
                goto found;
            }
            else {
                pulseDir = STEAM_DIR_UP;
                redirectsSinceFound += 1;
                continue;
            }
        }

        break;

found:

        TileInfo* currentTile = GetTileManager()->GetTile(currentPos.x, currentPos.y);
        if(!currentTile) {
            break;
        }

        const uint16 itemID = currentTile->GetDisplayedItem();

        switch(itemID) {
            case ITEM_ID_STEAM_FUNNEL: {
                pulseDir = currentTile->HasFlag(TILE_FLAG_FLIPPED_X) ? STEAM_DIR_LEFT : STEAM_DIR_RIGHT;
                break;
            }

            case ITEM_ID_STEAM_FUNNEL_UP: {
                pulseDir = STEAM_DIR_UP;
                break;
            }

            case ITEM_ID_STEAM_FUNNEL_DOWN: {
                pulseDir = STEAM_DIR_DOWN;
                break;
            }

            case ITEM_ID_STEAM_SCRAMBLER: {
                std::array<eSteamDirection, 4> dirs = { STEAM_DIR_UP, STEAM_DIR_RIGHT, STEAM_DIR_DOWN, STEAM_DIR_LEFT };
                std::vector<eSteamDirection> choices;

                for(eSteamDirection dir : dirs) {
                    if(IsReverseDirection(lastDir, dir)) {
                        continue;
                    }

                    TileInfo* nextTile = GetTileByDirection(this, currentPos, dir);
                    if(IsSteamConductor(nextTile)) {
                        choices.push_back(dir);
                    }
                }

                if(choices.empty()) {
                    cutPulse = true;
                    break;
                }

                pulseDir = choices[rand() % choices.size()];
                if(pulseDir == STEAM_DIR_UP) {
                    SendSteamPacket(9, currentPos);
                }
                else if(pulseDir == STEAM_DIR_RIGHT) {
                    SendSteamPacket(6, currentPos);
                }
                else if(pulseDir == STEAM_DIR_DOWN) {
                    SendSteamPacket(7, currentPos);
                }
                else if(pulseDir == STEAM_DIR_LEFT) {
                    SendSteamPacket(8, currentPos);
                }

                break;
            }

            case ITEM_ID_STEAM_CROSSOVER: {
                if(currentTile->HasFlag(TILE_FLAG_FLIPPED_X)) {
                    if(lastDir == STEAM_DIR_UP) pulseDir = STEAM_DIR_RIGHT;
                    else if(lastDir == STEAM_DIR_RIGHT) pulseDir = STEAM_DIR_UP;
                    else if(lastDir == STEAM_DIR_LEFT) pulseDir = STEAM_DIR_DOWN;
                    else if(lastDir == STEAM_DIR_DOWN) pulseDir = STEAM_DIR_LEFT;
                }
                else {
                    if(lastDir == STEAM_DIR_UP) pulseDir = STEAM_DIR_LEFT;
                    else if(lastDir == STEAM_DIR_RIGHT) pulseDir = STEAM_DIR_DOWN;
                    else if(lastDir == STEAM_DIR_LEFT) pulseDir = STEAM_DIR_UP;
                    else if(lastDir == STEAM_DIR_DOWN) pulseDir = STEAM_DIR_RIGHT;
                }
                break;
            }

            case ITEM_ID_STEAM_CRANK: {
                if(currentTile->HasFlag(TILE_FLAG_IS_ON)) {
                    if(lastDir == STEAM_DIR_DOWN || lastDir == STEAM_DIR_UP) {
                        noLookup = true;
                        cutPulse = true;
                    }
                }
                else {
                    if(lastDir == STEAM_DIR_LEFT || lastDir == STEAM_DIR_RIGHT) {
                        noLookup = true;
                        cutPulse = true;
                    }
                }
                break;
            }

            default: {
                break;
            }
        }
    }

    int8 lookupIterations = 0;
    while(lookupIterations < 2 && !noLookup) {
        if(pulseDir == STEAM_DIR_UP) {
            TileInfo* pLookupTile = GetTileManager()->GetTile(currentPos.x, currentPos.y - 1);
            if(pLookupTile && ActivateFunctionalSteamTile(this, pLookupTile, 49 - maxSteamConductors)) {
                break;
            }
            pulseDir = STEAM_DIR_RIGHT;
        }

        if(pulseDir == STEAM_DIR_RIGHT) {
            TileInfo* pLookupTile = GetTileManager()->GetTile(currentPos.x + 1, currentPos.y);
            if(pLookupTile && ActivateFunctionalSteamTile(this, pLookupTile, 49 - maxSteamConductors)) {
                break;
            }
            pulseDir = STEAM_DIR_DOWN;
        }

        if(pulseDir == STEAM_DIR_DOWN) {
            TileInfo* pLookupTile = GetTileManager()->GetTile(currentPos.x, currentPos.y + 1);
            if(pLookupTile && ActivateFunctionalSteamTile(this, pLookupTile, 49 - maxSteamConductors)) {
                break;
            }
            pulseDir = STEAM_DIR_LEFT;
        }

        if(pulseDir == STEAM_DIR_LEFT) {
            TileInfo* pLookupTile = GetTileManager()->GetTile(currentPos.x - 1, currentPos.y);
            if(pLookupTile && ActivateFunctionalSteamTile(this, pLookupTile, 49 - maxSteamConductors)) {
                break;
            }
            pulseDir = STEAM_DIR_UP;
        }

        ++lookupIterations;
    }
}

bool World::IsPlayerBanned(uint32 userID)
{
    auto it = m_bannedPlayers.find(userID);
    if(it != m_bannedPlayers.end()) {
        auto now = std::chrono::steady_clock::now();
        auto bannedTime = it->second.BannedAt;
        auto duration = it->second.Duration;

        // Check if ban has expired
        if(now > (bannedTime + duration)) {
            m_bannedPlayers.erase(it);
            return false;
        }
        return true;
    }
    return false;
}

void World::AddBannedPlayer(const WorldBanContext& banCtx)
{
    if(banCtx.UserID == 0) {
        return;
    }
    m_bannedPlayers[banCtx.UserID] = banCtx;
}

void World::RemoveBannedPlayer(uint32 userID)
{
    if(userID == 0) {
        return;
    }
    m_bannedPlayers.erase(userID);
}

void World::ClearBannedPlayers()
{
    m_bannedPlayers.clear();
}

void World::BanPlayer(GamePlayer* pTarget, GamePlayer* pInvoker, uint32 banDurationHours)
{
    if(!pTarget || !pInvoker) {
        return;
    }

    uint32 targetUserID = pTarget->GetUserID();
    if(targetUserID == 0) {
        return;
    }

    // Create ban context
    WorldBanContext banCtx;
    banCtx.UserID = targetUserID;
    banCtx.AdminUserID = pInvoker->GetUserID();
    banCtx.Duration = std::chrono::hours(banDurationHours);
    banCtx.BannedAt = std::chrono::steady_clock::now();

    // Add to banned players list
    AddBannedPlayer(banCtx);

    SendConsoleMessageToAll(pInvoker->GetDisplayName() + " `4world bans `w" + pTarget->GetDisplayName() + " `ofrom `w" + GetWorlName() + "`o!");
    PlaySFXForEveryone("audio/repair.wav", 0);
}
