#include "pch.h"
#include "util.h"
#include "framebuffer.h"
#include "renderer.h"

#ifdef _WIN32
#include "platform/windows/DXFrameBuffer.h"
#endif

namespace Raekor {

FrameBuffer* FrameBuffer::construct(FrameBuffer::ConstructInfo* info) {
    auto activeAPI = Renderer::getActiveAPI();
    switch (activeAPI) {
        case RenderAPI::OPENGL: {
            return nullptr;
        } break;
#ifdef _WIN32
        case RenderAPI::DIRECTX11: {
            return new DXFrameBuffer(info);
        } break;
#endif
    }
    return nullptr;
}

} // Raekor
