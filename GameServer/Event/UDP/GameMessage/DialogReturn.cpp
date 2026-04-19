#include "DialogReturn.h"
#include "IO/Log.h"

#include "../../../Player/Dialog/SignDialog.h"
#include "../../../Player/Dialog/PathMarkerDialog.h"
#include "../../../Player/Dialog/TrashDialog.h"
#include "../../../Player/Dialog/DropDialog.h"
#include "../../../Player/Dialog/LockDialog.h"
#include "../../../Player/Dialog/DoorDialog.h"
#include "../../../Player/Dialog/MachineDialog.h"
#include "../../../Player/Dialog/ChemsynthDialog.h"
#include "../../../Player/Dialog/RenderWorldDialog.h"
#include "../../../Player/GamePlayer.h"
#include "../../../Server/GameServer.h"
#include "../../../Server/MasterBroadway.h"
#include "../../../Player/Dialog/RegisterDialog.h"
#include "../../../Component/FossilComponent.h"
#include "../../../Component/GeigerComponent.h"
#include "../../../Context.h"
#include "../../../World/WorldManager.h"
#include "../../../../Source/World/TileInfo.h"
#include "Utils/StringUtils.h"
#include "Utils/Timer.h"
#include "Utils/DialogBuilder.h"
#include "Algorithm/GhostAlgorithm.h"
#include "Item/ItemInfoManager.h"
#include "Database/Table/PlayerDBTable.h"
#include "Database/Table/GuildDBTable.h"
#include "../../../Guild/GuildManager.h"

namespace {

constexpr uint16 kTelephoneItemID = 3898;
constexpr const char* kDailyQuestClaimDayStat = "DailyQuestClaimDay";
constexpr uint32 kGuildMemberBlobVersion = 1;
constexpr const char* kFindDialogName = "FindItem";

bool ParseUInt64Field(const string& text, uint64& out)
{
    if(text.empty()) {
        return false;
    }

    try {
        size_t parsed = 0;
        const unsigned long long value = std::stoull(text, &parsed, 10);
        if(parsed != text.size()) {
            return false;
        }

        out = static_cast<uint64>(value);
        return true;
    }
    catch(...) {
        return false;
    }
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

string BuildDailyQuestStatKey(const char* prefix, uint32 epochDay, uint16 itemID)
{
    return string(prefix) + "_" + ToString(epochDay) + "_" + ToString(itemID);
}

string GetItemNameSafe(uint32 itemID)
{
    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(itemID);
    if(!pItem) {
        return "Item #" + ToString(itemID);
    }

    return pItem->name;
}

bool ParseIntField(ParsedTextPacket<8>& packet, uint32 key, int32& out)
{
    auto pField = packet.Find(key);
    if(!pField) {
        return false;
    }

    return ToInt(string(pField->value, pField->size), out) == TO_INT_SUCCESS;
}

bool ParseBoolField(ParsedTextPacket<8>& packet, uint32 key, bool& out)
{
    auto pField = packet.Find(key);
    if(!pField) {
        return false;
    }

    string value(pField->value, pField->size);
    if(value == "1" || ToLower(value) == "true") {
        out = true;
        return true;
    }

    if(value == "0" || ToLower(value) == "false") {
        out = false;
        return true;
    }

    return false;
}

bool IsFindBypassPerm(GamePlayer* pPlayer)
{
    Role* pRole = pPlayer ? pPlayer->GetRole() : nullptr;
    return pRole && pRole->HasPerm(ROLE_PERM_COMMAND_GIVEITEM);
}

bool IsRestrictedFindItem(const ItemInfo& item)
{
    if(item.type == ITEM_TYPE_LOCK) {
        return true;
    }

    if(item.HasFlag(ITEM_FLAG_MOD) || item.HasFlag(ITEM_FLAG_UNTRADEABLE) || item.HasFlag(ITEM_FLAG_BETA) || item.HasFlag(ITEM_FLAG_DROPLESS)) {
        return true;
    }

    const string loweredName = ToLower(item.name);
    return loweredName.find("voucher") != string::npos || loweredName.find("crate") != string::npos;
}

uint32 GetFindItemPrice(uint32 count, const ItemInfo& item)
{
    if(count == 0) {
        return 0;
    }

    if(item.type == ITEM_TYPE_SEED) {
        int32 fruitDrop = item.rarity == 999 ? 3 : 6;
        int32 price = static_cast<int32>(item.rarity * (15 * fruitDrop) * (count / 200.0f));
        return (uint32)std::max<int32>(1, price);
    }

    if(item.type == ITEM_TYPE_CLOTHES) {
        int32 unit = item.rarity == 999 ? item.rarity * 20 : item.rarity * 15;
        return (uint32)std::max<int32>(1, unit * (int32)count);
    }

    int32 price = static_cast<int32>(item.rarity * 15 * (count / 200.0f));
    return (uint32)std::max<int32>(1, price);
}

void SendFindClaimDialog(GamePlayer* pPlayer, const ItemInfo& item)
{
    if(!pPlayer) {
        return;
    }

    const bool bypass = IsFindBypassPerm(pPlayer);

    DialogBuilder db;
    db.SetDefaultColor('o')
        ->AddLabelWithIcon("`wGet ``" + item.name, (uint16)item.id, true)
        ->AddTextBox("How many would you like to get?");

    if(bypass) {
        db.AddTextBox("`oYou have elevated permission. Restricted item checks and gem cost are bypassed.");
    }
    else {
        db.AddTextBox(item.name + " costs `w" + ToString((int32)GetFindItemPrice(1, item)) + "`` gems each and `w" + ToString((int32)GetFindItemPrice(200, item)) + "`` per stack.");
    }

    db.AddTextInput("GetAmount", "Amount", "1", 3)
        ->EndDialog(kFindDialogName, "Get", "Exit");

    pPlayer->SetPendingFindItemID((uint16)item.id);
    pPlayer->SendOnDialogRequest(db.Get());
}

bool IsBirthCertNameValid(const string& name)
{
    if(name.size() < 3 || name.size() > 18) {
        return false;
    }

    if(name.find(' ') != string::npos || name.find('`') != string::npos) {
        return false;
    }

    for(char c : name) {
        if(!std::isalnum((unsigned char)c)) {
            return false;
        }
    }

    return true;
}

void ShowBirthCertificateDialog(GamePlayer* pPlayer, const string& message = "", const string& prefillName = "")
{
    if(!pPlayer) {
        return;
    }

    pPlayer->ClearBirthCertificatePendingName();

    DialogBuilder db;
    db.SetDefaultColor('o')
        ->AddLabelWithIcon("`wChange your GrowID``", ITEM_ID_BIRTH_CERTIFICATE, true)
        ->AddSpacer();

    if(!message.empty()) {
        db.AddLabel(message);
    }

    db.AddLabel("This will change your GrowID `4permanently``. You can't change it again for `460 days``.")
        ->AddLabel("Your `wBirth Certificate`` will be consumed if you press `5Change It``.")
        ->AddLabel("Choose an appropriate name or `6we will change it for you!``")
        ->AddTextInput("NewName", "Enter your new name:", prefillName, 32)
        ->EndDialog("BirthCertificate", "Change it!", "Cancel");

    pPlayer->SendOnDialogRequest(db.Get());
}

void ShowBirthCertificateConfirmDialog(GamePlayer* pPlayer, const string& newName)
{
    if(!pPlayer) {
        return;
    }

    DialogBuilder db;
    db.SetDefaultColor('o')
        ->AddLabelWithIcon("`wChange your GrowID``", ITEM_ID_BIRTH_CERTIFICATE, true)
        ->AddLabel("`4Are you absolutely sure you want to use up 1 `2Birth Certificate`` to permanently change your name to \"`9" + newName + "``\"?")
        ->AddLabel("This will change your GrowID `4permanently``. You can't change it again for `460 days``.<CR>Your `wBirth Certificate`` will be consumed if you press `5Change It``.<CR>Capitalization will be exactly as shown above.<CR>Choose an appropriate name or `6we will change it for you!``")
        ->EndDialog("BirthCertificateConfirm", "Change it!", "Cancel");

    pPlayer->SendOnDialogRequest(db.Get());
}

void ShowCrazyJimMainMenu(GamePlayer* pPlayer)
{
    if(!pPlayer) {
        return;
    }

    DialogBuilder db;
    db.SetDefaultColor('o')
        ->AddLabelWithIcon("`wCrazy Jim's Quest Emporium``", kTelephoneItemID, true, true)
        ->AddLabel("HEEEEYYY there Growtopian! I'm Crazy Jim, and my quests are so crazy they're KERRRRAAAAZZY!! And that is clearly very crazy, so please, be cautious around them. What can I do ya for, partner?")
        ->AddButton("GotoCrazyJimDailyQuest", "Daily Quest")
        ->EndDialog("TelephoneEdit", "", "Hang Up");

    pPlayer->SendOnDialogRequest(db.Get());
}

void ShowCrazyJimDailyQuest(GamePlayer* pPlayer)
{
    if(!pPlayer) {
        return;
    }

    const uint32 currentEpochDay = (uint32)(Time::GetTimeSinceEpoch() / 86400ull);
    const TCPDailyQuestData& dq = GetMasterBroadway()->GetDailyQuestData();

    DialogBuilder db;
    db.SetDefaultColor('o')
        ->AddLabelWithIcon("`wCrazy Jim's Quest Emporium``", kTelephoneItemID, true, true)
        ->AddLabel("I guess some people call me Crazy Jim because I'm a bit of a hoarder. But I'm very particular about what I want! And today, what I want is this:")
        ->AddSpacer()
        ->AddLabelWithIcon("`2" + ToString(dq.questItemOneAmount) + " " + GetItemNameSafe(dq.questItemOneID) + "``", (uint16)dq.questItemOneID)
        ->AddLabel("and", true)
        ->AddLabelWithIcon("`2" + ToString(dq.questItemTwoAmount) + " " + GetItemNameSafe(dq.questItemTwoID) + "``", (uint16)dq.questItemTwoID)
        ->AddSpacer()
        ->AddLabel("You shove all that through the phone (it works, I've tried it), and I will hand you one of the `2Growtokens`` from my personal collection!  But hurry, this offer is only good until midnight, and only one `2Growtoken`` per person!")
        ->AddLabel("You have " + ToString(pPlayer->GetInventory().GetCountOfItem((uint16)dq.questItemOneID)) + " " + GetItemNameSafe(dq.questItemOneID) + " and " + ToString(pPlayer->GetInventory().GetCountOfItem((uint16)dq.questItemTwoID)) + " " + GetItemNameSafe(dq.questItemTwoID) + ".")
        ->AddButton("GotoCrazyJimMainMenu", "Back");

    const uint64 countOne = pPlayer->GetInventory().GetCountOfItem((uint16)dq.questItemOneID);
    const uint64 countTwo = pPlayer->GetInventory().GetCountOfItem((uint16)dq.questItemTwoID);
    if(countOne >= dq.questItemOneAmount && countTwo >= dq.questItemTwoAmount) {
        db.AddButton("GotoCrazyJimTurnInDailyQuest", "`2Turn in quest items");
    }

    db.EndDialog("TelephoneEdit", "", "Hang Up");
    pPlayer->SendOnDialogRequest(db.Get());
}

void ShowRemoveGhostDialog(GamePlayer* pPlayer, World* pWorld)
{
    if(!pPlayer || !pWorld) {
        return;
    }

    DialogBuilder db;
    db.SetDefaultColor('o')
        ->AddLabelWithIcon("`wRemove Ghost``", ITEM_ID_GHOST_BE_GONE, true, true)
        ->AddLabel(
            GhostAlgorithm::HasWorldGhosts(pWorld)
                ? "Wanna remove ghost? It's gonna cost you `515 World Locks `ofor our services."
                : "Well, there really ain't no ghosts in there for us to remove. Come back later when it does."
        );

    if(GhostAlgorithm::HasWorldGhosts(pWorld)) {
        db.AddButton("RemoveGhost", "Yes remove ghost.");
    }

    db.EndDialog("TelephoneEdit", "", "Hang Up");

    pPlayer->SendOnDialogRequest(db.Get());
}

void ShowRemoveGhostConfirmDialog(GamePlayer* pPlayer, World* pWorld)
{
    if(!pPlayer || !pWorld) {
        return;
    }

    DialogBuilder db;
    db.SetDefaultColor('o')
        ->AddLabelWithIcon("`wRemove Ghost``", ITEM_ID_GHOST_BE_GONE, true, true)
        ->AddLabel(
            GhostAlgorithm::HasWorldGhosts(pWorld)
                ? "Your total comes to `515`` World Locks. Will that be all tonight?"
                : "Well, there really ain't no ghosts in there for us to remove. Come back later when it does."
        );

    if(GhostAlgorithm::HasWorldGhosts(pWorld)) {
        db.AddLabel("You have " + ToString(pPlayer->GetInventory().GetCountOfItem(ITEM_ID_WORLD_LOCK)) + " World Locks.")
            ->AddButton("RemoveGhostConfirm", "Thank you!")
            ->AddButton("", "Nevermind");
    }

    db.EndDialog("TelephoneEdit", "", "Hang Up");

    pPlayer->SendOnDialogRequest(db.Get());
}

void ShowTelephoneMainMenu(GamePlayer* pPlayer)
{
    if(!pPlayer) {
        return;
    }

    DialogBuilder db;
    db.SetDefaultColor('o')
        ->AddLabelWithIcon("`wCrazy Jim's Quest Emporium``", kTelephoneItemID, true, true)
        ->AddLabel("HEEEEYYY there Growtopian! I'm Crazy Jim, and my quests are so crazy they're KERRRRAAAAZZY!!")
        ->AddSpacer()
        ->AddButton("GotoCrazyJimDailyQuest", "Daily Quest")
        ->EndDialog("TelephoneEdit", "", "Hang Up");

    pPlayer->SendOnDialogRequest(db.Get());
}

void ShowTelephoneQuestDialog(GamePlayer* pPlayer)
{
    if(!pPlayer) {
        return;
    }

    const uint32 currentEpochDay = (uint32)(Time::GetTimeSinceEpoch() / 86400ull);
    if(pPlayer->GetCustomStatValue(kDailyQuestClaimDayStat) == currentEpochDay) {
        DialogBuilder db;
        db.SetDefaultColor('o')
            ->AddLabelWithIcon("`wCrazy Jim's Quest Emporium``", kTelephoneItemID, true, true)
            ->AddLabel("You've already completed my Daily Quest for today! Call me back after midnight to hear about my next cravings.")
            ->AddButton("GotoCrazyJimMainMenu", "Back")
            ->EndDialog("TelephoneEdit", "", "Hang Up");

        pPlayer->SendOnDialogRequest(db.Get());
        return;
    }

    const TCPDailyQuestData& dq = GetMasterBroadway()->GetDailyQuestData();
    DialogBuilder db;
    db.SetDefaultColor('o')
        ->AddLabelWithIcon("`wCrazy Jim's Quest Emporium``", kTelephoneItemID, true, true)
        ->AddLabel("I guess some people call me Crazy Jim because I'm a bit of a hoarder. But I'm very particular about what I want! And today, what I want is this:")
        ->AddSpacer()
        ->AddLabelWithIcon("`2" + ToString(dq.questItemOneAmount) + " " + GetItemNameSafe(dq.questItemOneID) + "``", (uint16)dq.questItemOneID)
        ->AddLabel("and", true)
        ->AddLabelWithIcon("`2" + ToString(dq.questItemTwoAmount) + " " + GetItemNameSafe(dq.questItemTwoID) + "``", (uint16)dq.questItemTwoID)
        ->AddSpacer()
        ->AddLabel("You shove all that through the phone (it works, I've tried it), and I will hand you one of the `2Growtokens`` from my personal collection!  But hurry, this offer is only good until midnight, and only one `2Growtoken`` per person!")
        ->AddSpacer();

    const uint64 countOne = pPlayer->GetInventory().GetCountOfItem((uint16)dq.questItemOneID);
    const uint64 countTwo = pPlayer->GetInventory().GetCountOfItem((uint16)dq.questItemTwoID);
    db.AddLabel("You have " + ToString(countOne) + " " + GetItemNameSafe(dq.questItemOneID) + " and " + ToString(countTwo) + " " + GetItemNameSafe(dq.questItemTwoID) + ".");

    if(countOne >= dq.questItemOneAmount && countTwo >= dq.questItemTwoAmount) {
        db.AddButton("GotoCrazyJimTurnInDailyQuest", "`2Turn in quest items");
    }

    db.AddButton("GotoCrazyJimMainMenu", "Back")
        ->EndDialog("TelephoneEdit", "", "Hang Up");

    pPlayer->SendOnDialogRequest(db.Get());
}

void TurnInTelephoneQuest(GamePlayer* pPlayer)
{
    if(!pPlayer) {
        return;
    }

    const uint32 currentEpochDay = (uint32)(Time::GetTimeSinceEpoch() / 86400ull);
    if(pPlayer->GetCustomStatValue(kDailyQuestClaimDayStat) == currentEpochDay) {
        pPlayer->SendOnTalkBubble("You've already completed my Daily Quest for today!", true);
        return;
    }

    const TCPDailyQuestData& dq = GetMasterBroadway()->GetDailyQuestData();
    const uint64 haveOne = pPlayer->GetInventory().GetCountOfItem((uint16)dq.questItemOneID);
    const uint64 haveTwo = pPlayer->GetInventory().GetCountOfItem((uint16)dq.questItemTwoID);
    if(haveOne < dq.questItemOneAmount || haveTwo < dq.questItemTwoAmount) {
        pPlayer->SendOnTalkBubble("I don't have enough items to turn it in!", true);
        return;
    }

    const uint64 growtokenCount = pPlayer->GetInventory().GetCountOfItem(ITEM_ID_GROWTOKEN);
    if(growtokenCount >= 200) {
        pPlayer->ModifyInventoryItem(ITEM_ID_GROWTOKEN, -(int16)growtokenCount);
        pPlayer->ModifyInventoryItem(ITEM_ID_MEGA_GROWTOKEN, 1);
    }

    if(!pPlayer->GetInventory().HaveRoomForItem(ITEM_ID_GROWTOKEN, 1)) {
        pPlayer->SendOnTalkBubble("This seems far-fetched, but you've already reached the maximum amount of Growtokens.", true);
        return;
    }

    pPlayer->ModifyInventoryItem((uint16)dq.questItemOneID, -(int16)dq.questItemOneAmount);
    pPlayer->ModifyInventoryItem((uint16)dq.questItemTwoID, -(int16)dq.questItemTwoAmount);
    pPlayer->ModifyInventoryItem(ITEM_ID_GROWTOKEN, 1);
    pPlayer->SetCustomStatValue(kDailyQuestClaimDayStat, currentEpochDay);

    World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
    if(pWorld) {
        pWorld->SendParticleEffectToAll(pPlayer->GetWorldPos().x + 12.0f, pPlayer->GetWorldPos().y + 15.0f, 198);
        pWorld->PlaySFXForEveryone("piano_nice.wav", 0);
    }

    pPlayer->SendOnTalkBubble("`wThanks, pardner! Have 1 `2Growtoken`w!", true);
    pPlayer->SendOnConsoleMessage("[`6You jammed " + ToString(dq.questItemOneAmount) + " " + GetItemNameSafe(dq.questItemOneID) + " and " + ToString(dq.questItemTwoAmount) + " " + GetItemNameSafe(dq.questItemTwoID) + " into the phone, and 1 `2Growtoken`` popped out!``]");
}

void RemoveGhostConfirm(GamePlayer* pPlayer)
{
    if(!pPlayer) {
        return;
    }

    World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
    if(!pWorld) {
        return;
    }

    if(pPlayer->GetInventory().GetCountOfItem(ITEM_ID_WORLD_LOCK) < 15) {
        pPlayer->SendOnTalkBubble("You need 15 World Locks for that.", true);
        return;
    }

    if(!GhostAlgorithm::HasWorldGhosts(pWorld)) {
        pPlayer->SendOnTalkBubble("Well, there really ain't no ghosts in there for us to remove. Come back later when it does.", true);
        return;
    }

    pPlayer->ModifyInventoryItem(ITEM_ID_WORLD_LOCK, -15);
    GhostAlgorithm::ClearWorldGhosts(pWorld);
    pWorld->SendTalkBubbleAndConsoleToAll("`4The ghosts have vanished from the world!", true, pPlayer);
}

}

void DialogReturn::Execute(GamePlayer* pPlayer, ParsedTextPacket<8>& packet)
{
    if(!pPlayer) {
        return;
    }

    auto pDialogName = packet.Find(CompileTimeHashString("dialog_name"));
    if(!pDialogName || pDialogName->size > 50) {
        return;
    }

    uint32 hashedDialogName = HashString(pDialogName->value, pDialogName->size);

    switch(hashedDialogName) {
        case CompileTimeHashString("WrenchSelf"): {
            auto pButtonClicked = packet.Find(CompileTimeHashString("buttonClicked"));
            if(!pButtonClicked) {
                return;
            }

            string buttonClicked(pButtonClicked->value, pButtonClicked->size);
            if(buttonClicked == "PlayerInfo" || buttonClicked == "SocialProfile" || buttonClicked == "PlayerStats" ||
                buttonClicked == "Settings" || buttonClicked == "Titles" || buttonClicked == "Worlds" ||
                buttonClicked == "ske_titles" || buttonClicked == "set_online_status" ||
                buttonClicked == "notebook_edit" || buttonClicked == "billboard_edit" ||
                buttonClicked == "goals" || buttonClicked == "bonus" ||
                buttonClicked == "view_owned_worlds" || buttonClicked == "alist" ||
                buttonClicked == "emojis" || buttonClicked == "marvelous_missions" ||
                buttonClicked == "change_password" || buttonClicked == "open_social_portal" ||
                buttonClicked == "GuildInvitations")
            {
                if(buttonClicked == "Settings") {
                    bool showFriendNotifications = pPlayer->IsShowFriendNotification();
                    ParseBoolField(packet, CompileTimeHashString("checkbox_friend_alert"), showFriendNotifications);
                    pPlayer->SetShowFriendNotification(showFriendNotifications);
                }

                if(buttonClicked == "open_social_portal") {
                    pPlayer->SendSocialPortal();
                    break;
                }

                pPlayer->SendWrenchSelf(buttonClicked);
            }
            break;
        }

        case CompileTimeHashString("TitlesCustomization"): {
            bool usePrefix = pPlayer->IsPrefixEnabled();
            ParseBoolField(packet, CompileTimeHashString("Prefix"), usePrefix);
            pPlayer->SetPrefixEnabled(usePrefix);

            bool useLegendary = pPlayer->IsLegendaryTitleEnabled();
            if(ParseBoolField(packet, CompileTimeHashString("legentitle"), useLegendary)) {
                pPlayer->SetLegendaryTitleEnabled(useLegendary);
            }

            bool useGrow4Good = pPlayer->IsGrow4GoodTitleEnabled();
            if(ParseBoolField(packet, CompileTimeHashString("grow4good"), useGrow4Good)) {
                pPlayer->SetGrow4GoodTitleEnabled(useGrow4Good);
            }

            bool useMvp = pPlayer->IsMaxLevelTitleEnabled();
            if(ParseBoolField(packet, CompileTimeHashString("bluename"), useMvp)) {
                pPlayer->SetMaxLevelTitleEnabled(useMvp);
            }

            bool useVip = pPlayer->IsSuperSupporterTitleEnabled();
            if(ParseBoolField(packet, CompileTimeHashString("ssup"), useVip)) {
                pPlayer->SetSuperSupporterTitleEnabled(useVip);
            }

            World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
            if(pWorld) {
                pWorld->SendNameChangeToAll(pPlayer);
                pWorld->SendSetCharPacketToAll(pPlayer);
            }

            pPlayer->SendWrenchSelf("PlayerInfo");
            break;
        }

        case CompileTimeHashString("WrenchOthers"): {
            auto pButtonClicked = packet.Find(CompileTimeHashString("buttonClicked"));
            auto pNetID = packet.Find(CompileTimeHashString("OtherNetID"));

            if(!pButtonClicked || !pNetID) {
                return;
            }

            int32 netID = 0;
            if(ToInt(string(pNetID->value, pNetID->size), netID) != TO_INT_SUCCESS) {
                return;
            }

            GamePlayer* pTarget = GetGameServer()->GetPlayerByNetID(netID);
            if(!pTarget || pTarget == pPlayer || pTarget->GetCurrentWorld() != pPlayer->GetCurrentWorld()) {
                pPlayer->SendOnConsoleMessage("`4Target is no longer valid.``");
                return;
            }

            string buttonClicked(pButtonClicked->value, pButtonClicked->size);
            if(buttonClicked == "Trade" || buttonClicked == "trd_btn") {
                pPlayer->StartTrade(pTarget);
            }
            else if(buttonClicked == "sendpm") {
                pPlayer->SetLastWhisperUserID(pTarget->GetUserID());
                pPlayer->SetLastWhisperAtMS(Time::GetSystemTime());
                pPlayer->SendOnConsoleMessage("`oUse `/msg " + pTarget->GetRawName() + " <message>`` to send a private message.");
            }
            else if(buttonClicked == "show_clothes") {
                pPlayer->SendOnConsoleMessage(
                    "`oClothes for ``" + pTarget->GetDisplayName() +
                    "``: Hair `w" + ToString(pTarget->GetInventory().GetClothByPart(BODY_PART_HAIR)) +
                    "`` | Shirt `w" + ToString(pTarget->GetInventory().GetClothByPart(BODY_PART_SHIRT)) +
                    "`` | Pants `w" + ToString(pTarget->GetInventory().GetClothByPart(BODY_PART_PANT)) +
                    "`` | Shoes `w" + ToString(pTarget->GetInventory().GetClothByPart(BODY_PART_SHOE)) + "``"
                );
            }
            else if(buttonClicked == "Pull" || buttonClicked == "pull_btn") {
                std::vector<string> cmdArgs = { "/pull", pTarget->GetRawName() };
                GetGameServer()->ExecuteCommand(pPlayer, cmdArgs);
            }
            else if(buttonClicked == "Kick" || buttonClicked == "kick_btn") {
                std::vector<string> cmdArgs = { "/kick", pTarget->GetRawName() };
                GetGameServer()->ExecuteCommand(pPlayer, cmdArgs);
            }
            else if(buttonClicked == "Ban" || buttonClicked == "ban_btn") {
                std::vector<string> cmdArgs = { "/ban", pTarget->GetRawName() };
                GetGameServer()->ExecuteCommand(pPlayer, cmdArgs);
            }
            else if(buttonClicked == "Add" || buttonClicked == "friend_add" || buttonClicked == "add_btn") {
                if(pPlayer->IsFriendWith(pTarget->GetUserID())) {
                    pPlayer->SendOnConsoleMessage("`oYou are already friends with ``" + pTarget->GetDisplayName() + "``.");
                }
                else if(!pPlayer->AcceptFriendRequestFrom(pTarget)) {
                    pPlayer->SendFriendRequestTo(pTarget);
                }
            }
            else if(buttonClicked == "Unfriend") {
                const bool removedMine = pPlayer->RemoveFriendUserID(pTarget->GetUserID());
                const bool removedOther = pTarget->RemoveFriendUserID(pPlayer->GetUserID());
                if(removedMine || removedOther) {
                    pPlayer->SendOnConsoleMessage("`oYou are no longer friends with ``" + pTarget->GetDisplayName() + "``.");
                }
            }
            else if(buttonClicked == "Ignore" || buttonClicked == "ignore_player") {
                if(pPlayer->IsIgnoring(pTarget->GetUserID())) {
                    pPlayer->RemoveIgnoredUserID(pTarget->GetUserID());
                    pPlayer->SendOnConsoleMessage("`oYou no longer ignore ``" + pTarget->GetDisplayName() + "``.");
                }
                else {
                    pPlayer->AddIgnoredUserID(pTarget->GetUserID());
                    pPlayer->SendOnConsoleMessage("`oYou now ignore ``" + pTarget->GetDisplayName() + "``.");
                }
            }
            else if(buttonClicked == "Report" || buttonClicked == "report_player") {
                pPlayer->SendOnConsoleMessage("`oReport queued for review on ``" + pTarget->GetDisplayName() + "``.");
                pPlayer->SendOnTalkBubble("`wThanks for the report, our team will review it.", true);
            }
            else if(buttonClicked == "InviteGuild" || buttonClicked == "inv_to_kantut") {
                Guild* pGuild = GetGuildManager()->Get(pPlayer->GetGuildID());
                if(!pGuild) {
                    pPlayer->SendOnConsoleMessage("`4You are not in a guild.");
                    return;
                }

                if(pTarget->GetGuildID() != 0) {
                    pPlayer->SendOnConsoleMessage("`4That player is already in a guild.");
                    return;
                }

                if(pGuild->HasPendingInvite(pTarget->GetUserID())) {
                    pPlayer->SendOnConsoleMessage("`4That player already has a pending invite from your guild.");
                    return;
                }

                pGuild->AddPendingInvite(pTarget->GetUserID(), Time::GetSystemTime());
                pTarget->AddPendingGuildInvite(pGuild->GetGuildID(), pGuild);

                pPlayer->SendOnConsoleMessage("`2Guild invitation sent to ``" + pTarget->GetDisplayName() + "``");
                pTarget->SendOnTalkBubble("`3[``" + pPlayer->GetDisplayName() + "`` invited you to join guild `c" + pGuild->GetName() + "``!`3]``", true);
                pTarget->SendOnConsoleMessage("`2You received a guild invitation from ``" + pPlayer->GetDisplayName() + "``. Wrench yourself to see your invitations!");
                pTarget->PlaySFX("tip_start.wav", 0);
            }
            break;
        }

        case CompileTimeHashString("popup"): {
            auto pButtonClicked = packet.Find(CompileTimeHashString("buttonClicked"));
            auto pNetID = packet.Find(CompileTimeHashString("netID"));

            if(!pButtonClicked || !pNetID) {
                return;
            }

            int32 netID = 0;
            if(ToInt(string(pNetID->value, pNetID->size), netID) != TO_INT_SUCCESS) {
                return;
            }

            string buttonClicked(pButtonClicked->value, pButtonClicked->size);
            GamePlayer* pTarget = GetGameServer()->GetPlayerByNetID(netID);
            if(!pTarget || pTarget == pPlayer || pTarget->GetCurrentWorld() != pPlayer->GetCurrentWorld()) {
                pPlayer->SendOnConsoleMessage("`4Target is no longer valid.``");
                return;
            }

            if(buttonClicked == "trade" || buttonClicked == "Trade" || buttonClicked == "trd_btn") {
                pPlayer->StartTrade(pTarget);
            }
            else if(buttonClicked == "pull" || buttonClicked == "Pull" || buttonClicked == "pull_btn") {
                std::vector<string> cmdArgs = { "/pull", pTarget->GetRawName() };
                GetGameServer()->ExecuteCommand(pPlayer, cmdArgs);
            }
            else if(buttonClicked == "kick" || buttonClicked == "Kick" || buttonClicked == "kick_btn") {
                std::vector<string> cmdArgs = { "/kick", pTarget->GetRawName() };
                GetGameServer()->ExecuteCommand(pPlayer, cmdArgs);
            }
            else if(buttonClicked == "ban" || buttonClicked == "Ban" || buttonClicked == "ban_btn") {
                std::vector<string> cmdArgs = { "/ban", pTarget->GetRawName() };
                GetGameServer()->ExecuteCommand(pPlayer, cmdArgs);
            }
            else if(buttonClicked == "sendpm" || buttonClicked == "PM") {
                pPlayer->SetLastWhisperUserID(pTarget->GetUserID());
                pPlayer->SetLastWhisperAtMS(Time::GetSystemTime());
                pPlayer->SendOnConsoleMessage("`oUse `/msg " + pTarget->GetRawName() + " <message>`` to send a private message.");
            }
            else if(buttonClicked == "show_clothes" || buttonClicked == "Show Clothes") {
                pPlayer->SendOnConsoleMessage(
                    "`oClothes for ``" + pTarget->GetDisplayName() +
                    "``: Hair `w" + ToString(pTarget->GetInventory().GetClothByPart(BODY_PART_HAIR)) +
                    "`` | Shirt `w" + ToString(pTarget->GetInventory().GetClothByPart(BODY_PART_SHIRT)) +
                    "`` | Pants `w" + ToString(pTarget->GetInventory().GetClothByPart(BODY_PART_PANT)) +
                    "`` | Shoes `w" + ToString(pTarget->GetInventory().GetClothByPart(BODY_PART_SHOE)) + "``"
                );
            }
            else if(buttonClicked == "friend_add" || buttonClicked == "Add" || buttonClicked == "add_btn") {
                if(pPlayer->IsFriendWith(pTarget->GetUserID())) {
                    pPlayer->SendOnConsoleMessage("`oYou are already friends with ``" + pTarget->GetDisplayName() + "``.");
                }
                else if(!pPlayer->AcceptFriendRequestFrom(pTarget)) {
                    pPlayer->SendFriendRequestTo(pTarget);
                }
            }
            else if(buttonClicked == "Unfriend") {
                const bool removedMine = pPlayer->RemoveFriendUserID(pTarget->GetUserID());
                const bool removedOther = pTarget->RemoveFriendUserID(pPlayer->GetUserID());
                if(removedMine || removedOther) {
                    pPlayer->SendOnConsoleMessage("`oYou are no longer friends with ``" + pTarget->GetDisplayName() + "``.");
                }
            }
            else if(buttonClicked == "ignore_player" || buttonClicked == "Ignore") {
                if(pPlayer->IsIgnoring(pTarget->GetUserID())) {
                    pPlayer->RemoveIgnoredUserID(pTarget->GetUserID());
                    pPlayer->SendOnConsoleMessage("`oYou no longer ignore ``" + pTarget->GetDisplayName() + "``.");
                }
                else {
                    pPlayer->AddIgnoredUserID(pTarget->GetUserID());
                    pPlayer->SendOnConsoleMessage("`oYou now ignore ``" + pTarget->GetDisplayName() + "``.");
                }
            }
            else if(buttonClicked == "report_player" || buttonClicked == "Report") {
                pPlayer->SendOnConsoleMessage("`oReport sent for review: `w" + pTarget->GetDisplayName() + "``.");
                pPlayer->SendOnTalkBubble("`wThanks for the report, our team will review it.", true);
            }
            else if(buttonClicked == "InviteGuild" || buttonClicked == "inv_to_kantut") {
                Guild* pGuild = GetGuildManager()->Get(pPlayer->GetGuildID());
                if(!pGuild) {
                    pPlayer->SendOnConsoleMessage("`4You are not in a guild.");
                    return;
                }

                if(pTarget->GetGuildID() != 0) {
                    pPlayer->SendOnConsoleMessage("`4That player is already in a guild.");
                    return;
                }

                if(pGuild->HasPendingInvite(pTarget->GetUserID())) {
                    pPlayer->SendOnConsoleMessage("`4That player already has a pending invite from your guild.");
                    return;
                }

                pGuild->AddPendingInvite(pTarget->GetUserID(), Time::GetSystemTime());
                pTarget->AddPendingGuildInvite(pGuild->GetGuildID(), pGuild);

                pPlayer->SendOnConsoleMessage("`2Guild invitation sent to ``" + pTarget->GetDisplayName() + "``");
                pTarget->SendOnTalkBubble("`3[``" + pPlayer->GetDisplayName() + "`` invited you to join guild `c" + pGuild->GetName() + "``!`3]``", true);
                pTarget->SendOnConsoleMessage("`2You received a guild invitation from ``" + pPlayer->GetDisplayName() + "``. Wrench yourself to see your invitations!");
            }

            break;
        }

        case CompileTimeHashString("TradeModify"): {
            auto pButtonClicked = packet.Find(CompileTimeHashString("buttonClicked"));
            if(pButtonClicked) {
                string buttonClicked = ToLower(string(pButtonClicked->value, pButtonClicked->size));
                if(buttonClicked == "cancel" || buttonClicked == "close") {
                    pPlayer->ClearPendingTradeModifyItemID();
                    break;
                }
            }

            const uint16 pendingItemID = pPlayer->GetPendingTradeModifyItemID();
            if(pendingItemID == 0) {
                break;
            }

            int32 parsedAmount = 0;
            bool hasAmount = ParseIntField(packet, CompileTimeHashString("Amount"), parsedAmount);
            if(!hasAmount) {
                hasAmount = ParseIntField(packet, CompileTimeHashString("amount"), parsedAmount);
            }
            if(!hasAmount) {
                hasAmount = ParseIntField(packet, CompileTimeHashString("count"), parsedAmount);
            }
            if(!hasAmount) {
                hasAmount = ParseIntField(packet, CompileTimeHashString("ItemCount"), parsedAmount);
            }
            if(!hasAmount) {
                hasAmount = ParseIntField(packet, CompileTimeHashString("item_count"), parsedAmount);
            }

            uint32 amount = 0;
            if(hasAmount && parsedAmount > 0) {
                amount = (uint32)parsedAmount;
            }

            pPlayer->ClearPendingTradeModifyItemID();
            pPlayer->ModifyOffer(pendingItemID, (uint16)amount);
            break;
        }

        case CompileTimeHashString("TradeConfirm"): {
            auto pButtonClicked = packet.Find(CompileTimeHashString("buttonClicked"));
            if(!pButtonClicked) {
                return;
            }

            string buttonClicked(pButtonClicked->value, pButtonClicked->size);
            if(buttonClicked == "Confirm" || buttonClicked == "OK") {
                pPlayer->ConfirmOffer();
            }
            else if(buttonClicked == "Cancel") {
                pPlayer->CancelTrade(false);
            }
            break;
        }

        case CompileTimeHashString("sign_edit"):
        case CompileTimeHashString("SignEdit"): {
            auto pTileX = packet.Find(CompileTimeHashString("tilex"));
            auto pTileY = packet.Find(CompileTimeHashString("tiley"));
            auto pSignText = packet.Find(CompileTimeHashString("sign_text"));
            if(!pSignText) {
                pSignText = packet.Find(CompileTimeHashString("Text"));
            }

            if(!pTileX || !pTileY || !pSignText) {
                return;
            }

            // we need to int converter that supports non null term
            // idk if its good ways to convert it to a str
            // really we need it??

            int32 tileX = 0;
            if(ToInt(string(pTileX->value, pTileX->size), tileX) != TO_INT_SUCCESS) {
                return;
            }

            int32 tileY = 0;
            if(ToInt(string(pTileY->value, pTileY->size), tileY) != TO_INT_SUCCESS) {
                return;
            }

            World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
            TileInfo* pTile = pWorld ? pWorld->GetTileManager()->GetTile(tileX, tileY) : nullptr;

            if(pTile) {
                const uint16 itemID = pTile->GetDisplayedItem();
                if(itemID == ITEM_ID_OBJECTIVE_MARKER || itemID == ITEM_ID_PATH_MARKER || itemID == ITEM_ID_CARNIVAL_LANDING) {
                    PathMarkerDialog::Handle(pPlayer, string(pSignText->value, pSignText->size), tileX, tileY);
                }
                else {
                    SignDialog::Handle(pPlayer, string(pSignText->value, pSignText->size), tileX, tileY);
                }
            }
            break;
        }

        case CompileTimeHashString("trash_item"):
        case CompileTimeHashString("trash_item2"): {
            auto pItemID = packet.Find(CompileTimeHashString("itemID"));
            auto pCount = packet.Find(CompileTimeHashString("count"));

            if(!pItemID || !pCount) {
                return;
            }

            uint32 itemID = 0;
            if(ToUInt(string(pItemID->value, pItemID->size), itemID) != TO_INT_SUCCESS) {
                return;
            }

            int32 count = 0;
            if(ToInt(string(pCount->value, pCount->size), count) != TO_INT_SUCCESS) {
                return;
            }

            if(hashedDialogName == CompileTimeHashString("trash_item2")) {
                TrashDialog::HandleUntradeable(pPlayer, itemID, count);
            }
            else {
                TrashDialog::Handle(pPlayer, itemID, count);
            }
            break;
        }

        case CompileTimeHashString("drop_item"): {
            auto pItemID = packet.Find(CompileTimeHashString("itemID"));
            auto pCount = packet.Find(CompileTimeHashString("count"));

            if(!pItemID || !pCount) {
                return;
            }

            uint32 itemID = 0;
            if(ToUInt(string(pItemID->value, pItemID->size), itemID) != TO_INT_SUCCESS) {
                return;
            }

            int32 count = 0;
            if(ToInt(string(pCount->value, pCount->size), count) != TO_INT_SUCCESS) {
                return;
            }

            if(itemID > UINT16_MAX) {
                return;
            }

            DropDialog::Handle(pPlayer, (uint16)itemID, (int16)count);
            break;
        }

        case CompileTimeHashString("DonationEdit"): {
            string buttonClicked;
            if(const auto* pButtonClicked = packet.Find(CompileTimeHashString("buttonClicked"))) {
                buttonClicked.assign(pButtonClicked->value, pButtonClicked->size);
            }

            int32 tileX = 0;
            int32 tileY = 0;
            int32 tileItemID = 0;
            if(!ParseIntField(packet, CompileTimeHashString("tilex"), tileX) ||
                !ParseIntField(packet, CompileTimeHashString("tiley"), tileY) ||
                !ParseIntField(packet, CompileTimeHashString("TileItemID"), tileItemID))
            {
                return;
            }

            World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
            if(!pWorld) {
                return;
            }

            TileInfo* pTile = pWorld->GetTileManager()->GetTile(tileX, tileY);
            if(!pTile || pTile->GetDisplayedItem() != (uint16)tileItemID) {
                pPlayer->SendOnTalkBubble("Huh? The donation box is gone!", true);
                return;
            }

            TileExtra_DonationBox* pDonation = pTile->GetExtra<TileExtra_DonationBox>();
            if(!pDonation) {
                return;
            }

            if(buttonClicked == "ClearAll") {
                if(!pWorld->PlayerHasAccessOnTile(pPlayer, pTile)) {
                    return;
                }

                auto it = pDonation->donatedItems.begin();
                while(it != pDonation->donatedItems.end()) {
                    ItemInfo* pDonatedItem = GetItemInfoManager()->GetItemByID(it->itemID);
                    if(!pDonatedItem) {
                        it = pDonation->donatedItems.erase(it);
                        continue;
                    }

                    if(!pPlayer->GetInventory().HaveRoomForItem(it->itemID, it->amount)) {
                        pPlayer->SendOnConsoleMessage("`4" + ToString(it->amount) + " " + string(pDonatedItem->name) + " won't fit in my inventory.``");
                        ++it;
                        continue;
                    }

                    pPlayer->ModifyInventoryItem(it->itemID, it->amount);

                    const string pickupMsg =
                        "`7[```5[```w" + pPlayer->GetRawName() +
                        "`w picked up `5" + ToString(it->amount) + "`` `2" + string(pDonatedItem->name) +
                        "`` from " + it->username + "``, how nice!`5]```7]``";
                    pWorld->SendTalkBubbleAndConsoleToAll(pickupMsg, false, pPlayer);

                    it = pDonation->donatedItems.erase(it);
                }

                if(pDonation->donatedItems.empty()) {
                    pTile->RemoveFlag(TILE_FLAG_IS_ON);
                }

                pWorld->SendTileUpdate(pTile);
                pPlayer->PlaySFX("page_turn.wav");
                return;
            }

            if(buttonClicked == "ClearSelected") {
                if(!pWorld->PlayerHasAccessOnTile(pPlayer, pTile)) {
                    return;
                }

                auto it = pDonation->donatedItems.begin();
                while(it != pDonation->donatedItems.end()) {
                    const string checkboxKey = "_dn" + ToString(it->donateID);
                    bool selected = false;
                    if(!ParseBoolField(packet, HashString(checkboxKey), selected) || !selected) {
                        ++it;
                        continue;
                    }

                    ItemInfo* pDonatedItem = GetItemInfoManager()->GetItemByID(it->itemID);
                    if(!pDonatedItem) {
                        it = pDonation->donatedItems.erase(it);
                        continue;
                    }

                    if(!pPlayer->GetInventory().HaveRoomForItem(it->itemID, it->amount)) {
                        pPlayer->SendOnConsoleMessage("`4" + ToString(it->amount) + " " + string(pDonatedItem->name) + " won't fit in my inventory.``");
                        ++it;
                        continue;
                    }

                    pPlayer->ModifyInventoryItem(it->itemID, it->amount);

                    const string pickupMsg =
                        "`7[```5[```w" + pPlayer->GetRawName() +
                        "`w picked up `5" + ToString(it->amount) + "`` `2" + string(pDonatedItem->name) +
                        "`` from " + it->username + "``, how nice!`5]```7]``";
                    pWorld->SendTalkBubbleAndConsoleToAll(pickupMsg, false, pPlayer);

                    it = pDonation->donatedItems.erase(it);
                }

                if(pDonation->donatedItems.empty()) {
                    pTile->RemoveFlag(TILE_FLAG_IS_ON);
                }

                pWorld->SendTileUpdate(pTile);
                pPlayer->PlaySFX("page_turn.wav");
                return;
            }

            int32 selectedItemID = -1;
            if(!ParseIntField(packet, CompileTimeHashString("SelectedItem"), selectedItemID)) {
                ParseIntField(packet, CompileTimeHashString("SelectItem"), selectedItemID);
            }

            if(selectedItemID > 0) {
                ItemInfo* pSelectedItem = GetItemInfoManager()->GetItemByID((uint16)selectedItemID);
                if(!pSelectedItem) {
                    return;
                }

                if(pDonation->donatedItems.size() >= 20) {
                    pPlayer->SendOnTalkBubble("The donation box is already full!", true);
                    return;
                }

                int32 donatedByPlayer = 0;
                for(const TileExtra_DonatedItem& item : pDonation->donatedItems) {
                    if(item.userID == pPlayer->GetUserID()) {
                        ++donatedByPlayer;
                    }
                }

                if(donatedByPlayer >= 3 && !pWorld->PlayerHasAccessOnTile(pPlayer, pTile)) {
                    pPlayer->SendOnTalkBubble("You already donated items 3 times! Try again later.", true);
                    return;
                }

                if(pSelectedItem->rarity < 2) {
                    pPlayer->SendOnTalkBubble("This box only accepts items of rarity 2 and above.", true);
                    return;
                }

                if(pSelectedItem->HasFlag(ITEM_FLAG_UNTRADEABLE)) {
                    pPlayer->SendOnTalkBubble("That's too special to put in there!", true);
                    return;
                }

                if(pSelectedItem->id == ITEM_ID_FIST || pSelectedItem->id == ITEM_ID_WRENCH) {
                    pPlayer->SendOnTalkBubble("You can't put that there.", true);
                    return;
                }

                DialogBuilder db;
                db.SetDefaultColor('o')
                    ->AddLabelWithIcon("`w" + string(pSelectedItem->name) + "``", pSelectedItem->id, true)
                    ->AddLabel("How many to put in the box as a gift? (Note: You will `4LOSE`` the items you give!)")
                    ->AddTextInput("Amount", "Count:", ToString(pPlayer->GetInventory().GetCountOfItem(pSelectedItem->id)), 3)
                    ->AddTextInput("Comment", "Optional Note:", "", 128)
                    ->EmbedData("tilex", tileX)
                    ->EmbedData("tiley", tileY)
                    ->EmbedData("TileItemID", tileItemID)
                    ->EmbedData("ItemIDToDonate", pSelectedItem->id)
                    ->AddButton("Give", "`4Give the item(s)``")
                    ->EndDialog("DonationEdit", "", "Cancel");

                pPlayer->SendOnDialogRequest(db.Get());
                return;
            }

            if(buttonClicked != "Give" && buttonClicked != "Give the item(s)") {
                return;
            }

            int32 donateItemID = 0;
            int32 amount = 0;
            if(!ParseIntField(packet, CompileTimeHashString("ItemIDToDonate"), donateItemID) ||
                !ParseIntField(packet, CompileTimeHashString("Amount"), amount))
            {
                return;
            }

            ItemInfo* pDonateItem = GetItemInfoManager()->GetItemByID((uint16)donateItemID);
            if(!pDonateItem) {
                return;
            }

            if(pDonation->donatedItems.size() >= 20) {
                pPlayer->SendOnTalkBubble("The donation box is already full!", true);
                return;
            }

            int32 donatedByPlayer = 0;
            for(const TileExtra_DonatedItem& item : pDonation->donatedItems) {
                if(item.userID == pPlayer->GetUserID()) {
                    ++donatedByPlayer;
                }
            }

            if(donatedByPlayer >= 3 && !pWorld->PlayerHasAccessOnTile(pPlayer, pTile)) {
                pPlayer->SendOnTalkBubble("You already donated items 3 times! Try again later.", true);
                return;
            }

            if(pDonateItem->rarity < 2) {
                pPlayer->SendOnTalkBubble("This box only accepts items of rarity 2 and above.", true);
                return;
            }

            if(pDonateItem->HasFlag(ITEM_FLAG_UNTRADEABLE)) {
                pPlayer->SendOnTalkBubble("That's too special to put in there!", true);
                return;
            }

            if(pDonateItem->id == ITEM_ID_FIST || pDonateItem->id == ITEM_ID_WRENCH) {
                pPlayer->SendOnTalkBubble("You can't put that there.", true);
                return;
            }

            if(amount < 1) {
                pPlayer->SendOnTalkBubble("You can't donate nothing!", true);
                return;
            }

            const uint8 ownCount = pPlayer->GetInventory().GetCountOfItem((uint16)donateItemID);
            if(ownCount == 0) {
                pPlayer->SendOnTalkBubble("You can't donate nothing!", true);
                return;
            }

            amount = std::min<int32>(amount, ownCount);
            amount = std::min<int32>(amount, 200);
            if(amount < 1) {
                return;
            }

            const bool wasEmpty = pDonation->donatedItems.empty();

            string comment;
            auto pComment = packet.Find(CompileTimeHashString("Comment"));
            if(pComment && pComment->size > 0) {
                comment.assign(pComment->value, pComment->size);
                if(comment.size() > 128) {
                    comment.resize(128);
                }
            }

            pPlayer->ModifyInventoryItem((uint16)donateItemID, (int16)-amount);

            TileExtra_DonatedItem donation;
            donation.itemID = (uint16)donateItemID;
            donation.amount = (uint8)amount;
            donation.userID = pPlayer->GetUserID();
            donation.username = pPlayer->GetRawName();
            donation.comment = comment;
            donation.donateID = pDonation->currentDonateID++;
            donation.donatedAt = Time::GetTimeSinceEpoch();
            pDonation->donatedItems.push_back(std::move(donation));

            pPlayer->SendOnTalkBubble("`wThanks! You donated `#" + ToString(amount) + "`w `2" + string(pDonateItem->name) + "`w to the box.", true);

            const string placedMsg =
                "`7[```5[```w" + pPlayer->GetRawName() +
                " `wplaces `5" + ToString(amount) + "`` `2" + string(pDonateItem->name) +
                "`` `winto the Donation Box`5]```7]``";
            pWorld->SendTalkBubbleAndConsoleToAll(placedMsg, false, pPlayer);

            if(wasEmpty) {
                pTile->SetFlag(TILE_FLAG_IS_ON);
            }

            pWorld->SendTileUpdate(pTile);
            break;
        }

        case CompileTimeHashString("TelephoneEdit"): {
            auto pButtonClicked = packet.Find(CompileTimeHashString("buttonClicked"));
            if(pButtonClicked) {
                const string buttonClicked(pButtonClicked->value, pButtonClicked->size);
                if(buttonClicked == "GotoCrazyJimDailyQuest") {
                    ShowCrazyJimDailyQuest(pPlayer);
                    return;
                }
                if(buttonClicked == "GotoCrazyJimMainMenu") {
                    ShowCrazyJimMainMenu(pPlayer);
                    return;
                }
                if(buttonClicked == "GotoCrazyJimTurnInDailyQuest") {
                    TurnInTelephoneQuest(pPlayer);
                    return;
                }
                if(buttonClicked == "RemoveGhost") {
                    World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
                    ShowRemoveGhostConfirmDialog(pPlayer, pWorld);
                    return;
                }
                if(buttonClicked == "RemoveGhostConfirm") {
                    RemoveGhostConfirm(pPlayer);
                    return;
                }
                if(buttonClicked == "Dial" || buttonClicked == "TelephoneEdit") {
                    auto pNumber = packet.Find(CompileTimeHashString("Number"));
                    if(!pNumber) {
                        pNumber = packet.Find(CompileTimeHashString("number"));
                    }

                    if(!pNumber) {
                        return;
                    }

                    int32 phoneNumber = 0;
                    if(ToInt(string(pNumber->value, pNumber->size), phoneNumber) != TO_INT_SUCCESS) {
                        return;
                    }

                    if(phoneNumber == 12345) {
                        ShowCrazyJimMainMenu(pPlayer);
                        return;
                    }

                    if(phoneNumber == 52368) {
                        World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
                        ShowRemoveGhostDialog(pPlayer, pWorld);
                        return;
                    }

                    return;
                }
            }

            break;
        }

            case CompileTimeHashString("FossilPrepEdit"): {
                auto pButtonClicked = packet.Find(CompileTimeHashString("buttonClicked"));
                if(!pButtonClicked) {
                    return;
                }

                string buttonClicked(pButtonClicked->value, pButtonClicked->size);
                if(buttonClicked != "Prep") {
                    return;
                }

                int32 tileX = 0;
                int32 tileY = 0;
                if(!ParseIntField(packet, CompileTimeHashString("tilex"), tileX) ||
                    !ParseIntField(packet, CompileTimeHashString("tiley"), tileY)) {
                    return;
                }

                World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
                if(!pWorld) {
                    return;
                }

                TileInfo* pTile = pWorld->GetTileManager()->GetTile(tileX, tileY);
                if(!pTile) {
                    return;
                }

                FossilComponent::TryHandlePrepAction(pPlayer, pWorld, pTile);
                break;
            }

            case CompileTimeHashString("GeigerChargerEdit"): {
                auto pButtonClicked = packet.Find(CompileTimeHashString("buttonClicked"));
                if(!pButtonClicked) {
                    return;
                }

                string buttonClicked(pButtonClicked->value, pButtonClicked->size);
                if(buttonClicked != "Charge") {
                    return;
                }

                int32 tileX = 0;
                int32 tileY = 0;
                if(!ParseIntField(packet, CompileTimeHashString("tilex"), tileX) ||
                    !ParseIntField(packet, CompileTimeHashString("tiley"), tileY)) {
                    return;
                }

                World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
                if(!pWorld) {
                    return;
                }

                TileInfo* pTile = pWorld->GetTileManager()->GetTile(tileX, tileY);
                if(!pTile) {
                    return;
                }

                GeigerComponent::TryChargeCounter(pPlayer, pWorld, pTile);
                break;
            }

        case CompileTimeHashString("MannequinEdit"): {
            auto pButtonClicked = packet.Find(CompileTimeHashString("buttonClicked"));
            if(!pButtonClicked || string(pButtonClicked->value, pButtonClicked->size) != "Yes") {
                return;
            }

            int32 tileX = 0;
            int32 tileY = 0;
            int32 tileItemID = 0;
            int32 itemID = 0;
            if(!ParseIntField(packet, CompileTimeHashString("tilex"), tileX) ||
                !ParseIntField(packet, CompileTimeHashString("tiley"), tileY) ||
                !ParseIntField(packet, CompileTimeHashString("TileItemID"), tileItemID) ||
                !ParseIntField(packet, CompileTimeHashString("ItemID"), itemID))
            {
                return;
            }

            World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
            if(!pWorld) {
                return;
            }

            TileInfo* pTile = pWorld->GetTileManager()->GetTile(tileX, tileY);
            if(!pTile || pTile->GetDisplayedItem() != (uint16)tileItemID) {
                pPlayer->SendOnTalkBubble("Huh? The mannequin is gone!", true);
                return;
            }

            TileExtra_Mannequin* pMannequin = pTile->GetExtra<TileExtra_Mannequin>();
            if(!pMannequin) {
                return;
            }

            ItemInfo* pItem = GetItemInfoManager()->GetItemByID((uint16)itemID);
            if(!pItem || pItem->type != ITEM_TYPE_CLOTHES) {
                return;
            }

            uint16* pSlot = nullptr;
            switch(pItem->bodyPart) {
                case BODY_PART_BACK: pSlot = &pMannequin->clothing.back; break;
                case BODY_PART_HAND: pSlot = &pMannequin->clothing.hand; break;
                case BODY_PART_HAT: pSlot = &pMannequin->clothing.hat; break;
                case BODY_PART_FACEITEM: pSlot = &pMannequin->clothing.face; break;
                case BODY_PART_CHESTITEM: pSlot = &pMannequin->clothing.necklace; break;
                case BODY_PART_SHIRT: pSlot = &pMannequin->clothing.shirt; break;
                case BODY_PART_PANT: pSlot = &pMannequin->clothing.pants; break;
                case BODY_PART_SHOE: pSlot = &pMannequin->clothing.shoes; break;
                case BODY_PART_HAIR: pSlot = &pMannequin->clothing.hair; break;
                default: break;
            }

            if(!pSlot) {
                return;
            }

            if(pPlayer->GetInventory().GetCountOfItem((uint16)itemID) == 0) {
                return;
            }

            const uint16 oldItem = *pSlot;
            if(oldItem != ITEM_ID_BLANK && !pPlayer->GetInventory().HaveRoomForItem(oldItem, 1)) {
                pPlayer->SendOnTalkBubble("That won't fit in your backpack!", true);
                return;
            }

            pPlayer->ModifyInventoryItem((uint16)itemID, -1);
            if(oldItem != ITEM_ID_BLANK) {
                pPlayer->ModifyInventoryItem(oldItem, 1);
            }

            *pSlot = (uint16)itemID;
            pWorld->SendTileUpdate(pTile);
            break;
        }

        case CompileTimeHashString("BirthCertificate"): {
            auto pButtonClicked = packet.Find(CompileTimeHashString("buttonClicked"));
            if(!pButtonClicked) {
                return;
            }

            const string buttonClicked(pButtonClicked->value, pButtonClicked->size);
            if(buttonClicked != "Change It" && buttonClicked != "Change it!" && buttonClicked != "Change it") {
                return;
            }

            auto pNewName = packet.Find(CompileTimeHashString("NewName"));
            if(!pNewName) {
                return;
            }

            string newName(pNewName->value, pNewName->size);
            RemoveExtraWhiteSpaces(newName);

            if(!pPlayer->HasGrowID()) {
                pPlayer->SendOnTalkBubble("`4You need a GrowID first!", true);
                return;
            }

            if(pPlayer->GetCharData().HasPlayMod(PLAYMOD_TYPE_RECENTLY_NAME_CHANGED)) {
                pPlayer->SendOnTalkBubble("You can't change your `$GrowID`` again so soon!", true);
                return;
            }

            if(!IsBirthCertNameValid(newName)) {
                ShowBirthCertificateDialog(pPlayer, "`4Oops!`` Your GrowID must be 3-18 chars, with letters and numbers only.", newName);
                return;
            }

            if(ToUpper(newName) == ToUpper(pPlayer->GetRawName())) {
                ShowBirthCertificateDialog(pPlayer, "`4Oops!`` That's already your name.", newName);
                return;
            }

            if(pPlayer->GetInventory().GetCountOfItem(ITEM_ID_BIRTH_CERTIFICATE) == 0) {
                return;
            }

            pPlayer->SetBirthCertificatePendingName(newName);
            ShowBirthCertificateConfirmDialog(pPlayer, newName);
            break;
        }

        case CompileTimeHashString("BirthCertificateConfirm"): {
            auto pButtonClicked = packet.Find(CompileTimeHashString("buttonClicked"));
            if(!pButtonClicked) {
                return;
            }

            const string buttonClicked(pButtonClicked->value, pButtonClicked->size);
            if(buttonClicked != "Change It" && buttonClicked != "Change it!" && buttonClicked != "Change it") {
                return;
            }

            string newName = pPlayer->GetBirthCertificatePendingName();
            if(newName.empty()) {
                return;
            }
            RemoveExtraWhiteSpaces(newName);

            if(!pPlayer->HasGrowID()) {
                pPlayer->SendOnTalkBubble("`4You need a GrowID first!", true);
                return;
            }

            if(pPlayer->GetCharData().HasPlayMod(PLAYMOD_TYPE_RECENTLY_NAME_CHANGED)) {
                pPlayer->SendOnTalkBubble("You can't change your `$GrowID`` again so soon!", true);
                return;
            }

            if(!IsBirthCertNameValid(newName)) {
                pPlayer->ClearBirthCertificatePendingName();
                ShowBirthCertificateDialog(pPlayer, "`4Oops!`` Your GrowID must be 3-18 chars, with letters and numbers only.", newName);
                return;
            }

            if(ToUpper(newName) == ToUpper(pPlayer->GetRawName())) {
                pPlayer->ClearBirthCertificatePendingName();
                ShowBirthCertificateDialog(pPlayer, "`4Oops!`` That's already your name.", newName);
                return;
            }

            if(pPlayer->GetInventory().GetCountOfItem(ITEM_ID_BIRTH_CERTIFICATE) == 0) {
                pPlayer->ClearBirthCertificatePendingName();
                return;
            }

            pPlayer->ModifyInventoryItem(ITEM_ID_BIRTH_CERTIFICATE, -1);
            pPlayer->GetLoginDetail().tankIDName = newName;
            GetGameServer()->SetPlayerNameCache(pPlayer->GetUserID(), newName);
            pPlayer->AddPlayMod(PLAYMOD_TYPE_RECENTLY_NAME_CHANGED);
            pPlayer->SetCustomStatValue("BirthCertLastUseMS", Time::GetSystemTime());

            QueryRequest req = MakePlayerGrowIDRename(pPlayer->GetUserID(), newName, pPlayer->GetNetID());
            DatabasePlayerExec(GetContext()->GetDatabasePool(), DB_PLAYER_GROWID_RENAME, req);

            pPlayer->SendSetHasGrowID(true, pPlayer->GetLoginDetail().tankIDName, pPlayer->GetLoginDetail().tankIDPass);

            World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
            if(pWorld) {
                pWorld->SendNameChangeToAll(pPlayer);
            }

            pPlayer->SendOnPlayPositioned("audio/piano_nice.wav");
            pPlayer->SendOnTalkBubble("`wYou are now known as `#" + pPlayer->GetRawName() + "``!", true);
            pPlayer->ClearBirthCertificatePendingName();
            break;
        }

        case CompileTimeHashString("GhostBeGone"): {
            auto pButtonClicked = packet.Find(CompileTimeHashString("buttonClicked"));
            if(!pButtonClicked) {
                return;
            }

            const string buttonClicked(pButtonClicked->value, pButtonClicked->size);
            if(buttonClicked != "Remove Ghosts" && buttonClicked != "Yes remove ghost.") {
                return;
            }

            World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
            if(!pWorld) {
                return;
            }

            if(pPlayer->GetInventory().GetCountOfItem(ITEM_ID_WORLD_LOCK) < 15) {
                pPlayer->SendOnTalkBubble("You need 15 World Locks for that.", true);
                return;
            }

            if(!GhostAlgorithm::HasWorldGhosts(pWorld)) {
                pPlayer->SendOnTalkBubble("There are no ghosts in this world.", true);
                return;
            }

            pPlayer->ModifyInventoryItem(ITEM_ID_WORLD_LOCK, -15);
            GhostAlgorithm::ClearWorldGhosts(pWorld);
            pWorld->SendTalkBubbleAndConsoleToAll("`4The ghosts have vanished from the world!", true, pPlayer);
            break;
        }

        case CompileTimeHashString("RemoveGhostConfirm"): {
            auto pButtonClicked = packet.Find(CompileTimeHashString("buttonClicked"));
            if(!pButtonClicked || string(pButtonClicked->value, pButtonClicked->size) != "Yes remove ghost.") {
                return;
            }

            RemoveGhostConfirm(pPlayer);
            break;
        }

        case CompileTimeHashString("GotoCrazyJimMainMenu"): {
            ShowCrazyJimMainMenu(pPlayer);
            break;
        }

        case CompileTimeHashString("GotoCrazyJimDailyQuest"): {
            ShowCrazyJimDailyQuest(pPlayer);
            break;
        }

        case CompileTimeHashString("GotoCrazyJimTurnInDailyQuest"): {
            TurnInTelephoneQuest(pPlayer);
            break;
        }

        case CompileTimeHashString("lock_edit"):
        case CompileTimeHashString("lock_remove_self"): {
            LockDialog::Handle(pPlayer, packet);
            break;
        }

        case CompileTimeHashString("door_edit"):
        case CompileTimeHashString("DoorEdit"): {
            DoorDialog::Handle(pPlayer, packet);
            break;
        }

        case CompileTimeHashString("VendEdit"):
        case CompileTimeHashString("VendConfirm"):
        case CompileTimeHashString("Magplant"): {
            MachineDialog::Handle(pPlayer, packet);
            break;
        }

        case CompileTimeHashString("Chemsynth"): {
            ChemsynthDialog::Handle(pPlayer, packet);
            break;
        }

        case CompileTimeHashString("render_reply"): {
            RenderWorldDialog::Handle(pPlayer);
            break;
        }

        case CompileTimeHashString("command_mod_query"): {
            auto pButtonClicked = packet.Find(CompileTimeHashString("buttonClicked"));
            if(!pButtonClicked || pButtonClicked->size != 6 || string(pButtonClicked->value, pButtonClicked->size) != "Search") {
                return;
            }

            auto pTargetName = packet.Find(CompileTimeHashString("target_name"));
            if(!pTargetName || pTargetName->size == 0 || pTargetName->size > 24) {
                pPlayer->SendOnConsoleMessage("`4Invalid player name.``");
                return;
            }

            string query(pTargetName->value, pTargetName->size);
            RemoveExtraWhiteSpaces(query);

            std::vector<string> cmdArgs;
            cmdArgs.push_back("/mod");
            cmdArgs.push_back(query);
            GetGameServer()->ExecuteCommand(pPlayer, cmdArgs);
            break;
        }

        case CompileTimeHashString("command_mod_actions"): {
            auto pButtonClicked = packet.Find(CompileTimeHashString("buttonClicked"));
            if(!pButtonClicked || pButtonClicked->size == 0 || pButtonClicked->size > 40) {
                return;
            }

            string buttonClicked(pButtonClicked->value, pButtonClicked->size);

            auto onAction = buttonClicked.find('_');
            if(onAction == string::npos) {
                return;
            }

            string action = buttonClicked.substr(0, onAction);
            string netIDStr = buttonClicked.substr(onAction + 1);

            int32 netID = 0;
            if(ToInt(netIDStr, netID) != TO_INT_SUCCESS) {
                return;
            }

            GamePlayer* pTarget = GetGameServer()->GetPlayerByNetID(netID);
            if(!pTarget || pTarget == pPlayer || pTarget->GetCurrentWorld() != pPlayer->GetCurrentWorld()) {
                pPlayer->SendOnConsoleMessage("`4Target is no longer valid.``");
                return;
            }

            std::vector<string> cmdArgs;
            if(action == "pull") {
                cmdArgs.push_back("/pull");
                cmdArgs.push_back(pTarget->GetRawName());
            }
            else if(action == "kick") {
                cmdArgs.push_back("/kick");
                cmdArgs.push_back(pTarget->GetRawName());
            }
            else if(action == "ban") {
                cmdArgs.push_back("/ban");
                cmdArgs.push_back(pTarget->GetRawName());
            }
            else if(action == "warn") {
                string reason = "Please follow the rules.";
                auto pWarnReason = packet.Find(CompileTimeHashString("warn_reason"));
                if(pWarnReason && pWarnReason->size > 0 && pWarnReason->size <= 120) {
                    reason.assign(pWarnReason->value, pWarnReason->size);
                    RemoveExtraWhiteSpaces(reason);
                    if(reason.empty()) {
                        reason = "Please follow the rules.";
                    }
                }

                cmdArgs.push_back("/warn");
                cmdArgs.push_back(pTarget->GetRawName());
                cmdArgs.push_back(reason);
            }
            else if(action == "warpto") {
                cmdArgs.push_back("/warpto");
                cmdArgs.push_back(pTarget->GetRawName());
            }
            else {
                return;
            }

            GetGameServer()->ExecuteCommand(pPlayer, cmdArgs);
            break;
        }

        case CompileTimeHashString("find_item_dialog"):
        case CompileTimeHashString("FindItem"): {
            auto pButtonClicked = packet.Find(CompileTimeHashString("buttonClicked"));
            const string buttonClicked = pButtonClicked ? string(pButtonClicked->value, pButtonClicked->size) : "";
            const string loweredButtonClicked = ToLower(buttonClicked);

            if(loweredButtonClicked == "get" || loweredButtonClicked == "ok") {
                const uint16 pendingFindItemID = pPlayer->GetPendingFindItemID();
                if(pendingFindItemID == 0) {
                    break;
                }

                ItemInfo* pItem = GetItemInfoManager()->GetItemByID(pendingFindItemID);
                if(!pItem) {
                    pPlayer->ClearPendingFindItemID();
                    break;
                }

                int32 getAmount = 1;
                bool hasGetAmount = ParseIntField(packet, CompileTimeHashString("GetAmount"), getAmount);
                if(!hasGetAmount) {
                    hasGetAmount = ParseIntField(packet, CompileTimeHashString("Amount"), getAmount);
                }

                if(!hasGetAmount || getAmount < 1) {
                    getAmount = 1;
                }
                if(getAmount > 200) {
                    getAmount = 200;
                }

                const bool bypass = IsFindBypassPerm(pPlayer);
                if(IsRestrictedFindItem(*pItem) && !bypass) {
                    pPlayer->SendOnConsoleMessage("`4Oops:`` That item is unobtainable via /find.");
                    pPlayer->ClearPendingFindItemID();
                    break;
                }

                const uint8 ownCount = pPlayer->GetInventory().GetCountOfItem((uint16)pItem->id);
                int32 freeSlots = std::max<int32>(0, 200 - ownCount);
                if(freeSlots < 1 || !pPlayer->GetInventory().HaveRoomForItem((uint16)pItem->id, 1)) {
                    pPlayer->SendOnTalkBubble("`wThat won't fit in your backpack!", true);
                    break;
                }

                if(getAmount > freeSlots) {
                    getAmount = freeSlots;
                }

                if(!bypass) {
                    const uint32 gemPrice = GetFindItemPrice((uint32)getAmount, *pItem);
                    if(!pPlayer->TrySpendGems((int32)gemPrice)) {
                        pPlayer->SendOnConsoleMessage("`4Oops:`` You're out of gems for that claim.");
                        SendFindClaimDialog(pPlayer, *pItem);
                        break;
                    }

                    pPlayer->SendOnSetBux();
                    pPlayer->SendOnConsoleMessage("`oGot `w" + ToString(getAmount) + " " + pItem->name + "`` for `w" + ToString((int32)gemPrice) + "`` gems.");
                }
                else {
                    pPlayer->SendOnConsoleMessage("`oGot `w" + ToString(getAmount) + " " + pItem->name + "`` for `2FREE``.");
                }

                pPlayer->ModifyInventoryItem((uint16)pItem->id, (int16)getAmount);
                pPlayer->ClearPendingFindItemID();
                break;
            }

            uint32 selectedItemID = 0;
            if(!buttonClicked.empty() && ToUInt(buttonClicked, selectedItemID) == TO_INT_SUCCESS && selectedItemID <= UINT16_MAX) {
                ItemInfo* pSelectedItem = GetItemInfoManager()->GetItemByID((uint16)selectedItemID);
                if(!pSelectedItem) {
                    break;
                }

                if(IsRestrictedFindItem(*pSelectedItem) && !IsFindBypassPerm(pPlayer)) {
                    pPlayer->SendOnConsoleMessage("`4Oops:`` That item is unobtainable via /find.");
                    break;
                }

                SendFindClaimDialog(pPlayer, *pSelectedItem);
                break;
            }

            auto pSearch = packet.Find(CompileTimeHashString("SearchString"));
            string query;

            if(pSearch && pSearch->size > 0 && pSearch->size <= 250) {
                query.assign(pSearch->value, pSearch->size);
                RemoveExtraWhiteSpaces(query);
            }

            std::vector<string> cmdArgs;
            cmdArgs.push_back("/find");
            if(!query.empty()) {
                cmdArgs.push_back(query);
            }

            GetGameServer()->ExecuteCommand(pPlayer, cmdArgs);
            break;
        }

        case CompileTimeHashString("growid_apply"): {
            RegisterDialog::Handle(pPlayer, packet);
            break;
        }

        case CompileTimeHashString("SocialPortal"): {
            auto pButtonClicked = packet.Find(CompileTimeHashString("buttonClicked"));
            if(!pButtonClicked) {
                return;
            }

            string buttonClicked(pButtonClicked->value, pButtonClicked->size);
            if(buttonClicked == "GotoFriendsMenu") {
                pPlayer->SendFriendMenu("GotoFriendsMenu");
            }
            else if(buttonClicked == "FriendAll") {
                pPlayer->SendFriendMenu("FriendAll");
            }
            else if(buttonClicked == "SeeSent") {
                pPlayer->SendFriendMenu("SeeSent");
            }
            else if(buttonClicked == "SeeReceived") {
                pPlayer->SendFriendMenu("SeeReceived");
            }
            else if(buttonClicked == "FriendsOptions") {
                pPlayer->SendFriendMenu("FriendsOptions");
            }
            else if(buttonClicked == "ToggleFriendNotif") {
                pPlayer->SetShowFriendNotification(!pPlayer->IsShowFriendNotification());
                pPlayer->SendOnConsoleMessage(string("`oFriend notifications are now ") + (pPlayer->IsShowFriendNotification() ? "`2enabled``." : "`4disabled``."));
                pPlayer->SendSocialPortal();
            }
            else if(buttonClicked.find("GuildPromote_") == 0 || buttonClicked.find("GuildDemote_") == 0 || buttonClicked.find("GuildTransfer_") == 0) {
                Guild* pGuild = GetGuildManager()->Get(pPlayer->GetGuildID());
                if(!pGuild) {
                    break;
                }

                GuildMember* pSelfMember = pGuild->GetMember(pPlayer->GetUserID());
                if(!pSelfMember) {
                    break;
                }

                const bool isLeader = pSelfMember->Position == GuildPosition::LEADER;
                const bool isCoLeader = pSelfMember->Position == GuildPosition::CO_LEADER;
                if(!isLeader && !isCoLeader) {
                    break;
                }

                const string promotePrefix = "GuildPromote_";
                const string demotePrefix = "GuildDemote_";
                const string transferPrefix = "GuildTransfer_";

                uint32 targetUserID = 0;
                string targetText;
                if(buttonClicked.find(promotePrefix) == 0) {
                    targetText = buttonClicked.substr(promotePrefix.size());
                }
                else if(buttonClicked.find(demotePrefix) == 0) {
                    targetText = buttonClicked.substr(demotePrefix.size());
                }
                else {
                    targetText = buttonClicked.substr(transferPrefix.size());
                }

                if(ToUInt(targetText, targetUserID) != TO_INT_SUCCESS || targetUserID == pPlayer->GetUserID()) {
                    break;
                }

                GuildMember* pTargetMember = pGuild->GetMember(targetUserID);
                if(!pTargetMember) {
                    break;
                }

                if(buttonClicked.find(promotePrefix) == 0) {
                    if(isLeader) {
                        if(pTargetMember->Position < GuildPosition::LEADER) {
                            pTargetMember->Position = static_cast<GuildPosition>(static_cast<uint32>(pTargetMember->Position) + 1);
                        }
                    }
                    else if(isCoLeader && pTargetMember->Position == GuildPosition::MEMBER) {
                        pTargetMember->Position = GuildPosition::ELDER;
                    }
                }
                else if(buttonClicked.find(demotePrefix) == 0) {
                    if(isLeader) {
                        if(pTargetMember->Position > GuildPosition::MEMBER) {
                            pTargetMember->Position = static_cast<GuildPosition>(static_cast<uint32>(pTargetMember->Position) - 1);
                        }
                    }
                    else if(isCoLeader && pTargetMember->Position == GuildPosition::ELDER) {
                        pTargetMember->Position = GuildPosition::MEMBER;
                    }
                }
                else if(isLeader) {
                    if(pTargetMember->Position != GuildPosition::LEADER) {
                        pTargetMember->Position = GuildPosition::LEADER;
                        pSelfMember->Position = GuildPosition::CO_LEADER;
                        pGuild->SetOwnerID(targetUserID);
                    }
                }

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
                    pPlayer->GetNetID()
                );

                DatabaseGuildExec(GetContext()->GetDatabasePool(), DB_GUILD_UPDATE, updateGuildReq);
                pPlayer->SendGuildMenu("GotoGuildMenu");
                break;
            }
            else if(buttonClicked == "GotoGuildMenu" || buttonClicked == "CreateGuild") {
                if(buttonClicked == "GotoGuildMenu") {
                    if(pPlayer->GetGuildID() != 0) {
                        pPlayer->SendGuildMenu(buttonClicked);
                    }
                    break;
                }

                if(pPlayer->GetGuildID() != 0) {
                    break;
                }

                DialogBuilder db;
                db.SetDefaultColor('o')
                    ->AddLabelWithIcon("`wGrow Guilds Creation``", ITEM_ID_GUILD_LOCK, true)
                    ->AddLabel("Welcome to Grow Guilds where you can create a Guild! With a Guild you can compete in Guild Leaderboards to earn rewards and level up the Guild to add more members.")
                    ->AddSpacer()
                    ->AddLabel("To create a Guild you must be atleast level 20.")
                    ->AddLabel("The cost for creating a guild in this server is 500,000 Gems.")
                    ->AddSpacer()
                    ->AddLabel("`8Caution``: A guild can only be created in a world owned by you and locked with a `5World Lock``!")
                    ->AddSpacer()
                    ->AddButton("ShowCreateGuild", "Create a Guild")
                    ->AddButton("GotoSocialPortal", "Back")
                    ->EndDialog("SocialPortal", "", "");

                pPlayer->SendOnDialogRequest(db.Get());
            }
            else if(buttonClicked == "ShowCreateGuild") {
                if(pPlayer->GetGuildID() != 0) {
                    break;
                }

                if(pPlayer->GetLevel() < 20) {
                    DialogBuilder db;
                    db.SetDefaultColor('o')
                        ->AddLabelWithIcon("`wGrow Guilds Creation``", ITEM_ID_GUILD_LOCK, true)
                        ->AddLabel("`9You must be atleast level 20 to create a guild.")
                        ->EndDialog("SocialPortal", "", "Close");

                    pPlayer->SendOnDialogRequest(db.Get());
                    break;
                }

                DialogBuilder db;
                db.SetDefaultColor('o')
                    ->AddLabelWithIcon("`wGrow Guilds Creation``", ITEM_ID_GUILD_LOCK, true)
                    ->AddTextInput("GuildName", "Guild Name:", "", 18)
                    ->AddTextInput("GuildStatement", "Guild Statement:", "", 50)
                    ->AddLabel("`oConfirm your guild settings by selecting `2Create Guild`o below to create your guild.")
                    ->AddLabel("`9Take Note: `oA guild can only be created in a world owned by you and locked with a `#World Lock!```o")
                    ->AddSpacer()
                    ->AddLabel("`4Warning! `pThe guild name could only be changed by buying a `#Guild Name Certificate`o from the premium store.")
                    ->AddButton("FinalizeCreateGuild", "Proceed")
                    ->AddButton("GotoSocialPortal", "Back")
                    ->EndDialog("SocialPortal", "", "");

                pPlayer->SendOnDialogRequest(db.Get());
            }
            else if(buttonClicked == "FinalizeCreateGuild") {
                auto pGuildName = packet.Find(CompileTimeHashString("GuildName"));
                auto pGuildStatement = packet.Find(CompileTimeHashString("GuildStatement"));
                if(!pGuildName || !pGuildStatement) {
                    break;
                }

                string guildName(pGuildName->value, pGuildName->size);
                string guildStatement(pGuildStatement->value, pGuildStatement->size);

                RemoveAllSpaces(guildName);
                RemoveExtraWhiteSpaces(guildStatement);

                auto sendCreateDialog = [&](const string& errorLabel) {
                    DialogBuilder db;
                    db.SetDefaultColor('o')
                        ->AddLabelWithIcon("`wGrow Guilds Creation``", ITEM_ID_GUILD_LOCK, true)
                        ->AddLabel(errorLabel)
                        ->AddTextInput("GuildName", "Guild Name:", "", 18)
                        ->AddTextInput("GuildStatement", "Guild Statement:", "", 50)
                        ->AddLabel("`oConfirm your guild settings by selecting `2Create Guild`o below to create your guild.")
                        ->AddLabel("`9Take Note: `oA guild can only be created in a world owned by you and locked with a `#World Lock!```o")
                        ->AddSpacer()
                        ->AddLabel("`4Warning! `pThe guild name could only be changed by buying a `#Guild Name Certificate`o from the premium store.")
                        ->AddButton("FinalizeCreateGuild", "Proceed")
                        ->AddButton("GotoSocialPortal", "Back")
                        ->EndDialog("SocialPortal", "", "");

                    pPlayer->SendOnDialogRequest(db.Get());
                };

                if(guildName.empty() || guildStatement.empty()) {
                    sendCreateDialog("`9Guild name or statement cannot be empty!");
                    break;
                }

                bool isAlphaNumeric = true;
                for(char c : guildName) {
                    if(!IsAlpha(c) && !IsDigit(c)) {
                        isAlphaNumeric = false;
                        break;
                    }
                }

                if(!isAlphaNumeric) {
                    sendCreateDialog("`9Your guild name includes invalid charaters. Only A-Z, a-z and numbers from 0-9 are allowed.");
                    break;
                }

                if(guildName.size() > 18) {
                    sendCreateDialog("`9Guild name is too long!");
                    break;
                }

                if(guildName.size() < 3) {
                    sendCreateDialog("`9Guild name is too short!");
                    break;
                }

                if(guildStatement.size() > 50) {
                    sendCreateDialog("`9Guild statement is too long!");
                    break;
                }

                World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
                if(!pWorld || !pWorld->IsPlayerWorldOwner(pPlayer)) {
                    sendCreateDialog("`9You can only create a guild in a world that you own!");
                    break;
                }

                if(pPlayer->GetGems() < 500000) {
                    sendCreateDialog("`9You don't have enough gems to create a guild!");
                    break;
                }

                bool nameTaken = false;
                GetGuildManager()->Guilds([&](uint64, Guild* pGuild) {
                    if(pGuild && ToUpper(pGuild->GetName()) == ToUpper(guildName)) {
                        nameTaken = true;
                    }
                });

                if(nameTaken) {
                    sendCreateDialog("`9That guild name exists already! Try to think of a better name.");
                    break;
                }

                DialogBuilder db;
                db.SetDefaultColor('o')
                    ->AddLabelWithIcon("`wGrow Guilds Creation``", ITEM_ID_GUILD_LOCK, true)
                    ->AddLabel("`9WARNING: `oPlease confirm the information before proceeding.")
                    ->AddLabel("`3Guild Name: `w" + guildName)
                    ->AddLabel("`3Guild Statement: `w" + guildStatement)
                    ->AddSpacer()
                    ->AddLabel("`9You will be charged: `41,000,000 Gems")
                    ->EmbedData("GuildName", guildName)
                    ->EmbedData("GuildStatement", guildStatement)
                    ->AddButton("ConfirmGuildCreation", "Confirm Guild Creation")
                    ->AddButton("ShowCreateGuild", "Back")
                    ->EndDialog("SocialPortal", "", "");

                pPlayer->SendOnDialogRequest(db.Get());
            }
            else if(buttonClicked == "ConfirmGuildCreation") {
                auto pGuildName = packet.Find(CompileTimeHashString("GuildName"));
                auto pGuildStatement = packet.Find(CompileTimeHashString("GuildStatement"));
                if(!pGuildName || !pGuildStatement) {
                    break;
                }

                string guildName(pGuildName->value, pGuildName->size);
                string guildStatement(pGuildStatement->value, pGuildStatement->size);
                RemoveAllSpaces(guildName);
                RemoveExtraWhiteSpaces(guildStatement);

                if(guildName.empty() || guildStatement.empty()) {
                    break;
                }

                pPlayer->SetPendingGuildCreationData(guildName, guildStatement);

                if(!pPlayer->CreateGuildFromPendingData()) {
                    pPlayer->SendOnConsoleMessage("`4OOPS! ``An issue has occured on one of our internal services.");
                }
                else {
                    pPlayer->SendGuildMenu("GotoGuildMenu");
                }
            }
            else if(buttonClicked == "GotoTradeHistory") {
                DialogBuilder db;
                db.SetDefaultColor('o')
                    ->AddLabelWithIcon("`wTrade History``", ITEM_ID_WRENCH, true)
                    ->AddSpacer();

                const auto& history = pPlayer->GetTradeHistory();
                if(history.empty()) {
                    db.AddLabel("No recent trade history.");
                }
                else {
                    for(const auto& entry : history) {
                        db.AddLabel("`o- ``" + entry);
                    }
                }

                db.AddSpacer()
                    ->AddButton("BackToSocialPortal", "Back")
                    ->EndDialog("SocialPortal", "", "Close");

                pPlayer->SendOnDialogRequest(db.Get());
            }
            else if(buttonClicked == "BackToSocialPortal") {
                pPlayer->SendSocialPortal();
            }
            else if(buttonClicked == "BackToWrench") {
                pPlayer->SendWrenchSelf("PlayerInfo");
            }
            break;
        }

        case CompileTimeHashString("GuildInvitations"): {
            auto pButtonClicked = packet.Find(CompileTimeHashString("buttonClicked"));
            if(!pButtonClicked) {
                break;
            }

            string buttonClicked(pButtonClicked->value, pButtonClicked->size);

            if(buttonClicked.find("AcceptGuildInvite_") == 0) {
                string guildIDStr = buttonClicked.substr(18);
                uint64 guildID = 0;
                if(ParseUInt64Field(guildIDStr, guildID)) {
                    pPlayer->HandleGuildInviteResponse(guildID, true);
                }
            }
            else if(buttonClicked.find("RejectGuildInvite_") == 0) {
                string guildIDStr = buttonClicked.substr(18);
                uint64 guildID = 0;
                if(ParseUInt64Field(guildIDStr, guildID)) {
                    pPlayer->HandleGuildInviteResponse(guildID, false);
                }
            }

            break;
        }

        case CompileTimeHashString("FriendMenu"): {
            auto pButtonClicked = packet.Find(CompileTimeHashString("buttonClicked"));
            if(!pButtonClicked) {
                return;
            }

            string buttonClicked(pButtonClicked->value, pButtonClicked->size);
            if(buttonClicked == "GotoSocialPortal") {
                pPlayer->SendSocialPortal();
            }
            else if(buttonClicked == "FriendAll") {
                pPlayer->SendFriendMenu("FriendAll");
            }
            else if(buttonClicked == "FriendsOptions") {
                pPlayer->SendFriendMenu("FriendsOptions");
            }
            else if(buttonClicked == "GotoFriendsMenuAndApplyOptions") {
                bool showFriendNotifications = false;
                if(ParseBoolField(packet, CompileTimeHashString("checkbox_notifications"), showFriendNotifications)) {
                    pPlayer->SetShowFriendNotification(showFriendNotifications);
                }

                pPlayer->SendFriendMenu("GotoFriendsMenu");
            }
            else if(buttonClicked == "SeeSent") {
                pPlayer->SendFriendMenu("SeeSent");
            }
            else if(buttonClicked == "SeeReceived") {
                pPlayer->SendFriendMenu("SeeReceived");
            }
            else if(buttonClicked.rfind("friend_accept_", 0) == 0) {
                uint32 userID = 0;
                if(ToUInt(buttonClicked.substr(14), userID) == TO_INT_SUCCESS && userID != 0) {
                    GamePlayer* pTarget = GetGameServer()->GetPlayerByUserID(userID);
                    if(pTarget) {
                        if(!pPlayer->AcceptFriendRequestFrom(pTarget)) {
                            pPlayer->SendOnConsoleMessage("`4Unable to accept this friend request right now.``");
                        }
                    }
                    else {
                        pPlayer->DenyFriendRequestFrom(userID);
                        pPlayer->SendOnConsoleMessage("`oRequest cleaned up because that player is offline.``");
                    }
                }

                pPlayer->SendFriendMenu("SeeReceived");
            }
            else if(buttonClicked.rfind("friend_deny_", 0) == 0) {
                uint32 userID = 0;
                if(ToUInt(buttonClicked.substr(12), userID) == TO_INT_SUCCESS && userID != 0) {
                    pPlayer->DenyFriendRequestFrom(userID);
                }

                pPlayer->SendFriendMenu("SeeReceived");
            }
            break;
        }
    }
}