#pragma once

#include "widget.h"
#include "camera.h"

namespace Raekor {

enum SequenceState
{
    SEQUENCE_STOPPED = 0,
    SEQUENCE_PAUSED,
    SEQUENCE_PLAYING
};

class SequenceWidget : public IWidget
{
public:
    RTTI_DECLARE_TYPE(SequenceWidget);
    
    static constexpr auto cIncrSequence = 1.0f;
    static constexpr auto cMinSequenceLength = 1.0f;
    static constexpr auto cMaxSequenceLength = 60.0f;

    SequenceWidget(Application* inApp);

    virtual void Draw(Widgets* inWidgets, float dt) override;
    virtual void OnEvent(Widgets* inWidgets, const SDL_Event& ev) override;

    bool DrawTimeline(const char* label, ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags = 0);

    void ApplyToCamera(Camera& ioCamera);
    bool IsLockedToCamera() const { return m_LockedToCamera; }

private:
    float m_Time = 0.0f;
    std::string m_OpenFile = "";
    bool m_LockedToCamera = false;
    SequenceState m_State = SEQUENCE_STOPPED;
    CameraSequence m_Sequence;
};

}