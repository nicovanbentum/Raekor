#include "pch.h"
#include "physics.h"

namespace Raekor {

Physics::Physics() {
    foundation = PxCreateFoundation(PX_PHYSICS_VERSION, allocator,
        errorCallback);

    if (!foundation) {
        std::cerr << "failed to create physx foundation\n";
        return;
    }

    physics = PxCreatePhysics(PX_PHYSICS_VERSION, *foundation,
        physx::PxTolerancesScale(), false, nullptr);
    if (!physics) {
        std::cerr << "failed to create physx physics\n";
        return;
    }

    if (!PxInitExtensions(*physics, nullptr)) {
        std::cerr << "failed to init physx extensions\n";
        return;
    }

    // create the cpu dispatcher and scene
    physx::PxSceneDesc sceneDesc(physics->getTolerancesScale());
    sceneDesc.gravity = physx::PxVec3(0.0f, -9.81f, 0.0f);
    dispatcher = physx::PxDefaultCpuDispatcherCreate(2);
    sceneDesc.cpuDispatcher = dispatcher;
    sceneDesc.filterShader = physx::PxDefaultSimulationFilterShader;
    scene = physics->createScene(sceneDesc);

    std::cout << "PhysX initialized.\n";
}

Physics::~Physics() {
    scene->release();
    PxCloseExtensions();
    physics->release();
    foundation->release();
}


} // raekor

