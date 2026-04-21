#include "GamePlayer.h"
#include "Database/Table/PlayerDBTable.h"
#include "../Context.h"
#include "Math/Random.h"
#include "IO/Log.h"
#include "../Server/ServerManager.h"
#include "../Server/GameServer.h"
#include "Proton/ProtonUtils.h"
#include "Packet/GamePacket.h"

namespace {

void LogAccountFlow(const char* eventName, uint32 userID, uint32 accountID, uint64 version, const char* sourceOfTruth, const char* reasonCode)
{
    LOGGER_LOG_INFO(
        "event=%s user_id=%u account_id=%u version=%llu source_of_truth=%s reason=%s",
        eventName ? eventName : "unknown",
        userID,
        accountID,
        (unsigned long long)version,
        sourceOfTruth ? sourceOfTruth : "unknown",
        reasonCode ? reasonCode : "none"
    );
}

void SendConsoleAndFail(GamePlayer* pPlayer, const string& consoleMessage)
{
    if(!pPlayer) {
        return;
    }

    if(!consoleMessage.empty()) {
        pPlayer->SendOnConsoleMessage(consoleMessage);
    }

    pPlayer->SendLogonFailWithLog("");
}

void SendConsoleOnly(GamePlayer* pPlayer, const string& consoleMessage)
{
    if(!pPlayer || consoleMessage.empty()) {
        return;
    }

    pPlayer->SendOnConsoleMessage(consoleMessage);
}

}

GamePlayer::GamePlayer(ENetPeer* pPeer)
: Player(pPeer)
{
}

GamePlayer::~GamePlayer()
{
}

void GamePlayer::OnHandleDatabase(QueryTaskResult&& result)
{    
    switch(m_state) {
        case PLAYER_STATE_LOGIN_CHECKING_ACCOUNT: {
            LoginCheckingAccount(std::move(result));
            break;
        }

        case PLAYER_STATE_COUNT_CREATED_FROM_IP: {
            CheckCountOfCreatedAccsFromIP(std::move(result));
            break;
        }

        case PLAYER_STATE_CREATING_ACCOUNT: {
            CreatingAccount(std::move(result));
            break;
        }

        case PLAYER_STATE_UPDATE_IDENTIFIERS: {
            UpdatedIdentifiers(std::move(result));
        }
    }

    result.Destroy();
}

void GamePlayer::StartLoginRequest(ParsedTextPacket<25> &packet)
{
    if(m_state != PLAYER_STATE_LOGIN_REQUEST) {
        SendLogonFailWithLog("");
        return;
    }

    if(!m_loginDetail.Serialize(packet, this, false)) {
        SendLogonFailWithLog("`4HUH?! ``Are you sure everything is alright?");
        return;
    }

    m_triedHashFallback = false;
    m_triedMacFallback = false;
    LogAccountFlow("login_start", 0, 0, 0, "login_request", "master_begin_lookup");

    if(m_loginDetail.tankIDName.empty()) {
        if(m_loginDetail.requestedName.size() < 3) {
            SendConsoleAndFail(this, "`oYou'll need a name `$3 chars`o or longer to play online with. (select cancel and enter a longer name)``");
            return;
        }

        if(m_loginDetail.requestedName.size() > 28) {
            SendConsoleAndFail(this, "`oIs that guest name length even possible? Contact the devs!``");
            return;
        }
    }

    m_state = PLAYER_STATE_LOGIN_GETTING_ACCOUNT;
    LoginGetAccount();
}

void GamePlayer::LoginGetAccount()
{
    if(m_state != PLAYER_STATE_LOGIN_GETTING_ACCOUNT) {
        SendLogonFailWithLog("");
        return;
    }

    m_state = PLAYER_STATE_LOGIN_CHECKING_ACCOUNT;

    if(m_loginDetail.tankIDName.empty()) {
        if(m_loginDetail.platformType == Proton::PLATFORM_ID_IOS) {
            if(m_loginDetail.mac == "02:00:00:00:00:00") {
                QueryRequest req = MakePlayerByVIDReq(m_loginDetail.vid, m_loginDetail.platformType, GetNetID());
                DatabasePlayerExec(GetContext()->GetDatabasePool(), DB_PLAYER_GET_BY_VID, req);
            }
            else {
                QueryRequest req = MakePlayerByHashReq(m_loginDetail.hash, m_loginDetail.platformType, GetNetID());
                DatabasePlayerExec(GetContext()->GetDatabasePool(), DB_PLAYER_GET_BY_HASH, req);
            }
        }
        else if(m_loginDetail.platformType == Proton::PLATFORM_ID_ANDROID) {
            if(!m_loginDetail.gid.empty()) {
                QueryRequest req = MakePlayerByGIDReq(m_loginDetail.gid, m_loginDetail.platformType, GetNetID());
                DatabasePlayerExec(GetContext()->GetDatabasePool(), DB_PLAYER_GET_BY_GID, req);
            }
            else {
                if(m_loginDetail.mac == "02:00:00:00:00:00") {
                    SendLogonFailWithLog("`4Unable to log on: ``Unfortunately your device has a Mac address of 02:00:00:00:00:00 which is invalid.");
                    return;
                }

                QueryRequest req = MakePlayerByGIDReq(m_loginDetail.mac, m_loginDetail.platformType, GetNetID());
                DatabasePlayerExec(GetContext()->GetDatabasePool(), DB_PLAYER_GET_BY_MAC, req);
            }
        }
        else {
            if(m_loginDetail.mac == "02:00:00:00:00:00") {
                SendLogonFailWithLog("`4Unable to log on: ``Unfortunately your device has a Mac address of 02:00:00:00:00:00 which is invalid.");
                return;
            }
    
            QueryRequest req = MakePlayerByMacReq(m_loginDetail.mac, m_loginDetail.platformType, GetNetID());
            DatabasePlayerExec(GetContext()->GetDatabasePool(), DB_PLAYER_GET_BY_MAC, req);
        }
    }
    else {
        QueryRequest req = MakeGetPlayerByNameAndPass(m_loginDetail.tankIDName, m_loginDetail.tankIDPass, GetNetID());
        DatabasePlayerExec(GetContext()->GetDatabasePool(), DB_PLAYER_GET_BY_NAME_AND_PASS, req);
    }
}

void GamePlayer::LoginCheckingAccount(QueryTaskResult&& result)
{
    if(m_state != PLAYER_STATE_LOGIN_CHECKING_ACCOUNT) {
        SendLogonFailWithLog("");
        return;
    }

    if(!result.result) {
        m_state = PLAYER_STATE_LOGIN_REQUEST;
        SendConsoleOnly(this, "`oServer requesting you relog-on");
        return;
    }

    if(result.result->GetRowCount() > 0) {
        m_userID = result.result->GetField("ID", 0).GetUINT();
        LogAccountFlow("profile_loaded", m_userID, m_userID, 0, "db", "master_identifier_match");
    }
    else {
        if(m_loginDetail.tankIDName.empty()) {
            if(!m_triedHashFallback && m_loginDetail.hash != 0) {
                m_triedHashFallback = true;
                m_state = PLAYER_STATE_LOGIN_CHECKING_ACCOUNT;

                QueryRequest req = MakePlayerByHashReq(m_loginDetail.hash, m_loginDetail.platformType, GetNetID());
                DatabasePlayerExec(GetContext()->GetDatabasePool(), DB_PLAYER_GET_BY_HASH, req);
                return;
            }

            if(!m_triedMacFallback && !m_loginDetail.mac.empty() && m_loginDetail.mac != "02:00:00:00:00:00") {
                m_triedMacFallback = true;
                m_state = PLAYER_STATE_LOGIN_CHECKING_ACCOUNT;

                QueryRequest req = MakePlayerByMacReq(m_loginDetail.mac, m_loginDetail.platformType, GetNetID());
                DatabasePlayerExec(GetContext()->GetDatabasePool(), DB_PLAYER_GET_BY_MAC, req);
                return;
            }

            LogAccountFlow("profile_create_path", 0, 0, 0, "db", "identifier_lookup_miss");

            m_state = PLAYER_STATE_COUNT_CREATED_FROM_IP;
        
            QueryRequest req = MakeCountCreatedAccByIP(m_address, GetNetID());
            DatabasePlayerExec(GetContext()->GetDatabasePool(), DB_PLAYER_COUNT_ACC_IP, req);
        }
        else {
            m_state = PLAYER_STATE_LOGIN_REQUEST;
            SendConsoleOnly(this, "`oWrong GrowID or Password``");
            return;
        }

        return;
    }

    if(PlayerSession* pCachedSession = GetGameServer()->GetPlayerSessionByUserID(m_userID)) {
        if(pCachedSession->serverID != 0) {
            GetServerManager()->SendCrossServerActionExecute(
                pCachedSession->serverID,
                TCP_CROSS_ACTION_KICK,
                m_userID,
                0,
                "SYSTEM",
                "",
                0,
                pCachedSession->name
            );
        }

        GetGameServer()->DeletePlayerSession(m_userID);
        SendOnConsoleMessage("`4ALREADY ON: `oThis account is already online, kicking it off so you could log on.");
    }

    m_state = PLAYER_STATE_UPDATE_IDENTIFIERS;
    ExecUpdatePlayerIdentifier(
        GetContext()->GetDatabasePool(), m_userID,
        m_loginDetail.mac, m_loginDetail.vid, m_loginDetail.sid, m_loginDetail.rid,
        m_loginDetail.gid, m_loginDetail.hash, GetNetID()
    );
}

void GamePlayer::CheckCountOfCreatedAccsFromIP(QueryTaskResult&& result)
{
    uint32 createdAccCountFromIP = result.result->GetRowCount();
    if(createdAccCountFromIP >= GetContext()->GetGameConfig()->maxAccountsPerIP) {
        SendLogonFailWithLog("``Too many accounts created from this IP address (" + string(m_address) + "). `4Unable to create new account for guest.");
        return;
    }

    QueryRequest req = MakePlayerCreateReq(
        m_loginDetail.requestedName, 
        m_loginDetail.platformType, 
        RandomRangeInt(100, 999),
        m_loginDetail.mac,
        m_address,
        GetNetID());

    m_state = PLAYER_STATE_CREATING_ACCOUNT;
    DatabasePlayerExec(GetContext()->GetDatabasePool(), DB_PLAYER_CREATE, req);
}

void GamePlayer::CreatingAccount(QueryTaskResult&& result)
{
    if(m_state != PLAYER_STATE_CREATING_ACCOUNT) {
        SendLogonFailWithLog("");
        return;
    }

    if(result.increment == 0) {
        SendLogonFailWithLog("`4OOPS! ``Please re-connect");
        return;
    }

    m_userID = result.increment;
    LogAccountFlow("profile_create_path", m_userID, m_userID, 0, "db", "new_guest_created");

    m_state = PLAYER_STATE_UPDATE_IDENTIFIERS;
    ExecUpdatePlayerIdentifier(
        GetContext()->GetDatabasePool(), m_userID,
        m_loginDetail.mac, m_loginDetail.vid, m_loginDetail.sid, m_loginDetail.rid,
        m_loginDetail.gid, m_loginDetail.hash, GetNetID()
    );
}

void GamePlayer::UpdatedIdentifiers(QueryTaskResult&& result)
{
    m_state = PLAYER_STATE_SWITCHING_TO_GAME;
    SwitchingToGame();
}

void GamePlayer::SwitchingToGame()
{
    if(m_state != PLAYER_STATE_SWITCHING_TO_GAME) {
        SendLogonFailWithLog("");
        return;
    }

    ServerInfo* pServer = GetServerManager()->GetBestGameServer();
    if(!pServer) {
        SendLogonFailWithLog("");
        LOGGER_LOG_WARN("Tried to send player to game but the server is NULL?");
        return;
    }

    PlayerSession session;
    session.serverID = pServer->serverID;
    session.userID = m_userID;
    session.loginToken = RandomRangeInt(100000, 9999999);
    session.ip = m_address;
    session.name = m_loginDetail.tankIDName.empty() ? m_loginDetail.requestedName : m_loginDetail.tankIDName;

    GetGameServer()->AddPlayerSession(session);
    SendOnSendToServer(pServer->port, session.loginToken, m_userID, pServer->wanIP);
}
