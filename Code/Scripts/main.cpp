#define RAEKOR_SCRIPT
#include "../Engine/raekor.h"
using namespace RK;

// For DllMain
#include <Windows.h>
#include <process.h>


DECLARE_SCRIPT_CLASS(TestScript);
DECLARE_SCRIPT_CLASS(LightsScript);
DECLARE_SCRIPT_CLASS(AnimateSunScript);
DECLARE_SCRIPT_CLASS(VehicleControllerScript);
DECLARE_SCRIPT_CLASS(CharacterControllerScript);

static RTTI* types[] =
{
    &RTTI_OF<TestScript>(),
    &RTTI_OF<LightsScript>(),
    &RTTI_OF<AnimateSunScript>(),
    &RTTI_OF<VehicleControllerScript>(),
    &RTTI_OF<CharacterControllerScript>(),
};

void gRegisterScriptTypes()
{
    g_RTTIFactory.Register<TestScript>();
    g_RTTIFactory.Register<LightsScript>();
    g_RTTIFactory.Register<AnimateSunScript>();
    g_RTTIFactory.Register<VehicleControllerScript>();
    g_RTTIFactory.Register<CharacterControllerScript>();
}


extern "C" __declspec(dllexport) int __cdecl SCRIPT_EXPORTED_FUNCTION_NAME(RTTI** outTypes)
{  
    constexpr int nr_of_types = sizeof(types) / sizeof(types[0]);

    if (outTypes)
    {
        for (int i = 0; i < nr_of_types; i++)
            outTypes[i] = types[i];
    }
        
    return nr_of_types;
}


BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    SDL_Init(SDL_INIT_VIDEO);

    //Physics::sInit();

    return TRUE;  // Successful DLL_PROCESS_ATTACH.
}