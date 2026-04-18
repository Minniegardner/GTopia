#include <windows.h>

#include "ReGTClient.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <map>
#include <random>
#include <sstream>
#include <string_view>
#include <vector>

namespace {

constexpr uint16_t kDefaultServerPort = 17196;

uint32_t ReadLeU32(const uint8_t* ptr)
{
    return static_cast<uint32_t>(ptr[0]) |
        (static_cast<uint32_t>(ptr[1]) << 8) |
        (static_cast<uint32_t>(ptr[2]) << 16) |
        (static_cast<uint32_t>(ptr[3]) << 24);
}

std::string BuildActionPacket(const std::map<std::string, std::string>& fields)
{
    std::ostringstream out;
    for(const auto& [k, v] : fields) {
        out << k << "|" << v << "\n";
    }
    return out.str();
}

std::string ToUtf8(const std::wstring& ws)
{
    if(ws.empty()) {
        return {};
    }

    int size = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()), nullptr, 0, nullptr, nullptr);
    if(size <= 0) {
        return {};
    }

    std::string out;
    out.resize(static_cast<size_t>(size));
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()), out.data(), size, nullptr, nullptr);
    return out;
}

} // namespace

ReGTClient::ReGTClient() = default;

ReGTClient::~ReGTClient()
{
    Stop();
}

bool ReGTClient::Start(const std::string& host, uint16_t port, LogCallback callback, EventCallback eventCallback)
{
    Stop();

    if(enet_initialize() != 0) {
        return false;
    }

    m_logCallback = std::move(callback);
    m_eventCallback = std::move(eventCallback);
    m_enetInitialized = true;

    m_client = enet_host_create(nullptr, 1, 2, 0, 0);
    if(!m_client) {
        enet_deinitialize();
        m_enetInitialized = false;
        return false;
    }

    ENetAddress address{};
    if(enet_address_set_host(&address, host.c_str()) != 0) {
        EmitLog("[Client] Failed to resolve host: " + host);
        enet_host_destroy(m_client);
        m_client = nullptr;
        enet_deinitialize();
        m_enetInitialized = false;
        return false;
    }

    address.port = port == 0 ? kDefaultServerPort : port;

    m_peer = enet_host_connect(m_client, &address, 2, 0);
    if(!m_peer) {
        EmitLog("[Client] Failed to create ENet peer");
        enet_host_destroy(m_client);
        m_client = nullptr;
        enet_deinitialize();
        m_enetInitialized = false;
        return false;
    }

    m_running = true;
    m_connected = false;
    m_helloReceived = false;

    m_worker = std::thread(&ReGTClient::WorkerMain, this);
    return true;
}

void ReGTClient::Stop()
{
    m_running = false;

    if(m_worker.joinable()) {
        m_worker.join();
    }

    if(m_peer) {
        enet_peer_reset(m_peer);
        m_peer = nullptr;
    }

    if(m_client) {
        enet_host_destroy(m_client);
        m_client = nullptr;
    }

    if(m_enetInitialized) {
        enet_deinitialize();
        m_enetInitialized = false;
    }

    {
        std::scoped_lock lock(m_queueMutex);
        while(!m_outgoingText.empty()) {
            m_outgoingText.pop();
        }
    }

    m_connected = false;
    m_helloReceived = false;
}

bool ReGTClient::IsConnected() const
{
    return m_connected;
}

bool ReGTClient::IsHelloReceived() const
{
    return m_helloReceived;
}

void ReGTClient::QueueLogin(const std::string& growId, const std::string& password)
{
    QueueRawTextMessage(BuildLoginPayload(growId, password));
}

void ReGTClient::QueueJoinWorld(const std::string& worldName)
{
    std::map<std::string, std::string> fields;
    fields["action"] = "join_request";
    fields["name"] = worldName;
    QueueRawTextMessage(BuildActionPacket(fields));
}

void ReGTClient::QueueRawTextMessage(const std::string& payload)
{
    std::scoped_lock lock(m_queueMutex);
    m_outgoingText.push(payload);
}

void ReGTClient::WorkerMain()
{
    EmitLog("[Client] Connecting...");

    ENetEvent event{};
    while(m_running) {
        while(enet_host_service(m_client, &event, 16) > 0) {
            switch(event.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    m_connected = true;
                    EmitLog("[Net] Connected to server");
                    break;

                case ENET_EVENT_TYPE_RECEIVE:
                    ProcessReceive(event.packet);
                    enet_packet_destroy(event.packet);
                    break;

                case ENET_EVENT_TYPE_DISCONNECT:
                    m_connected = false;
                    m_helloReceived = false;
                    EmitLog("[Net] Disconnected from server");
                    m_running = false;
                    break;

                default:
                    break;
            }
        }

        ProcessOutgoingQueue();
        enet_host_flush(m_client);

        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }

    EmitLog("[Client] Network worker stopped");
}

void ReGTClient::EmitLog(const std::string& line)
{
    if(m_logCallback) {
        m_logCallback(line);
    }
}

void ReGTClient::EmitEvent(const std::string& eventName, const std::string& payload)
{
    if(m_eventCallback) {
        m_eventCallback(eventName, payload);
    }
}

void ReGTClient::ProcessOutgoingQueue()
{
    if(!m_peer || m_peer->state != ENET_PEER_STATE_CONNECTED) {
        return;
    }

    std::queue<std::string> local;
    {
        std::scoped_lock lock(m_queueMutex);
        std::swap(local, m_outgoingText);
    }

    while(!local.empty()) {
        const std::string payload = local.front();
        local.pop();

        std::vector<uint8_t> packetData;
        packetData.resize(4 + payload.size());
        packetData[0] = static_cast<uint8_t>(NET_MESSAGE_GAME_MESSAGE);
        packetData[1] = 0;
        packetData[2] = 0;
        packetData[3] = 0;
        std::memcpy(packetData.data() + 4, payload.data(), payload.size());

        ENetPacket* packet = enet_packet_create(packetData.data(), packetData.size(), ENET_PACKET_FLAG_RELIABLE);
        if(packet) {
            enet_peer_send(m_peer, 0, packet);
        }
    }
}

void ReGTClient::ProcessReceive(ENetPacket* packet)
{
    if(!packet || packet->dataLength < 4) {
        return;
    }

    const uint8_t* data = packet->data;
    const uint32_t messageType = ReadLeU32(data);
    const uint8_t* payload = data + 4;
    const size_t payloadLen = packet->dataLength - 4;

    switch(messageType) {
        case NET_MESSAGE_SERVER_HELLO:
            m_helloReceived = true;
            EmitLog("[Net] Server hello received");
            break;

        case NET_MESSAGE_GENERIC_TEXT:
            HandleTextPacket(reinterpret_cast<const char*>(payload), payloadLen, "GENERIC_TEXT");
            break;

        case NET_MESSAGE_GAME_MESSAGE:
            HandleTextPacket(reinterpret_cast<const char*>(payload), payloadLen, "GAME_MESSAGE");
            break;

        case NET_MESSAGE_GAME_PACKET:
            HandleGamePacket(payload, payloadLen);
            break;

        default:
            EmitLog("[Net] Unhandled message type: " + std::to_string(messageType));
            break;
    }
}

void ReGTClient::HandleTextPacket(const char* payload, size_t payloadLen, const char* packetKind)
{
    if(!payload || payloadLen == 0) {
        return;
    }

    std::string text(payload, payload + payloadLen);

    std::map<std::string, std::string> fields;
    size_t lineStart = 0;
    while(lineStart < text.size()) {
        const size_t lineEnd = text.find('\n', lineStart);
        const size_t end = (lineEnd == std::string::npos) ? text.size() : lineEnd;
        const std::string line = text.substr(lineStart, end - lineStart);

        const size_t sep = line.find('|');
        if(sep != std::string::npos) {
            fields[line.substr(0, sep)] = line.substr(sep + 1);
        }

        if(lineEnd == std::string::npos) {
            break;
        }
        lineStart = lineEnd + 1;
    }

    const auto actionIt = fields.find("action");
    const std::string action = actionIt != fields.end() ? actionIt->second : "";

    if(action == "log") {
        const auto msgIt = fields.find("msg");
        if(msgIt != fields.end()) {
            EmitLog("[Server] " + TrimColorCodes(msgIt->second));
            return;
        }
    }

    if(action == "logon_fail") {
        EmitLog("[Login] Failed");
        return;
    }

    EmitLog(std::string("[") + packetKind + "] " + text);
}

void ReGTClient::HandleGamePacket(const uint8_t* payload, size_t payloadLen)
{
    if(!payload || payloadLen < sizeof(GameUpdatePacket)) {
        return;
    }

    GameUpdatePacket packet{};
    std::memcpy(&packet, payload, sizeof(GameUpdatePacket));

    if(packet.type != NET_GAME_PACKET_CALL_FUNCTION) {
        if(packet.type == 4) {
            EmitEvent("OnMapData", std::to_string(payloadLen));
        }
        return;
    }

    if((packet.flags & NET_GAME_PACKET_FLAGS_EXTENDED) == 0) {
        return;
    }

    const size_t expectedSize = sizeof(GameUpdatePacket) + packet.extraDataSize;
    if(payloadLen < expectedSize) {
        return;
    }

    const uint8_t* ext = payload + sizeof(GameUpdatePacket);
    HandleCallFunction(ext, packet.extraDataSize);
}

void ReGTClient::HandleCallFunction(const uint8_t* payload, size_t payloadLen)
{
    if(!payload || payloadLen < 1) {
        return;
    }

    struct ParsedVariant {
        uint8_t type = 0;
        std::string stringValue;
        int32_t intValue = 0;
        uint32_t uintValue = 0;
        float floatValue = 0;
    };

    std::array<ParsedVariant, 16> variants{};

    const uint8_t count = payload[0];
    size_t pos = 1;

    for(uint8_t i = 0; i < count && pos + 2 <= payloadLen; ++i) {
        const uint8_t index = payload[pos++];
        const uint8_t type = payload[pos++];

        if(index >= variants.size()) {
            break;
        }

        variants[index].type = type;

        switch(type) {
            case 1: {
                if(pos + 4 > payloadLen) {
                    return;
                }
                float v = 0;
                std::memcpy(&v, payload + pos, 4);
                variants[index].floatValue = v;
                pos += 4;
                break;
            }
            case 2: {
                if(pos + 4 > payloadLen) {
                    return;
                }
                uint32_t len = 0;
                std::memcpy(&len, payload + pos, 4);
                pos += 4;
                if(pos + len > payloadLen) {
                    return;
                }
                variants[index].stringValue.assign(reinterpret_cast<const char*>(payload + pos), len);
                pos += len;
                break;
            }
            case 3:
                if(pos + 8 > payloadLen) {
                    return;
                }
                pos += 8;
                break;
            case 4:
                if(pos + 12 > payloadLen) {
                    return;
                }
                pos += 12;
                break;
            case 5: {
                if(pos + 4 > payloadLen) {
                    return;
                }
                uint32_t v = 0;
                std::memcpy(&v, payload + pos, 4);
                variants[index].uintValue = v;
                pos += 4;
                break;
            }
            case 9: {
                if(pos + 4 > payloadLen) {
                    return;
                }
                int32_t v = 0;
                std::memcpy(&v, payload + pos, 4);
                variants[index].intValue = v;
                pos += 4;
                break;
            }
            default:
                return;
        }
    }

    if(variants[0].type != 2) {
        return;
    }

    const std::string functionName = variants[0].stringValue;

    if(functionName.rfind("OnSuperMainStartAcceptLogon", 0) == 0) {
        EmitLog("[Login] Welcome packet received");
        EmitEvent("Welcome", functionName);
        QueueRawTextMessage("action|refresh_item_data\n");
        QueueRawTextMessage("action|refresh_player_tribute_data\n");
        QueueRawTextMessage("action|enter_game\n");
        return;
    }

    if(functionName == "OnConsoleMessage") {
        if(variants[1].type == 2) {
            const std::string text = TrimColorCodes(variants[1].stringValue);
            EmitLog("[Console] " + text);
            EmitEvent("OnConsoleMessage", text);
        }
        return;
    }

    if(functionName == "OnDialogRequest") {
        if(variants[1].type == 2) {
            const std::string text = TrimColorCodes(variants[1].stringValue);
            EmitLog("[Dialog] " + text);
            EmitEvent("OnDialogRequest", text);
        }
        return;
    }

    if(functionName == "OnTextOverlay") {
        if(variants[1].type == 2) {
            const std::string text = TrimColorCodes(variants[1].stringValue);
            EmitLog("[Overlay] " + text);
            EmitEvent("OnTextOverlay", text);
        }
        return;
    }

    if(functionName == "OnSendToServer") {
        EmitLog("[Redirect] Server sent OnSendToServer request");
        EmitEvent("OnSendToServer", functionName);
        return;
    }

    if(functionName == "OnSpawn") {
        if(variants[1].type == 2) {
            EmitEvent("OnSpawn", variants[1].stringValue);
        }
        return;
    }

    if(functionName == "OnRequestWorldSelectMenu") {
        if(variants[1].type == 2) {
            EmitEvent("OnRequestWorldSelectMenu", variants[1].stringValue);
        }
        else {
            EmitEvent("OnRequestWorldSelectMenu", "");
        }
        return;
    }

    EmitLog("[Call] " + functionName);
}

std::string ReGTClient::BuildLoginPayload(const std::string& growId, const std::string& password) const
{
    std::map<std::string, std::string> fields;
    fields["action"] = "login";
    fields["requestedName"] = growId;
    fields["tankIDName"] = growId;
    fields["tankIDPass"] = password;
    fields["fhash"] = "0";
    fields["hash"] = "0";
    fields["f"] = "1";
    fields["protocol"] = "210";
    fields["game_version"] = "4.66";
    fields["platformID"] = "0";
    fields["mac"] = MakeRandomMac();
    fields["rid"] = "11111111111111111111111111111111";
    fields["wk"] = "REGTWIN";
    fields["sid"] = MakePseudoSid();
    fields["country"] = "us";
    fields["meta"] = "ReGTByVinz";
    fields["token"] = "0";
    fields["user"] = "0";
    fields["gid"] = "";
    fields["vid"] = "";

    return BuildActionPacket(fields);
}

std::string ReGTClient::MakeRandomMac()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 255);

    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for(int i = 0; i < 6; ++i) {
        if(i > 0) {
            out << ':';
        }
        out << std::setw(2) << dist(gen);
    }
    return out.str();
}

std::string ReGTClient::MakePseudoSid()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 15);

    std::ostringstream out;
    out << "REGT-";
    for(int i = 0; i < 16; ++i) {
        out << std::hex << dist(gen);
    }
    return out.str();
}

std::string ReGTClient::TrimColorCodes(const std::string& in)
{
    std::string out;
    out.reserve(in.size());

    for(size_t i = 0; i < in.size(); ++i) {
        if(in[i] == '`' && i + 1 < in.size()) {
            ++i;
            continue;
        }
        out.push_back(in[i]);
    }

    return out;
}
