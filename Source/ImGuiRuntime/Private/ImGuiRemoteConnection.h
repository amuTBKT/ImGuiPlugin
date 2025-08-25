#pragma once

#include "CoreMinimal.h"
#include "IWebSocketServer.h"
#include "remoteimgui/RemoteImGui.h"

struct ImGuiIO;
struct ImDrawData;
class INetworkingWebSocket;

class FImGuiRemoteConnection : FNoncopyable
{
public:
    ~FImGuiRemoteConnection();

    bool Connect(const FString& Addr, int32 Port);
    void NewFrame();
    void Shutdown();

    bool IsConnected() const;
    bool CanDrawWidgets() const;
    void CopyClientState(ImGuiIO& IO) const;

    void SendRawPacket(const TArray<uint8>& Packet);
    void SendDrawData(const ImDrawData* DrawData, int32 MouseCursor);
    void SendTextureData(int32 TextureWidth, int32 TextureHeight, int32 BytesPerPixel, uint8* TextureData);

public:
    FSimpleDelegate OnConnected;
    FSimpleDelegate OnDisconnected;

private:
    RemoteImGui::FInputPacket RemoteInputs = {};
    TUniquePtr<IWebSocketServer> WebServer = nullptr;
    INetworkingWebSocket* WebClient = nullptr;
};
