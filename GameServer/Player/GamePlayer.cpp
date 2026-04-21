#include "GamePlayer.h"
#include "../Server/MasterBroadway.h"
#include "../Context.h"
#include "IO/Log.h"
#include "Utils/Timer.h"
#include "Item/ItemInfoManager.h"
#include "Player/PlayerTribute.h"
#include "Player/RoleManager.h"
#include "Database/Table/PlayerDBTable.h"
#include "Database/Table/GuildDBTable.h"
#include "Player/PlayModManager.h"
#include "../World/WorldManager.h"
#include "Math/Random.h"
#include "Dialog/PlayerDialog.h"
#include "Dialog/RenderWorldDialog.h"
#include "AchievementCatalog.h"
#include "Dialog/RegisterDialog.h"
#include "Utils/DialogBuilder.h"
#include "../Guild/GuildManager.h"
#include "../Server/GameServer.h"
#include "../Component/GeigerComponent.h"
#include "Proton/ProtonUtils.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
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
constexpr uint64 kFriendAlertDebounceWindowMS = 3500;
constexpr const char* kFriendStatPrefix = "FRIEND_UID_";
constexpr const char* kFriendReqOutPrefix = "FRIEND_REQ_OUT_";
constexpr const char* kFriendReqInPrefix = "FRIEND_REQ_IN_";
constexpr const char* kIgnoredStatPrefix = "IGNORED_UID_";
constexpr const char* kPBanStatActive = "PBAN_ACTIVE";
constexpr const char* kPBanStatUntil = "PBAN_UNTIL";
constexpr const char* kPBanStatSetAt = "PBAN_SET_AT";
constexpr const char* kPBanStatDuration = "PBAN_DURATION";
constexpr const char* kPBanStatPerma = "PBAN_PERMA";
constexpr uint32 kGuildMemberBlobVersion = 1;

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

bool TryReadIntField(DatabaseResult* pResult, const char* fieldName, int32& outValue)
{
    if(!pResult || !fieldName) {
        return false;
    }

    Variant& field = pResult->GetField(fieldName, 0);
    if(field.GetType() == VARIANT_TYPE_INT) {
        outValue = field.GetINT();
        return true;
    }

    if(field.GetType() == VARIANT_TYPE_UINT) {
        outValue = (int32)std::min<uint32>(field.GetUINT(), (uint32)INT32_MAX);
        return true;
    }

    return false;
}

bool TryReadUIntField(DatabaseResult* pResult, const char* fieldName, uint32& outValue)
{
    if(!pResult || !fieldName) {
        return false;
    }

    Variant& field = pResult->GetField(fieldName, 0);
    if(field.GetType() == VARIANT_TYPE_UINT) {
        outValue = field.GetUINT();
        return true;
    }

    if(field.GetType() == VARIANT_TYPE_INT) {
        const int32 signedValue = field.GetINT();
        outValue = signedValue < 0 ? 0u : (uint32)signedValue;
        return true;
    }

    return false;
}

string SerializeGuildMembersHex(const std::vector<GuildMember>& members)
{
    const uint32 memberCount = static_cast<uint32>(members.size());
    const uint32 memSize = sizeof(uint32) + memberCount * (sizeof(uint32) + sizeof(uint32));
    std::vector<uint8> data(memSize, 0);

    MemoryBuffer memBuffer(data.data(), memSize);
    uint32 version = kGuildMemberBlobVersion;
    memBuffer.ReadWrite(version, true);

    for(const GuildMember& member : members) {
        uint32 userID = member.UserID;
        uint32 position = static_cast<uint32>(member.Position);
        memBuffer.ReadWrite(userID, true);
        memBuffer.ReadWrite(position, true);
    }

    return ToHex(data.data(), memSize);
}

std::vector<GuildMember> ParseGuildMembersHex(const string& membersHex)
{
    std::vector<GuildMember> out;
    if(membersHex.empty() || (membersHex.size() % 2) != 0) {
        return out;
    }

    const uint32 memSize = static_cast<uint32>(membersHex.size() / 2);
    if(memSize < sizeof(uint32)) {
        return out;
    }

    std::vector<uint8> data(memSize, 0);
    HexToBytes(membersHex, data.data());

    MemoryBuffer memBuffer(data.data(), memSize);
    uint32 version = 0;
    memBuffer.ReadWrite(version, false);
    if(version != kGuildMemberBlobVersion) {
        return out;
    }

    while(memBuffer.GetOffset() + (sizeof(uint32) * 2) <= memSize) {
        GuildMember member;

        uint32 userID = 0;
        uint32 position = 0;
        memBuffer.ReadWrite(userID, false);
        memBuffer.ReadWrite(position, false);

        member.UserID = userID;
        member.Position = static_cast<GuildPosition>(position);
        out.push_back(member);
    }

    return out;
}

bool IsValidGuildName(const string& guildName)
{
    if(guildName.size() < 3 || guildName.size() > 18) {
        return false;
    }

    for(char c : guildName) {
        if(!IsAlpha(c) && !IsDigit(c)) {
            return false;
        }
    }

    return true;
}

string FormatBanCountdown(uint64 totalSeconds)
{
    uint64 days = totalSeconds / 86400ull;
    totalSeconds %= 86400ull;

    uint64 hours = totalSeconds / 3600ull;
    totalSeconds %= 3600ull;

    uint64 mins = totalSeconds / 60ull;
    uint64 secs = totalSeconds % 60ull;

    string out;
    if(days > 0) {
        out += ToString(days) + " days, ";
    }
    if(hours > 0 || !out.empty()) {
        out += ToString(hours) + " hours, ";
    }
    if(mins > 0 || !out.empty()) {
        out += ToString(mins) + " mins, ";
    }
    out += ToString(secs) + " secs";
    return out;
}

bool TryParsePBanTargetUserID(const string& target, uint32& outUserID)
{
    if(target.empty() || target[0] != '#') {
        return false;
    }

    const string userIDText = target.substr(1);
    if(userIDText.empty()) {
        return false;
    }

    return ToUInt(userIDText, outUserID) == TO_INT_SUCCESS && outUserID != 0;
}

uint32 BuildPBanUntilEpochSec(int32 durationSec)
{
    if(durationSec < 0) {
        return (uint32)INT32_MAX;
    }

    const uint64 nowSec = Time::GetTimeSinceEpoch();
    const uint64 untilSec = nowSec + (uint64)durationSec;
    if(untilSec > (uint64)INT32_MAX) {
        return (uint32)INT32_MAX;
    }

    return (uint32)untilSec;
}

string BuildAncientsSuspendBroadcast(const string& targetName)
{
    return "`#** ```$The Ancient Ones have `4suspended`` " + targetName + " `#**`` (`4/rules`` to see the rules!)";
}

string BuildAncientsBanConsole(const string& targetName)
{
    return "`#**`$ The Ancients`o have used `#Ban `oon " + targetName + "`o!`# **";
}

string BuildSuspendedLoginMessage(const string& targetName, uint64 remainingSeconds, bool permanentBan)
{
    if(!permanentBan && remainingSeconds > 0) {
        return "`4Sorry, this account has been temporarily suspended.`` This suspension will be removed in `w" + FormatBanCountdown(remainingSeconds) + "``.";
    }

    return "`4Sorry, this account (`w" + targetName + "`4) has been suspended from GTopia.";
}

string BuildMaintenanceLoginMessage(const string& targetName)
{
    return targetName + " is currently under maintenance. Please come back later.";
}

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
    pPlayer->ClearPendingTradeModifyItemID();
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
        const uint16 clientItemID = (uint16)GetItemInfoManager()->GetClientIDByItemID(offer.ID);
        dialog += fmt::format("add_slot|{}|{}|\n", clientItemID, offer.Amount);
    }

    dialog += "locked|0\n";
    dialog += "reset_locks|1\n";
    dialog += fmt::format("accepted|{}\n", pOfferOwner->IsTradeAccepted() ? 1 : 0);
    return dialog;
}

void SyncTradeStatus(GamePlayer* pA, GamePlayer* pB)
{
    if(!pA) {
        return;
    }

    pA->SendTradeStatus(pA);

    if(!pB) {
        return;
    }

    pA->SendTradeStatus(pB);
    pB->SendTradeStatus(pA);
    pB->SendTradeStatus(pB);
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

    if(!IsValidTradePair(this, player)) {
        SendOnConsoleMessage("`4Target is no longer valid.``");
        return;
    }

    CancelTrade(false);

    if(player->IsTrading()) {
        SendOnTalkBubble("That player is busy.", true);
        return;
    }

    SetTradingWithUserID(player->GetUserID());
    SendStartTrade(player);
}

void GamePlayer::CancelTrade(bool busy, std::string customMessage)
{
    if(!IsTrading()) {
        return;
    }

    GamePlayer* pTarget = GetGameServer()->GetPlayerByUserID(GetTradingWithUserID());
    const bool isTradingWithMe = pTarget && pTarget != this && pTarget->GetTradingWithUserID() == GetUserID();

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
    else if(busy && isTradingWithMe) {
        const std::string message = fmt::format("`8[`w{} `4is too busy to trade!`8]``", GetRawName());
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

    GamePlayer* pTarget = GetGameServer()->GetPlayerByUserID(GetTradingWithUserID());
    if(!IsValidTradePair(this, pTarget)) {
        CancelTrade(false);
        return;
    }

    if(!IsTradeItemAllowed(itemID)) {
        return;
    }

    const uint64 nowMS = Time::GetSystemTime();
    if(GetLastChangeTradeDeal() != 0 && nowMS - GetLastChangeTradeDeal() < kTradeChangeThrottleMS) {
        SendOnTextOverlay("Please slow down when changing trade offers.");
        return;
    }

    const uint8 ownedCount = GetInventory().GetCountOfItem(itemID);
    if(ownedCount == 0) {
        return;
    }

    if(amount == 0) {
        if(ownedCount > 1) {
            DialogBuilder db;
            db.SetDefaultColor('o');
            db.AddLabelWithIcon(fmt::format("`oTrade `w{}``", GetTradeItemName(itemID)), itemID, true);
            db.AddLabel("How many would you like to trade?");
            db.EmbedData("ItemID", itemID);
            db.AddTextInput("Amount", "", "", 3);
            db.EndDialog("TradeModify", "OK", "Cancel");

            SetPendingTradeModifyItemID(itemID);
            SendOnDialogRequest(db.Get());
            return;
        }

        amount = ownedCount;
    }

    if(amount == 0 || amount > 200 || amount > ownedCount) {
        amount = ownedCount;
    }

    if(!IsTradeOfferExists(itemID) && m_tradeOffers.size() >= kTradeMaxOfferItems) {
        PlaySFX("cant_place_tile.wav");
        return;
    }

    if(pTarget->GetTradingWithUserID() == GetUserID()) {
        if(pTarget->IsTradeAccepted() && IsTradeAccepted()) {
            return;
        }

        if(pTarget->IsTradeConfirmed() && IsTradeConfirmed()) {
            return;
        }
    }

    for(const auto& offer : m_tradeOffers) {
        if(GetInventory().GetCountOfItem(offer.ID) < offer.Amount) {
            RemoveTradeOffer(offer.ID);
        }
    }

    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(itemID);
    if(!pItem) {
        PlaySFX("cant_place_tile.wav");
        return;
    }

    if(amount > pItem->maxCanHold) {
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

    SendOnTextOverlay("`#The trade deal has been updated.");
    SendOnTextOverlay(fmt::format("{}`` offered ``{}x {}``", GetRawName(), amount, GetTradeItemName(itemID)));
    PlaySFX("tile_removed.wav");

    if(pTarget->GetTradingWithUserID() == GetUserID()) {
        SendTradeStatus(pTarget);

        pTarget->SendTradeStatus(this);
        pTarget->SendTradeStatus(pTarget);

        pTarget->SendOnTextOverlay("`#The trade deal has been updated.");
        pTarget->SendOnTextOverlay(fmt::format("{}`` offered ``{}x {}``", GetRawName(), amount, GetTradeItemName(itemID)));
        pTarget->PlaySFX("tile_removed.wav");
    }
}

void GamePlayer::RemoveOffer(uint16 itemID)
{
    if(!IsTrading()) {
        return;
    }

    GamePlayer* pTarget = GetGameServer()->GetPlayerByUserID(GetTradingWithUserID());
    if(!IsValidTradePair(this, pTarget)) {
        CancelTrade(false);
        return;
    }

    const uint64 nowMS = Time::GetSystemTime();
    if(GetLastChangeTradeDeal() != 0 && nowMS - GetLastChangeTradeDeal() < kTradeChangeThrottleMS) {
        SendOnTextOverlay("Please slow down when changing trade offers.");
        return;
    }

    if(pTarget->GetTradingWithUserID() == GetUserID()) {
        if(pTarget->IsTradeAccepted() && IsTradeAccepted()) {
            return;
        }

        if(pTarget->IsTradeConfirmed() && IsTradeConfirmed()) {
            return;
        }
    }

    uint8 removedAmount = 0;
    for(const auto& offer : m_tradeOffers) {
        if(offer.ID == itemID) {
            removedAmount = offer.Amount;
        }
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

    SendOnTextOverlay("`#The trade deal has been updated.");
    SendOnTextOverlay(fmt::format("{}`` `4removed ``{}x {}``", GetRawName(), removedAmount, GetTradeItemName(itemID)));
    PlaySFX("tile_removed.wav");

    if(pTarget->GetTradingWithUserID() == GetUserID()) {
        SendTradeStatus(pTarget);

        pTarget->SendTradeStatus(this);
        pTarget->SendTradeStatus(pTarget);

        pTarget->SendOnTextOverlay("`#The trade deal has been updated.");
        pTarget->SendOnTextOverlay(fmt::format("{}`` `4removed ``{}x {}``", GetRawName(), removedAmount, GetTradeItemName(itemID)));
        pTarget->PlaySFX("tile_removed.wav");
    }
}

void GamePlayer::AcceptOffer(bool status)
{
    if(!IsTrading()) {
        return;
    }

    GamePlayer* pTarget = GetGameServer()->GetPlayerByUserID(GetTradingWithUserID());
    if(!IsValidTradePair(this, pTarget)) {
        CancelTrade(false);
        return;
    }

    SetTradeAccepted(status);
    SetTradeAcceptedAt(status ? Time::GetSystemTime() : 0);

    if(pTarget->GetTradingWithUserID() == GetUserID()) {
        pTarget->SendTradeStatus(this);
    }

    if(!(pTarget->IsTradeAccepted() && IsTradeAccepted() && pTarget->GetTradingWithUserID() == GetUserID())) {
        return;
    }

    bool allowTrade = true;
    for(const auto& offer : pTarget->GetTradeOffers()) {
        TradeReceiveState receiveState = GetTradeReceiveState(this, offer.ID, offer.Amount);
        if(receiveState == TradeReceiveState::FULL_SLOTS) {
            const std::string message = fmt::format("`w{}`` needs more backpack room first!", GetRawName());
            SendOnTextOverlay(message);
            pTarget->SendOnTextOverlay(message);
            allowTrade = false;
            break;
        }

        if(receiveState == TradeReceiveState::WILL_MAX) {
            const std::string message = fmt::format("`w{}`` `4is carrying too many `w{}`4.", GetRawName(), GetTradeItemName(offer.ID));
            SendOnTextOverlay(message);
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
                SendOnTextOverlay(message);
                pTarget->SendOnTextOverlay(message);
                allowTrade = false;
                break;
            }

            if(receiveState == TradeReceiveState::WILL_MAX) {
                const std::string message = fmt::format("`w{}`` `4is carrying too many `w{}`4.", pTarget->GetRawName(), GetTradeItemName(offer.ID));
                SendOnTextOverlay(message);
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
        }

        SendTradeStatus(this);
        SendTradeStatus(pTarget);
        pTarget->SendTradeStatus(this);
        pTarget->SendTradeStatus(pTarget);

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
    SetTradeConfirmedAt(0);
    pTarget->SetTradeConfirmed(false);
    pTarget->SetTradeConfirmedAt(0);
}

void GamePlayer::ConfirmOffer()
{
    if(!IsTrading()) {
        return;
    }

    GamePlayer* pTarget = GetGameServer()->GetPlayerByUserID(GetTradingWithUserID());
    if(!IsValidTradePair(this, pTarget)) {
        CancelTrade(false);
        return;
    }

    if(!(pTarget->IsTradeAccepted() && IsTradeAccepted() && pTarget->GetTradingWithUserID() == GetUserID())) {
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

    const std::vector<TradeOffer> myOffers = GetTradeOffers();
    const std::vector<TradeOffer> targetOffers = pTarget->GetTradeOffers();

    auto hasInvalidOffer = [](const std::vector<TradeOffer>& offers) {
        for(const TradeOffer& offer : offers) {
            if(offer.Amount == 0 || !IsTradeItemAllowed(offer.ID)) {
                return true;
            }
        }

        return false;
    };

    if(hasInvalidOffer(myOffers) || hasInvalidOffer(targetOffers)) {
        const std::string message = "`4Trade cancelled - invalid items were detected in the offer.";
        SendOnConsoleMessage(message);
        SendOnTextOverlay(message);
        pTarget->SendOnConsoleMessage(message);
        pTarget->SendOnTextOverlay(message);
        CancelTrade(false);
        return;
    }

    for(const auto& offer : targetOffers) {
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

    for(const auto& offer : myOffers) {
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

    for(const auto& offer : targetOffers) {
        const TradeReceiveState receiveState = GetTradeReceiveState(this, offer.ID, offer.Amount);
        if(receiveState != TradeReceiveState::OK) {
            const std::string message = "`4Trade cancelled - your backpack cannot receive the other player's offer.";
            SendOnConsoleMessage(message);
            SendOnTextOverlay(message);
            pTarget->SendOnConsoleMessage(message);
            pTarget->SendOnTextOverlay(message);
            CancelTrade(false);
            return;
        }
    }

    for(const auto& offer : myOffers) {
        const TradeReceiveState receiveState = GetTradeReceiveState(pTarget, offer.ID, offer.Amount);
        if(receiveState != TradeReceiveState::OK) {
            const std::string message = "`4Trade cancelled - the other player's backpack cannot receive your offer.";
            SendOnConsoleMessage(message);
            SendOnTextOverlay(message);
            pTarget->SendOnConsoleMessage(message);
            pTarget->SendOnTextOverlay(message);
            CancelTrade(false);
            return;
        }
    }

    World* pWorld = GetWorldManager()->GetWorldByID(GetCurrentWorld());
    const uint64 nowMS = Time::GetSystemTime();
    std::string myOffersSummary = BuildTradeOfferSummary(myOffers);
    std::string theirOffersSummary = BuildTradeOfferSummary(targetOffers);

    for(const auto& offer : targetOffers) {
        pTarget->ModifyInventoryItem(offer.ID, -(int16)offer.Amount);
        ModifyInventoryItem(offer.ID, offer.Amount);
    }

    for(const auto& offer : myOffers) {
        ModifyInventoryItem(offer.ID, -(int16)offer.Amount);
        pTarget->ModifyInventoryItem(offer.ID, offer.Amount);
    }

    if(pWorld) {
        pWorld->SendConsoleMessageToAll(fmt::format("`1{} `1traded {} to {}.``", GetRawName(), myOffersSummary, pTarget->GetRawName()));
        pWorld->SendConsoleMessageToAll(fmt::format("`1{} `1traded {} to {}.``", pTarget->GetRawName(), theirOffersSummary, GetRawName()));
        pWorld->PlaySFXForEveryone("keypad_hit.wav");
    }

    AddTradeHistory(fmt::format("Traded {} with {}", BuildTradeHistorySummary(myOffers), pTarget->GetRawName()));
    pTarget->AddTradeHistory(fmt::format("Traded {} with {}", BuildTradeHistorySummary(targetOffers), GetRawName()));

    SetLastTradedAt(nowMS);
    pTarget->SetLastTradedAt(nowMS);

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

    if(page == "set_online_status") {
        SetShowFriendNotification(!IsShowFriendNotification());
        SendOnConsoleMessage(string("`oFriend notifications are now ") + (IsShowFriendNotification() ? "`2enabled``." : "`4disabled``."));
        page = "PlayerInfo";
    }
    else if(page == "ske_titles") {
        page = "Titles";
    }
    else if(page == "view_owned_worlds") {
        page = "Worlds";
    }
    else if(page == "alist") {
        page = "PlayerStats";
    }
    else if(page == "goals" || page == "bonus" || page == "emojis" || page == "marvelous_missions") {
        SendOnConsoleMessage("`oThis menu is still being expanded in this source.``");
        page = "PlayerInfo";
    }
    else if(page == "notebook_edit") {
        SendOnConsoleMessage("`oNotebook feature is not yet available in this source build.``");
        page = "PlayerInfo";
    }
    else if(page == "billboard_edit") {
        SendOnConsoleMessage("`oBillboard editor is not yet available in this source build.``");
        page = "PlayerInfo";
    }
    else if(page == "change_password") {
        SendOnConsoleMessage("`oPassword change is handled through account tools in this source.``");
        page = "PlayerInfo";
    }
    else if(page == "GuildInvitations") {
        SendPendingGuildInviteDialog();
        return;
    }

    const Vector2Float worldPos = GetWorldPos();
    const int32 tileX = (int32)(worldPos.x / 32.0f) + 1;
    const int32 tileY = (int32)(worldPos.y / 32.0f) + 1;

    string standingNote = "C";
    switch(((tileY - 1) % 14 + 14) % 14) {
        case 0: standingNote = "B"; break;
        case 1: standingNote = "A"; break;
        case 2: standingNote = "G"; break;
        case 3: standingNote = "F"; break;
        case 4: standingNote = "E"; break;
        case 5: standingNote = "D"; break;
        case 6: standingNote = "C"; break;
        case 7: standingNote = "B"; break;
        case 8: standingNote = "A"; break;
        case 9: standingNote = "G"; break;
        case 10: standingNote = "F"; break;
        case 11: standingNote = "E"; break;
        case 12: standingNote = "D"; break;
        case 13: standingNote = "C"; break;
    }

    string activeEffects = "add_label_with_icon|small|`wNothing!``|left|2|\n";
    auto& updateMods = m_characterData.GetReqUpdatePlayMods();
    if(!updateMods.empty()) {
        activeEffects.clear();
        for(const auto& mod : updateMods) {
            PlayMod* pMod = GetPlayModManager()->GetPlayMod(mod.modType);
            if(!pMod) {
                continue;
            }

            string line = "add_label_with_icon|small|`w" + pMod->GetName() + "``";
            if(mod.durationMS > 0) {
                Timer timer = mod.timer;
                const uint64 elapsed = timer.GetElapsedTime();
                const uint64 leftMS = elapsed >= mod.durationMS ? 0 : ((uint64)mod.durationMS - elapsed);
                line += " (`o" + Time::ConvertTimeToStr(leftMS) + " left``)";
            }

            line += "|left|" + ToString((uint32)pMod->GetDisplayItem()) + "|\n";
            activeEffects += line;
        }

        if(activeEffects.empty()) {
            activeEffects = "add_label_with_icon|small|`wNothing!``|left|2|\n";
        }
    }

    const string worldName = pWorld ? pWorld->GetWorlName() : "EXIT";
    const uint32 worldPlayers = pWorld ? pWorld->GetPlayerCount() : 0;
    const uint32 bagSlots = GetInventory().GetCapacity();

    DialogBuilder db;
    db.SetDefaultColor('o');

    if(page.empty() || page == "PlayerInfo") {
        const uint32 onlineFriends = CountOnlineFriends();

        db.EmbedData("invitedGuildID", 0)
            ->EmbedData("worldName", worldName)
            ->EmbedData("netID", GetNetID())
            ->AddCustomLine("add_player_info|`2" + GetDisplayName() + "``|" + ToString(GetLevel()) + "|" + ToString(GetXP()) + "|" + ToString(GetLevel() * 1500) + "|")
            ->AddSpacer()
            ->AddButton("change_password", "`oChange Password")
            ->AddButton("open_social_portal", "`oSocial Portal")
            ->AddCustomLine("set_custom_spacing|x:5;y:10|")
            ->AddCustomLine("add_custom_button|ske_titles|image:interface/large/gui_wrench_title.rttex;image_size:400,260;width:0.19;|")
            ->AddCustomLine("add_custom_button|set_online_status|image:interface/large/gui_wrench_online_status_1green.rttex;image_size:400,260;width:0.19;|")
            ->AddCustomLine("add_custom_button|notebook_edit|image:interface/large/gui_wrench_notebook.rttex;image_size:400,260;width:0.19;|")
            ->AddCustomLine("add_custom_button|billboard_edit|image:interface/large/gui_wrench_edit_billboard.rttex;image_size:400,260;width:0.19;|")
            ->AddCustomLine("add_custom_button|goals|image:interface/large/gui_wrench_goals_quests.rttex;image_size:400,260;width:0.19;|")
            ->AddCustomLine("add_custom_button|bonus|image:interface/large/gui_wrench_daily_bonus_active.rttex;image_size:400,260;width:0.19;|")
            ->AddCustomLine("add_custom_button|view_owned_worlds|image:interface/large/gui_wrench_my_worlds.rttex;image_size:400,260;width:0.19;|")
            ->AddCustomLine("add_custom_button|alist|image:interface/large/gui_wrench_achievements.rttex;image_size:400,260;width:0.19;|")
            ->AddCustomLine("add_custom_button|emojis|image:interface/large/gui_wrench_growmojis.rttex;image_size:400,260;width:0.19;|")
            ->AddCustomLine("add_custom_button|marvelous_missions|image:interface/large/gui_wrench_marvelous_missions.rttex;image_size:400,260;width:0.19;|")
            ->AddCustomLine("add_custom_break|")
            ->AddSpacer()
            ->AddTextBox("`wActive effects:``", false)
            ->AddCustomLine(activeEffects)
            ->AddSpacer()
            ->AddTextBox("`oYou have `w" + ToString(bagSlots) + "`` backpack slots.``")
            ->AddTextBox("`oCurrent world: `w" + worldName + "`` (`w" + ToString(tileX) + "``, `w" + ToString(tileY) + "``) (`w" + ToString(worldPlayers) + "`` person).``")
            ->AddTextBox("`oYou are standing on the note \"`w" + standingNote + "`o\".``")
            ->AddTextBox("`oFriends online: `w" + ToString(onlineFriends) + "`` / `w" + ToString((uint32)m_friendUserIDs.size()) + "``")
            ->AddTextBox("`oFriend notifications: `w" + string(IsShowFriendNotification() ? "On" : "Off") + "``")
            ->AddSpacer()
            ->AddButton("SocialProfile", "Social Profile")
            ->AddButton("PlayerStats", "Player Stats")
            ->AddButton("Settings", "Settings")
            ->AddButton("Titles", "Title Settings")
            ->AddButton("Worlds", "My Worlds")
            ;

        if(!m_pendingGuildInvites.empty()) {
            db.AddButton("GuildInvitations", "Guild Invitations");
        }

        db.EndDialog("WrenchSelf", "", "Continue")
            ->AddQuickExit();
    }
    else if(page == "SocialProfile") {
        db.AddLabelWithIcon("`wSocial Profile``", ITEM_ID_WRENCH, true)
        ->AddLabel("`oFriends online: `w" + ToString(CountOnlineFriends()) + "``")
        ->AddLabel("`oFriend notifications: `w" + string(IsShowFriendNotification() ? "On" : "Off") + "``")
        ->AddLabel("`oUse wrench on a player to send PM, trade, add friend, ignore, or moderate actions.")
        ->AddSpacer()
        ->AddButton("set_online_status", "Toggle Friend Notifications")
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
        ->AddLabel("`oToggle profile options and shortcut actions from here.")
        ->AddCheckBox("checkbox_friend_alert", "Show friend notifications", IsShowFriendNotification())
        ->AddSpacer()
        ->AddButton("set_online_status", "Quick Toggle Notifications")
        ->AddButton("PlayerInfo", "Back")
        ->EndDialog("WrenchSelf", "", "Continue");
    }
    else if(page == "Titles") {
        const bool canLegendary = IsLegend();
        const bool canGrow4Good = IsGrow4Good();
        const bool canMvp = IsMVP();
        const bool canVip = IsVIP();

        db.AddLabelWithIcon("`wTitles and Prefix``", ITEM_ID_GROWMOJI_GROW, true)
            ->AddSpacer()
            ->AddLabel("`wGlobal Preference``")
            ->AddCheckBox("Prefix", "Show Prefix", IsPrefixEnabled());

        if(canLegendary) {
            db.AddCheckBox("legentitle", "Enable Legendary Title", IsLegendaryTitleEnabled());
        }

        if(canGrow4Good) {
            db.AddCheckBox("grow4good", "Enable MVP+ Title", IsGrow4GoodTitleEnabled());
        }

        if(canMvp) {
            db.AddCheckBox("bluename", "Enable MVP Title", IsMaxLevelTitleEnabled());
        }

        if(canVip) {
            db.AddCheckBox("ssup", "Enable VIP Title", IsSuperSupporterTitleEnabled());
        }

        db.AddQuickExit()
            ->EndDialog("TitlesCustomization", "Save", "Cancel");
    }
    else if(page == "Worlds") {
        db.AddLabelWithIcon("`wMy Worlds``", ITEM_ID_WRENCH, true)
        ->AddLabel("`oYour current world is `w" + worldName + "``.");

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

void GamePlayer::SendSocialPortal()
{
    DialogBuilder db;
    db.SetDefaultColor('o')
        ->AddLabelWithIcon("`wSocial Portal``", ITEM_ID_WRENCH, true)
        ->AddSpacer()
        ->AddButton("GotoFriendsMenu", "Show Friends");

    if(m_guildID != 0) {
        db.AddButton("GotoGuildMenu", "Guild Menu");
    }
    else {
        db.AddButton("CreateGuild", "Create Guild");
    }

    db.AddButton("GotoTradeHistory", "Trade History")
        ->AddButton("BackToWrench", "Back to Wrench")
        ->AddQuickExit()
        ->EndDialog("SocialPortal", "", "OK");

    SendOnDialogRequest(db.Get());
}

void GamePlayer::SendGuildMenu(const string& button)
{
    if(m_guildID == 0) {
        SendSocialPortal();
        return;
    }

    Guild* pGuild = GetGuildManager()->Get(m_guildID);
    if(!pGuild) {
        SendOnConsoleMessage("`4OOPS! ``Failed to load your guild data.");
        return;
    }

    GuildMember* pSelfMember = pGuild->GetMember(GetUserID());

    DialogBuilder db;
    db.SetDefaultColor('o')
        ->AddLabelWithIcon("`wGuild Menu``", ITEM_ID_GUILD_LOCK, true)
        ->AddSpacer()
        ->AddLabel("`3Guild Name: `w" + pGuild->GetName())
        ->AddLabel("`3Statement: `w" + pGuild->GetStatement())
        ->AddLabel("`3Level: `w" + ToString(pGuild->GetLevel()) + "`` `3XP: `w" + ToString(pGuild->GetXP()))
        ->AddLabel("`3Members: `w" + ToString((uint32)pGuild->GetMembers().size()))
        ->AddSpacer();

    for(const GuildMember& member : pGuild->GetMembers()) {
        string roleName = "Member";
        if(member.Position == GuildPosition::ELDER) {
            roleName = "Elder";
        }
        else if(member.Position == GuildPosition::CO_LEADER) {
            roleName = "Co-Leader";
        }
        else if(member.Position == GuildPosition::LEADER) {
            roleName = "Leader";
        }

        GamePlayer* pMember = GetGameServer()->GetPlayerByUserID(member.UserID);
        const string memberName = pMember ? pMember->GetRawName() : ("User #" + ToString(member.UserID));
        db.AddLabel("`o- ``" + memberName + " `8(" + roleName + ")``");

        if(pSelfMember && member.UserID != GetUserID()) {
            if(pSelfMember->Position == GuildPosition::LEADER) {
                if(member.Position != GuildPosition::LEADER) {
                    db.AddButton("GuildPromote_" + ToString(member.UserID), "`2Promote``");
                }
                if(member.Position != GuildPosition::MEMBER) {
                    db.AddButton("GuildDemote_" + ToString(member.UserID), "`4Demote``");
                }
                db.AddButton("GuildTransfer_" + ToString(member.UserID), "`9Transfer Leader``");
            }
            else if(pSelfMember->Position == GuildPosition::CO_LEADER) {
                if(member.Position == GuildPosition::MEMBER) {
                    db.AddButton("GuildPromote_" + ToString(member.UserID), "`2Promote``");
                }
                else if(member.Position == GuildPosition::ELDER) {
                    db.AddButton("GuildDemote_" + ToString(member.UserID), "`4Demote``");
                }
            }
        }
    }

    db.AddSpacer()
        ->AddButton("GotoSocialPortal", "Back")
        ->EndDialog("SocialPortal", "", "Close");

    SendOnDialogRequest(db.Get());
}

void GamePlayer::SendGuildDataChanged(Guild* pGuild)
{
    if(!pGuild) {
        pGuild = GetGuildManager()->Get(m_guildID);
    }

    if(!pGuild) {
        return;
    }

    GuildMember* pMember = pGuild->GetMember(GetUserID());
    if(!pMember) {
        return;
    }

    VariantVector data(5);
    data[0] = "OnGuildDataChanged";
    data[1] = pGuild->IsShowMascot() ? 1 : 0;
    data[2] = (int32)2;
    data[3] = (int32)GetItemInfoManager()->GetClientIDByItemID(pGuild->GetLogoForeground()) |
        ((int32)GetItemInfoManager()->GetClientIDByItemID(pGuild->GetLogoBackground()) << 16);
    data[4] = (int32)pMember->Position;

    SendCallFunctionPacket(data, GetNetID());
    SetGuildMascotEnabled(pGuild->IsShowMascot());
}

bool GamePlayer::CreateGuildFromPendingData()
{
    if(m_pendingGuildName.empty() || m_pendingGuildStatement.empty()) {
        return false;
    }

    if(m_guildID != 0 || GetLevel() < 20 || GetGems() < 500000) {
        return false;
    }

    if(!IsValidGuildName(m_pendingGuildName) || m_pendingGuildStatement.size() > 50) {
        return false;
    }

    World* pWorld = GetWorldManager()->GetWorldByID(GetCurrentWorld());
    if(!pWorld || !pWorld->IsPlayerWorldOwner(this)) {
        return false;
    }

    TileInfo* pWorldLockTile = pWorld->GetTileManager()->GetKeyTile(KEY_TILE_WORLD_LOCK);
    if(!pWorldLockTile || !pWorldLockTile->GetExtra<TileExtra_Lock>()) {
        return false;
    }

    const uint64 guildID = (Time::GetSystemTime() << 12) ^ GetUserID();
    Guild* pGuild = new Guild();
    pGuild->SetGuildID(guildID);
    pGuild->SetName(m_pendingGuildName);
    pGuild->SetStatement(m_pendingGuildStatement);
    pGuild->SetOwnerID(GetUserID());
    pGuild->SetLevel(1);
    pGuild->SetXP(0);
    pGuild->SetShowMascot(true);
    pGuild->SetWorldID(pWorld->GetID());
    pGuild->SetLogoForeground(ITEM_ID_BLANK);
    pGuild->SetLogoBackground(ITEM_ID_BLANK);
    pGuild->SetCreatedAt(Time::GetSystemTime());
    pGuild->AddMember({ GetUserID(), GuildPosition::LEADER });

    const string memberHex = SerializeGuildMembersHex(pGuild->GetMembers());

    QueryRequest insertGuildReq = MakeInsertGuildReq(
        pGuild->GetGuildID(),
        pGuild->GetName(),
        pGuild->GetStatement(),
        pGuild->GetNotebook(),
        pGuild->GetWorldID(),
        pGuild->GetOwnerID(),
        pGuild->GetLevel(),
        pGuild->GetXP(),
        pGuild->GetLogoForeground(),
        pGuild->GetLogoBackground(),
        pGuild->IsShowMascot(),
        pGuild->GetCreatedAt(),
        static_cast<uint32>(pGuild->GetMembers().size()),
        memberHex,
        GetNetID()
    );

    DatabaseGuildExec(GetContext()->GetDatabasePool(), DB_GUILD_INSERT, insertGuildReq);
    GetGuildManager()->Add(pGuild);

    SetGems(GetGems() - 500000);
    SendOnSetBux();
    SetGuildID(pGuild->GetGuildID());
    SetGuildMascotEnabled(true);

    pWorldLockTile->SetFG(ITEM_ID_GUILD_LOCK, pWorld->GetTileManager());
    TileExtra_Lock* pLockExtra = pWorldLockTile->GetExtra<TileExtra_Lock>();
    if(!pLockExtra) {
        return false;
    }

    pLockExtra->ownerID = (int32)GetUserID();
    pLockExtra->SetGuildID(pGuild->GetGuildID());
    pLockExtra->SetGuildLevel(pGuild->GetLevel());
    pLockExtra->SetGuildFG(pGuild->GetLogoForeground());
    pLockExtra->SetGuildBG(pGuild->GetLogoBackground());
    pLockExtra->SetGuildFlags(pGuild->IsShowMascot() ? 1 : 0);
    pWorld->SendTileUpdate(pWorldLockTile);

    SendOnTalkBubble("Guild Created", true);
    SendOnConsoleMessage("Guild Created!");

    SendGuildDataChanged(pGuild);

    m_pendingGuildName.clear();
    m_pendingGuildStatement.clear();
    return true;
}

void GamePlayer::AddPendingGuildInvite(uint64 guildID, Guild* pGuild)
{
    if(!pGuild) {
        return;
    }

    m_pendingGuildInvites[guildID] = pGuild;
}

void GamePlayer::RemovePendingGuildInvite(uint64 guildID)
{
    auto it = m_pendingGuildInvites.find(guildID);
    if(it != m_pendingGuildInvites.end()) {
        m_pendingGuildInvites.erase(it);
    }
}

Guild* GamePlayer::GetPendingGuildInvite(uint64 guildID) const
{
    auto it = m_pendingGuildInvites.find(guildID);
    if(it != m_pendingGuildInvites.end()) {
        return it->second;
    }

    return nullptr;
}

void GamePlayer::SendPendingGuildInviteDialog()
{
    if(m_pendingGuildInvites.empty()) {
        return;
    }

    DialogBuilder db;
    db.SetDefaultColor('o')
        ->AddLabelWithIcon("`wGuild Invitations``", ITEM_ID_FIST, true)
        ->AddSpacer();

    for(const auto& pair : m_pendingGuildInvites) {
        Guild* pGuild = pair.second;
        if(!pGuild) {
            continue;
        }

        db.AddLabel(fmt::format("`c{}``", pGuild->GetName()))
            ->AddLabel(fmt::format("`7{} members``, invited by guild leader", pGuild->GetMembers().size()))
            ->AddSpacer()
            ->AddButton("AcceptGuildInvite_" + std::to_string(pair.first), "`2Accept``")
            ->AddButton("RejectGuildInvite_" + std::to_string(pair.first), "`4Reject``")
            ->AddSpacer();
    }

    db.AddQuickExit()
        ->EndDialog("GuildInvitations", "OK", "Cancel");

    SendOnDialogRequest(db.Get());
}

void GamePlayer::HandleGuildInviteResponse(uint64 guildID, bool accepted)
{
    Guild* pGuild = GetPendingGuildInvite(guildID);
    if(!pGuild) {
        return;
    }

    if(accepted) {
        if(m_guildID != 0 || GetLevel() < 1) {
            SendOnConsoleMessage("`4Cannot join guild: already in a guild or insufficient level.");
            RemovePendingGuildInvite(guildID);
            return;
        }

        pGuild->RemovePendingInvite(GetUserID());
        pGuild->AddMember({GetUserID(), GuildPosition::MEMBER});
        SetGuildID(pGuild->GetGuildID());

        const string memberHex = SerializeGuildMembersHex(pGuild->GetMembers());
        QueryRequest updateGuildReq = MakeUpdateGuildReq(
            pGuild->GetGuildID(),
            pGuild->GetName(),
            pGuild->GetStatement(),
            pGuild->GetNotebook(),
            pGuild->GetWorldID(),
            pGuild->GetOwnerID(),
            pGuild->GetLevel(),
            pGuild->GetXP(),
            pGuild->GetLogoForeground(),
            pGuild->GetLogoBackground(),
            pGuild->IsShowMascot(),
            pGuild->GetCreatedAt(),
            static_cast<uint32>(pGuild->GetMembers().size()),
            memberHex,
            GetNetID()
        );

        DatabaseGuildExec(GetContext()->GetDatabasePool(), DB_GUILD_UPDATE, updateGuildReq);

        VariantVector joinNotif(4);
        joinNotif[0] = "OnGuildMemberJoined";
        joinNotif[1] = GetUserID();
        joinNotif[2] = GetDisplayName();
        joinNotif[3] = (int32)GuildPosition::MEMBER;

        World* pWorld = GetWorldManager()->GetWorldByID(pGuild->GetWorldID());
        if(pWorld) {
            for(GamePlayer* pMember : pWorld->GetPlayers()) {
                if(pMember && pMember->GetGuildID() == guildID) {
                    pMember->SendCallFunctionPacket(joinNotif, GetNetID());
                }
            }
        }

        SendOnConsoleMessage(fmt::format("`2You joined guild `c{}``", pGuild->GetName()));
        SendGuildDataChanged(pGuild);
    }
    else {
        pGuild->RemovePendingInvite(GetUserID());
        SendOnConsoleMessage(fmt::format("`4You rejected invitation from `c{}``", pGuild->GetName()));
    }

    RemovePendingGuildInvite(guildID);
}

void GamePlayer::SendWrenchOthers(GamePlayer* otherPlayer)
{
    if(!otherPlayer) {
        return;
    }

    if(otherPlayer == this) {
        SendWrenchSelf("PlayerInfo");
        return;
    }

    DialogBuilder db;
    db.SetDefaultColor('o')
        ->EmbedData("netID", otherPlayer->GetNetID())
        ->AddLabelWithIcon(otherPlayer->GetDisplayName(), ITEM_ID_WRENCH, false)
        ->AddSpacer()
        ->AddButton("trd_btn", "`oTrade``")
        ->AddButton("sendpm", "Private Message")
        ->AddButton("show_clothes", "Show Clothes");

    if(IsFriendWith(otherPlayer->GetUserID())) {
        db.AddButton("Unfriend", "Unfriend");
    }
    else {
        db.AddButton("add_btn", "`oAdd Friend");
    }

    db.AddButton("ignore_player", IsIgnoring(otherPlayer->GetUserID()) ? "Unignore" : "Ignore")
        ->AddButton("report_player", "Report");

    if(GetRole() && GetRole()->HasPerm(ROLE_PERM_COMMAND_PULL)) {
        db.AddButton("pull_btn", "`#Pull");
    }

    if(GetRole() && GetRole()->HasPerm(ROLE_PERM_COMMAND_KICK)) {
        db.AddButton("kick_btn", "`4Kick");
    }

    if(GetRole() && GetRole()->HasPerm(ROLE_PERM_COMMAND_BAN)) {
        db.AddButton("ban_btn", "`4Ban");
    }

    if(GetGuildID() != 0 && otherPlayer->GetGuildID() == 0) {
        db.AddButton("inv_to_kantut", "Invite To Guild");
    }

    db.AddSpacer()
        ->EndDialog("popup", "", "Continue")
        ->AddQuickExit();

    SendOnDialogRequest(db.Get());
}

void GamePlayer::SendFriendMenu(const string& action)
{
    if(action == "GotoSocialPortal") {
        SendSocialPortal();
        return;
    }

    if(action == "FriendsOptions") {
        DialogBuilder db;
        db.SetDefaultColor('o')
            ->AddLabelWithIcon("Friend Options", ITEM_ID_DUMB_FRIEND, true)
            ->AddSpacer()
            ->AddCheckBox("checkbox_notifications", "Show friend notifications", IsShowFriendNotification())
            ->AddSpacer()
            ->AddButton("GotoFriendsMenuAndApplyOptions", "Back")
            ->EndDialog("FriendMenu", "", "Close");

        SendOnDialogRequest(db.Get());
        return;
    }

    if(action == "SeeSent") {
        DialogBuilder db;
        db.SetDefaultColor('o')
            ->AddLabelWithIcon("`wSent Friend Requests``", ITEM_ID_DUMB_FRIEND, true)
            ->AddSpacer();

        if(m_sentFriendRequestUserIDs.empty()) {
            db.AddLabel("You have no pending sent friend requests.");
        }
        else {
            for(uint32 userID : m_sentFriendRequestUserIDs) {
                GamePlayer* pTarget = GetGameServer()->GetPlayerByUserID(userID);
                const string name = pTarget ? pTarget->GetDisplayName() : ("User #" + ToString(userID));
                db.AddLabel("`o" + name);
            }
        }

        db.AddSpacer()
            ->AddButton("GotoFriendsMenu", "Back")
            ->EndDialog("FriendMenu", "", "Close");

        SendOnDialogRequest(db.Get());
        return;
    }

    if(action == "SeeReceived") {
        DialogBuilder db;
        db.SetDefaultColor('o')
            ->AddLabelWithIcon("`wReceived Friend Requests``", ITEM_ID_DUMB_FRIEND, true)
            ->AddSpacer();

        if(m_receivedFriendRequestUserIDs.empty()) {
            db.AddLabel("You have no pending received friend requests.")
                ->AddSpacer();
        }
        else {
            for(uint32 userID : m_receivedFriendRequestUserIDs) {
                GamePlayer* pTarget = GetGameServer()->GetPlayerByUserID(userID);
                const string name = pTarget ? pTarget->GetDisplayName() : ("User #" + ToString(userID));

                db.AddLabel("`o" + name)
                    ->AddButton("friend_accept_" + ToString(userID), "Accept")
                    ->AddButton("friend_deny_" + ToString(userID), "Deny")
                    ->AddSpacer();
            }
        }

        db.AddButton("GotoFriendsMenu", "Back")
            ->EndDialog("FriendMenu", "", "Close");

        SendOnDialogRequest(db.Get());
        return;
    }

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
      ->AddButton("FriendsOptions", "Friend Options")
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

bool GamePlayer::IsShowFriendNotification() const
{
    auto it = m_stats.find("ShowFriendNotification");
    if(it == m_stats.end()) {
        return true;
    }

    return it->second != 0;
}

void GamePlayer::SetShowFriendNotification(bool enabled)
{
    m_stats["ShowFriendNotification"] = enabled ? 1 : 0;
}

bool GamePlayer::ShouldProcessFriendAlert(uint32 sourceUserID, bool loggedIn, uint64 nowMS)
{
    if(sourceUserID == 0) {
        return true;
    }

    const uint64 key = ((uint64)sourceUserID << 1) | (loggedIn ? 1ull : 0ull);
    auto it = m_friendAlertDebounce.find(key);
    if(it != m_friendAlertDebounce.end()) {
        const uint64 elapsed = nowMS >= it->second ? (nowMS - it->second) : 0;
        if(elapsed < kFriendAlertDebounceWindowMS) {
            return false;
        }
    }

    m_friendAlertDebounce[key] = nowMS;
    return true;
}

void GamePlayer::NotifyFriendsStatusChange(bool loggedIn)
{
    const string myName = GetRawName();
    if(myName.empty()) {
        return;
    }

    const string consoleMsg = loggedIn
        ? "`3FRIEND ALERT : `w" + myName + " `ohas `2logged on`o."
        : "`!FRIEND ALERT : `w" + myName + " `ohas `4logged off`o.";

    for(uint32 friendUserID : m_friendUserIDs) {
        if(friendUserID == 0) {
            continue;
        }

        GamePlayer* pLocalFriend = GetGameServer()->GetPlayerByUserID(friendUserID);
        if(pLocalFriend && pLocalFriend->HasState(PLAYER_STATE_IN_GAME) && pLocalFriend->IsShowFriendNotification() &&
            pLocalFriend->ShouldProcessFriendAlert(GetUserID(), loggedIn, Time::GetSystemTime())) {
            pLocalFriend->PlaySFX(loggedIn ? "friend_logon.wav" : "friend_logoff.wav", 2000);
            pLocalFriend->SendOnConsoleMessage(consoleMsg);
        }

        GetMasterBroadway()->SendCrossServerActionRequest(
            loggedIn ? TCP_CROSS_ACTION_FRIEND_LOGIN : TCP_CROSS_ACTION_FRIEND_LOGOUT,
            GetUserID(),
            myName,
            ToString(friendUserID),
            true,
            "",
            0
        );
    }
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
    SyncSocialDataToStats();
    otherPlayer->SyncSocialDataToStats();
    SaveSocialDataToDatabase();
    otherPlayer->SaveSocialDataToDatabase();

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
        pOther->SyncSocialDataToStats();
        pOther->SaveSocialDataToDatabase();
    }

    SyncSocialDataToStats();
    SaveSocialDataToDatabase();
}

bool GamePlayer::AddFriendUserID(uint32 userID)
{
    if(userID == 0 || userID == GetUserID()) {
        return false;
    }

    const bool added = m_friendUserIDs.insert(userID).second;
    if(added) {
        SyncSocialDataToStats();
        SaveSocialDataToDatabase();
    }

    return added;
}

bool GamePlayer::RemoveFriendUserID(uint32 userID)
{
    if(userID == 0) {
        return false;
    }

    const bool removed = m_friendUserIDs.erase(userID) > 0;
    if(removed) {
        SyncSocialDataToStats();
        SaveSocialDataToDatabase();
    }

    return removed;
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

    const bool added = m_ignoredUserIDs.insert(userID).second;
    if(added) {
        SyncSocialDataToStats();
        SaveSocialDataToDatabase();
    }

    return added;
}

bool GamePlayer::RemoveIgnoredUserID(uint32 userID)
{
    if(userID == 0) {
        return false;
    }

    const bool removed = m_ignoredUserIDs.erase(userID) > 0;
    if(removed) {
        SyncSocialDataToStats();
        SaveSocialDataToDatabase();
    }

    return removed;
}

void GamePlayer::SyncSocialDataToStats()
{
    auto shouldErase = [](const string& key) {
        return key.rfind(kFriendStatPrefix, 0) == 0 ||
            key.rfind(kFriendReqOutPrefix, 0) == 0 ||
            key.rfind(kFriendReqInPrefix, 0) == 0 ||
            key.rfind(kIgnoredStatPrefix, 0) == 0;
    };

    for(auto it = m_stats.begin(); it != m_stats.end();) {
        if(shouldErase(it->first)) {
            it = m_stats.erase(it);
        }
        else {
            ++it;
        }
    }

    for(uint32 userID : m_friendUserIDs) {
        if(userID != 0) {
            m_stats[string(kFriendStatPrefix) + ToString(userID)] = 1;
        }
    }

    for(uint32 userID : m_sentFriendRequestUserIDs) {
        if(userID != 0) {
            m_stats[string(kFriendReqOutPrefix) + ToString(userID)] = 1;
        }
    }

    for(uint32 userID : m_receivedFriendRequestUserIDs) {
        if(userID != 0) {
            m_stats[string(kFriendReqInPrefix) + ToString(userID)] = 1;
        }
    }

    for(uint32 userID : m_ignoredUserIDs) {
        if(userID != 0) {
            m_stats[string(kIgnoredStatPrefix) + ToString(userID)] = 1;
        }
    }
}

void GamePlayer::LoadSocialDataFromStats()
{
    m_friendUserIDs.clear();
    m_sentFriendRequestUserIDs.clear();
    m_receivedFriendRequestUserIDs.clear();
    m_ignoredUserIDs.clear();

    for(const auto& entry : m_stats) {
        uint32 userID = 0;

        if(entry.first.rfind(kFriendStatPrefix, 0) == 0) {
            if(ToUInt(entry.first.substr(strlen(kFriendStatPrefix)), userID) == TO_INT_SUCCESS && userID != 0 && userID != GetUserID()) {
                m_friendUserIDs.insert(userID);
            }
            continue;
        }

        if(entry.first.rfind(kFriendReqOutPrefix, 0) == 0) {
            if(ToUInt(entry.first.substr(strlen(kFriendReqOutPrefix)), userID) == TO_INT_SUCCESS && userID != 0 && userID != GetUserID()) {
                m_sentFriendRequestUserIDs.insert(userID);
            }
            continue;
        }

        if(entry.first.rfind(kFriendReqInPrefix, 0) == 0) {
            if(ToUInt(entry.first.substr(strlen(kFriendReqInPrefix)), userID) == TO_INT_SUCCESS && userID != 0 && userID != GetUserID()) {
                m_receivedFriendRequestUserIDs.insert(userID);
            }
            continue;
        }

        if(entry.first.rfind(kIgnoredStatPrefix, 0) == 0) {
            if(ToUInt(entry.first.substr(strlen(kIgnoredStatPrefix)), userID) == TO_INT_SUCCESS && userID != 0 && userID != GetUserID()) {
                m_ignoredUserIDs.insert(userID);
            }
        }
    }
}

void GamePlayer::SaveSocialDataToDatabase()
{
    if(m_userID == 0) {
        return;
    }

    DatabasePool* pPool = GetContext()->GetDatabasePool();
    const int32 ownerID = GetNetID();

    QueryRequest deleteFriendsReq = MakeDeletePlayerFriendsReq(m_userID, ownerID);
    DatabasePlayerExec(pPool, DB_PLAYER_DELETE_FRIENDS, deleteFriendsReq);

    for(uint32 friendUserID : m_friendUserIDs) {
        if(friendUserID == 0 || friendUserID == m_userID) {
            continue;
        }

        QueryRequest insertFriendReq = MakeInsertPlayerFriendReq(m_userID, friendUserID, ownerID);
        DatabasePlayerExec(pPool, DB_PLAYER_INSERT_FRIEND, insertFriendReq);
    }

    QueryRequest deleteFriendRequestsReq = MakeDeletePlayerFriendRequestsReq(m_userID, ownerID);
    DatabasePlayerExec(pPool, DB_PLAYER_DELETE_FRIEND_REQUESTS, deleteFriendRequestsReq);

    for(uint32 targetUserID : m_sentFriendRequestUserIDs) {
        if(targetUserID == 0 || targetUserID == m_userID) {
            continue;
        }

        QueryRequest insertRequestReq = MakeInsertPlayerFriendRequestReq(m_userID, targetUserID, ownerID);
        DatabasePlayerExec(pPool, DB_PLAYER_INSERT_FRIEND_REQUEST, insertRequestReq);
    }

    QueryRequest deleteIgnoresReq = MakeDeletePlayerIgnoresReq(m_userID, ownerID);
    DatabasePlayerExec(pPool, DB_PLAYER_DELETE_IGNORES, deleteIgnoresReq);

    for(uint32 ignoredUserID : m_ignoredUserIDs) {
        if(ignoredUserID == 0 || ignoredUserID == m_userID) {
            continue;
        }

        QueryRequest insertIgnoreReq = MakeInsertPlayerIgnoreReq(m_userID, ignoredUserID, ownerID);
        DatabasePlayerExec(pPool, DB_PLAYER_INSERT_IGNORE, insertIgnoreReq);
    }
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
    ePlayModType mutePlayModType = PLAYMOD_TYPE_NONE;
    for(uint32 i = 1; i <= 64; ++i) {
        PlayMod* pPlayMod = GetPlayModManager()->GetPlayMod((ePlayModType)i);
        if(!pPlayMod) {
            continue;
        }

        if(ToLower(pPlayMod->GetName()) == "mute") {
            mutePlayModType = (ePlayModType)i;
            break;
        }
    }

    m_mutedUntilMS = untilMS;
    m_muteReason = reason;

    if(mutePlayModType != PLAYMOD_TYPE_NONE && !m_characterData.HasPlayMod(mutePlayModType)) {
        AddPlayMod(mutePlayModType, true);
    }
}

void GamePlayer::ClearMute()
{
    ePlayModType mutePlayModType = PLAYMOD_TYPE_NONE;
    for(uint32 i = 1; i <= 64; ++i) {
        PlayMod* pPlayMod = GetPlayModManager()->GetPlayMod((ePlayModType)i);
        if(!pPlayMod) {
            continue;
        }

        if(ToLower(pPlayMod->GetName()) == "mute") {
            mutePlayModType = (ePlayModType)i;
            break;
        }
    }

    m_mutedUntilMS = 0;
    m_muteReason.clear();

    if(mutePlayModType != PLAYMOD_TYPE_NONE && m_characterData.HasPlayMod(mutePlayModType)) {
        RemovePlayMod(mutePlayModType, true);
    }
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

    LoadSocialDataFromStats();
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

uint64 GamePlayer::GetCustomStatValue(const std::string& statName) const
{
    return GetStatCount(statName);
}

void GamePlayer::SetCustomStatValue(const std::string& statName, uint64 value)
{
    if(statName.empty()) {
        return;
    }

    if(value == 0) {
        m_stats.erase(statName);
        return;
    }

    m_stats[statName] = value;
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

void GamePlayer::ResetGauntletSwapState()
{
    m_gauntletSwapIndex[0] = -1;
    m_gauntletSwapIndex[1] = -1;
    m_gauntletAvailableSwap.clear();
}

void GamePlayer::SetGauntletSwapIndex(int32 slot, int32 tileIndex)
{
    if(slot < 0 || slot > 1) {
        return;
    }

    m_gauntletSwapIndex[slot] = tileIndex;
}

int32 GamePlayer::GetGauntletSwapIndex(int32 slot) const
{
    if(slot < 0 || slot > 1) {
        return -1;
    }

    return m_gauntletSwapIndex[slot];
}

void GamePlayer::SetGauntletAvailableSwap(const std::vector<int32>& tiles)
{
    m_gauntletAvailableSwap = tiles;
}

void GamePlayer::OnHandleDatabase(QueryTaskResult&& result)
{
    if(!result.extraData.empty()) {
        const int32 subType = result.extraData[0].GetINT();
        if(subType == PLAYER_SUB_PBAN_BY_PREFIX || subType == PLAYER_SUB_PBAN_BY_USERID) {
            HandlePBanRequestResult(std::move(result));
            result.Destroy();
            return;
        }

        if(subType == PLAYER_SUB_SOCIAL_LOAD) {
            HandleSocialDataLoad(std::move(result));
            result.Destroy();
            return;
        }

        if(subType == PLAYER_SUB_GUILD_LOAD) {
            HandleGuildDataLoad(std::move(result));
            result.Destroy();
            return;
        }
    }

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

    m_switchingSubserver = false;
    m_accountDataLoaded = false;
    m_loadedAccountVersionEpochSec = 0;

    m_userID = m_loginDetail.user;
    LogAccountFlow("login_start", m_userID, m_userID, 0, "session", "session_validating");
    SetState(PLAYER_STATE_CHECKING_SESSION);
    GetMasterBroadway()->SendCheckSessionPacket(GetNetID(), m_loginDetail.user, m_loginDetail.token, GetContext()->GetID());
}

void GamePlayer::CheckingLoginSession(VariantVector&& result)
{
    RemoveState(PLAYER_STATE_CHECKING_SESSION);

    bool sessionAgreed = result[2].GetBool();
    if(!sessionAgreed) {
        SendLogonFailWithLog("`oServer requesting you relog-on");
        return;
    }

    SetState(PLAYER_STATE_LOGIN_GETTING_ACCOUNT);

    QueryRequest req = MakeGetPlayerDataReq(m_userID, GetNetID());
    DatabasePlayerExec(GetContext()->GetDatabasePool(), DB_PLAYER_GET_DATA, req);
}

void GamePlayer::LoadingAccount(QueryTaskResult&& result)
{
    RemoveState(PLAYER_STATE_LOGIN_GETTING_ACCOUNT);
    if(!result.result || result.result->GetRowCount() == 0) {
        m_accountDataLoaded = false;
        m_loadedAccountVersionEpochSec = 0;
        LogAccountFlow("profile_loaded", m_userID, m_userID, 0, "db", "missing_row");
        SendLogonFailWithLog("`4OOPS! ``Failed to load your account, please re-connect");
        return;
    }

    uint32 roleID = 0;
    if(!TryReadUIntField(result.result, "RoleID", roleID)) {
        m_accountDataLoaded = false;
        m_loadedAccountVersionEpochSec = 0;
        LogAccountFlow("profile_loaded", m_userID, m_userID, 0, "db", "invalid_role_type");
        SendLogonFailWithLog("`4OOPS! ``Failed to load your account, please re-connect");
        return;
    }

    if(roleID == 0) {
        roleID = GetRoleManager()->GetDefaultRoleID();
    }

    if(!TryReadIntField(result.result, "Gems", m_gems)) {
        m_accountDataLoaded = false;
        m_loadedAccountVersionEpochSec = 0;
        LogAccountFlow("profile_loaded", m_userID, m_userID, 0, "db", "invalid_gems_type");
        SendLogonFailWithLog("`4OOPS! ``Failed to load your account, please re-connect");
        return;
    }

    if(m_gems < 0) {
        m_gems = 0;
    }

    if(!TryReadUIntField(result.result, "Level", m_level)) {
        m_accountDataLoaded = false;
        m_loadedAccountVersionEpochSec = 0;
        LogAccountFlow("profile_loaded", m_userID, m_userID, 0, "db", "invalid_level_type");
        SendLogonFailWithLog("`4OOPS! ``Failed to load your account, please re-connect");
        return;
    }

    if(m_level == 0) {
        m_level = 1;
    }

    if(!TryReadUIntField(result.result, "XP", m_xp)) {
        m_accountDataLoaded = false;
        m_loadedAccountVersionEpochSec = 0;
        LogAccountFlow("profile_loaded", m_userID, m_userID, 0, "db", "invalid_xp_type");
        SendLogonFailWithLog("`4OOPS! ``Failed to load your account, please re-connect");
        return;
    }

    uint32 loadedVersion = 0;
    if(!TryReadUIntField(result.result, "LastSeenVersion", loadedVersion)) {
        m_accountDataLoaded = false;
        m_loadedAccountVersionEpochSec = 0;
        LogAccountFlow("profile_loaded", m_userID, m_userID, 0, "db", "invalid_version_type");
        SendLogonFailWithLog("`4OOPS! ``Failed to load your account, please re-connect");
        return;
    }
    m_loadedAccountVersionEpochSec = (uint64)loadedVersion;

    m_dailyRewardStreak = result.result->GetField("DailyRewardStreak", 0).GetUINT();
    m_dailyRewardClaimDay = result.result->GetField("DailyRewardClaimDay", 0).GetUINT();
    m_lastClaimDailyRewardMs = (uint64)result.result->GetField("LastClaimDailyReward", 0).GetUINT();
    m_guildID = (uint64)result.result->GetField("GuildID", 0).GetUINT();
    m_showLocationToGuild = result.result->GetField("ShowLocationToGuild", 0).GetUINT() != 0;
    m_showGuildNotification = result.result->GetField("ShowGuildNotification", 0).GetUINT() != 0;
    m_titleShowPrefix = result.result->GetField("TitleShowPrefix", 0).GetUINT() != 0;
    m_titlePermLegend = result.result->GetField("TitlePermLegend", 0).GetUINT() != 0;
    m_titlePermGrow4Good = result.result->GetField("TitlePermGrow4Good", 0).GetUINT() != 0;
    m_titlePermMVP = result.result->GetField("TitlePermMVP", 0).GetUINT() != 0;
    m_titlePermVIP = result.result->GetField("TitlePermVIP", 0).GetUINT() != 0;
    m_titleEnabledLegend = result.result->GetField("TitleEnabledLegend", 0).GetUINT() != 0;
    m_titleEnabledGrow4Good = result.result->GetField("TitleEnabledGrow4Good", 0).GetUINT() != 0;
    m_titleEnabledMVP = result.result->GetField("TitleEnabledMVP", 0).GetUINT() != 0;
    m_titleEnabledVIP = result.result->GetField("TitleEnabledVIP", 0).GetUINT() != 0;
    if(!m_titlePermLegend) {
        m_titleEnabledLegend = false;
    }
    if(!m_titlePermGrow4Good) {
        m_titleEnabledGrow4Good = false;
    }
    if(!m_titlePermMVP) {
        m_titleEnabledMVP = false;
    }
    if(!m_titlePermVIP) {
        m_titleEnabledVIP = false;
    }

    m_pRole = GetRoleManager()->GetRole(roleID);

    if(!m_pRole) {
        SendLogonFailWithLog("`4OOPS! ``Something went wrong while setting you up, please re-connect");
        LOGGER_LOG_WARN("Failed to set player role %d for user %d", roleID, m_userID);
        return;
    }

    GetGameServer()->SetPlayerNameCache(m_userID, GetRawName());

    if(GetGameServer()->IsMaintenance() && !m_pRole->HasPerm(ROLE_PERM_MAINTENANCE_EXCEPTION)) {
        SendLogonFailWithLog(BuildMaintenanceLoginMessage(GetRawName()));
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

    const uint64 pbanActive = GetCustomStatValue(kPBanStatActive);
    const uint64 pbanUntil = GetCustomStatValue(kPBanStatUntil);
    const bool isPermanentlyBanned = pbanUntil >= (uint64)INT32_MAX;
    const uint64 nowEpochSec = Time::GetTimeSinceEpoch();
    if(pbanActive != 0) {
        if(isPermanentlyBanned || nowEpochSec < pbanUntil) {
            const uint64 remainingSeconds = isPermanentlyBanned ? 0 : (pbanUntil - nowEpochSec);
            SendLogonFailWithLog(BuildSuspendedLoginMessage(GetRawName(), remainingSeconds, isPermanentlyBanned));
            return;
        }

        SetCustomStatValue(kPBanStatActive, 0);
        SetCustomStatValue(kPBanStatUntil, 0);
        SetCustomStatValue(kPBanStatSetAt, 0);
        SetCustomStatValue(kPBanStatDuration, 0);
        SetCustomStatValue(kPBanStatPerma, 0);
    }

    m_guestID = result.result->GetField("GuestID", 0).GetUINT();
    m_accountDataLoaded = true;
    LogAccountFlow("profile_loaded", m_userID, m_userID, m_loadedAccountVersionEpochSec, "db", "hydrate_success");

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

    if(m_guildID != 0) {
        RequestGuildDataLoad();
    }
    else {
        RequestSocialDataLoad();
    }
}

void GamePlayer::RequestGuildDataLoad()
{
    if(m_guildID == 0) {
        RequestSocialDataLoad();
        return;
    }

    if(GetGuildManager()->Get(m_guildID)) {
        RequestSocialDataLoad();
        return;
    }

    SetState(PLAYER_STATE_LOADING_GUILD);

    QueryRequest req = MakeGetGuildByIDReq(m_guildID, GetNetID());
    req.extraData.resize(1);
    req.extraData[0] = PLAYER_SUB_GUILD_LOAD;
    DatabaseGuildExec(GetContext()->GetDatabasePool(), DB_GUILD_GET_BY_ID, req);
}

void GamePlayer::HandleGuildDataLoad(QueryTaskResult&& result)
{
    RemoveState(PLAYER_STATE_LOADING_GUILD);

    if(!result.result || result.result->GetRowCount() == 0) {
        m_guildID = 0;
        RequestSocialDataLoad();
        return;
    }

    Guild* pGuild = GetGuildManager()->Get(m_guildID);
    const bool newlyAllocated = (pGuild == nullptr);
    if(!pGuild) {
        pGuild = new Guild();
    }

    pGuild->SetGuildID((uint64)result.result->GetField("ID", 0).GetUINT());
    pGuild->SetName(result.result->GetField("Name", 0).GetString());
    pGuild->SetStatement(result.result->GetField("Statement", 0).GetString());
    pGuild->SetNotebook(result.result->GetField("Notebook", 0).GetString());
    pGuild->SetWorldID(result.result->GetField("WorldID", 0).GetUINT());
    pGuild->SetOwnerID(result.result->GetField("OwnerID", 0).GetUINT());
    pGuild->SetLevel(result.result->GetField("Level", 0).GetUINT());
    pGuild->SetXP(result.result->GetField("XP", 0).GetUINT());
    pGuild->SetLogoForeground((uint16)result.result->GetField("LogoFG", 0).GetUINT());
    pGuild->SetLogoBackground((uint16)result.result->GetField("LogoBG", 0).GetUINT());
    pGuild->SetShowMascot(result.result->GetField("ShowMascot", 0).GetUINT() != 0);
    pGuild->SetCreatedAt((uint64)result.result->GetField("CreatedAt", 0).GetUINT());
    pGuild->SetMembers(ParseGuildMembersHex(result.result->GetField("MemberDataHex", 0).GetString()));

    if(newlyAllocated) {
        if(!GetGuildManager()->Add(pGuild)) {
            SAFE_DELETE(pGuild);
        }
    }

    RequestSocialDataLoad();
}

void GamePlayer::RequestSocialDataLoad()
{
    SetState(PLAYER_STATE_LOADING_SOCIAL);

    QueryRequest req = MakeGetPlayerSocialDataReq(m_userID, GetNetID());
    req.extraData.resize(1);
    req.extraData[0] = PLAYER_SUB_SOCIAL_LOAD;
    DatabasePlayerExec(GetContext()->GetDatabasePool(), DB_PLAYER_GET_SOCIAL_DATA, req);
}

void GamePlayer::HandleSocialDataLoad(QueryTaskResult&& result)
{
    RemoveState(PLAYER_STATE_LOADING_SOCIAL);

    const bool hadLegacySocialData =
        !m_friendUserIDs.empty() ||
        !m_sentFriendRequestUserIDs.empty() ||
        !m_receivedFriendRequestUserIDs.empty() ||
        !m_ignoredUserIDs.empty();

    bool loadedFromTable = false;
    if(result.result) {
        const uint32 rowCount = result.result->GetRowCount();

        m_friendUserIDs.clear();
        m_sentFriendRequestUserIDs.clear();
        m_receivedFriendRequestUserIDs.clear();
        m_ignoredUserIDs.clear();

        for(uint32 i = 0; i < rowCount; ++i) {
            const uint32 relationType = result.result->GetField("relation_type", i).GetUINT();
            const uint32 sourceUserID = result.result->GetField("source_user_id", i).GetUINT();
            const uint32 targetUserID = result.result->GetField("target_user_id", i).GetUINT();

            if(relationType == 1) {
                if(targetUserID != 0 && targetUserID != GetUserID()) {
                    m_friendUserIDs.insert(targetUserID);
                }
                continue;
            }

            if(relationType == 2) {
                if(sourceUserID == GetUserID() && targetUserID != 0 && targetUserID != GetUserID()) {
                    m_sentFriendRequestUserIDs.insert(targetUserID);
                }
                else if(targetUserID == GetUserID() && sourceUserID != 0 && sourceUserID != GetUserID()) {
                    m_receivedFriendRequestUserIDs.insert(sourceUserID);
                }
                continue;
            }

            if(relationType == 3) {
                if(targetUserID != 0 && targetUserID != GetUserID()) {
                    m_ignoredUserIDs.insert(targetUserID);
                }
            }
        }

        loadedFromTable = rowCount > 0;
    }

    if(!loadedFromTable && hadLegacySocialData) {
        SaveSocialDataToDatabase();
    }

    SyncSocialDataToStats();

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
    if(!m_accountDataLoaded || m_userID == 0) {
        LogAccountFlow("save_blocked_pre_hydration", m_userID, m_userID, m_loadedAccountVersionEpochSec, "db", "not_hydrated_or_invalid_user");
        return;
    }

    if(m_loadedAccountVersionEpochSec == 0) {
        LogAccountFlow("save_blocked_pre_hydration", m_userID, m_userID, 0, "db", "missing_loaded_version");
        return;
    }

    SyncSocialDataToStats();

    uint32 invMemSize = m_inventory.GetMemEstimate(true);
    uint8* pInvData = new uint8[invMemSize];

    MemoryBuffer invMemBuffer(pInvData, invMemSize);
    m_inventory.Serialize(invMemBuffer, true, true);

    const int32 roleID = m_pRole ? m_pRole->GetID() : static_cast<int32>(GetRoleManager()->GetDefaultRoleID());

    QueryRequest req = MakeSavePlayerReq(
        m_userID,
        roleID,
        ToHex(pInvData, invMemSize),
        m_gems,
        m_level,
        m_xp,
        SerializeAchievements(),
        SerializeStatistics(),
        m_dailyRewardStreak,
        m_dailyRewardClaimDay,
        m_lastClaimDailyRewardMs,
        m_guildID,
        m_showLocationToGuild,
        m_showGuildNotification,
        m_titleShowPrefix,
        m_titlePermLegend,
        m_titlePermGrow4Good,
        m_titlePermMVP,
        m_titlePermVIP,
        m_titleEnabledLegend,
        m_titleEnabledGrow4Good,
        m_titleEnabledMVP,
        m_titleEnabledVIP,
        m_loadedAccountVersionEpochSec,
        GetNetID()
    );
    
    DatabasePlayerExec(GetContext()->GetDatabasePool(), DB_PLAYER_SAVE, req);
    m_loadedAccountVersionEpochSec = std::max<uint64>(m_loadedAccountVersionEpochSec, Time::GetTimeSinceEpoch());
    LogAccountFlow("save_committed", m_userID, m_userID, m_loadedAccountVersionEpochSec, "db", "optimistic_guarded_update");

    if(m_guildID != 0) {
        Guild* pGuild = GetGuildManager()->Get(m_guildID);
        if(pGuild) {
            const string memberHex = SerializeGuildMembersHex(pGuild->GetMembers());

            QueryRequest updateGuildReq = MakeUpdateGuildReq(
                pGuild->GetGuildID(),
                pGuild->GetName(),
                pGuild->GetStatement(),
                pGuild->GetNotebook(),
                pGuild->GetWorldID(),
                pGuild->GetOwnerID(),
                pGuild->GetLevel(),
                pGuild->GetXP(),
                pGuild->GetLogoForeground(),
                pGuild->GetLogoBackground(),
                pGuild->IsShowMascot(),
                pGuild->GetCreatedAt(),
                static_cast<uint32>(pGuild->GetMembers().size()),
                memberHex,
                GetNetID()
            );

            DatabaseGuildExec(GetContext()->GetDatabasePool(), DB_GUILD_UPDATE, updateGuildReq);
        }
    }

    SaveSocialDataToDatabase();
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
    if(m_loggingOff) {
        return;
    }

    bool isInGame = HasState(PLAYER_STATE_IN_GAME);
    bool switchingSubserver = m_switchingSubserver;
    const bool canSaveAccount = m_userID != 0 && m_accountDataLoaded;

    if(IsTrading()) {
        CancelTrade(false);
    }

    // Persist first on disconnect so runtime cleanup cannot cause account rollback.
    if(canSaveAccount) {
        SaveToDatabase();
    }

    if(isInGame && !switchingSubserver) {
        NotifyFriendsStatusChange(false);
    }

    RemoveState(PLAYER_STATE_IN_GAME);
    m_loggingOff = true;
    ResetGauntletSwapState();

    if(m_currentWorldID != 0) {
        World* pWorld = GetWorldManager()->GetWorldByID(m_currentWorldID);
        if(pWorld) {
            pWorld->PlayerLeaverWorld(this);
        }
    }

    SendENetPacket(NET_MESSAGE_GAME_MESSAGE, "action|log_off\n", m_pPeer);
    if(!switchingSubserver) {
        GetMasterBroadway()->SendEndPlayerSession(m_userID);
    }
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

bool GamePlayer::IsPrefixEnabled() const
{
    return m_titleShowPrefix;
}

void GamePlayer::SetPrefixEnabled(bool enabled)
{
    m_titleShowPrefix = enabled;
}

bool GamePlayer::IsLegend() const
{
    return m_titlePermLegend;
}

void GamePlayer::SetIsLegend(bool enabled)
{
    m_titlePermLegend = enabled;
    if(!m_titlePermLegend) {
        m_titleEnabledLegend = false;
    }
}

bool GamePlayer::IsGrow4Good() const
{
    return m_titlePermGrow4Good;
}

void GamePlayer::SetIsGrow4Good(bool enabled)
{
    m_titlePermGrow4Good = enabled;
    if(!m_titlePermGrow4Good) {
        m_titleEnabledGrow4Good = false;
    }
}

bool GamePlayer::IsMVP() const
{
    return m_titlePermMVP;
}

void GamePlayer::SetIsMVP(bool enabled)
{
    m_titlePermMVP = enabled;
    if(!m_titlePermMVP) {
        m_titleEnabledMVP = false;
    }
}

bool GamePlayer::IsVIP() const
{
    return m_titlePermVIP;
}

void GamePlayer::SetIsVIP(bool enabled)
{
    m_titlePermVIP = enabled;
    if(!m_titlePermVIP) {
        m_titleEnabledVIP = false;
    }
}

bool GamePlayer::IsLegendaryTitleEnabled()
{
    if(!m_titlePermLegend) {
        m_titleEnabledLegend = false;
        return false;
    }

    return m_titleEnabledLegend;
}

void GamePlayer::SetLegendaryTitleEnabled(bool enabled)
{
    m_titleEnabledLegend = m_titlePermLegend && enabled;
}

bool GamePlayer::IsGrow4GoodTitleEnabled()
{
    if(!m_titlePermGrow4Good) {
        m_titleEnabledGrow4Good = false;
        return false;
    }

    return m_titleEnabledGrow4Good;
}

void GamePlayer::SetGrow4GoodTitleEnabled(bool enabled)
{
    m_titleEnabledGrow4Good = m_titlePermGrow4Good && enabled;
}

bool GamePlayer::IsMaxLevelTitleEnabled()
{
    if(!m_titlePermMVP) {
        m_titleEnabledMVP = false;
        return false;
    }

    return m_titleEnabledMVP;
}

void GamePlayer::SetMaxLevelTitleEnabled(bool enabled)
{
    m_titleEnabledMVP = m_titlePermMVP && enabled;
}

bool GamePlayer::IsSuperSupporterTitleEnabled()
{
    if(!m_titlePermVIP) {
        m_titleEnabledVIP = false;
        return false;
    }

    return m_titleEnabledVIP;
}

void GamePlayer::SetSuperSupporterTitleEnabled(bool enabled)
{
    m_titleEnabledVIP = m_titlePermVIP && enabled;
}

string GamePlayer::GetDisplayName()
{
    string displayName;
    Role* pRole = m_pRole ? m_pRole : GetRoleManager()->GetRole(GetRoleManager()->GetDefaultRoleID());

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

    if(pRole && pRole->GetNameColor() != 0) {
        displayName += "`"; 
        displayName += pRole->GetNameColor();
    }

    if(pRole && IsPrefixEnabled()) {
        displayName += pRole->GetPrefix();
    }

    if(HasNickname()) {
        displayName += m_nickname;
    }
    else if(!m_loginDetail.tankIDName.empty()) {
        displayName += m_loginDetail.tankIDName;
    }
    else {
        displayName += m_loginDetail.requestedName + "_" + ToString(m_guestID);
    }

    if(IsLegendaryTitleEnabled()) {
        displayName += " of Legend";
    }

    if(pRole) {
        displayName += pRole->GetSuffix();
    }
    return displayName;
}

string GamePlayer::GetRawName()
{
    if(HasNickname()) {
        return m_nickname;
    }

    return m_loginDetail.tankIDName.empty() ? 
        m_loginDetail.requestedName + "_" + ToString(m_guestID)
        : m_loginDetail.tankIDName;
}

string GamePlayer::GetSpawnData(bool local)
{
    string spawnData;
    Role* pRole = m_pRole ? m_pRole : GetRoleManager()->GetRole(GetRoleManager()->GetDefaultRoleID());
    spawnData += "spawn|avatar\n";
    spawnData += "netID|" + ToString(GetNetID()) + "\n";
    spawnData += "userID|" + ToString(m_userID) + "\n";
    spawnData += "colrect|0|0|20|30\n"; //its ok to hardcoded (for now?)
    spawnData += "posXY|" + ToString(m_worldPos.x)  + "|" + ToString(m_worldPos.y) + "\n"; 
    spawnData += "name|" + GetDisplayName() + "``\n";
    spawnData += "country|" + m_loginDetail.country + "\n";
    spawnData += "invis|0\n"; // todo
    spawnData += "mstate|";
    spawnData += (pRole && pRole->HasPerm(ROLE_PERM_MSTATE)) ? "1\n" : "0\n";
    spawnData += "smstate|" ;
    spawnData += (pRole && pRole->HasPerm(ROLE_PERM_SMSTATE)) ? "1\n" : "0\n";
    spawnData += "onlineID|\n";

    if(local) {
        spawnData += "type|local\n";
    }

    return spawnData;
}

bool GamePlayer::SetRoleByID(int32 roleID)
{
    Role* pRole = GetRoleManager()->GetRole(roleID);
    if(!pRole) {
        return false;
    }

    m_pRole = pRole;
    return true;
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
        if(pWorld) {
            pWorld->SendClothUpdateToAll(this);
            pWorld->PlaySFXForEveryone("change_clothes.wav", 0);
        }
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

void GamePlayer::BeginPBanRequest(const string& target, int32 durationSec, const string& reason)
{
    if(target.empty()) {
        SendOnConsoleMessage("`4Oops:`` Usage: /pban <target|#userid> <seconds|-1> <reason>");
        return;
    }

    if(m_pendingPBan.active) {
        SendOnConsoleMessage("`oPlease wait, previous /pban lookup is still processing.``");
        return;
    }

    m_pendingPBan.active = true;
    m_pendingPBan.durationSec = durationSec;
    m_pendingPBan.reason = reason;
    m_pendingPBan.target = target;

    uint32 targetUserID = 0;
    if(TryParsePBanTargetUserID(target, targetUserID)) {
        GamePlayer* pLocalTarget = GetGameServer()->GetPlayerByUserID(targetUserID);
        if(pLocalTarget) {
            FinalizePBanResolvedTarget(targetUserID, pLocalTarget->GetRawName());
            return;
        }

        QueryRequest req = MakeGetPlayerBasicByID(targetUserID, GetNetID());
        req.extraData.resize(1);
        req.extraData[0] = PLAYER_SUB_PBAN_BY_USERID;
        DatabasePlayerExec(GetContext()->GetDatabasePool(), DB_PLAYER_GET_BASIC_BY_ID, req);
        return;
    }

    if(target.size() < 3) {
        m_pendingPBan.active = false;
        SendOnConsoleMessage("`4Oops:`` You need at least the first three characters of the person's name, or use #userid.");
        return;
    }

    auto localMatches = GetGameServer()->FindPlayersByNamePrefix(target, false, 0);
    if(localMatches.size() == 1 && localMatches[0]) {
        FinalizePBanResolvedTarget(localMatches[0]->GetUserID(), localMatches[0]->GetRawName());
        return;
    }

    QueryRequest req = MakeFindPlayersByNamePrefix(target, GetNetID());
    req.extraData.resize(1);
    req.extraData[0] = PLAYER_SUB_PBAN_BY_PREFIX;
    DatabasePlayerExec(GetContext()->GetDatabasePool(), DB_PLAYER_FIND_BY_NAME_PREFIX, req);
}

void GamePlayer::FinalizePBanResolvedTarget(uint32 targetUserID, const string& resolvedTargetName)
{
    if(targetUserID == 0) {
        m_pendingPBan.active = false;
        SendOnConsoleMessage("`4Oops:`` Invalid target for /pban.");
        return;
    }

    if(targetUserID == GetUserID()) {
        m_pendingPBan.active = false;
        SendOnConsoleMessage("Nope, you can't use that on yourself.");
        return;
    }

    const uint32 nowEpochSec = (uint32)Time::GetTimeSinceEpoch();
    const uint32 untilEpochSec = BuildPBanUntilEpochSec(m_pendingPBan.durationSec);
    const uint32 persistedDurationSec = m_pendingPBan.durationSec < 0 ? 0u : (uint32)m_pendingPBan.durationSec;

    QueryRequest updateReq = MakeAppendPlayerPBanStatsByID(targetUserID, untilEpochSec, nowEpochSec, persistedDurationSec, NET_ID_FALLBACK);
    DatabasePlayerExec(GetContext()->GetDatabasePool(), DB_PLAYER_APPEND_PBAN_STATS_BY_ID, updateReq);

    GamePlayer* pLocalTarget = GetGameServer()->GetPlayerByUserID(targetUserID);
    if(pLocalTarget) {
        pLocalTarget->ApplyAccountBan(m_pendingPBan.durationSec, m_pendingPBan.reason, GetRawName(), false);
    }
    else {
        const uint64 payloadDuration = m_pendingPBan.durationSec < 0 ? (uint64)UINT32_MAX : (uint64)m_pendingPBan.durationSec;
        GetMasterBroadway()->SendCrossServerActionRequest(
            TCP_CROSS_ACTION_PBAN,
            GetUserID(),
            GetRawName(),
            ToString(targetUserID),
            true,
            m_pendingPBan.reason,
            payloadDuration
        );
    }

    const string displayTargetName = resolvedTargetName.empty() ? "Unknown" : resolvedTargetName;
    const string superBroadcast = BuildAncientsSuspendBroadcast(displayTargetName);
    const string ancientConsole = BuildAncientsBanConsole(displayTargetName);

    GetMasterBroadway()->SendCrossServerActionRequest(
        TCP_CROSS_ACTION_SUPER_BROADCAST,
        GetUserID(),
        GetRawName(),
        "ALL",
        true,
        superBroadcast,
        0
    );

    GetMasterBroadway()->SendCrossServerActionRequest(
        TCP_CROSS_ACTION_SUPER_BROADCAST,
        GetUserID(),
        GetRawName(),
        "ALL",
        true,
        ancientConsole,
        0
    );

    SendOnConsoleMessage(
        m_pendingPBan.durationSec < 0
            ? "`oApplied permanent account ban to ``" + displayTargetName + "`` (`9#" + ToString(targetUserID) + "``)."
            : "`oApplied account ban to ``" + displayTargetName + "`` (`9#" + ToString(targetUserID) + "``) for `w" + ToString(m_pendingPBan.durationSec) + "`` second(s)."
    );

    m_pendingPBan.active = false;
}

void GamePlayer::HandlePBanRequestResult(QueryTaskResult&& result)
{
    if(!m_pendingPBan.active || result.extraData.empty()) {
        return;
    }

    const int32 subType = result.extraData[0].GetINT();
    if(!result.result) {
        m_pendingPBan.active = false;
        SendOnConsoleMessage("`4OOPS! ``Failed to process /pban request from database.");
        return;
    }

    uint32 targetUserID = 0;
    string targetName;

    if(subType == PLAYER_SUB_PBAN_BY_PREFIX) {
        const uint32 rowCount = result.result->GetRowCount();
        if(rowCount == 0) {
            m_pendingPBan.active = false;
            SendOnConsoleMessage("`4Oops:`` No player found with prefix `w" + m_pendingPBan.target + "``.");
            return;
        }

        if(rowCount > 1) {
            SendOnConsoleMessage("`oMultiple players matched prefix `w" + m_pendingPBan.target + "``. Be more specific or use `w#userid``:");

            const uint32 maxRows = std::min<uint32>(10, rowCount);
            for(uint32 i = 0; i < maxRows; ++i) {
                const uint32 userID = result.result->GetField("ID", i).GetUINT();
                const string rawName = result.result->GetField("Name", i).GetString();
                SendOnConsoleMessage("`o- `w" + rawName + "`` (`9#" + ToString(userID) + "``)");
            }

            m_pendingPBan.active = false;
            return;
        }

        targetUserID = result.result->GetField("ID", 0).GetUINT();
        targetName = result.result->GetField("Name", 0).GetString();
    }
    else if(subType == PLAYER_SUB_PBAN_BY_USERID) {
        if(result.result->GetRowCount() == 0) {
            m_pendingPBan.active = false;
            SendOnConsoleMessage("`4Oops:`` No player found with that userID.");
            return;
        }

        targetUserID = result.result->GetField("ID", 0).GetUINT();
        targetName = result.result->GetField("Name", 0).GetString();
    }
    else {
        m_pendingPBan.active = false;
        return;
    }

    FinalizePBanResolvedTarget(targetUserID, targetName);
}

void GamePlayer::ApplyAccountBan(int32 durationSec, const string& reason, const string& sourceRawName, bool sendWorldMessage)
{
    const uint32 nowEpochSec = (uint32)Time::GetTimeSinceEpoch();
    const uint32 untilEpochSec = BuildPBanUntilEpochSec(durationSec);

    SetCustomStatValue(kPBanStatActive, 1);
    SetCustomStatValue(kPBanStatUntil, untilEpochSec);
    SetCustomStatValue(kPBanStatSetAt, nowEpochSec);
    SetCustomStatValue(kPBanStatDuration, durationSec < 0 ? 0 : (uint64)durationSec);
    SetCustomStatValue(kPBanStatPerma, durationSec < 0 ? 1 : 0);

    if(sendWorldMessage && m_currentWorldID != 0) {
        World* pWorld = GetWorldManager()->GetWorldByID(m_currentWorldID);
        if(pWorld) {
            pWorld->SendConsoleMessageToAll(BuildAncientsBanConsole(GetRawName()));
        }
    }

    SendOnConsoleMessage(BuildSuspendedLoginMessage(GetRawName(), durationSec < 0 ? 0u : (uint64)std::max<int32>(0, durationSec), durationSec < 0));
    if(!reason.empty()) {
        SendOnConsoleMessage("`oReason: `w" + reason + "``");
    }
    if(!sourceRawName.empty()) {
        SendOnConsoleMessage("`oApplied by `w" + sourceRawName + "``.");
    }

    SaveToDatabase();
    LogOff();
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

    uint32 pingCompensation = 2;
    if(GetPeer() && GetPeer()->lastRoundTripTime > 20) {
        pingCompensation = GetPeer()->lastRoundTripTime / 20;
    }

    const float softDistanceLimit = 32.0f * (3.0f + static_cast<float>(pingCompensation));
    const float hardDistanceLimit = 32.0f * (5.0f + static_cast<float>(pingCompensation));
    const bool isSuspicious = distance > softDistanceLimit;

    if(m_lastCheckMoveWindow.GetElapsedTime() >= 5000) {
        m_lastCheckMoveWindow.Reset();
        m_moveViolationsInWindow = 0;
    }

    if(isSuspicious) {
        ++m_moveViolationsInWindow;
        if(distance > hardDistanceLimit || m_moveViolationsInWindow >= 10) {
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