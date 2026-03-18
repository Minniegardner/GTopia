#include "TCPEventRenderWorld.h"
#include "../WorldRendererManager.h"

void TCPEventRenderWorld::Execute(NetClient* pClient, VariantVector& data)
{
    if(data.size() < 3) {
        /**
         * send fail
         */
        return;
    }

    GetWorldRendererManager()->AddTask(data[1].GetUINT(), data[2].GetUINT());
}