#include "WorldRendererManager.h"
#include "WorldRenderer.h"
#include "Utils/Timer.h"
#include "MasterBroadway.h"
#include "IO/Log.h"

WorldRendererManager::WorldRendererManager()
{
}

WorldRendererManager::~WorldRendererManager()
{
}

void WorldRendererManager::Update()
{
    uint64 startTime = Time::GetSystemTime();
    MasterBroadway* pMasterBroadway = GetMasterBroadway();

    WorldRenderInfo renderInfo;
    while(m_renderQueue.try_dequeue(renderInfo)) {
        if(renderInfo.userID == 0) {
            LOGGER_LOG_ERROR("User id is 0? skipping");
            continue;
        }

        if(renderInfo.worldID == 0) {
            LOGGER_LOG_ERROR("World id is 0? skippping");
            pMasterBroadway->SendWorldRenderResult(TCP_RENDER_WORLD_FAIL, renderInfo.userID, renderInfo.worldID);
            continue;
        }

        if(startTime - renderInfo.reqTime > 5000) {
            LOGGER_LOG_ERROR("Render time exceed %d skipping", startTime - renderInfo.reqTime);
            pMasterBroadway->SendWorldRenderResult(TCP_RENDER_WORLD_FAIL, renderInfo.userID, renderInfo.worldID);
            continue;
        }

        WorldRenderer renderWorld;
        if(!renderWorld.LoadWorld(renderInfo.worldID)) {
            LOGGER_LOG_ERROR("Failed to load world %d, skipping", renderInfo.worldID);
            pMasterBroadway->SendWorldRenderResult(TCP_RENDER_WORLD_FAIL, renderInfo.userID, renderInfo.worldID);
            continue;   
        }

        renderWorld.Draw();
        pMasterBroadway->SendWorldRenderResult(TCP_RENDER_WORLD_SUCCSESS, renderInfo.userID, renderInfo.worldID);

        LOGGER_LOG_INFO("Rendered world %d took %dms", renderInfo.worldID, Time::GetSystemTime() - startTime);
    }
}

void WorldRendererManager::AddTask(uint32 worldID, uint32 playerNetID)
{
    WorldRenderInfo renderInfo;
    renderInfo.reqTime = Time::GetSystemTime();
    renderInfo.worldID = worldID;
    renderInfo.userID = playerNetID;

    m_renderQueue.enqueue(std::move(renderInfo));
}

WorldRendererManager* GetWorldRendererManager() { return WorldRendererManager::GetInstance(); }
