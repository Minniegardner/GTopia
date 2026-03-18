#pragma once

#include "Precompiled.h"

// actually not sure need thread safe queue so need to decide concurrent or mutex
#include <concurrentqueue.h>

struct WorldRenderInfo
{
    uint64 reqTime = 0;
    uint32 worldID = 0;
    uint32 userID = 0;
};

class WorldRendererManager {
public:
    WorldRendererManager();
    ~WorldRendererManager();

public:
    static WorldRendererManager* GetInstance()
    {
        static WorldRendererManager instance;
        return &instance;
    }

public:
    void Update();
    void AddTask(uint32 worldID, uint32 userID);

private:
    moodycamel::ConcurrentQueue<WorldRenderInfo> m_renderQueue;
    uint64 sleepUntilMS;
};

WorldRendererManager* GetWorldRendererManager();