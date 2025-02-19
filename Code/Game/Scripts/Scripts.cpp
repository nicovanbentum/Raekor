#include "PCH.h"
#include "Scripts.h"
#include "../Engine/RTTI.h"
#include "../Engine/Script.h"

namespace RK {

DECLARE_SCRIPT_CLASS(GunScript);
DECLARE_SCRIPT_CLASS(TestScript);
DECLARE_SCRIPT_CLASS(LightsScript);
DECLARE_SCRIPT_CLASS(AnimateSunScript);
DECLARE_SCRIPT_CLASS(VehicleControllerScript);
DECLARE_SCRIPT_CLASS(CharacterControllerScript);

void gRegisterScriptTypes()
{
    g_RTTIFactory.Register<GunScript>();
    g_RTTIFactory.Register<TestScript>();
    g_RTTIFactory.Register<LightsScript>();
    g_RTTIFactory.Register<AnimateSunScript>();
    g_RTTIFactory.Register<VehicleControllerScript>();
    g_RTTIFactory.Register<CharacterControllerScript>();
}

} // RK

