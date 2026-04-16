#include "ChemsynthDialog.h"

#include "IO/Log.h"
#include "Item/ItemInfoManager.h"
#include "Utils/DialogBuilder.h"
#include "Utils/StringUtils.h"
#include "World/TileInfo.h"
#include "World/WorldManager.h"
#include "Algorithm/ChemsynthAlgorithm.h"

namespace {

bool ParseIntField(ParsedTextPacket<8>& packet, uint32 key, int32& out)
{
    auto pField = packet.Find(key);
    if(!pField) {
        return false;
    }

    return ToInt(string(pField->value, pField->size), out) == TO_INT_SUCCESS;
}

void ShowInstructionManual(GamePlayer* pPlayer)
{
    if(!pPlayer) {
        return;
    }

    DialogBuilder db;
    db.SetDefaultColor('o')
        ->AddLabelWithIcon("`9GrowTech Chemsynth Manual``", ITEM_ID_CHEMSYNTH_PROCESSOR, true)
        ->AddSpacer()
        ->AddTextBox("Place a Chemsynth Processor with exactly 10 Chemsynth Tanks connected in a straight line to its right.")
        ->AddTextBox("To start synthesis, you need at least 5 of each basic Chemical: R, Y, G, B, and P.")
        ->AddTextBox("While synthesis is running, the active tank colors move forward until a tank matches its target color.")
        ->AddTextBox("When all 10 tanks match their targets, the processor rewards 1 Synthetic Chemical.")
        ->EndDialog("ChemsynthManual", "", "Close");

    pPlayer->SendOnDialogRequest(db.Get());
}

void ShowStartConfirmDialog(GamePlayer* pPlayer, TileInfo* pProcessorTile)
{
    if(!pPlayer || !pProcessorTile) {
        return;
    }

    DialogBuilder db;
    db.SetDefaultColor('o')
        ->AddLabelWithIcon("`9GrowTech Chemsynth``", ITEM_ID_CHEMSYNTH_PROCESSOR, true)
        ->AddSpacer()
        ->AddTextBox("You have " + ToString(pPlayer->GetInventory().GetCountOfItem(ITEM_ID_CHEMICAL_R)) + " Chemical R, " + ToString(pPlayer->GetInventory().GetCountOfItem(ITEM_ID_CHEMICAL_Y)) + " Chemical Y, " + ToString(pPlayer->GetInventory().GetCountOfItem(ITEM_ID_CHEMICAL_G)) + " Chemical G, " + ToString(pPlayer->GetInventory().GetCountOfItem(ITEM_ID_CHEMICAL_B)) + " Chemical B, and " + ToString(pPlayer->GetInventory().GetCountOfItem(ITEM_ID_CHEMICAL_P)) + " Chemical P.")
        ->EmbedData("tilex", pProcessorTile->GetPos().x)
        ->EmbedData("tiley", pProcessorTile->GetPos().y)
        ->EmbedData("TileItemID", pProcessorTile->GetDisplayedItem());

    if(
        pPlayer->GetInventory().GetCountOfItem(ITEM_ID_CHEMICAL_R) < 5 ||
        pPlayer->GetInventory().GetCountOfItem(ITEM_ID_CHEMICAL_Y) < 5 ||
        pPlayer->GetInventory().GetCountOfItem(ITEM_ID_CHEMICAL_G) < 5 ||
        pPlayer->GetInventory().GetCountOfItem(ITEM_ID_CHEMICAL_B) < 5 ||
        pPlayer->GetInventory().GetCountOfItem(ITEM_ID_CHEMICAL_P) < 5
    ) {
        db.AddLabel("`4Error: You need 5 of each Chemical to start, and we recommend at least 30 of each, as every Chemsynth tool uses up 1 chemical of the selected color.``")
            ->EndDialog("Chemsynth", "", "Exit");
    }
    else {
        db.AddSpacer()
            ->AddLabel("Are you sure you want to spend `25 of each Chemical`` to begin Chemical Synthesis?")
            ->AddSpacer()
            ->AddButton("Start", "Really Start")
            ->EndDialog("Chemsynth", "", "No");
    }

    pPlayer->SendOnDialogRequest(db.Get());
}

}

namespace ChemsynthDialog {

void Request(GamePlayer* pPlayer, TileInfo* pTile)
{
    if(!pPlayer || !pTile || pPlayer->GetCurrentWorld() == 0) {
        return;
    }

    World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
    if(!pWorld) {
        return;
    }

    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(pTile->GetDisplayedItem());
    if(!pItem) {
        return;
    }

    TileInfo* pProcessorTile = pTile;
    if(pItem->type == ITEM_TYPE_CHEMTANK) {
        pProcessorTile = ChemsynthAlgorithm::FindProcessorTile(pWorld, pTile);
        if(!pProcessorTile) {
            return;
        }
    }

    const bool ready = ChemsynthAlgorithm::IsChemsynthReady(pWorld, pProcessorTile);
    const uint32 percent = ChemsynthAlgorithm::GetPercentFinished(pWorld, pProcessorTile);
    const bool active = pProcessorTile->HasFlag(TILE_FLAG_IS_ON);

    DialogBuilder db;
    db.SetDefaultColor('o')
        ->AddLabelWithIcon("`9GrowTech Chemsynth``", ITEM_ID_CHEMSYNTH_PROCESSOR, true)
        ->AddSpacer()
        ->EmbedData("tilex", pProcessorTile->GetPos().x)
        ->EmbedData("tiley", pProcessorTile->GetPos().y)
        ->EmbedData("TileItemID", pProcessorTile->GetDisplayedItem());

    if(!ready) {
        db.AddLabel("The Chemsynth can't function without exactly 10 Chemsynth Tanks connected in a straight line to its right.");
        db.AddSpacer()
            ->AddButton("Help", "Instruction Manual")
            ->EndDialog("Chemsynth", "", "Exit");
    }
    else if(active) {
        db.AddLabel("Chemical Synthesis System online. Synthesis in progress: " + ToString(percent) + "% complete.")
            ->AddSpacer()
            ->AddButton("Help", "Instruction Manual")
            ->AddButton("Shutdown", "Shutdown")
            ->EndDialog("Chemsynth", "", "Exit");
    }
    else {
        db.AddLabel("Chemical Synthesis System online. Press Start to begin synthesis.")
            ->AddSpacer()
            ->AddButton("Help", "Instruction Manual")
            ->EndDialog("Chemsynth", "Start", "Exit");
    }

    pPlayer->SendOnDialogRequest(db.Get());
}

void Handle(GamePlayer* pPlayer, ParsedTextPacket<8>& packet)
{
    if(!pPlayer || pPlayer->GetCurrentWorld() == 0) {
        return;
    }

    int32 tileX = 0;
    int32 tileY = 0;
    if(!ParseIntField(packet, CompileTimeHashString("tilex"), tileX) || !ParseIntField(packet, CompileTimeHashString("tiley"), tileY)) {
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

    auto pButtonClicked = packet.Find(CompileTimeHashString("buttonClicked"));
    string buttonClicked = pButtonClicked ? string(pButtonClicked->value, pButtonClicked->size) : "";
    string buttonLower = ToLower(buttonClicked);

    LOGGER_LOG_DEBUG("Chemsynth dialog button='%s' world=%u tile=%d,%d", buttonClicked.c_str(), pPlayer->GetCurrentWorld(), tileX, tileY);
    pPlayer->SendOnConsoleMessage("ChemsynthDebug: button='" + (buttonClicked.empty() ? string("<empty>") : buttonClicked) + "'");

    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(pTile->GetDisplayedItem());
    if(!pItem) {
        return;
    }

    if(buttonLower.empty()) {
        TileInfo* pProcessorTile = pTile->GetDisplayedItem() == ITEM_ID_CHEMSYNTH_PROCESSOR ? pTile : ChemsynthAlgorithm::FindProcessorTile(pWorld, pTile);
        if(!pProcessorTile) {
            pPlayer->SendOnTalkBubble("Chemsynth processor not found.", true);
            pPlayer->SendOnConsoleMessage("Chemsynth processor not found.");
            return;
        }

        ShowStartConfirmDialog(pPlayer, pProcessorTile);
        return;
    }

    if(buttonLower == "help") {
        ShowInstructionManual(pPlayer);
        return;
    }

    if(buttonLower == "start" || buttonLower == "ok" || buttonLower == "confirm" || buttonLower == "yes" || buttonLower == "accept") {
        TileInfo* pProcessorTile = pTile->GetDisplayedItem() == ITEM_ID_CHEMSYNTH_PROCESSOR ? pTile : ChemsynthAlgorithm::FindProcessorTile(pWorld, pTile);
        if(!pProcessorTile) {
            pPlayer->SendOnTalkBubble("Chemsynth processor not found.", true);
            pPlayer->SendOnConsoleMessage("Chemsynth processor not found.");
            return;
        }

        if(pProcessorTile->HasFlag(TILE_FLAG_IS_ON)) {
            pPlayer->SendOnTalkBubble("Chemical synthesis is already running.", true);
            pPlayer->SendOnConsoleMessage("Chemical synthesis is already running.");
            return;
        }

        ChemsynthAlgorithm::StartChemsynth(pPlayer, pWorld, pProcessorTile);
        return;
    }

    if(buttonLower == "shutdown") {
        TileInfo* pProcessorTile = pTile->GetDisplayedItem() == ITEM_ID_CHEMSYNTH_PROCESSOR ? pTile : ChemsynthAlgorithm::FindProcessorTile(pWorld, pTile);
        if(pProcessorTile) {
            ChemsynthAlgorithm::CancelChemsynth(pWorld, pProcessorTile);
        }
        return;
    }

    if(buttonLower == "exit" || buttonLower == "no" || buttonLower == "cancel" || buttonLower == "close") {
        return;
    }

    if(!buttonClicked.empty()) {
        pPlayer->SendOnConsoleMessage("Chemsynth: unknown button action '" + buttonClicked + "'.");
    }
}

}