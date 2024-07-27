#pragma once

#include "widget.h"
#include "camera.h"

namespace RK {

enum SequenceState
{
    SEQUENCE_STOPPED = 0,
    SEQUENCE_PAUSED,
    SEQUENCE_PLAYING
};

class SequenceWidget : public IWidget
{
public:
    RTTI_DECLARE_VIRTUAL_TYPE(SequenceWidget);
    
    static constexpr float cIncrSequence = 1.0f;
    static constexpr float cMinSequenceLength = 1.0f;
    static constexpr float cMaxSequenceLength = 60.0f;

    SequenceWidget(Application* inApp);

    virtual void Draw(Widgets* inWidgets, float dt) override;
    virtual void OnEvent(Widgets* inWidgets, const SDL_Event& ev) override;

    CameraSequence& GetSequence() { return m_Sequence; }
    int GetSelectedKeyFrame() const { return m_SelectedKeyframe; }

    void ApplyToCamera(Camera& ioCamera, float inDeltaTime);
    bool IsLockedToCamera() const { return m_LockedToCamera; }

private:
    bool DrawTimeline(const char* inLabel, float& inTime, const float inMinTime, const float inMaxTime);
    
private:
    float m_Time = 0.0f;
    float m_Speed = 1.0f;
    String m_OpenFile = "";
    int m_SelectedKeyframe = -1;
    bool m_LockedToCamera = false;
    bool m_IsDraggingKeyframe = false;
    SequenceState m_State = SEQUENCE_STOPPED;
    CameraSequence m_Sequence;
};

}