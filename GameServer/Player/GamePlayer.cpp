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
#include "AchievementCatalog.h"
#include "Dialog/RegisterDialog.h"
#include "Utils/DialogBuilder.h"
#include "../Server/GameServer.h"
#include "Proton/ProtonUtils.h"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace fmt {

inline void AppendFormat(std::string& output, const std::string& pattern, size_t& position)
{
    output.append(pattern, position, std::string::npos);
    position = pattern.size();
}

template<typename TValue, typename... TRest>
void AppendFormat(std::string& output, const std::string& pattern, size_t& position, TValue&& value, TRest&&... rest)
{
    const size_t placeholderPos = pattern.find("{}", position);
    if(placeholderPos == std::string::npos) {
        output.append(pattern, position, std::string::npos);
        position = pattern.size();
        return;
    }

    output.append(pattern, position, placeholderPos - position);

    std::ostringstream stream;
    stream << std::forward<TValue>(value);
    output += stream.str();

    position = placeholderPos + 2;
    AppendFormat(output, pattern, position, std::forward<TRest>(rest)...);
}

inline std::string format(const std::string& pattern)
{
    return pattern;
}

template<typename... TArgs>
std::string format(const std::string& pattern, TArgs&&... args)
{
    std::string output;
    output.reserve(pattern.size() + sizeof...(TArgs) * 8);

    size_t position = 0;
    AppendFormat(output, pattern, position, std::forward<TArgs>(args)...);
    if(position < pattern.size()) {
        output.append(pattern, position, std::string::npos);
    }

    return output;
}

}

namespace {

constexpr uint64 kTradeChangeThrottleMS = 1800;
constexpr uint8 kTradeMaxOfferItems = 4;
constexpr uint64 kTradeRequestExpireMS = 15000;

bool IsTradeSystemItem(uint16 itemID)
{
    return itemID == ITEM_ID_BLANK || itemID == ITEM_ID_FIST || itemID == ITEM_ID_WRENCH;
}

bool IsTradeItemAllowed(uint16 itemID)
{
    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(itemID);
    if(!pItem) {
        return false;
    }

    if(IsTradeSystemItem(itemID)) {
        return false;
    }

    return !pItem->HasFlag(ITEM_FLAG_UNTRADEABLE);
}

const char* GetTradeItemName(uint16 itemID)
{
    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(itemID);
    return pItem ? pItem->name.c_str() : "Unknown Item";
}

std::string BuildTradeOfferSummary(const std::vector<TradeOffer>& offers)
{
    if(offers.empty()) {
        return "nothing";
    }

    std::string summary;
    for(const auto& offer : offers) {
        if(!summary.empty()) {
            summary += ", ";
        }

        summary += fmt::format("{} {}", offer.Amount, GetTradeItemName(offer.ID));
    }

    return summary;
}

std::string BuildTradeHistorySummary(const std::vector<TradeOffer>& offers)
{
    std::string summary;
    for(const auto& offer : offers) {
        summary += fmt::format("[{} {}]", offer.Amount, GetTradeItemName(offer.ID));
    }

    return summary;
}

enum class TradeReceiveState {
    OK,
    FULL_SLOTS,
    WILL_MAX
};

TradeReceiveState GetTradeReceiveState(GamePlayer* pPlayer, uint16 itemID, uint8 amount)
{
    if(!pPlayer || amount == 0) {
        return TradeReceiveState::FULL_SLOTS;
    }

    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(itemID);
    if(!pItem) {
        return TradeReceiveState::FULL_SLOTS;
    }

    InventoryItemInfo* pCurrentItem = pPlayer->GetInventory().GetItemByID(itemID);
    if(pCurrentItem) {
        if((uint32)pCurrentItem->count + amount > pItem->maxCanHold) {
            return TradeReceiveState::WILL_MAX;
        }

        return TradeReceiveState::OK;
    }

    if(amount > pItem->maxCanHold) {
        return TradeReceiveState::WILL_MAX;
    }

    if(!pPlayer->GetInventory().HaveRoomForItem(itemID, amount)) {
        return TradeReceiveState::FULL_SLOTS;
    }

    return TradeReceiveState::OK;
}

bool IsValidTradePair(GamePlayer* pPlayer, GamePlayer* pTarget)
{
    if(!pPlayer || !pTarget || pPlayer == pTarget) {
        return false;
    }

    if(pPlayer->GetCurrentWorld() == 0 || pPlayer->GetCurrentWorld() != pTarget->GetCurrentWorld()) {
        return false;
    }

    return true;
}

void ResetTradeState(GamePlayer* pPlayer)
{
    if(!pPlayer) {
        return;
    }

    pPlayer->SetTrading(false);
    pPlayer->SetTradingWithUserID(0);
    pPlayer->SetTradeAccepted(false);
    pPlayer->SetTradeConfirmed(false);
    pPlayer->SetTradeAcceptedAt(0);
    pPlayer->SetTradeConfirmedAt(0);
    pPlayer->SetLastChangeTradeDeal(0);
    pPlayer->ClearPendingTradeRequest();
    pPlayer->ClearTradeOffers();
}

std::string BuildTradeStatusDialog(GamePlayer* pViewer, GamePlayer* pOfferOwner)
{
    if(!pViewer || !pOfferOwner) {
        return "";
    }

    std::string dialog;
    for(const auto& offer : pOfferOwner->GetTradeOffers()) {
        dialog += fmt::format("add_slot|{}|{}|\n", offer.ID, offer.Amount);
    }

    dialog += "locked|0\n";
    dialog += "reset_locks|1\n";
    dialog += fmt::format("accepted|{}\n", pOfferOwner->IsTradeAccepted() ? 1 : 0);
    return dialog;
}

DialogBuilder BuildTradeConfirmationDialog(GamePlayer* pViewer, GamePlayer* pOfferOwner, World* pWorld)
{
    DialogBuilder db;
    db.SetDefaultColor('o');
    db.AddLabelWithIcon("`wTrade Confirmation``", ITEM_ID_WRENCH, true);
    db.AddSpacer();
    db.AddLabel("`4You'll give:``");
    db.AddSpacer();

    for(const auto& offer : pViewer->GetTradeOffers()) {
        ItemInfo* pItem = GetItemInfoManager()->GetItemByID(offer.ID);
        if(!pItem) {
            continue;
        }

        db.AddLabelWithIcon(fmt::format("(`w{}``)`` {}", offer.Amount, pItem->name), offer.ID);
    }

    if(pViewer->GetTradeOffers().empty()) {
        db.AddLabel("`4Nothing!``");
    }

    db.AddSpacer();
    db.AddLabel("`2You'll get:``");
    db.AddSpacer();

    for(const auto& offer : pOfferOwner->GetTradeOffers()) {
        ItemInfo* pItem = GetItemInfoManager()->GetItemByID(offer.ID);
        if(!pItem) {
            continue;
        }

        db.AddLabelWithIcon(fmt::format("(`w{}``)`` {}", offer.Amount, pItem->name), offer.ID);
    }

    if(pOfferOwner->GetTradeOffers().empty()) {
        db.AddLabel("`4Nothing!``");
    }

    db.AddSpacer();

    if(pOfferOwner->GetTradeOffers().empty()) {
        db.AddLabel("`4SCAM WARNING: ``You are about to do a trade without anything in return. Once you do the trade, you can't get your items back.");
        db.AddLabel("`4Do you really wanna do this trade?``");
        db.AddSpacer();
    }

    if(pOfferOwner->IsTradeOfferExists(ITEM_ID_WORLD_KEY)) {
        db.AddLabel(fmt::format("`4SCAM WARNING: ``You are buying this world, `#{}``. Don't buy a world just to get its rare items, because scammers can easily place secret doors allowing someone to jump in and `4steal the items back`` after a trade!", pWorld ? pWorld->GetWorlName() : "unknown"));
        db.AddSpacer();
        db.AddLabel("Also, all ratings will be removed from a world when it is traded. Don't buy a world for its ranking!");
        db.AddSpacer();
        db.AddLabel("To be safe, only buy a world for its name and placed blocks, not loose items or high ratings. Consider yourself warned!");
        db.AddSpacer();
    }
    else if(pViewer->IsTradeOfferExists(ITEM_ID_WORLD_LOCK) || pViewer->IsTradeOfferExists(ITEM_ID_DIAMOND_LOCK) || pViewer->IsTradeOfferExists(ITEM_ID_HARMONIC_LOCK) || pViewer->IsTradeOfferExists(ITEM_ID_ROBOTIC_LOCK)) {
        db.AddLabel(fmt::format("``4WARNING: ``You are about to sell your world `#{}`` - the world lock ownership will be transferred over to `w{}`o.``", pWorld ? pWorld->GetWorlName() : "unknown", pOfferOwner->GetRawName()));
        db.AddSpacer();
    }

    db.AddButton("Confirm", "Confirm Trade");
    db.AddButton("Cancel", "Cancel");
    db.EndDialog("TradeConfirm", "", "");
    return db;
}

}

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

    if(m_tradeOffers.size() >= kTradeMaxOfferItems) {
        return;
    }

    m_tradeOffers.push_back(tradeOffer);
}

void GamePlayer::ClearTradeOffers()
{
    m_tradeOffers.clear();
}

void GamePlayer::SetPendingTradeRequest(uint32 requesterUserID, uint64 requestedAtMS)
{
    m_pendingTradeRequesterUserID = requesterUserID;
    m_pendingTradeRequestedAtMS = requestedAtMS;
}

void GamePlayer::ClearPendingTradeRequest()
{
    m_pendingTradeRequesterUserID = 0;
    m_pendingTradeRequestedAtMS = 0;
}

bool GamePlayer::HasPendingTradeRequestFrom(uint32 requesterUserID, uint64 nowMS) const
{
    if(requesterUserID == 0 || m_pendingTradeRequesterUserID != requesterUserID || m_pendingTradeRequestedAtMS == 0) {
        return false;
    }

    return nowMS >= m_pendingTradeRequestedAtMS && (nowMS - m_pendingTradeRequestedAtMS) <= kTradeRequestExpireMS;
}

void GamePlayer::StartTrade(GamePlayer* player)
{
    if(!player || player == this) {
        return;
    }

    CancelTrade(false);

    if(!IsValidTradePair(this, player)) {
        SendOnConsoleMessage("`4Target is no longer valid.``");
        return;
    }

    if(player->IsTrading()) {
        SendOnConsoleMessage(fmt::format("`8[`w{} `4is too busy to trade!`8]``", player->GetRawName()));
        return;
    }

    if(player->GetTradingWithUserID() != 0 && player->GetTradingWithUserID() != GetNetID()) {
        SendOnConsoleMessage(fmt::format("`8[`w{} `4is already handling another trade request!`8]``", player->GetRawName()));
        return;
    }

    const uint64 nowMS = Time::GetSystemTime();

    SetTrading(false);
    SetTradingWithUserID(player->GetNetID());
    SetTradeAccepted(false);
    SetTradeConfirmed(false);
    SetTradeAcceptedAt(0);
    SetTradeConfirmedAt(0);
    ClearPendingTradeRequest();
    ClearTradeOffers();

    const bool mutualTradeRequest =
        player->GetTradingWithUserID() == GetNetID() ||
        player->HasPendingTradeRequestFrom(GetUserID(), nowMS);

    if(!mutualTradeRequest) {
        player->SetPendingTradeRequest(GetUserID(), nowMS);
        SendOnConsoleMessage("`6[``Trade request sent. Waiting for the other player to trade back.`6]``");
        player->SendTradeAlert(this);
        SendStartTrade(player);
        return;
    }

    player->ClearPendingTradeRequest();

    SetTrading(true);

    player->SetTrading(true);
    player->SetTradingWithUserID(GetNetID());
    player->SetTradeAccepted(false);
    player->SetTradeConfirmed(false);
    player->SetTradeAcceptedAt(0);
    player->SetTradeConfirmedAt(0);
    player->ClearPendingTradeRequest();
    player->ClearTradeOffers();

    SendTradeAlert(player);
    player->SendTradeAlert(this);
    SendStartTrade(player);
    player->SendStartTrade(this);

    SendTradeStatus(this);
    SendTradeStatus(player);
    player->SendTradeStatus(this);
    player->SendTradeStatus(player);
}

void GamePlayer::CancelTrade(bool busy, std::string customMessage)
{
    GamePlayer* pTarget = GetGameServer()->GetPlayerByNetID(GetTradingWithUserID());
    const bool isTradingWithMe = pTarget && pTarget != this && pTarget->GetTradingWithUserID() == GetNetID();

    if(isTradingWithMe) {
        if(!customMessage.empty()) {
            pTarget->SendOnConsoleMessage(customMessage);
            pTarget->SendOnTextOverlay(customMessage);
        }
        else if(busy) {
            const std::string message = fmt::format("`8[`w{} `4is too busy to trade!`8]``", GetRawName());
            pTarget->SendOnConsoleMessage(message);
            pTarget->SendOnTextOverlay(message);
        }
        else {
            const std::string message = fmt::format("`8[`4Trade cancelled by `w{}`4!`8]``", GetRawName());
            pTarget->SendOnConsoleMessage(message);
            pTarget->SendOnTextOverlay(message);
        }

        pTarget->SendForceTradeEnd();
        ResetTradeState(pTarget);
    }

    if(!customMessage.empty()) {
        SendOnConsoleMessage(customMessage);
        SendOnTextOverlay(customMessage);
    }
    else if(busy) {
        const std::string message = fmt::format("`8[`w{} `4is too busy to trade!`8]``", pTarget ? pTarget->GetRawName() : GetRawName());
        SendOnConsoleMessage(message);
        SendOnTextOverlay(message);
    }
    else if(isTradingWithMe) {
        const std::string message = fmt::format("`8[`4Trade cancelled by `w{}`4!`8]``", pTarget->GetRawName());
        SendOnConsoleMessage(message);
        SendOnTextOverlay(message);
    }

    SendForceTradeEnd();
    ResetTradeState(this);
}

void GamePlayer::ModifyOffer(uint16 itemID, uint16 amount)
{
    if(!IsTrading()) {
        return;
    }

    GamePlayer* pTarget = GetGameServer()->GetPlayerByNetID(GetTradingWithUserID());
    if(!IsValidTradePair(this, pTarget)) {
        CancelTrade(false);
        return;
    }

    if(!IsTradeItemAllowed(itemID)) {
        return;
    }

    const uint64 nowMS = Time::GetSystemTime();
    if(GetLastChangeTradeDeal() != 0 && nowMS - GetLastChangeTradeDeal() < kTradeChangeThrottleMS) {
        SendOnConsoleMessage("Please slow down when changing trade offers.");
        return;
    }

    const uint8 ownedCount = GetInventory().GetCountOfItem(itemID);
    if(ownedCount == 0) {
        return;
    }

    if(amount == 0) {
        amount = ownedCount;
    }

    if(amount > ownedCount) {
        return;
    }

    if(!IsTradeOfferExists(itemID) && m_tradeOffers.size() >= kTradeMaxOfferItems) {
        return;
    }

    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(itemID);
    if(!pItem || amount > pItem->maxCanHold) {
        return;
    }

    AddTradeOffer({ itemID, (uint8)amount });

    SetTradeAccepted(false);
    SetTradeConfirmed(false);
    SetTradeAcceptedAt(0);
    SetTradeConfirmedAt(0);
    SetLastChangeTradeDeal(nowMS);

    if(pTarget) {
        pTarget->SetTradeAccepted(false);
        pTarget->SetTradeConfirmed(false);
        pTarget->SetTradeAcceptedAt(0);
        pTarget->SetTradeConfirmedAt(0);
    }

    SendTradeStatus(this);
    if(pTarget) {
        pTarget->SendTradeStatus(this);
        pTarget->SendTradeStatus(pTarget);
    }

    const std::string itemName = GetTradeItemName(itemID);
    SendOnConsoleMessage("`#The trade deal has been updated.");
    SendOnTextOverlay("`#The trade deal has been updated.");
    SendOnConsoleMessage(fmt::format("{} `` offered ``{}x {}``", GetRawName(), amount, itemName));
    SendOnTextOverlay(fmt::format("{} `` offered ``{}x {}``", GetRawName(), amount, itemName));
    PlaySFX("tile_removed.wav");

    if(pTarget) {
        pTarget->SendOnConsoleMessage("`#The trade deal has been updated.");
        pTarget->SendOnTextOverlay("`#The trade deal has been updated.");
        pTarget->SendOnConsoleMessage(fmt::format("{} `` offered ``{}x {}``", GetRawName(), amount, itemName));
        pTarget->SendOnTextOverlay(fmt::format("{} `` offered ``{}x {}``", GetRawName(), amount, itemName));
        pTarget->PlaySFX("tile_removed.wav");
    }
}

void GamePlayer::RemoveOffer(uint16 itemID)
{
    if(!IsTrading()) {
        return;
    }

    GamePlayer* pTarget = GetGameServer()->GetPlayerByNetID(GetTradingWithUserID());
    if(!IsValidTradePair(this, pTarget)) {
        CancelTrade(false);
        return;
    }

    if(!IsTradeItemAllowed(itemID)) {
        return;
    }

    const uint64 nowMS = Time::GetSystemTime();
    if(GetLastChangeTradeDeal() != 0 && nowMS - GetLastChangeTradeDeal() < kTradeChangeThrottleMS) {
        SendOnConsoleMessage("Please slow down when changing trade offers.");
        return;
    }

    uint8 removedAmount = 0;
    for(const auto& offer : m_tradeOffers) {
        if(offer.ID == itemID) {
            removedAmount = offer.Amount;
            break;
        }
    }

    if(removedAmount == 0) {
        return;
    }

    RemoveTradeOffer(itemID);

    SetTradeAccepted(false);
    SetTradeConfirmed(false);
    SetTradeAcceptedAt(0);
    SetTradeConfirmedAt(0);
    SetLastChangeTradeDeal(nowMS);

    if(pTarget) {
        pTarget->SetTradeAccepted(false);
        pTarget->SetTradeConfirmed(false);
        pTarget->SetTradeAcceptedAt(0);
        pTarget->SetTradeConfirmedAt(0);
    }

    SendTradeStatus(this);
    if(pTarget) {
        pTarget->SendTradeStatus(this);
        pTarget->SendTradeStatus(pTarget);
    }

    const std::string itemName = GetTradeItemName(itemID);
    SendOnConsoleMessage("`#The trade deal has been updated.");
    SendOnTextOverlay("`#The trade deal has been updated.");
    SendOnConsoleMessage(fmt::format("{} `` `4removed ``{}x {}``", GetRawName(), removedAmount, itemName));
    SendOnTextOverlay(fmt::format("{} `` `4removed ``{}x {}``", GetRawName(), removedAmount, itemName));
    PlaySFX("tile_removed.wav");

    if(pTarget) {
        pTarget->SendOnConsoleMessage("`#The trade deal has been updated.");
        pTarget->SendOnTextOverlay("`#The trade deal has been updated.");
        pTarget->SendOnConsoleMessage(fmt::format("{} `` `4removed ``{}x {}``", GetRawName(), removedAmount, itemName));
        pTarget->SendOnTextOverlay(fmt::format("{} `` `4removed ``{}x {}``", GetRawName(), removedAmount, itemName));
        pTarget->PlaySFX("tile_removed.wav");
    }
}

void GamePlayer::AcceptOffer(bool status)
{
    if(!IsTrading()) {
        return;
    }

    GamePlayer* pTarget = GetGameServer()->GetPlayerByNetID(GetTradingWithUserID());
    if(!IsValidTradePair(this, pTarget)) {
        CancelTrade(false);
        return;
    }

    if(!status) {
        SetTradeAccepted(false);
        SetTradeConfirmed(false);
        SetTradeAcceptedAt(0);
        SetTradeConfirmedAt(0);

        if(pTarget) {
            pTarget->SetTradeAccepted(false);
            pTarget->SetTradeConfirmed(false);
            pTarget->SetTradeAcceptedAt(0);
            pTarget->SetTradeConfirmedAt(0);
            pTarget->SendTradeStatus(this);
            pTarget->SendTradeStatus(pTarget);
        }

        SendTradeStatus(this);
        if(pTarget) {
            SendTradeStatus(pTarget);
        }
        return;
    }

    SetTradeAccepted(true);
    SetTradeAcceptedAt(Time::GetSystemTime());

    if(pTarget && pTarget->GetTradingWithUserID() == GetNetID()) {
        pTarget->SendTradeStatus(this);
    }

    if(!pTarget || !pTarget->IsTradeAccepted() || !IsTradeAccepted() || pTarget->GetTradingWithUserID() != GetNetID()) {
        SendOnConsoleMessage("`6[``Trade accepted, waiting for other player to accept`6]``");
        SendOnTextOverlay("`6[``Trade accepted, waiting for other player to accept`6]``");

        if(pTarget) {
            pTarget->SendOnConsoleMessage("`6[``Trade accepted by other player, waiting for you`6]``");
            pTarget->SendOnTextOverlay("`6[``Trade accepted by other player, waiting for you`6]``");
        }

        return;
    }

    bool allowTrade = true;
    for(const auto& offer : pTarget->GetTradeOffers()) {
        TradeReceiveState receiveState = GetTradeReceiveState(this, offer.ID, offer.Amount);
        if(receiveState == TradeReceiveState::FULL_SLOTS) {
            const std::string message = fmt::format("`w{}`` needs more backpack room first!", GetRawName());
            SendOnConsoleMessage(message);
            SendOnTextOverlay(message);
            pTarget->SendOnConsoleMessage(message);
            pTarget->SendOnTextOverlay(message);
            allowTrade = false;
            break;
        }

        if(receiveState == TradeReceiveState::WILL_MAX) {
            const std::string message = fmt::format("`w{}`` `4is carrying too many `w{}`4.", GetRawName(), GetTradeItemName(offer.ID));
            SendOnConsoleMessage(message);
            SendOnTextOverlay(message);
            pTarget->SendOnConsoleMessage(message);
            pTarget->SendOnTextOverlay(message);
            allowTrade = false;
            break;
        }
    }

    if(allowTrade) {
        for(const auto& offer : GetTradeOffers()) {
            TradeReceiveState receiveState = GetTradeReceiveState(pTarget, offer.ID, offer.Amount);
            if(receiveState == TradeReceiveState::FULL_SLOTS) {
                const std::string message = fmt::format("`w{}`` needs more backpack room first!", pTarget->GetRawName());
                SendOnConsoleMessage(message);
                SendOnTextOverlay(message);
                pTarget->SendOnConsoleMessage(message);
                pTarget->SendOnTextOverlay(message);
                allowTrade = false;
                break;
            }

            if(receiveState == TradeReceiveState::WILL_MAX) {
                const std::string message = fmt::format("`w{}`` `4is carrying too many `w{}`4.", pTarget->GetRawName(), GetTradeItemName(offer.ID));
                SendOnConsoleMessage(message);
                SendOnTextOverlay(message);
                pTarget->SendOnConsoleMessage(message);
                pTarget->SendOnTextOverlay(message);
                allowTrade = false;
                break;
            }
        }
    }

    if(!allowTrade) {
        SetTradeConfirmed(false);
        SetTradeAccepted(false);
        SetTradeConfirmedAt(0);
        SetTradeAcceptedAt(0);

        if(pTarget) {
            pTarget->SetTradeConfirmed(false);
            pTarget->SetTradeAccepted(false);
            pTarget->SetTradeConfirmedAt(0);
            pTarget->SetTradeAcceptedAt(0);

            SendTradeStatus(this);
            SendTradeStatus(pTarget);
            pTarget->SendTradeStatus(this);
            pTarget->SendTradeStatus(pTarget);
        }

        return;
    }

    SendForceTradeEnd();
    pTarget->SendForceTradeEnd();

    World* pWorld = GetWorldManager()->GetWorldByID(GetCurrentWorld());
    DialogBuilder ours = BuildTradeConfirmationDialog(this, pTarget, pWorld);
    DialogBuilder theirs = BuildTradeConfirmationDialog(pTarget, this, pWorld);

    SendOnDialogRequest(ours.Get());
    pTarget->SendOnDialogRequest(theirs.Get());

    SetTradeConfirmed(false);
    pTarget->SetTradeConfirmed(false);
}

void GamePlayer::ConfirmOffer()
{
    if(!IsTrading() || !IsTradeAccepted()) {
        return;
    }

    GamePlayer* pTarget = GetGameServer()->GetPlayerByNetID(GetTradingWithUserID());
    if(!IsValidTradePair(this, pTarget) || !pTarget->IsTrading()) {
        CancelTrade(false);
        return;
    }

    SetTradeConfirmed(true);
    SetTradeConfirmedAt(Time::GetSystemTime());

    if(!pTarget->IsTradeConfirmed()) {
        SendOnConsoleMessage("`6[``Trade accepted, waiting for other player to accept`6]``");
        SendOnTextOverlay("`6[``Trade accepted, waiting for other player to accept`6]``");

        pTarget->SendOnConsoleMessage("`6[``Trade accepted by other player, waiting for you`6]``");
        pTarget->SendOnTextOverlay("`6[``Trade accepted by other player, waiting for you`6]``");
        return;
    }

    for(const auto& offer : pTarget->GetTradeOffers()) {
        if(GetInventory().GetCountOfItem(offer.ID) < offer.Amount) {
            const std::string message = "`4Trade cancelled - the other player doesn't have enough items anymore.";
            SendOnConsoleMessage(message);
            SendOnTextOverlay(message);
            pTarget->SendOnConsoleMessage(message);
            pTarget->SendOnTextOverlay(message);
            CancelTrade(false);
            return;
        }
    }

    for(const auto& offer : GetTradeOffers()) {
        if(pTarget->GetInventory().GetCountOfItem(offer.ID) < offer.Amount) {
            const std::string message = "`4Trade cancelled - the other player doesn't have enough items anymore.";
            SendOnConsoleMessage(message);
            SendOnTextOverlay(message);
            pTarget->SendOnConsoleMessage(message);
            pTarget->SendOnTextOverlay(message);
            CancelTrade(false);
            return;
        }
    }

    World* pWorld = GetWorldManager()->GetWorldByID(GetCurrentWorld());
    std::string myOffersSummary = BuildTradeOfferSummary(GetTradeOffers());
    std::string theirOffersSummary = BuildTradeOfferSummary(pTarget->GetTradeOffers());

    for(const auto& offer : pTarget->GetTradeOffers()) {
        pTarget->ModifyInventoryItem(offer.ID, -(int16)offer.Amount);
        ModifyInventoryItem(offer.ID, offer.Amount);
    }

    for(const auto& offer : GetTradeOffers()) {
        ModifyInventoryItem(offer.ID, -(int16)offer.Amount);
        pTarget->ModifyInventoryItem(offer.ID, offer.Amount);
    }

    if(pWorld) {
        pWorld->SendConsoleMessageToAll(fmt::format("`1{} `1traded {} to {}.``", GetRawName(), myOffersSummary, pTarget->GetRawName()));
        pWorld->SendConsoleMessageToAll(fmt::format("`1{} `1traded {} to {}.``", pTarget->GetRawName(), theirOffersSummary, GetRawName()));
        pWorld->PlaySFXForEveryone("keypad_hit.wav");
    }

    AddTradeHistory(fmt::format("Traded {} with {}", BuildTradeHistorySummary(GetTradeOffers()), pTarget->GetRawName()));
    pTarget->AddTradeHistory(fmt::format("Traded {} with {}", BuildTradeHistorySummary(pTarget->GetTradeOffers()), GetRawName()));

    SetLastTradedAt(Time::GetSystemTime());
    pTarget->SetLastTradedAt(Time::GetSystemTime());

    ResetTradeState(this);
    ResetTradeState(pTarget);
}

void GamePlayer::SendTradeStatus(GamePlayer* player)
{
    if(!player) {
        return;
    }

    SendOnTradeStatus(player->GetNetID(), player == this ? "`oSelect an item from your Inventory.``" : fmt::format("`w{}`o's trade offer.``", player->GetRawName()), BuildTradeStatusDialog(this, player));
}

void GamePlayer::SendTradeAlert(GamePlayer* player)
{
    if(!player) {
        return;
    }

    const std::string message = fmt::format("`#TRADE ALERT:`` {} `owants to trade with you!  To start, use the `wWrench`` on that person's wrench icon, or type `w/trade {}``", player->GetRawName(), player->GetRawName());
    SendOnConsoleMessage(message);
    SendOnTextOverlay(message);
    PlaySFX("cash_register.wav");
}

void GamePlayer::SendStartTrade(GamePlayer* player)
{
    if(!player) {
        return;
    }

    SendOnStartTrade(player->GetRawName(), player->GetNetID());
}

void GamePlayer::SendForceTradeEnd()
{
    SendOnForceTradeEnd();
}

void GamePlayer::SendWrenchSelf(std::string page)
{
    World* pWorld = GetWorldManager()->GetWorldByID(GetCurrentWorld());

    DialogBuilder db;
    db.SetDefaultColor('o');

    if(page.empty() || page == "PlayerInfo") {
        db.AddLabelWithIcon("`w" + GetDisplayName() + "``", ITEM_ID_WRENCH, true)
        ->AddLabel("`oCurrent world: `w" + ToString(GetCurrentWorld()) + "``")
        ->AddLabel("`oLevel: `w" + ToString(GetLevel()) + "``")
        ->AddLabel("`oXP: `w" + ToString(GetXP()) + "``")
        ->AddSpacer()
        ->AddButton("PlayerInfo", "Player Info")
        ->AddButton("SocialProfile", "Social Profile")
        ->AddButton("PlayerStats", "Player Stats")
        ->AddButton("Settings", "Settings")
        ->AddSpacer()
        ->AddButton("Worlds", "My Worlds")
        ->AddButton("Titles", "Title Settings")
        ->EndDialog("WrenchSelf", "", "Continue");
    }
    else if(page == "SocialProfile") {
        db.AddLabelWithIcon("`wSocial Profile``", ITEM_ID_WRENCH, true)
        ->AddLabel("`oFriend and messaging shortcuts are available through wrenching other players.")
        ->AddLabel("`oUse this menu to navigate your own profile tabs.")
        ->AddSpacer()
        ->AddButton("PlayerInfo", "Back")
        ->EndDialog("WrenchSelf", "", "Continue");
    }
    else if(page == "PlayerStats") {
        db.AddLabelWithIcon("`wPlayer Stats``", ITEM_ID_WRENCH, true)
        ->AddLabel("`oStatistics and achievement tracking are active in this source.")
        ->AddLabel("`oBlocks placed: `w" + ToString(GetStatCount("BLOCKS_PLACED")) + "``")
        ->AddLabel("`oBlocks destroyed: `w" + ToString(GetStatCount("BLOCKS_DESTROYED")) + "``")
        ->AddLabel("`oTrees harvested: `w" + ToString(GetStatCount("TREES_HARVESTED")) + "``")
        ->AddLabel("`oLocks placed: `w" + ToString(GetStatCount("LOCKS_PLACED")) + "``")
        ->AddLabel("`oConsumables used: `w" + ToString(GetStatCount("CONSUMABLES_USED")) + "``")
        ->AddSpacer()
        ->AddButton("PlayerInfo", "Back")
        ->EndDialog("WrenchSelf", "", "Continue");
    }
    else if(page == "Settings") {
        db.AddLabelWithIcon("`wSettings``", ITEM_ID_WRENCH, true)
        ->AddLabel("`oWrench settings are currently lightweight in this source.")
        ->AddLabel("`oUse Titles and Worlds tabs for currently supported options.")
        ->AddSpacer()
        ->AddButton("PlayerInfo", "Back")
        ->EndDialog("WrenchSelf", "", "Continue");
    }
    else if(page == "Titles") {
        db.AddLabelWithIcon("`wTitle Settings``", ITEM_ID_WRENCH, true)
        ->AddLabel("`oRole prefix and title switching is server-side in this source.")
        ->AddSpacer()
        ->AddButton("PlayerInfo", "Back")
        ->EndDialog("WrenchSelf", "", "Continue");
    }
    else if(page == "Worlds") {
        db.AddLabelWithIcon("`wMy Worlds``", ITEM_ID_WRENCH, true)
        ->AddLabel("`oYour current world is `w" + ToString(GetCurrentWorld()) + "``.");

        if(pWorld) {
            db.AddLabel("`oPlayers inside: `w" + ToString(pWorld->GetPlayerCount()) + "``");
        }

        db.AddSpacer()
        ->AddButton("PlayerInfo", "Back")
        ->EndDialog("WrenchSelf", "", "Continue");
    }
    else {
        SendWrenchSelf("PlayerInfo");
        return;
    }

    SendOnDialogRequest(db.Get());
}

void GamePlayer::SendWrenchOthers(GamePlayer* otherPlayer)
{
    if(!otherPlayer) {
        return;
    }

    DialogBuilder db;
    db.SetDefaultColor('o')
    ->EmbedData("MyNetID", GetNetID())
    ->EmbedData("OtherNetID", otherPlayer->GetNetID())
    ->AddLabelWithIcon("`w" + otherPlayer->GetDisplayName() + "``", ITEM_ID_WRENCH, true)
    ->AddLabel("`oCurrent level: `w" + ToString(otherPlayer->GetLevel()) + "``")
    ->AddLabel("`oCurrent world: `w" + ToString(otherPlayer->GetCurrentWorld()) + "``")
    ->AddLabel("`oRole: `w" + string(otherPlayer->GetRole() ? otherPlayer->GetRole()->GetName() : "Player") + "``")
    ->AddSpacer()
    ->AddButton("Trade", "`wTrade``");

    db.AddLabel("(No Battle Leash equipped)")
    ->AddLabel("(Not enough Superpower Cards to battle)")
    ->AddSpacer();

    Role* pRole = GetRole();
    bool hasWorldAdminAccess = false;
    World* pWorld = GetWorldManager()->GetWorldByID(GetCurrentWorld());
    if(pWorld) {
        hasWorldAdminAccess = pWorld->IsPlayerWorldOwner(this) || pWorld->IsPlayerWorldAdmin(this);
    }

    if((pRole && pRole->HasPerm(ROLE_PERM_COMMAND_PULL)) || hasWorldAdminAccess) {
        db.AddButton("Pull", "`#Pull``");
    }

    if((pRole && pRole->HasPerm(ROLE_PERM_COMMAND_KICK)) || hasWorldAdminAccess) {
        db.AddButton("Kick", "`4Kick``");
    }

    if((pRole && pRole->HasPerm(ROLE_PERM_COMMAND_BAN)) || hasWorldAdminAccess) {
        db.AddButton("Ban", "`4World Ban``");
    }

    if(!IsFriendWith(otherPlayer->GetUserID())) {
        db.AddButton("Add", "`wAdd as Friend``");
    }
    else {
        db.AddButton("Unfriend", "`4Remove Friend``");
    }

    db.AddButton("Ignore", "`wIgnore Player``")
    ->AddSpacer()
    ->EndDialog("WrenchOthers", "", "Continue");

    SendOnDialogRequest(db.Get());
}

void GamePlayer::SendFriendMenu(const string& action)
{
    const bool showAll = (action == "FriendAll");
    const uint32 friendsOnline = CountOnlineFriends();

    DialogBuilder db;
    db.SetDefaultColor('o')
      ->AddLabelWithIcon(ToString(friendsOnline) + " of " + ToString((uint32)m_friendUserIDs.size()) + " `wFriends Online``", ITEM_ID_DUMB_FRIEND, true)
      ->AddSpacer();

    if(m_friendUserIDs.empty()) {
        db.AddLabel("You currently have no friends.")
          ->AddSpacer();
    }
    else {
        std::vector<uint32> friendIDs(m_friendUserIDs.begin(), m_friendUserIDs.end());
        std::sort(friendIDs.begin(), friendIDs.end(), [&](uint32 lhs, uint32 rhs) {
            GamePlayer* pLhs = GetGameServer()->GetPlayerByUserID(lhs);
            GamePlayer* pRhs = GetGameServer()->GetPlayerByUserID(rhs);
            const bool lhsOnline = pLhs && pLhs->HasState(PLAYER_STATE_IN_GAME);
            const bool rhsOnline = pRhs && pRhs->HasState(PLAYER_STATE_IN_GAME);
            if(lhsOnline != rhsOnline) {
                return lhsOnline;
            }

            return lhs < rhs;
        });

        bool addedAny = false;
        for(uint32 friendUserID : friendIDs) {
            GamePlayer* pFriend = GetGameServer()->GetPlayerByUserID(friendUserID);
            const bool isOnline = pFriend && pFriend->HasState(PLAYER_STATE_IN_GAME);
            if(!showAll && !isOnline) {
                continue;
            }

            string label = isOnline ? pFriend->GetDisplayName() : ("`4(Offline)`` User #" + ToString(friendUserID));
            db.AddLabel("`o- ``" + label);
            addedAny = true;
        }

        if(!addedAny) {
            db.AddLabel("None of your friends are currently online.");
        }

        db.AddSpacer();
    }

    db.AddButton("FriendAll", "Show offline and ignored too")
      ->AddButton("SeeSent", "Sent Friend Requests (`$" + ToString((uint32)m_sentFriendRequestUserIDs.size()) + "``)")
      ->AddButton("SeeReceived", "Received Friend Requests (`$" + ToString((uint32)m_receivedFriendRequestUserIDs.size()) + "``)")
      ->AddButton("GotoSocialPortal", "Back")
      ->EndDialog("FriendMenu", "", "Close");

    SendOnDialogRequest(db.Get());
}

bool GamePlayer::IsFriendWith(uint32 userID) const
{
    if(userID == 0) {
        return false;
    }

    return m_friendUserIDs.find(userID) != m_friendUserIDs.end();
}

uint32 GamePlayer::CountOnlineFriends() const
{
    uint32 onlineFriends = 0;
    GameServer* pGameServer = GetGameServer();
    if(!pGameServer) {
        return 0;
    }

    for(uint32 friendUserID : m_friendUserIDs) {
        GamePlayer* pFriend = pGameServer->GetPlayerByUserID(friendUserID);
        if(pFriend && pFriend->HasState(PLAYER_STATE_IN_GAME)) {
            ++onlineFriends;
        }
    }

    return onlineFriends;
}

bool GamePlayer::IsFriendRequestSentTo(uint32 userID) const
{
    if(userID == 0) {
        return false;
    }

    return m_sentFriendRequestUserIDs.find(userID) != m_sentFriendRequestUserIDs.end();
}

bool GamePlayer::IsFriendRequestReceivedFrom(uint32 userID) const
{
    if(userID == 0) {
        return false;
    }

    return m_receivedFriendRequestUserIDs.find(userID) != m_receivedFriendRequestUserIDs.end();
}

void GamePlayer::SendFriendRequestTo(GamePlayer* otherPlayer)
{
    if(!otherPlayer || otherPlayer == this) {
        return;
    }

    const uint32 otherUserID = otherPlayer->GetUserID();
    if(otherUserID == 0 || IsFriendWith(otherUserID)) {
        return;
    }

    if(IsFriendRequestReceivedFrom(otherUserID)) {
        AcceptFriendRequestFrom(otherPlayer);
        return;
    }

    if(IsFriendRequestSentTo(otherUserID)) {
        SendOnConsoleMessage("`oFriend request already sent to ``" + otherPlayer->GetDisplayName() + "``.");
        return;
    }

    m_sentFriendRequestUserIDs.insert(otherUserID);
    otherPlayer->m_receivedFriendRequestUserIDs.insert(GetUserID());

    SendOnTalkBubble("`5[``Friend request sent to `w" + otherPlayer->GetDisplayName() + "`` `5]``", true);
    otherPlayer->SendOnConsoleMessage("`3FRIEND REQUEST:`` You've received a friend request from `w" + GetDisplayName() + "``. Wrench this player and choose `wAdd as Friend`` to accept.");
    otherPlayer->PlaySFX("tip_start.wav", 0);
}

bool GamePlayer::AcceptFriendRequestFrom(GamePlayer* otherPlayer)
{
    if(!otherPlayer || otherPlayer == this) {
        return false;
    }

    const uint32 otherUserID = otherPlayer->GetUserID();
    if(otherUserID == 0) {
        return false;
    }

    if(!IsFriendRequestReceivedFrom(otherUserID) && !otherPlayer->IsFriendRequestReceivedFrom(GetUserID())) {
        return false;
    }

    m_receivedFriendRequestUserIDs.erase(otherUserID);
    m_sentFriendRequestUserIDs.erase(otherUserID);
    otherPlayer->m_receivedFriendRequestUserIDs.erase(GetUserID());
    otherPlayer->m_sentFriendRequestUserIDs.erase(GetUserID());

    const bool addedMine = AddFriendUserID(otherUserID);
    const bool addedOther = otherPlayer->AddFriendUserID(GetUserID());
    if(!addedMine && !addedOther) {
        return false;
    }

    SendOnConsoleMessage("`3FRIEND ADDED:`` You're now friends with `w" + otherPlayer->GetDisplayName() + "``!");
    otherPlayer->SendOnConsoleMessage("`3FRIEND ADDED:`` You're now friends with `w" + GetDisplayName() + "``!");
    PlaySFX("love_in.wav", 0);
    otherPlayer->PlaySFX("love_in.wav", 0);
    return true;
}

void GamePlayer::DenyFriendRequestFrom(uint32 userID)
{
    if(userID == 0) {
        return;
    }

    m_receivedFriendRequestUserIDs.erase(userID);
    m_sentFriendRequestUserIDs.erase(userID);

    GamePlayer* pOther = GetGameServer()->GetPlayerByUserID(userID);
    if(pOther) {
        pOther->m_sentFriendRequestUserIDs.erase(GetUserID());
        pOther->m_receivedFriendRequestUserIDs.erase(GetUserID());
    }
}

bool GamePlayer::AddFriendUserID(uint32 userID)
{
    if(userID == 0 || userID == GetUserID()) {
        return false;
    }

    return m_friendUserIDs.insert(userID).second;
}

bool GamePlayer::RemoveFriendUserID(uint32 userID)
{
    if(userID == 0) {
        return false;
    }

    return m_friendUserIDs.erase(userID) > 0;
}

bool GamePlayer::IsIgnoring(uint32 userID) const
{
    if(userID == 0) {
        return false;
    }

    return m_ignoredUserIDs.find(userID) != m_ignoredUserIDs.end();
}

bool GamePlayer::AddIgnoredUserID(uint32 userID)
{
    if(userID == 0 || userID == GetUserID()) {
        return false;
    }

    return m_ignoredUserIDs.insert(userID).second;
}

bool GamePlayer::RemoveIgnoredUserID(uint32 userID)
{
    if(userID == 0) {
        return false;
    }

    return m_ignoredUserIDs.erase(userID) > 0;
}

bool GamePlayer::IsMuted() const
{
    if(m_mutedUntilMS == 0) {
        return false;
    }

    return Time::GetSystemTime() < m_mutedUntilMS;
}

void GamePlayer::SetMutedUntilMS(uint64 untilMS, const string& reason)
{
    m_mutedUntilMS = untilMS;
    m_muteReason = reason;
}

void GamePlayer::ClearMute()
{
    m_mutedUntilMS = 0;
    m_muteReason.clear();
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

    const std::string message = fmt::format("`w{}`` `5earned the achievement \"{}\"!", GetDisplayName(), achievementName);

    if(m_currentWorldID != 0) {
        World* pWorld = GetWorldManager()->GetWorldByID(m_currentWorldID);
        if(pWorld) {
            pWorld->SendTalkBubbleAndConsoleToAll(message, false, this);
            pWorld->SendParticleEffectToAll(m_worldPos.x + 12.0f, m_worldPos.y + 15.0f, 46);
            return;
        }
    }

    SendOnConsoleMessage(message);
}

std::string GamePlayer::SerializeAchievements() const
{
    if(m_achievements.empty()) {
        return "";
    }

    std::string data;
    for(const auto& achievement : m_achievements) {
        if(!data.empty()) {
            data += '\n';
        }

        data += achievement;
    }

    return data;
}

std::string GamePlayer::SerializeStatistics() const
{
    if(m_stats.empty()) {
        return "";
    }

    std::string data;
    for(const auto& entry : m_stats) {
        if(!data.empty()) {
            data += '\n';
        }

        data += entry.first;
        data += '=';
        data += ToString(entry.second);
    }

    return data;
}

void GamePlayer::LoadAchievements(const std::string& data)
{
    m_achievements.clear();

    if(data.empty()) {
        return;
    }

    auto lines = Split(data, '\n');
    for(auto achievement : lines) {
        RemoveExtraWhiteSpaces(achievement);
        if(achievement.empty()) {
            continue;
        }

        if(!AchievementCatalog::IsKnownAchievement(achievement)) {
            continue;
        }

        m_achievements.insert(achievement);
    }
}

void GamePlayer::LoadStatistics(const std::string& data)
{
    m_stats.clear();

    if(data.empty()) {
        return;
    }

    auto lines = Split(data, '\n');
    for(auto line : lines) {
        RemoveExtraWhiteSpaces(line);
        if(line.empty()) {
            continue;
        }

        const size_t equalPos = line.find('=');
        if(equalPos == std::string::npos) {
            continue;
        }

        std::string key = line.substr(0, equalPos);
        std::string valueStr = line.substr(equalPos + 1);
        RemoveExtraWhiteSpaces(key);
        RemoveExtraWhiteSpaces(valueStr);
        if(key.empty() || valueStr.empty()) {
            continue;
        }

        int32 value = 0;
        if(ToInt(valueStr, value) != TO_INT_SUCCESS || value < 0) {
            continue;
        }

        m_stats[key] = (uint64)value;
    }
}

void GamePlayer::IncreaseStat(const std::string& statName, uint64 amount)
{
    if(statName.empty() || amount == 0) {
        return;
    }

    uint64& count = m_stats[statName];
    count += amount;

    AchievementCatalog::ApplyStatAchievements(this, statName, count);
}

uint64 GamePlayer::GetStatCount(const std::string& statName) const
{
    auto it = m_stats.find(statName);
    if(it == m_stats.end()) {
        return 0;
    }

    return it->second;
}

GamePlayer::GamePlayer(ENetPeer* pPeer) 
: Player(pPeer), m_currentWorldID(0), m_joiningWorld(false), m_guestID(1), m_lastItemActivateTime(0), m_state(0),
m_loggingOff(false), m_gems(0), m_lastObjectCollectTime(0), m_lastCollectFailCheckTime(0), m_magplantPos({ -1, -1 })
{
}

GamePlayer::~GamePlayer()
{
}

void GamePlayer::SetMagplantPos(Vector2Int pos)
{
    if(pos.x == -1 && pos.y == -1) {
        const uint8 remoteCount = GetInventory().GetCountOfItem(ITEM_ID_MAGPLANT_5000_REMOTE);
        if(remoteCount > 0) {
            ModifyInventoryItem(ITEM_ID_MAGPLANT_5000_REMOTE, (int16)-remoteCount);
        }
    }

    m_magplantPos = pos;
}

void GamePlayer::OnHandleDatabase(QueryTaskResult&& result)
{
    if(HasState(PLAYER_STATE_LOGIN_GETTING_ACCOUNT)) {
        LoadingAccount(std::move(result));
    }

    if(HasState(PLAYER_STATE_CREATING_GROWID) && HasState(PLAYER_STATE_IN_GAME)) {
        HandleCreateGrowID(std::move(result));
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
        roleID = GetRoleManager()->GetDefaultRoleID();
    }

    m_gems = result.result->GetField("Gems", 0).GetINT();
    if(m_gems < 0) {
        m_gems = 0;
    }

    m_level = result.result->GetField("Level", 0).GetUINT();
    if(m_level == 0) {
        m_level = 1;
    }

    m_xp = result.result->GetField("XP", 0).GetUINT();

    m_dailyRewardStreak = result.result->GetField("DailyRewardStreak", 0).GetUINT();
    m_dailyRewardClaimDay = result.result->GetField("DailyRewardClaimDay", 0).GetUINT();
    m_lastClaimDailyRewardMs = (uint64)result.result->GetField("LastClaimDailyReward", 0).GetUINT();

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

    const std::string achievementData = result.result->GetField("AchievementData", 0).GetString();
    const std::string statisticData = result.result->GetField("StatisticData", 0).GetString();
    LoadAchievements(achievementData);
    LoadStatistics(statisticData);

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

void GamePlayer::HandleCreateGrowID(QueryTaskResult&& result)
{
    if(!HasState(PLAYER_STATE_CREATING_GROWID)) {
        return;
    }
    RemoveState(PLAYER_STATE_CREATING_GROWID);
    
    if(result.extraData.empty()) {
        return;
    }

    int32 subType = result.extraData[0].GetINT();

    if(subType == PLAYER_SUB_GROWID_CHECK_IDENTIFIERS) {
        if(!result.result || result.extraData.size() < 2) {
            return;
        }

        Variant* pMac = result.result->GetFieldSafe("mac_count", 0);
        Variant* pIP = result.result->GetFieldSafe("ip_count", 0);
        Variant* pOther = nullptr;
        bool shouldSetOther = true;

        if(m_loginDetail.platformType == Proton::PLATFORM_ID_WINDOWS && !m_loginDetail.sid.empty()) {
            pOther = result.result->GetFieldSafe("sid_count", 0);
        }
        else if(m_loginDetail.platformType == Proton::PLATFORM_ID_ANDROID && !m_loginDetail.gid.empty()) {
            pOther = result.result->GetFieldSafe("gid_count", 0);
        }
        else if(m_loginDetail.platformType == Proton::PLATFORM_ID_IOS && !m_loginDetail.vid.empty()) {
            pOther = result.result->GetFieldSafe("vid_count", 0);
        }
        else {
            shouldSetOther = false;
        }

        if(!pMac && !pIP && (!shouldSetOther || !pOther)) {
            SendOnTalkBubble("`4Oops! ``Something went wrong while checking for account creating.", true);
            return;
        }

        GameConfig* pGameConfig = GetContext()->GetGameConfig();
        if(
            pIP->GetUINT() >= pGameConfig->maxAccountsPerIP ||
            pMac->GetUINT() >= pGameConfig->maxAccountsPerMac ||
            (shouldSetOther && (
                pOther->GetUINT() >
                (
                    m_loginDetail.platformType == Proton::PLATFORM_ID_WINDOWS ? pGameConfig->maxAccountsPerSid :
                    m_loginDetail.platformType == Proton::PLATFORM_ID_ANDROID ? pGameConfig->maxAccountsPerGid :
                    pGameConfig->maxAccountsPerVid
                )
            ))
        ) {
            SendOnTalkBubble("`4Oops! ``You've reached the max `5GrowID ``accounts you can make for this device or IP address!", true);
            return;
        }

        bool fromDialog = result.extraData[1].GetBool();
        if(fromDialog) {
            if(result.extraData.size() < 3) {
                return;
            }

            QueryRequest req = MakePlayerGrowIDExists(result.extraData[2].GetString(), GetNetID());
            req.extraData = result.extraData;
            req.extraData[0] = PLAYER_SUB_GROWID_CHECK_NAME;

            SetState(PLAYER_STATE_CREATING_GROWID);
            DatabasePlayerExec(GetContext()->GetDatabasePool(), DB_PLAYER_GROWID_EXISTS, req);
        }
        else {
            RegisterDialog::Request(this);
        }
    }
    else if(subType == PLAYER_SUB_GROWID_CHECK_NAME) {
        if(result.extraData.size() < 5) {
            return;
        }

        if(result.result->GetRowCount() > 0) {
            RegisterDialog::Request(
                this, result.extraData[2].GetString(),
                result.extraData[3].GetString(), result.extraData[4].GetString(),
                "`4Oops!`` The name `w" + result.extraData[2].GetString() + "`` is so cool someone else has already taken it. Please choose a different name."
            );
        }
        else {
            QueryRequest req = MakePlayerGrowIDCreate(m_userID, result.extraData[2].GetString(), result.extraData[3].GetString(), GetNetID());
            req.extraData.resize(3);
            req.extraData[0] = PLAYER_SUB_GROWID_SUCCESS;
            req.extraData[1] = result.extraData[2].GetString();
            req.extraData[2] = result.extraData[3].GetString();

            SetState(PLAYER_STATE_CREATING_GROWID);
            DatabasePlayerExec(GetContext()->GetDatabasePool(), DB_PLAYER_GROWID_CREATE, req);
        }
    }
    else if(subType == PLAYER_SUB_GROWID_SUCCESS) {
        m_loginDetail.tankIDName = result.extraData[1].GetString();
        m_loginDetail.tankIDPass = result.extraData[2].GetString();

        RegisterDialog::Success(this, m_loginDetail.tankIDName, m_loginDetail.tankIDPass);
    }
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
        m_level,
        m_xp,
        SerializeAchievements(),
        SerializeStatistics(),
        m_dailyRewardStreak,
        m_dailyRewardClaimDay,
        m_lastClaimDailyRewardMs,
        GetNetID()
    );
    
    DatabasePlayerExec(GetContext()->GetDatabasePool(), DB_PLAYER_SAVE, req);
    SAFE_DELETE_ARRAY(pInvData);
}

bool GamePlayer::CanClaimDailyReward(uint32 currentEpochDay) const
{
    if(currentEpochDay == 0) {
        return false;
    }

    if(m_dailyRewardClaimDay == currentEpochDay) {
        return false;
    }

    return true;
}

void GamePlayer::ResetDailyRewardProgressIfNewDay(uint32 currentEpochDay)
{
    if(currentEpochDay == 0 || m_dailyRewardClaimDay == currentEpochDay) {
        return;
    }

    const uint32 previousClaimDay = m_dailyRewardClaimDay;
    const bool isConsecutiveDay = (currentEpochDay == previousClaimDay + 1);

    if(!isConsecutiveDay) {
        m_dailyRewardStreak = 0;
    }

    m_dailyRewardClaimDay = currentEpochDay;
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

void GamePlayer::ExecGrowIDIdentifierCheck(bool fromDialog, const VariantVector& extraData)
{
    SetState(PLAYER_STATE_CREATING_GROWID);

    QueryRequest req;

    if (m_loginDetail.platformType == Proton::PLATFORM_ID_WINDOWS && !m_loginDetail.sid.empty()) {
        req = MakeCountPlayerBySidMacIP(m_loginDetail.sid, m_loginDetail.mac, GetAddress(), GetNetID());
    }
    else if (m_loginDetail.platformType == Proton::PLATFORM_ID_ANDROID && !m_loginDetail.gid.empty()) {
        req = MakeCountPlayerByGidMacIP(m_loginDetail.gid, m_loginDetail.mac, GetAddress(), GetNetID());
    }
    else if (m_loginDetail.platformType == Proton::PLATFORM_ID_IOS && !m_loginDetail.vid.empty()) {
        req = MakeCountPlayerByVidMacIP(m_loginDetail.vid, m_loginDetail.mac, GetAddress(), GetNetID());
    }
    else {
        req = MakeCountPlayerByMacIP(m_loginDetail.mac, GetAddress(), GetNetID());
    }

    req.extraData.resize(2);
    req.extraData[0] = PLAYER_SUB_GROWID_CHECK_IDENTIFIERS;
    req.extraData[1] = fromDialog;

    if (!extraData.empty()) {
        req.extraData.resize(5);
        req.extraData[2] = extraData[0]; // name
        req.extraData[3] = extraData[1]; // pass
        req.extraData[4] = extraData[2]; // verify pass
    }

    if (m_loginDetail.platformType == Proton::PLATFORM_ID_WINDOWS && !m_loginDetail.sid.empty()) {
        DatabasePlayerExec(GetContext()->GetDatabasePool(), DB_PLAYER_COUNT_SID_MAC_IP, req);
    }
    else if (m_loginDetail.platformType == Proton::PLATFORM_ID_ANDROID && !m_loginDetail.gid.empty()) {
        DatabasePlayerExec(GetContext()->GetDatabasePool(), DB_PLAYER_COUNT_GID_MAC_IP, req);
    }
    else if (m_loginDetail.platformType == Proton::PLATFORM_ID_IOS && !m_loginDetail.vid.empty()) {
        DatabasePlayerExec(GetContext()->GetDatabasePool(), DB_PLAYER_COUNT_VID_MAC_IP, req);
    }
    else {
        DatabasePlayerExec(GetContext()->GetDatabasePool(), DB_PLAYER_COUNT_MAC_IP, req);
    }
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

bool GamePlayer::CanTriggerSteamByStep(const Vector2Int& tilePos, uint64 nowMS)
{
    const bool sameTile = (tilePos == m_lastSteamStepTilePos);
    const uint64 elapsed = nowMS > m_lastSteamStepTriggerTime ? (nowMS - m_lastSteamStepTriggerTime) : 0;

    if(sameTile && elapsed < 450) {
        return false;
    }

    m_lastSteamStepTilePos = tilePos;
    m_lastSteamStepTriggerTime = nowMS;
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

    AchievementCatalog::ApplyGemAchievements(this, m_gems);
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

void GamePlayer::AddXP(uint32 amount)
{
    if(amount == 0) {
        return;
    }

    m_xp += amount;

    bool leveledUp = false;
    while(true) {
        const uint32 requiredXP = m_level * (1500 * 5);
        if(m_xp < requiredXP) {
            break;
        }

        m_xp -= requiredXP;
        m_level += 1;
        leveledUp = true;
    }

    AchievementCatalog::ApplyLevelAchievements(this, m_level);

    if(!leveledUp) {
        return;
    }

    if(m_currentWorldID == 0) {
        return;
    }

    World* pWorld = GetWorldManager()->GetWorldByID(m_currentWorldID);
    if(!pWorld) {
        return;
    }

    pWorld->SendTalkBubbleAndConsoleToAll("`o" + GetDisplayName() + " `wis now level " + ToString(m_level) + "!", false);
    pWorld->SendParticleEffectToAll(m_worldPos.x + 12.0f, m_worldPos.y + 15.0f, 46);
}