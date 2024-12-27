#include "PCH.h"
#include "DiscordRPC.h"
#include "Timer.h"
#include "Application.h"

namespace RK {

bool DiscordRPC::Init(Application* inApp)
{
	discord::Result result = discord::Core::Create(1322276781851672647, DiscordCreateFlags_Default, &m_Core);
	
	if (result != discord::Result::Ok) 
		return false;

	m_Core->SetLogHook(discord::LogLevel::Debug, [inApp](discord::LogLevel level, const char* message) 
	{
		inApp->LogMessage(std::format("[Discord] {}", message));
	});

	m_Activity = {};
	m_Activity.SetType(discord::ActivityType::Playing);
	m_Activity.GetTimestamps().SetStart(std::chrono::duration_cast<std::chrono::seconds>(
		std::chrono::system_clock::now().time_since_epoch() ).count());

	m_Core->ActivityManager().UpdateActivity(m_Activity, [](discord::Result result) {});

	return true;
}


void DiscordRPC::SetActivityState(const char* inState)
{
	m_Activity.SetState(inState);
	m_Core->ActivityManager().UpdateActivity(m_Activity, [](discord::Result result) {});
}


void DiscordRPC::SetActivityDetails(const char* inDetails)
{
	m_Activity.SetDetails(inDetails);
	m_Core->ActivityManager().UpdateActivity(m_Activity, [](discord::Result result) {});
}


void DiscordRPC::OnUpdate()
{
	m_Core->RunCallbacks();
}


void DiscordRPC::Destroy()
{
	delete m_User;
	delete m_Core;
}

}