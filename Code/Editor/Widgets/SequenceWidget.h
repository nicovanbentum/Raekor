#pragma once

#include "Widget.h"
#include "Camera.h"
#include "Animation.h"
#include "Components.h"

namespace RK {

enum SequenceState
{
    SEQUENCE_STOPPED = 0,
    SEQUENCE_PAUSED,
    SEQUENCE_PLAYING
};

class Sequence
{
public:
    struct KeyFrame
    {
        KeyFrame() = default;
        KeyFrame(float inTime, Vec3 inScale, Quat inRot, Vec3 inPos) : mTime(inTime), mScale(inScale), mRotation(inRot), mPosition(inPos) {}

        float mTime = 0.0f;
        Vec3 mScale = {};
        Quat mRotation = {};
        Vec3 mPosition = {};
    };

    void AddKeyFrame(const KeyFrame& inKeyFrame, float inTime);
    void AddKeyFrame(const Transform& inTransform, float inTime);
    void RemoveKeyFrame(uint32_t inIndex);

    uint32_t GetKeyFrameCount() const { return m_KeyFrames.size(); }
    const Array<KeyFrame>& GetKeyFrames() const { return m_KeyFrames; }

    KeyFrame& GetKeyFrame(uint32_t inIndex) { return m_KeyFrames[inIndex]; }
    const KeyFrame& GetKeyFrame(uint32_t inIndex) const { return m_KeyFrames[inIndex]; }

private:
    Array<KeyFrame> m_KeyFrames;
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

    int GetSelectedKeyFrame() const { return m_SelectedKeyframe; }

    void ApplyAnimationToScene(Scene& inScene);
    void RemoveAnimationFromScene(Scene& inScene);

    Sequence& GetSequence(Entity inEntity) { return m_Sequences[inEntity]; }
    const Sequence& GetSequence(Entity inEntity) const { return m_Sequences.at(inEntity); }

    float GetDuration() const { return m_Duration; }
    void SetDuration(float inSeconds) { m_Duration = inSeconds; }

private:
    bool DrawTimeline(const char* inLabel, float& inTime, const float inMinTime, const float inMaxTime);
    
private:
    float m_Time = 0.0f;
    float m_Speed = 1.0f;
    float m_Duration = 15.0f;
    String m_OpenFile = "";
    int m_SelectedKeyframe = -1;
    bool m_LockedToCamera = false;
    bool m_IsDraggingKeyframe = false;
    SequenceState m_State = SEQUENCE_STOPPED;
    Entity m_ActiveAnimationEntity = Entity::Null;
    HashMap<Entity, Sequence> m_Sequences;

};

}