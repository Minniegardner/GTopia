#include "PlayerManager.h"
#include "GamePlayer.h"
#include "../Context.h"
#include "Math/Math.h"
#include "Utils/StringUtils.h"

namespace {

bool EqualsIgnoreCase(const string& left, const string& right)
{
    if(left.size() != right.size()) {
        return false;
    }

    for(size_t index = 0; index < left.size(); ++index) {
        if(std::tolower((unsigned char)left[index]) != std::tolower((unsigned char)right[index])) {
            return false;
        }
    }

    return true;
}

bool StartsWithIgnoreCase(const string& value, const string& prefix)
{
    if(prefix.size() > value.size()) {
        return false;
    }

    for(size_t index = 0; index < prefix.size(); ++index) {
        if(std::tolower((unsigned char)value[index]) != std::tolower((unsigned char)prefix[index])) {
            return false;
        }
    }

    return true;
}

}

PlayerManager::PlayerManager()
: m_totalPlayerCount(0)
{
}

PlayerManager::~PlayerManager()
{
    RemoveAllPlayers();
}

GamePlayer* PlayerManager::GetPlayerByNetID(uint32 netID)
{
    auto it = m_gamePlayers.find(netID);
    if(it != m_gamePlayers.end()) {
        return it->second;
    }

    return nullptr;
}

GamePlayer* PlayerManager::IsPlayerAlreadyOn(GamePlayer* pNewPlayer)
{
    if(!pNewPlayer) {
        return nullptr;
    }

    for(auto& [_, pPlayer] : m_gamePlayers) {
        if(!pPlayer || pPlayer == pNewPlayer) {
            continue;
        }

        if(pNewPlayer->GetUserID() == pPlayer->GetUserID() && !pPlayer->HasState(PLAYER_STATE_DELETE)) {
            return pPlayer;
        }
    }

    return nullptr;
}

GamePlayer* PlayerManager::GetPlayerByUserID(uint32 userID)
{
    for(auto& [_, pPlayer] : m_gamePlayers) {
        if(!pPlayer) {
            continue;
        }

        if(pPlayer->GetUserID() == userID) {
            return pPlayer;
        }
    }

    return nullptr;
}

GamePlayer* PlayerManager::GetPlayerByRawName(const string& rawName)
{
    if(rawName.empty()) {
        return nullptr;
    }

    for(auto& [_, pPlayer] : m_gamePlayers) {
        if(!pPlayer) {
            continue;
        }

        if(EqualsIgnoreCase(pPlayer->GetRawName(), rawName)) {
            return pPlayer;
        }
    }

    return nullptr;
}

std::vector<GamePlayer*> PlayerManager::FindPlayersByNamePrefix(const string& query, bool sameWorldOnly, uint32 worldID)
{
    std::vector<GamePlayer*> matches;
    matches.reserve(m_gamePlayers.size());

    for(auto& [_, pPlayer] : m_gamePlayers) {
        if(!pPlayer) {
            continue;
        }

        if(sameWorldOnly && pPlayer->GetCurrentWorld() != worldID) {
            continue;
        }

        if(!query.empty() && !StartsWithIgnoreCase(pPlayer->GetRawName(), query)) {
            continue;
        }

        matches.push_back(pPlayer);
    }

    return matches;
}

void PlayerManager::AddPlayer(GamePlayer* pPlayer)
{
    if(!pPlayer) {
        return;
    }

    m_gamePlayers.insert_or_assign(pPlayer->GetNetID(), pPlayer);
}

void PlayerManager::RemovePlayer(uint32 netID)
{
    GamePlayer* pPlayer = GetPlayerByNetID(netID);
    if(!pPlayer) {
        return;
    }

    SAFE_DELETE(pPlayer);
    m_gamePlayers.erase(netID);
}

void PlayerManager::RemoveAllPlayers()
{
    for(auto& [_, pPlayer] : m_gamePlayers) {
        SAFE_DELETE(pPlayer);
    }

    m_gamePlayers.clear();
}

uint32 PlayerManager::GetPlayerCount()
{
    return m_gamePlayers.size();
}

uint32 PlayerManager::GetTotalPlayerCount()
{
    return Max(m_gamePlayers.size(), m_totalPlayerCount);
}

void PlayerManager::UpdatePlayers()
{
    if(m_lastUpdateTime.GetElapsedTime() < TICK_INTERVAL) {
        return;
    }

    for(auto& [_, pPlayer] : m_gamePlayers) {
        if(!pPlayer) {
            continue;
        }

        if(!pPlayer->HasState(PLAYER_STATE_IN_GAME)) {
            continue;
        }

        if(!pPlayer->HasState(PLAYER_STATE_LOGGING_OFF)) {
            pPlayer->Update();

            if(pPlayer->GetLastDBSaveTime().GetElapsedTime() >= pPlayer->GetNextDBSaveTime()) {
                pPlayer->SaveToDatabase();
            }
        }

        if(pPlayer->HasState(PLAYER_STATE_DELETE)) {
            m_pendingDelete.push_back(pPlayer);
        }
    }

    for(auto& pPlayer : m_pendingDelete) {
        if(!GetPlayerByNetID(pPlayer->GetNetID())) {
            continue;
        }

        m_gamePlayers.erase(pPlayer->GetNetID());
    }

    m_pendingDelete.clear();
    m_lastUpdateTime.Reset();
}

void PlayerManager::SaveAllToDatabase()
{
    for(auto& [_, pPlayer] : m_gamePlayers) {
        if(!pPlayer) {
            continue;
        }

        pPlayer->SaveToDatabase();
    }
}

PlayerManager* GetPlayerManager() { return PlayerManager::GetInstance(); }