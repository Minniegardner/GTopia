#pragma once

#include <windows.h>

#include <enet/enet.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

class ReGTClient {
public:
    using LogCallback = std::function<void(const std::string&)>;
    using EventCallback = std::function<void(const std::string&, const std::string&)>;

    ReGTClient();
    ~ReGTClient();

    bool Start(const std::string& host, uint16_t port, LogCallback callback, EventCallback eventCallback = {});
    void Stop();

    bool IsConnected() const;
    bool IsHelloReceived() const;

    void QueueLogin(const std::string& growId, const std::string& password);
    void QueueJoinWorld(const std::string& worldName);
    void QueueRawTextMessage(const std::string& payload);

private:
    struct GameUpdatePacket {
        uint8_t type;
        uint8_t field0;
        uint8_t field1;
        uint8_t field2;
        int32_t field3;
        int32_t field4;
        int32_t flags;
        float field5;
        int32_t field6;
        float field7;
        float field8;
        float field9;
        float field10;
        float field11;
        uint32_t field12;
        uint32_t field13;
        uint32_t extraDataSize;
    };

    enum : uint32_t {
        NET_MESSAGE_SERVER_HELLO = 1,
        NET_MESSAGE_GENERIC_TEXT = 2,
        NET_MESSAGE_GAME_MESSAGE = 3,
        NET_MESSAGE_GAME_PACKET = 4
    };

    enum : uint8_t {
        NET_GAME_PACKET_CALL_FUNCTION = 1
    };

    enum : int32_t {
        NET_GAME_PACKET_FLAGS_EXTENDED = 1 << 3
    };

    void WorkerMain();
    void EmitLog(const std::string& line);
    void EmitEvent(const std::string& eventName, const std::string& payload);

    void ProcessOutgoingQueue();
    void ProcessReceive(ENetPacket* packet);

    void HandleTextPacket(const char* payload, size_t payloadLen, const char* packetKind);
    void HandleGamePacket(const uint8_t* payload, size_t payloadLen);
    void HandleCallFunction(const uint8_t* payload, size_t payloadLen);

    std::string BuildLoginPayload(const std::string& growId, const std::string& password) const;
    static std::string MakeRandomMac();
    static std::string MakePseudoSid();
    static std::string TrimColorCodes(const std::string& in);

private:
    ENetHost* m_client = nullptr;
    ENetPeer* m_peer = nullptr;

    std::atomic<bool> m_running = false;
    std::atomic<bool> m_connected = false;
    std::atomic<bool> m_helloReceived = false;
    bool m_enetInitialized = false;

    std::thread m_worker;

    mutable std::mutex m_queueMutex;
    std::queue<std::string> m_outgoingText;

    LogCallback m_logCallback;
    EventCallback m_eventCallback;
};
