#include "TCPEventRenderWorld.h"
#include "../WorldRendererManager.h"

void TCPRenderWorldEventData::FromVariant(VariantVector& varVec)
{
    if(varVec.size() < 4) {
        return;
    }

    subType = varVec[1].GetINT();
    playerUserID = varVec[2].GetUINT();
    worldID = varVec[3].GetUINT();
}

void TCPEventRenderWorld::Execute(NetClient* pClient, VariantVector& data)
{
    TCPRenderWorldEventData eventData;
    eventData.FromVariant(data);

    if(eventData.subType != TCP_RENDER_REQUEST) {
        LOGGER_LOG_ERROR("HUH!? Client tried to send a result instead of request? LOL");
        return;
    }

    GetWorldRendererManager()->AddTask(eventData.playerUserID, eventData.worldID);
}
