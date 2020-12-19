#pragma once

namespace Raekor {

class Physics {
public:
    Physics();
    ~Physics();

private:
    physx::PxScene* scene;
    physx::PxPhysics* physics;
    physx::PxFoundation* foundation;
    physx::PxCpuDispatcher* dispatcher;
    physx::PxDefaultAllocator allocator;
    physx::PxDefaultErrorCallback errorCallback;
};

}
