#pragma once

#include "discord.h"

namespace RK {

class Application;

// Sends activity updates to Discord Rich Presence
class DiscordRPC
{
public:
    bool Init(Application* inApp);
    void Destroy();
    void OnUpdate();

    void SetActivityState(const char* inState);
    void SetActivityDetails(const char* inDetails);

private:
    discord::Activity m_Activity;
    discord::User* m_User = nullptr;
    discord::Core* m_Core = nullptr;
};

}