#include "TCPEventCrossServerAction.h"
#include "../../Server/GameServer.h"

void TCPEventCrossServerAction::Execute(NetClient* pClient, VariantVector& data)
{
    GetGameServer()->HandleCrossServerAction(std::move(data));
}
