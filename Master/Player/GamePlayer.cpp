#include "GamePlayer.h"
#include "Database/Table/PlayerDBTable.h"
#include "../Context.h"
#include "Math/Random.h"
#include "IO/Log.h"
#include "../Server/ServerManager.h"
#include "../Server/GameServer.h"
#include "Proton/ProtonUtils.h"

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
        SendLogonFailWithLog("`4OOPS! ``Something went wrong please re-connect");
        return;
    }

    if(result.result->GetRowCount() > 0) {
        m_userID = result.result->GetField("ID", 0).GetUINT();
    }
    else {
        if(m_loginDetail.tankIDName.empty()) {
            m_state = PLAYER_STATE_COUNT_CREATED_FROM_IP;
        
            QueryRequest req = MakeCountCreatedAccByIP(m_address, GetNetID());
            DatabasePlayerExec(GetContext()->GetDatabasePool(), DB_PLAYER_COUNT_ACC_IP, req);
        }
        else {
            SendLogonFailWithLog("`4Unable to log on:`` That `wGrowID`` doesn't seem valid, or the password is wrong. If you don't have one, press `wCancel``, un-check `w'I have a GrowID'``, then click `wConnect``.");
            return;
        }

        return;
    }

    if(GetGameServer()->GetPlayerSessionByUserID(m_userID)) { // add ALREADY ON THINGGGGGGG
        SendLogonFailWithLog("`#You are still online, please wait few seconds and re-login again.");
        return;
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
