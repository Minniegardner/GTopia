#include "GamePlayer.h"
#include "../Server/MasterBroadway.h"
#include "../Context.h"
#include "IO/Log.h"
#include "Utils/Timer.h"
#include "Item/ItemInfoManager.h"
#include "Player/PlayerTribute.h"
#include "Player/RoleManager.h"
#include "Database/Table/PlayerDBTable.h"

GamePlayer::GamePlayer(ENetPeer* pPeer) 
: Player(pPeer), m_currentWorldID(0), m_joiningWorld(false), m_guestID(1)
{
}

GamePlayer::~GamePlayer()
{
}

void GamePlayer::OnHandleDatabase(QueryTaskResult&& result)
{
    if(HasState(PLAYER_STATE_LOGIN_GETTING_ACCOUNT)) {
        LoadingAccount(std::move(result));
    }

    result.Destroy();
}

void GamePlayer::OnHandleTCP(VariantVector&& result)
{
    if(HasState(PLAYER_STATE_CHECKING_SESSION)) {
        CheckingLoginSession(std::move(result));
    }
}

void GamePlayer::StartLoginRequest(ParsedTextPacket<25>& packet)
{
    if(!HasState(PLAYER_STATE_LOGIN_REQUEST)) {
        SendLogonFailWithLog("");
        return;
    }

    RemoveState(PLAYER_STATE_LOGIN_REQUEST);

    bool val = m_loginDetail.Serialize(packet, this, true);
    LOGGER_LOG_ERROR(" GOT VALLLLLLLLLL %d", val);
    if(!val) {
        SendLogonFailWithLog("`4HUH?! ``Are you sure everything is alright?");
        return;
    }

    m_userID = m_loginDetail.user;
    SetState(PLAYER_STATE_CHECKING_SESSION);
    GetMasterBroadway()->SendCheckSessionPacket(GetNetID(), m_loginDetail.user, m_loginDetail.token, GetContext()->GetID());
}

void GamePlayer::CheckingLoginSession(VariantVector&& result)
{
    RemoveState(PLAYER_STATE_CHECKING_SESSION);
    /*if(!result[2].GetBool()) {
        SendLogonFailWithLog("`4OOPS! ``Please re-connect server says you're not belong to this server");
        return;
    }*/

    SetState(PLAYER_STATE_LOGIN_GETTING_ACCOUNT);

    QueryRequest req = MakeGetPlayerDataReq(m_userID, GetNetID());
    DatabasePlayerExec(GetContext()->GetDatabasePool(), DB_PLAYER_GET_DATA, req);
}

void GamePlayer::LoadingAccount(QueryTaskResult&& result)
{
    RemoveState(PLAYER_STATE_LOGIN_GETTING_ACCOUNT);
    if(!result.result) {
        SendLogonFailWithLog("`4OOPS! ``Failed to load your account, please re-connect");
        return;
    }

    uint32 roleID = result.result->GetField("RoleID", 0).GetUINT();
    if(roleID == 0) {
        roleID = 3; // default role
    }

    m_pRole = GetRoleManager()->GetRole(roleID);

    if(!m_pRole) {
        SendLogonFailWithLog("`4OOPS! ``Something went wrong while setting you up, please re-connect");
        LOGGER_LOG_WARN("Failed to set player role %d for user %d", roleID, m_userID);
        return;
    }

    m_inventory.SetVersion(m_loginDetail.protocol);
    string dbInv = result.result->GetField("Inventory", 0).GetString();
    if(dbInv != "NULL") {
        uint32 invMemEstimate = dbInv.size()/2;
        uint8* pInvData = new uint8[invMemEstimate];

        MemoryBuffer invMemBuffer(pInvData, invMemEstimate);
        m_inventory.Serialize(invMemBuffer, false, true);

        SAFE_DELETE_ARRAY(pInvData);
    }

    m_guestID = result.result->GetField("GuestID", 0).GetUINT();

    SetState(PLAYER_STATE_ENTERING_GAME);
    TransferingPlayerToGame();
}

void GamePlayer::TransferingPlayerToGame()
{
    string settings;
    settings += "proto=144"; /** search it what it effects in client */
    settings += "|server_tick=" + ToString(Time::GetSystemTime());
    settings += "|choosemusic=audio/mp3/about_theme.mp3";
    settings += "|usingStoreNavigation=1";
    settings += "|enableInventoryTab=1";

    ItemsClientData itemData = GetItemInfoManager()->GetClientData(m_loginDetail.platformType);
    auto pGameConfig = GetContext()->GetGameConfig();

    PlayerTributeClientData tributeData = GetPlayerTributeManager()->GetClientData();

    SendWelcomePacket(itemData.hash, pGameConfig->cdnServer, pGameConfig->cdnPath, settings, tributeData.hash);
}

void GamePlayer::SaveToDatabase()
{
    uint32 invMemSize = m_inventory.GetMemEstimate(true);
    uint8* pInvData = new uint8[invMemSize];

    MemoryBuffer invMemBuffer(pInvData, invMemSize);
    m_inventory.Serialize(invMemBuffer, true, true);

    QueryRequest req = MakeSavePlayerReq(
        m_userID,
        m_pRole->GetID(),
        ToHex(pInvData, invMemSize),
        GetNetID()
    );

    DatabasePlayerExec(GetContext()->GetDatabasePool(), DB_PLAYER_SAVE, req);
    SAFE_DELETE_ARRAY(pInvData);
}

string GamePlayer::GetDisplayName()
{
    string displayName;
    if(m_pRole->GetNameColor() != 0) {
        displayName += "`"; 
        displayName += m_pRole->GetNameColor();
    }
    displayName += m_pRole->GetPrefix();

    if(!m_loginDetail.tankIDName.empty()) {
        displayName += m_loginDetail.tankIDName;
    }
    else {
        displayName += m_loginDetail.requestedName + "_" + ToString(m_guestID);
    }

    displayName += m_pRole->GetSuffix();
    return displayName;
}

string GamePlayer::GetSpawnData(bool local)
{
    string spawnData;
    spawnData += "spawn|avatar\n";
    spawnData += "netID|" + ToString(GetNetID()) + "\n";
    spawnData += "userID|" + ToString(m_userID) + "\n";
    spawnData += "colrect|0|0|20|30\n"; //its ok to hardcoded (for now?)
    spawnData += "posXY|" + ToString(m_worldPos.x)  + "|" + ToString(m_worldPos.y) + "\n"; 
    spawnData += "name|" + GetDisplayName() + "``\n";
    spawnData += "country|" + m_loginDetail.country + "\n";
    spawnData += "invis|0\n"; // todo
    spawnData += "mstate|";
    spawnData += m_pRole->HasPerm(ROLE_PERM_MSTATE) ? "1\n" : "0\n";
    spawnData += "smstate|" ;
    spawnData += m_pRole->HasPerm(ROLE_PERM_SMSTATE) ? "1\n" : "0\n";
    spawnData += "onlineID|\n";

    if(local) {
        spawnData += "type|local\n";
    }

    return spawnData;
}
