#include "GamePlayer.h"
#include "../Server/MasterBroadway.h"
#include "../Context.h"
#include "IO/Log.h"
#include "Utils/Timer.h"
#include "Item/ItemInfoManager.h"
#include "Player/PlayerTribute.h"
#include "Player/RoleManager.h"
#include "Database/Table/PlayerDBTable.h"
#include "Player/PlayModManager.h"
#include "../World/WorldManager.h"
#include "Math/Random.h"
#include "Dialog/PlayerDialog.h"
#include "Dialog/RenderWorldDialog.h"
#include "Utils/DialogBuilder.h"
#include "../Server/GameServer.h"
#include <algorithm>
#include <cmath>

bool GamePlayer::IsTradeOfferExists(uint16 itemID) const
{
    return std::any_of(m_tradeOffers.begin(), m_tradeOffers.end(), [itemID](const TradeOffer& tradeOffer) {
        return tradeOffer.ID == itemID;
    });
}

void GamePlayer::RemoveTradeOffer(uint16 itemID)
{
    m_tradeOffers.erase(
        std::remove_if(m_tradeOffers.begin(), m_tradeOffers.end(), [itemID](const TradeOffer& tradeOffer) {
            return tradeOffer.ID == itemID;
        }),
        m_tradeOffers.end()
    );
}

void GamePlayer::AddTradeOffer(TradeOffer tradeOffer)
{
    if(tradeOffer.Amount == 0) {
        RemoveTradeOffer(tradeOffer.ID);
        return;
    }

    if(IsTradeOfferExists(tradeOffer.ID)) {
        for(auto& existingOffer : m_tradeOffers) {
            if(existingOffer.ID == tradeOffer.ID) {
                existingOffer.Amount = tradeOffer.Amount;
                return;
            }
        }
    }

    m_tradeOffers.push_back(tradeOffer);
}

void GamePlayer::ClearTradeOffers()
{
    m_tradeOffers.clear();
}

void GamePlayer::StartTrade(GamePlayer* player)
{
    if(!player || player == this || IsTrading() || player->IsTrading()) {
        return;
    }

    SetTrading(true);
    SetTradingWithUserID(player->GetNetID());
    SetTradeAccepted(false);
    SetTradeConfirmed(false);
    SetTradeAcceptedAt(0);
    SetTradeConfirmedAt(0);
    ClearTradeOffers();

    player->SetTrading(true);
    player->SetTradingWithUserID(GetNetID());
    player->SetTradeAccepted(false);
    player->SetTradeConfirmed(false);
    player->SetTradeAcceptedAt(0);
    player->SetTradeConfirmedAt(0);
    player->ClearTradeOffers();

    SendTradeAlert(player);
    player->SendTradeAlert(this);
    SendStartTrade(player);
    player->SendStartTrade(this);
}

void GamePlayer::CancelTrade(bool busy, std::string customMessage)
{
    GamePlayer* pTarget = GetGameServer()->GetPlayerByNetID(GetTradingWithUserID());
    if(pTarget && pTarget != this) {
        pTarget->SetTrading(false);
        pTarget->SetTradingWithUserID(0);
        pTarget->SetTradeAccepted(false);
        pTarget->SetTradeConfirmed(false);
        pTarget->SetTradeAcceptedAt(0);
        pTarget->SetTradeConfirmedAt(0);
        pTarget->ClearTradeOffers();
        pTarget->SendForceTradeEnd();
        if(!customMessage.empty()) {
            pTarget->SendOnConsoleMessage(customMessage);
        }
    }

    SetTrading(false);
    SetTradingWithUserID(0);
    SetTradeAccepted(false);
    SetTradeConfirmed(false);
    SetTradeAcceptedAt(0);
    SetTradeConfirmedAt(0);
    ClearTradeOffers();

    if(busy) {
        SendForceTradeEnd();
    }
}

void GamePlayer::ModifyOffer(uint16 itemID, uint16 amount)
{
    if(!IsTrading()) {
        return;
    }

    if(amount == 0) {
        RemoveOffer(itemID);
        return;
    }

    uint8 ownedCount = GetInventory().GetCountOfItem(itemID);
    if(ownedCount == 0) {
        return;
    }

    if(amount > ownedCount) {
        amount = ownedCount;
    }

    AddTradeOffer({ itemID, (uint8)amount });

    SetTradeAccepted(false);
    SetTradeConfirmed(false);
    SetTradeAcceptedAt(0);
    SetTradeConfirmedAt(0);
    SetLastChangeTradeDeal(Time::GetSystemTime());

    GamePlayer* pTarget = GetGameServer()->GetPlayerByNetID(GetTradingWithUserID());
    if(pTarget) {
        pTarget->SetTradeAccepted(false);
        pTarget->SetTradeConfirmed(false);
        pTarget->SetTradeAcceptedAt(0);
        pTarget->SetTradeConfirmedAt(0);
    }
}

void GamePlayer::RemoveOffer(uint16 itemID)
{
    RemoveTradeOffer(itemID);
    SetTradeAccepted(false);
    SetTradeConfirmed(false);
    SetTradeAcceptedAt(0);
    SetTradeConfirmedAt(0);
    SetLastChangeTradeDeal(Time::GetSystemTime());
}

void GamePlayer::AcceptOffer(bool status)
{
    if(!IsTrading()) {
        return;
    }

    SetTradeAccepted(status);
    SetTradeAcceptedAt(status ? Time::GetSystemTime() : 0);

    GamePlayer* pTarget = GetGameServer()->GetPlayerByNetID(GetTradingWithUserID());
    if(pTarget) {
        pTarget->SetTradeAccepted(false);
        pTarget->SetTradeConfirmed(false);
        pTarget->SetTradeConfirmedAt(0);
    }
}

void GamePlayer::ConfirmOffer()
{
    if(!IsTrading() || !IsTradeAccepted()) {
        return;
    }

    GamePlayer* pTarget = GetGameServer()->GetPlayerByNetID(GetTradingWithUserID());
    if(!pTarget || !pTarget->IsTrading()) {
        CancelTrade();
        return;
    }

    for(const auto& tradeOffer : m_tradeOffers) {
        if(GetInventory().GetCountOfItem(tradeOffer.ID) < tradeOffer.Amount) {
            CancelTrade(true, "`4Trade failed.`` You no longer have enough items.");
            return;
        }
    }

    for(const auto& tradeOffer : pTarget->GetTradeOffers()) {
        if(pTarget->GetInventory().GetCountOfItem(tradeOffer.ID) < tradeOffer.Amount) {
            pTarget->CancelTrade(true, "`4Trade failed.`` You no longer have enough items.");
            return;
        }
    }

    for(const auto& tradeOffer : m_tradeOffers) {
        ModifyInventoryItem(tradeOffer.ID, -(int16)tradeOffer.Amount);
        pTarget->ModifyInventoryItem(tradeOffer.ID, tradeOffer.Amount);
    }

    for(const auto& tradeOffer : pTarget->GetTradeOffers()) {
        pTarget->ModifyInventoryItem(tradeOffer.ID, -(int16)tradeOffer.Amount);
        ModifyInventoryItem(tradeOffer.ID, tradeOffer.Amount);
    }

    AddTradeHistory("Traded with " + pTarget->GetRawName());
    pTarget->AddTradeHistory("Traded with " + GetRawName());
    SetLastTradedAt(Time::GetSystemTime());
    pTarget->SetLastTradedAt(Time::GetSystemTime());

    CancelTrade(false);
    pTarget->CancelTrade(false);
}

void GamePlayer::SendTradeStatus(GamePlayer* player)
{
    if(!player) {
        return;
    }

    DialogBuilder db;
    db.SetDefaultColor('o')
    ->AddLabelWithIcon("`wTrade Status``", ITEM_ID_WRENCH, true)
    ->AddTextBox("`oTrading with `w" + player->GetDisplayName() + "``")
    ->AddTextBox("`oYour offers: `w" + ToString((int)m_tradeOffers.size()) + "``")
    ->AddTextBox("`oTheir offers: `w" + ToString((int)player->GetTradeOffers().size()) + "``")
    ->EndDialog("popup", "", "Continue");

    SendOnDialogRequest(db.Get());
}

void GamePlayer::SendTradeAlert(GamePlayer* player)
{
    if(!player) {
        return;
    }

    SendOnConsoleMessage("`oTrade request sent to `w" + player->GetDisplayName() + "``.");
}

void GamePlayer::SendStartTrade(GamePlayer* player)
{
    if(!player) {
        return;
    }

    DialogBuilder db;
    db.SetDefaultColor('o')
    ->AddLabelWithIcon("`wTrade``", ITEM_ID_WRENCH, true)
    ->AddTextBox("`oTrading with `w" + player->GetDisplayName() + "``")
    ->AddTextBox("`oUse the trade command to add or remove items.")
    ->EndDialog("popup", "", "Continue");

    SendOnDialogRequest(db.Get());
}

void GamePlayer::SendForceTradeEnd()
{
    SendOnConsoleMessage("`oTrade ended.``");
}

void GamePlayer::AddTradeHistory(std::string entry)
{
    if(entry.empty()) {
        return;
    }

    m_tradeHistory.push_back(std::move(entry));
    if(m_tradeHistory.size() > 20) {
        m_tradeHistory.erase(m_tradeHistory.begin());
    }
}

bool GamePlayer::HasAchievement(const std::string& achievement) const
{
    return m_achievements.find(achievement) != m_achievements.end();
}

void GamePlayer::GiveAchievement(const std::string& achievement)
{
    if(achievement.empty() || HasAchievement(achievement)) {
        return;
    }

    m_achievements.insert(achievement);
    SendAchievement(achievement);
}

void GamePlayer::SendAchievement(std::string achievementName)
{
    if(achievementName.empty()) {
        return;
    }

    SendOnConsoleMessage("`wAchievement unlocked:`` " + achievementName);
}

GamePlayer::GamePlayer(ENetPeer* pPeer) 
: Player(pPeer), m_currentWorldID(0), m_joiningWorld(false), m_guestID(1), m_lastItemActivateTime(0), m_state(0),
m_loggingOff(false), m_gems(0), m_lastObjectCollectTime(0)
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

    if(!HasState(PLAYER_STATE_IN_GAME)) {
        return;
    }

    if(HasState(PLAYER_STATE_RENDERING_WORLD)) {
        HandleRenderWorld(std::move(result));
    }
}

void GamePlayer::StartLoginRequest(ParsedTextPacket<25>& packet)
{
    if(!HasState(PLAYER_STATE_LOGIN_REQUEST)) {
        SendLogonFailWithLog("");
        return;
    }

    RemoveState(PLAYER_STATE_LOGIN_REQUEST);

    if(!m_loginDetail.Serialize(packet, this, true)) {
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

    bool sessionAgreed = result[2].GetBool();
    if(!sessionAgreed) {
        SendLogonFailWithLog("`4OOPS! ``Please re-connect server says you're not belong to this server");
        return;
    }

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

    m_gems = result.result->GetField("Gems", 0).GetINT();
    if(m_gems < 0) {
        m_gems = 0;
    }

    m_pRole = GetRoleManager()->GetRole(roleID);

    if(!m_pRole) {
        SendLogonFailWithLog("`4OOPS! ``Something went wrong while setting you up, please re-connect");
        LOGGER_LOG_WARN("Failed to set player role %d for user %d", roleID, m_userID);
        return;
    }

    m_inventory.SetVersion(m_loginDetail.protocol);
    string dbInv = result.result->GetField("Inventory", 0).GetString();
    if(!dbInv.empty()) {
        uint32 invMemEstimate = dbInv.size() / 2;
        uint8* pInvData = new uint8[invMemEstimate];

        HexToBytes(dbInv, pInvData);

        MemoryBuffer invMemBuffer(pInvData, invMemEstimate);
        m_inventory.Serialize(invMemBuffer, false, true);

        SAFE_DELETE_ARRAY(pInvData);
    }

    if(m_inventory.GetCountOfItem(ITEM_ID_FIST) == 0) {
        m_inventory.AddItem(ITEM_ID_FIST, 1);
    }

    if(m_inventory.GetCountOfItem(ITEM_ID_WRENCH) == 0) {
        m_inventory.AddItem(ITEM_ID_WRENCH, 1);
    }

    m_guestID = result.result->GetField("GuestID", 0).GetUINT();

    for(uint8 i = 0; i < BODY_PART_SIZE; ++i) {
        uint16 cloth = m_inventory.GetClothByPart((eBodyPart)i);

        ItemInfo* pItem = GetItemInfoManager()->GetItemByID(cloth);
        if(!pItem) {
            continue;
        }

        if(pItem->type == ITEM_TYPE_CLOTHES && pItem->playModType != PLAYMOD_TYPE_NONE) {
            AddPlayMod(pItem->playModType, true);
        }
    }

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

void GamePlayer::HandleRenderWorld(VariantVector&& result)
{
    if(!HasState(PLAYER_STATE_RENDERING_WORLD)) {
        return;
    }

    int32 renderResult = result[2].GetINT();

    if(renderResult == TCP_RESULT_OK) {
        string worldName = result[4].GetString();
        SendOnConsoleMessage("`oYour world \"`#" + worldName + "`o\" has been rendered!");

        RenderWorldDialog::OnRendered(this, worldName);
    }
    else {
        SendOnConsoleMessage("`4OOPS! ``Unable to render your world right now.");
    }

    RemoveState(PLAYER_STATE_RENDERING_WORLD);
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
        m_gems,
        GetNetID()
    );
    
    DatabasePlayerExec(GetContext()->GetDatabasePool(), DB_PLAYER_SAVE, req);
    SAFE_DELETE_ARRAY(pInvData);
}

void GamePlayer::LogOff()
{
    bool isInGame = HasState(PLAYER_STATE_IN_GAME);

    RemoveState(PLAYER_STATE_IN_GAME);
    m_loggingOff = true;

    if(m_currentWorldID != 0) {
        World* pWorld = GetWorldManager()->GetWorldByID(m_currentWorldID);
        if(pWorld) {
            pWorld->PlayerLeaverWorld(this);
        }
    }

    if(isInGame) {
        SaveToDatabase();
    }

    SendENetPacket(NET_MESSAGE_GAME_MESSAGE, "action|log_off\n", m_pPeer);
    GetMasterBroadway()->SendEndPlayerSession(m_userID);
}

void GamePlayer::Update()
{
    UpdatePlayMods();

    if(m_currentWorldID != 0) {
        World* pWorld = GetWorldManager()->GetWorldByID(m_currentWorldID);
        if(pWorld) {
            if(m_characterData.NeededCharStateUpdate()) {
                pWorld->SendSetCharPacketToAll(this);
                m_characterData.SetNeedCharStateUpdate(false);
            }
        }
    }
}

string GamePlayer::GetDisplayName()
{
    string displayName;

    if(m_currentWorldID != 0) {
        World* pWorld = GetWorldManager()->GetWorldByID(m_currentWorldID);
        if(pWorld) {
            if(pWorld->IsPlayerWorldOwner(this)) {
                displayName += "`2";
            }
            else if(pWorld->IsPlayerWorldAdmin(this)) {
                displayName += "`^";
            }
        }
    }

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

string GamePlayer::GetRawName()
{
    return m_loginDetail.tankIDName.empty() ? 
        m_loginDetail.requestedName + "_" + ToString(m_guestID)
        : m_loginDetail.tankIDName;
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

void GamePlayer::ToggleCloth(uint16 itemID)
{
    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(itemID);
    if(!pItem || pItem->bodyPart > BODY_PART_SIZE) {
        return;
    }

    if((pItem->type != ITEM_TYPE_CLOTHES && pItem->type != ITEM_TYPE_ARTIFACT) && !m_pRole->HasPerm(ROLE_PERM_CAN_WEAR_ANY)) {
        return;
    }

    if(pItem->type == ITEM_TYPE_ARTIFACT) {
        /**
         * 
         */
        return;
    }

    uint16 wornItem = m_inventory.GetClothes()[pItem->bodyPart];
    if(wornItem == pItem->id) {
        m_inventory.SetClothByPart(ITEM_ID_BLANK, pItem->bodyPart);

        if(pItem->playModType != PLAYMOD_TYPE_NONE) {
            RemovePlayMod(pItem->playModType);
        }

        PlayerInventory& playerInv = GetInventory();

        uint8 itemCount = playerInv.GetCountOfItem(pItem->id);
        switch(pItem->id) {
            case ITEM_ID_DIAMOND_HORN: {
                ModifyInventoryItem(ITEM_ID_DIAMOND_HORN, -itemCount);
                ModifyInventoryItem(ITEM_ID_DIAMOND_HORNS, itemCount);
                break;
            }
            case ITEM_ID_DIAMOND_HORNS: {
                ModifyInventoryItem(ITEM_ID_DIAMOND_HORNS, -itemCount);
                ModifyInventoryItem(ITEM_ID_DIAMOND_DEVIL_HORNS, itemCount);
                break;
            }
            case ITEM_ID_DIAMOND_DEVIL_HORNS: {
                ModifyInventoryItem(ITEM_ID_DIAMOND_DEVIL_HORNS, -itemCount);
                ModifyInventoryItem(ITEM_ID_DIAMOND_HORN, itemCount);
                break;
            }
        }
    }
    else {
        m_inventory.SetClothByPart(pItem->id, pItem->bodyPart);

        ItemInfo* pWornItem = GetItemInfoManager()->GetItemByID(wornItem);
        if(pWornItem) {
            RemovePlayMod(pWornItem->playModType);
        }

        if(pItem->playModType != PLAYMOD_TYPE_NONE) {
            AddPlayMod(pItem->playModType);
        }
    }

    if(m_currentWorldID != 0) {
        World* pWorld = GetWorldManager()->GetWorldByID(m_currentWorldID);
        pWorld->SendClothUpdateToAll(this);
    }
}

void GamePlayer::ModifyInventoryItem(uint16 itemID, int16 amount)
{
    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(itemID);
    if(!pItem || amount == 0) {
        return;
    }

    if(amount > 0 && IsIllegalItem(itemID) && !m_pRole->HasPerm(ROLE_PERM_BYPASS_ILLEGAl_ITEM)) {
        return;
    }

    if(amount > 0 && pItem->HasFlag(ITEM_FLAG_MOD) && !m_pRole->HasPerm(ROLE_PERM_USE_ITEM_TYPE_MOD)) {
        return;
    }

    if(amount < 0) {
        m_inventory.RemoveItem(itemID, -amount, this);

        if(m_inventory.GetCountOfItem(itemID) == 0 && m_inventory.IsWearingItem(itemID)) {
            ToggleCloth(itemID);
        }
    }
    else {
        m_inventory.AddItem(itemID, amount, this);
    }
}

void GamePlayer::TrashItem(uint16 itemID, uint8 amount)
{
    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(itemID);
    if(!pItem) {
        LOGGER_LOG_WARN("Player %d tried to trash non exist item %d ?!", m_userID, itemID)
        return;
    }

    if(amount > pItem->maxCanHold) {
        return;
    }

    if(amount > m_inventory.GetCountOfItem(itemID)) {
        PlaySFX("cant_place_tile.wav");
        return;
    }

    ModifyInventoryItem(itemID, -amount);

    PlaySFX("trash.vaw");
    SendOnConsoleMessage("Trashed " + ToString(amount) + " " + pItem->name);
}

void GamePlayer::AddPlayMod(ePlayModType modType, bool silent)
{
    if(modType == PLAYMOD_TYPE_NONE) {
        return;
    }

    PlayMod* pPlayMod = m_characterData.AddPlayMod(modType);
    if(!pPlayMod) {
        return;
    }

    if(!silent) {
        if(pPlayMod->GetTime() != 0) {
            SendOnConsoleMessage("`o" + pPlayMod->GetName() + " (`$" + pPlayMod->GetAddMessage() + " `omod added, `$" + Time::ConvertTimeToStr(pPlayMod->GetTime() * 1000) + "`oleft)");
        }
        else {
            SendOnConsoleMessage("`o" + pPlayMod->GetName() + " (`$" + pPlayMod->GetAddMessage() + " `omod added)");
        }
    }

    UpdateNeededPlayModThings();
}

void GamePlayer::RemovePlayMod(ePlayModType modType, bool silent)
{
    if(modType == PLAYMOD_TYPE_NONE) {
        return;
    }

    PlayMod* pPlayMod = m_characterData.RemovePlayMod(modType);
    if(!pPlayMod) {
        return;
    }

    if(!silent) {
        SendOnConsoleMessage("`o" + pPlayMod->GetName() + " (`$" + pPlayMod->GetRemoveMessage() + " `omod removed)");
    }
    UpdateNeededPlayModThings();
}

void GamePlayer::UpdateNeededPlayModThings()
{
    if(
        m_characterData.NeededSkinUpdate() && m_currentWorldID != 0
    ) {
        World* pWorld = GetWorldManager()->GetWorldByID(m_currentWorldID);
        if(!pWorld) {
            return;
        }

        if(m_characterData.NeededSkinUpdate()) {
            pWorld->SendSkinColorUpdateToAll(this);
            m_characterData.SetNeedSkinUpdate(false);
        }
    }
}

void GamePlayer::UpdatePlayMods()
{
    auto& reqUpdtMods = m_characterData.GetReqUpdatePlayMods();

    for(int32 i = reqUpdtMods.size() - 1; i >= 0; --i) {
        PlayerPlayModInfo& playMod = reqUpdtMods[i];
        
        if(playMod.modType == PLAYMOD_TYPE_CARRYING_A_TORCH) {
            if(m_currentWorldID == 0) {
                continue;
            }

            if(GetInventory().GetClothByPart(BODY_PART_HAND) != ITEM_ID_HAND_TORCH) {
                RemovePlayMod(PLAYMOD_TYPE_CARRYING_A_TORCH);
                continue;
            }

            UpdateTorchPlayMod();
        }
        else if(reqUpdtMods[i].timer.GetElapsedTime() >= playMod.durationMS) {
            RemovePlayMod(playMod.modType);
        }
    }
}

void GamePlayer::UpdateTorchPlayMod()
{
    if(RandomRangeInt(0, 250) != 18) { // is it a good idea? lol since tick based
        return;
    }

    ModifyInventoryItem(ITEM_ID_HAND_TORCH, -1);

    uint8 leftTorchCount = GetInventory().GetCountOfItem(ITEM_ID_HAND_TORCH);
    if(leftTorchCount == 0) {
        SendOnTalkBubble("`2My torch went out!", true);
        return;
    }

    SendOnTalkBubble("`2My torch went out, i have " + ToString(leftTorchCount) + " more!", true);
}

void GamePlayer::SendPositionToWorldPlayers()
{
    if(m_currentWorldID == 0) {
        return;
    }

    World* pWorld = GetWorldManager()->GetWorldByID(m_currentWorldID);
    if(!pWorld) {
        return;
    }

    GameUpdatePacket packet;
    packet.type = NET_GAME_PACKET_STATE;
    packet.posX = m_worldPos.x;
    packet.posY = m_worldPos.y;
    packet.netID = GetNetID();

    if(m_characterData.HasPlayerFlag(PLAYER_FLAG_FACING_LEFT)) {
        packet.SetFlag(NET_GAME_PACKET_FLAGS_FACINGLEFT);
    }

    pWorld->SendGamePacketToAll(&packet, this);
}

bool GamePlayer::CanProcessGamePacket(eGamePacketType packetType)
{
    const uint64 elapsed = m_lastCheckGamePacketWindow.GetElapsedTime();
    int32 packetLimit = 150;

    if(packetType == NET_GAME_PACKET_ITEM_ACTIVATE_OBJECT_REQUEST) {
        packetLimit = 220;
    }
    else if(packetType == NET_GAME_PACKET_ITEM_ACTIVATE_REQUEST) {
        packetLimit = 200;
    }

    if(elapsed < 1000) {
        if(m_gamePacketsInWindow >= packetLimit) {
            ++m_gamePacketsInWindow;
            return false;
        }

        ++m_gamePacketsInWindow;
        return true;
    }

    m_lastCheckGamePacketWindow.Reset();
    m_gamePacketsInWindow = 1;
    return true;
}

bool GamePlayer::CanProcessGameMessage()
{
    if(m_lastCheckGameMessageWindow.GetElapsedTime() < 1000) {
        if(m_gameMessagesInWindow >= 10) {
            ++m_gameMessagesInWindow;
            return false;
        }

        ++m_gameMessagesInWindow;
        return true;
    }

    m_lastCheckGameMessageWindow.Reset();
    m_gameMessagesInWindow = 1;
    return true;
}

bool GamePlayer::CanProcessMovePacket(float nextX, float nextY, uint64 nowMS)
{
    if(nextX <= 0.0f || nextY <= 0.0f) {
        return false;
    }

    if(!std::isfinite(nextX) || !std::isfinite(nextY)) {
        return false;
    }

    if(!m_hasLastMovePacketPos) {
        m_lastMovePacketPos = { nextX, nextY };
        m_lastMovePacketTime = nowMS;
        m_hasLastMovePacketPos = true;
        return true;
    }

    uint64 deltaMS = nowMS > m_lastMovePacketTime ? (nowMS - m_lastMovePacketTime) : 0;
    if(deltaMS == 0) {
        deltaMS = 1;
    }

    const float dx = nextX - m_lastMovePacketPos.x;
    const float dy = nextY - m_lastMovePacketPos.y;
    const float distance = std::sqrt((dx * dx) + (dy * dy));

    // Allow generous burst movement while still rejecting obvious teleports.
    const float maxAllowedDistance = 180.0f + (2.5f * static_cast<float>(deltaMS));
    const bool isSuspicious = distance > maxAllowedDistance;

    if(m_lastCheckMoveWindow.GetElapsedTime() >= 5000) {
        m_lastCheckMoveWindow.Reset();
        m_moveViolationsInWindow = 0;
    }

    if(isSuspicious) {
        ++m_moveViolationsInWindow;
        if(m_moveViolationsInWindow >= 8) {
            return false;
        }
    }

    m_lastMovePacketPos = { nextX, nextY };
    m_lastMovePacketTime = nowMS;
    return true;
}

bool GamePlayer::TrySpendGems(int32 amount)
{
    if(amount <= 0) {
        return true;
    }

    if(m_gems < amount) {
        return false;
    }

    m_gems -= amount;
    return true;
}

void GamePlayer::AddGems(int32 amount)
{
    if(amount <= 0) {
        return;
    }

    int64 next = (int64)m_gems + amount;
    if(next > INT32_MAX) {
        next = INT32_MAX;
    }
    m_gems = (int32)next;
}

void GamePlayer::SendOnSetBux()
{
    VariantVector data(4);
    data[0] = "OnSetBux";
    data[1] = m_gems;
    data[2] = (uint32)1;
    data[3] = (uint32)1;

    SendCallFunctionPacket(data);
}