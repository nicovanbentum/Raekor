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

    CameraSequence& GetSequence() { return m_Sequence; }
    int GetSelectedKeyFrame() const { return m_SelectedKeyframe; }

    void ApplyToCamera(Camera& ioCamera, float inDeltaTime);
    bool IsLockedToCamera() const { return m_LockedToCamera; }

private:
    bool DrawTimeline(const char* label, ImGuiDataType data_type, float& inTime, const float p_min, const float p_max, const char* format, ImGuiSliderFlags flags = 0);
    
private:
    float m_Time = 0.0f;
    std::string m_OpenFile = "";
    int m_SelectedKeyframe = -1;
    bool m_LockedToCamera = false;
    bool m_IsDraggingKeyframe = false;
    SequenceState m_State = SEQUENCE_STOPPED;
    CameraSequence m_Sequence;
};

}