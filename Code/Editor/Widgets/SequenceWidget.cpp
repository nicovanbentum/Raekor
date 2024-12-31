#include "PCH.h"
#include "SequenceWidget.h"

#include "OS.h"
#include "JSON.h"
#include "Scene.h"
#include "Timer.h"
#include "Camera.h"
#include "Editor.h"
#include "Archive.h"
#include "Components.h"
#include "Application.h"

namespace RK {

void Sequence::AddKeyFrame(const Transform& inTransform, float inTime)
{
    m_KeyFrames.emplace_back(inTime, inTransform.scale, inTransform.rotation, inTransform.position);

    std::sort(m_KeyFrames.begin(), m_KeyFrames.end(), [](const KeyFrame& inKey1, const KeyFrame& inKey2) { return inKey1.mTime < inKey2.mTime; });
}


void Sequence::AddKeyFrame(const KeyFrame& inKeyFrame, float inTime)
{
    KeyFrame keyframe = inKeyFrame;
    keyframe.mTime = inTime;
    m_KeyFrames.push_back(keyframe);

    std::sort(m_KeyFrames.begin(), m_KeyFrames.end(), [](const KeyFrame& inKey1, const KeyFrame& inKey2) { return inKey1.mTime < inKey2.mTime; });
}


void Sequence::RemoveKeyFrame(uint32_t inIndex)
{
    m_KeyFrames.erase(m_KeyFrames.begin() + inIndex);

    std::sort(m_KeyFrames.begin(), m_KeyFrames.end(), [](const KeyFrame& inKey1, const KeyFrame& inKey2) { return inKey1.mTime < inKey2.mTime; });
}

RTTI_DEFINE_TYPE_NO_FACTORY(SequenceWidget) {}

SequenceWidget::SequenceWidget(Editor* inEditor) : 
    IWidget(inEditor, (const char*)( ICON_FA_FILM "  Sequencer " )) {}


void SequenceWidget::Draw(Widgets* inWidgets, float inDeltaTime)
{

    m_Visible = ImGui::Begin(m_Title.c_str(), &m_Open);

    ImGui::SameLine();

    const char* sequence_animation_preview_text = "...";

    Sequence& sequence = m_Sequences[GetActiveEntity()];

    if (m_State == SEQUENCE_PLAYING)
    {
        m_Time += inDeltaTime * m_Speed;

        if (m_Time > m_Duration)
            m_Time = 0.0f;
    }

    if (m_ActiveAnimationEntity != Entity::Null)
    {
        if (GetScene().Has<Animation>(m_ActiveAnimationEntity))
            sequence_animation_preview_text = GetScene().Get<Animation>(m_ActiveAnimationEntity).GetName().c_str();
        else
            m_ActiveAnimationEntity = Entity::Null;
    }


    if (ImGui::BeginCombo("##SequenceAnimationCombo", sequence_animation_preview_text))
    {
        for (const auto& [entity, animation] : GetScene().Each<Animation>())
        {
            if (ImGui::Selectable(animation.GetName().c_str(), m_ActiveAnimationEntity == entity))
            {
                m_ActiveAnimationEntity = entity;
            }
        }

        if (ImGui::Selectable("New.."))
        {
            Entity entity = GetScene().Create();
            Name& name = GetScene().Add<Name>(entity);
            Animation& animation = GetScene().Add<Animation>(entity);

            name.name = "Animation";
            animation.SetName("Animation");

            m_ActiveAnimationEntity = entity;
        }

        ImGui::EndCombo();
    }

    if (ImGui::BeginDragDropSource())
    {
        ImGui::SetDragDropPayload("drag_drop_entity", &m_ActiveAnimationEntity, sizeof(Entity));
        ImGui::EndDragDropSource();
    }

    ImGui::SameLine();

    const bool locked_to_camera = m_LockedToCamera;

    if (locked_to_camera)
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));

    if (ImGui::Button((const char*)( m_LockedToCamera ? ICON_FA_LOCK : ICON_FA_LOCK_OPEN )))
    {
        m_LockedToCamera = !m_LockedToCamera;

        if (m_LockedToCamera)
            ApplyAnimationToScene(GetScene());
        else
            RemoveAnimationFromScene(GetScene());
    }

    if (locked_to_camera)
        ImGui::PopStyleColor();

    ImGui::SameLine();

    if (!m_LockedToCamera)
    {
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    }

    if (ImGui::Button(m_State == SEQUENCE_PLAYING ? (const char*)ICON_FA_PAUSE : (const char*)ICON_FA_PLAY))
    {
        switch (m_State)        
        {
            case SEQUENCE_PLAYING: { m_State = SEQUENCE_PAUSED;  } break;
            case SEQUENCE_PAUSED:  { m_State = SEQUENCE_PLAYING; } break;
            case SEQUENCE_STOPPED: { m_State = SEQUENCE_PLAYING; } break;
        }

    }

    ImGui::SameLine();

    if (ImGui::Button((const char*)ICON_FA_STOP))
    {
        m_State = SEQUENCE_STOPPED;
        m_Time = 0.0f;
    }
    
    if (GetScene().Has<Animation>(m_ActiveAnimationEntity))
        GetScene().Get<Animation>(m_ActiveAnimationEntity).SetIsPlaying(m_State == SEQUENCE_PLAYING);

    if (!m_LockedToCamera)
    {
        ImGui::PopItemFlag();
        ImGui::PopStyleColor();
    }

    ImGui::SameLine();

    if (ImGui::Button("+"))
    {
        if (GetScene().Has<Transform>(GetActiveEntity()))
            sequence.AddKeyFrame(GetScene().Get<Transform>(GetActiveEntity()), m_Time);
    }

    ImGui::SameLine();

    if (ImGui::Button("-"))
    {
        if (m_SelectedKeyframe != -1)
        {
            sequence.RemoveKeyFrame(m_SelectedKeyframe);
        }

        m_SelectedKeyframe = -1;
    }

    ImGui::SameLine();

    if (!m_OpenFile.empty())
    {
        ImGui::Text(m_OpenFile.c_str());
        ImGui::SameLine();
    }

    const float text_width = ImGui::CalcTextSize("Duration: 60.000 seconds").x;
    ImGui::SetNextItemWidth(text_width);

    float cursor_pos_x = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - text_width;

    ImGui::SetCursorPosX(cursor_pos_x);

    float duration_value = m_Duration;
    const char* duration_text = duration_value > 1 ? "Duration: %.1f seconds" : "Duration: %.1f second";

    if (m_State == SEQUENCE_PLAYING)
    {
        duration_value = m_Time;
        duration_text = duration_value > 1 ? "Playing: %.1f seconds" : "Playing: %.1f second";
    }

    if (ImGui::DragFloat("##sequenceduration", &duration_value, cIncrSequence, cMinSequenceLength, cMaxSequenceLength, duration_text))
    {
        if (m_State != SEQUENCE_PLAYING)
            m_Duration = duration_value;
    }

    const ImVec2 avail = ImGui::GetContentRegionAvail();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));

    ImGui::VSliderFloat("##sequencespeed", ImVec2(avail.x * 0.01f, avail.y), &m_Speed, 0.0f, 1.0f, "%", ImGuiSliderFlags_NoRoundToFormat);

    ImGui::SameLine();

    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);

    // don't pass m_Time directly as DrawTimeline's return value indicates if we should update the value or not
    float time = m_Time;
    
    if (DrawTimeline("Timeline", time, 0.0f, glm::min(m_Duration, cMaxSequenceLength)))
        m_Time = time;

    ImGui::PopStyleVar();

    ImGui::End();
}



void SequenceWidget::ApplyAnimationToScene(Scene& inScene)
{
    if (m_ActiveAnimationEntity == Entity::Null)
        return;

    Animation& animation = inScene.Get<Animation>(m_ActiveAnimationEntity);
    animation.ClearKeyFrames();

    for (const auto& [entity, sequence] : m_Sequences)
    {
        if (!inScene.Has<Transform>(entity))
            continue;

        float sequence_duration = Timer::sToMilliseconds(m_Duration);
        animation.SetTotalDuration(glm::max(animation.GetTotalDuration(), sequence_duration));

        KeyFrames keyframes;
        for (const Sequence::KeyFrame& kf : sequence.GetKeyFrames())
        {
            float time_ms = Timer::sToMilliseconds(kf.mTime);
            keyframes.AddScaleKey(Vec3Key(time_ms, kf.mScale));
            keyframes.AddPositionKey(Vec3Key(time_ms, kf.mPosition));
            keyframes.AddRotationKey(QuatKey(time_ms, kf.mRotation));
        }

        animation.LoadKeyframes(std::to_string(entity), keyframes);

        Transform& transform = inScene.Get<Transform>(entity);
        transform.animation = m_ActiveAnimationEntity;
        transform.animationChannel = std::to_string(entity);
    }
}



void SequenceWidget::RemoveAnimationFromScene(Scene& inScene)
{
    if (m_ActiveAnimationEntity == Entity::Null)
        return;

    for (const auto& [entity, transform] : inScene.Each<Transform>())
    {
        if (transform.animation == m_ActiveAnimationEntity)
        {
            transform.animation = Entity::Null;
            transform.animationChannel.clear();
        }
    }

    Animation& animation = inScene.Get<Animation>(m_ActiveAnimationEntity);
    animation.ClearKeyFrames();
}



bool SequenceWidget::DrawTimeline(const char* inLabel, float& inTime, const float inMinTime, const float inMaxTime)
{
    Sequence& sequence = m_Sequences[GetActiveEntity()];

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& imgui = *GImGui;
    const ImGuiStyle& style = imgui.Style;
    const ImGuiID id = window->GetID(inLabel);

    ImRect frame_bb = ImRect(window->DC.CursorPos, window->DC.CursorPos + ImGui::GetContentRegionAvail());

    const ImVec2 label_size = ImGui::CalcTextSize(inLabel, NULL, true);
    const ImRect total_bb   = ImRect(frame_bb.Min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0.0f));

    const uint32_t flags = 0u;
    const bool temp_input_allowed = (flags & ImGuiSliderFlags_NoInput) == 0;

    ImGui::ItemSize(total_bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(total_bb, id, &frame_bb, temp_input_allowed ? ImGuiItemFlags_Inputable : 0))
        return false;

    const bool is_hovered  = ImGui::ItemHoverable(frame_bb, id, ImGuiItemFlags_None);
    const bool is_clicked  = is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    const bool make_active = (is_clicked || imgui.NavActivateId == id);

    if (make_active)
    {
        ImGui::SetActiveID(id, window);
        ImGui::SetFocusID(id, window);
        ImGui::FocusWindow(window);
        imgui.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
    }

    // Draw frame
    const ImU32 frame_col = ImGui::GetColorU32(imgui.ActiveId == id ? ImGuiCol_FrameBgActive : ImGuiCol_FrameBg);
    ImGui::RenderFrame(frame_bb.Min, frame_bb.Max, frame_col, true, imgui.Style.FrameRounding);

    ImGui::PushClipRect(frame_bb.Min, frame_bb.Max, false);

    uint32_t line_nr = 0u;

    for (float f = 0.0f; f < m_Duration; f += cIncrSequence)
    {
        const float x = frame_bb.Min.x + frame_bb.GetWidth() * (f / m_Duration);

        const ImVec4 odd_color  = ImVec4(0.2f, 0.2f, 0.2f, 0.5f);
        const ImVec4 even_color = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
        
        ImVec4 line_color = line_nr % 2 != 0 ? odd_color : even_color;

        window->DrawList->AddLine(ImVec2(x, frame_bb.Min.y), ImVec2(x, frame_bb.Max.y), ImGui::GetColorU32(line_color), 2.0f);

        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));

        char value_buf[64];
        const char* value_buf_end = value_buf + ImGui::DataTypeFormatString(value_buf, IM_ARRAYSIZE(value_buf), ImGuiDataType_Float, &f, "%.1f");

        ImGui::RenderText(ImVec2(x + 3.0f, frame_bb.Min.y), value_buf, value_buf_end);

        ImGui::PopStyleColor();

        line_nr++;
    }

    bool was_left_clicked = false;
    bool was_right_clicked = false;
    bool hovering_any_keyframe = false;

    for (int keyframe_index = 0; keyframe_index < sequence.GetKeyFrameCount(); keyframe_index++)
    {
        Sequence::KeyFrame& keyframe = sequence.GetKeyFrame(keyframe_index);

        const float radius = 8.0f;
        const float segments = 4;

        ImRect padded_bb = frame_bb;
        padded_bb.Min += style.FramePadding;
        padded_bb.Max -= style.FramePadding;

        float ngon_center_x = padded_bb.Min.x + padded_bb.GetWidth() * (keyframe.mTime / m_Duration);
        float ngon_center_y = padded_bb.Min.y + 0.5f * padded_bb.GetHeight();

        const ImVec2 ngon_bb_min = ImVec2(ngon_center_x - radius, ngon_center_y - radius);
        const ImVec2 ngon_bb_max = ImVec2(ngon_center_x + radius, ngon_center_y + radius);
        const bool is_hovering_keyframe = ImGui::IsMouseHoveringRect(ngon_bb_min, ngon_bb_max);
        
        if (m_SelectedKeyframe == keyframe_index && ImGui::IsMouseDragging(0) && m_IsDraggingKeyframe)
        {
            ngon_center_x = ImGui::GetMousePos().x; // move the keyframe to the cursor, horizontal only
            keyframe.mTime = (ngon_center_x - padded_bb.Min.x) / padded_bb.GetWidth() * m_Duration;
            keyframe.mTime = glm::clamp(keyframe.mTime, 0.0f, m_Duration);

            if (m_SelectedKeyframe - 1 >= 0)
            {
                Sequence::KeyFrame& prev_keyframe = sequence.GetKeyFrame(m_SelectedKeyframe - 1);
                // if we moved backwards in time past a previous keyframe, swap
                if (keyframe.mTime < prev_keyframe.mTime)
                {
                    std::swap(prev_keyframe, keyframe);
                    m_SelectedKeyframe = m_SelectedKeyframe - 1;
                }
            }

            if (m_SelectedKeyframe + 1 < sequence.GetKeyFrameCount())
            {
                Sequence::KeyFrame& next_keyframe = sequence.GetKeyFrame(m_SelectedKeyframe + 1);
                // if we moved forwards in time past the next keyframe, swap
                if (keyframe.mTime > next_keyframe.mTime)
                {
                    std::swap(keyframe, next_keyframe);
                    m_SelectedKeyframe = m_SelectedKeyframe + 1;
                }
            }
        }

        if (keyframe_index == m_SelectedKeyframe)
        {
            const ImU32 color = ImGui::GetColorU32(ImVec4(1, 0.9, 0, 1));
            window->DrawList->AddNgonFilled(ImVec2(ngon_center_x, ngon_center_y), radius + 2.0f, color, segments);
        }
        
        const ImU32 regular_color  = ImGui::GetColorU32(ImVec4(1.0f, 0.647, 0.0f, 1.0f));
        const ImU32 selected_color = ImGui::GetColorU32(ImVec4(1.0f, 0.2f, 0.0f, 1.0f));
        window->DrawList->AddNgonFilled(ImVec2(ngon_center_x, ngon_center_y), radius, keyframe_index == m_SelectedKeyframe ? selected_color : regular_color, segments);

        // handle keyframe click interactions
        if (is_hovering_keyframe && ImGui::IsMouseClicked(0))
        {
            was_left_clicked = true;
            m_SelectedKeyframe = keyframe_index;
            m_IsDraggingKeyframe = true;
        }
        else if (is_hovering_keyframe && ImGui::IsMouseClicked(1))
        {
            was_right_clicked = true;
            m_SelectedKeyframe = keyframe_index;
        }

        hovering_any_keyframe |= is_hovering_keyframe;
    }

    if (ImGui::IsMouseReleased(0))
        m_IsDraggingKeyframe = false;

    if (ImGui::BeginPopup("##keyframerightclicked"))
    {
        if (ImGui::MenuItem("Edit"))
        // not very future proof but will do for now..
            m_Editor->SetActiveEntity(Entity::Null);

        if (ImGui::MenuItem("Delete"))
        {
            if (m_SelectedKeyframe != -1)
                sequence.RemoveKeyFrame(m_SelectedKeyframe);

            m_SelectedKeyframe = -1;
        }

        if (ImGui::MenuItem("Duplicate"))
        {
            Sequence::KeyFrame key_frame = sequence.GetKeyFrame(m_SelectedKeyframe);
            // slightly offset the time so we can actually see the duplicate and it doesn't perfectly overlap
            sequence.AddKeyFrame(key_frame, key_frame.mTime + 0.05f);
        }

        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("##timelinerightclicked"))
    {
        if (ImGui::MenuItem("Clear"))
        {
            sequence = {};
        }

        ImGui::EndPopup();
    }

    if (was_right_clicked)
        ImGui::OpenPopup("##keyframerightclicked");

    // clicked somewhere inside the timeline, but not on any keyframes or popups
    const bool can_click_timeline = frame_bb.Contains(ImGui::GetMousePos()) && 
                                    !was_right_clicked && !was_left_clicked && 
                                    !ImGui::IsPopupOpen("##keyframerightclicked");

    if (can_click_timeline && ImGui::IsMouseClicked(0))
        m_SelectedKeyframe = -1;

    if (can_click_timeline && ImGui::IsMouseClicked(1))
        ImGui::OpenPopup("##timelinerightclicked");

    ImGui::PopClipRect();

    // Slider behavior
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 4.0f);

    const float v_min = inMinTime;
    const float v_max = inMaxTime;

    const ImGuiAxis axis = ImGuiAxis_X;
    const bool is_floating_point = true;
    const float v_range_f = (float)(v_min < v_max ? v_max - v_min : v_min - v_max); // We don't need high precision for what we do with it.

    // Calculate bounds
    const ImRect bb = frame_bb;
    const float grab_padding = 2.0f; // FIXME: Should be part of style.
    const float slider_sz = (bb.Max[axis] - bb.Min[axis]) - grab_padding * 2.0f;

    float grab_sz = style.GrabMinSize;
    if (!is_floating_point && v_range_f >= 0.0f)                         // v_range_f < 0 may happen on integer overflows
        grab_sz = ImMax(slider_sz / (v_range_f + 1), style.GrabMinSize); // For integer sliders: if possible have the grab size represent 1 unit

    grab_sz = ImMin(grab_sz, slider_sz);
    const float slider_usable_sz = slider_sz - grab_sz;
    const float slider_usable_pos_min = bb.Min[axis] + grab_padding + grab_sz * 0.5f;
    const float slider_usable_pos_max = bb.Max[axis] - grab_padding - grab_sz * 0.5f;

    const bool is_logarithmic = false;
    float logarithmic_zero_epsilon = 0.0f; // Only valid when is_logarithmic is true
    float zero_deadzone_halfsize = 0.0f; // Only valid when is_logarithmic is true

    // Process interacting with the slider
    bool value_changed = false;
    const bool allow_slider_input = !was_right_clicked && !was_left_clicked && !m_IsDraggingKeyframe;

    if (imgui.ActiveId == id && allow_slider_input)
    {
        float clicked_t = 0.0f;
        bool set_new_value = false;

        if (imgui.ActiveIdSource == ImGuiInputSource_Mouse)
        {
            if (!imgui.IO.MouseDown[0])
            {
                ImGui::ClearActiveID();
            }
            else
            {
                const float mouse_abs_pos = imgui.IO.MousePos[axis];

                if (imgui.ActiveIdIsJustActivated)
                {
                    float grab_t = ImGui::ScaleRatioFromValueT<float, float, float>(ImGuiDataType_Float, inTime, v_min, v_max, is_logarithmic, logarithmic_zero_epsilon, zero_deadzone_halfsize);

                    const float grab_pos = ImLerp(slider_usable_pos_min, slider_usable_pos_max, grab_t);
                    const bool clicked_around_grab = (mouse_abs_pos >= grab_pos - grab_sz * 0.5f - 1.0f) && (mouse_abs_pos <= grab_pos + grab_sz * 0.5f + 1.0f); // No harm being extra generous here.
                    imgui.SliderGrabClickOffset = (clicked_around_grab && is_floating_point) ? mouse_abs_pos - grab_pos : 0.0f;
                }

                if (slider_usable_sz > 0.0f)
                    clicked_t = ImSaturate((mouse_abs_pos - imgui.SliderGrabClickOffset - slider_usable_pos_min) / slider_usable_sz);

                set_new_value = true;
            }
        }

        if (set_new_value)
        {
            float v_new = ImGui::ScaleValueFromRatioT<float, float, float>(ImGuiDataType_Float, clicked_t, v_min, v_max, is_logarithmic, logarithmic_zero_epsilon, zero_deadzone_halfsize);

            // Round to user desired precision based on format string
            if (is_floating_point && !(flags & ImGuiSliderFlags_NoRoundToFormat))
                v_new = ImGui::RoundScalarWithFormatT<float>("%.2f", ImGuiDataType_Float, v_new);

            // Apply result
            if (inTime != v_new)
            {
                inTime = v_new;
                value_changed = true;
            }
        }
    }

    ImRect grab_bb = ImRect(ImVec2(FLT_MAX, FLT_MAX), ImVec2(FLT_MIN, FLT_MIN));

    if (slider_sz < 1.0f)
        grab_bb = ImRect(bb.Min, bb.Min);
    else
    {
        // Output grab position so it can be displayed by the caller
        float grab_t = ImGui::ScaleRatioFromValueT<float, float, float>(ImGuiDataType_Float, inTime, v_min, v_max, is_logarithmic, logarithmic_zero_epsilon, zero_deadzone_halfsize);

        const float grab_pos = ImLerp(slider_usable_pos_min, slider_usable_pos_max, grab_t);

        grab_bb = ImRect(grab_pos - grab_sz * 0.5f, bb.Min.y + grab_padding, grab_pos + grab_sz * 0.5f, bb.Max.y - grab_padding);
    }

    if (value_changed)
        ImGui::MarkItemEdited(id);

    ImGui::PopStyleVar();
    
    // I don't think we need this but just to be safe..
    if (was_right_clicked || was_left_clicked || hovering_any_keyframe)
        value_changed = false;

    ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImGui::GetStyleColorVec4(ImGuiCol_Text));

    // Render grab
    if (grab_bb.Max.x > grab_bb.Min.x)
    {
        grab_bb.Min.y -= style.FrameRounding * 0.30f;
        grab_bb.Max.y += style.FrameRounding * 0.30f;

        ImGuiCol color = (imgui.ActiveId == id && allow_slider_input) ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab;

        if (m_State == SEQUENCE_PLAYING)
            color = ImGuiCol_SliderGrabActive;

        window->DrawList->AddRectFilled(grab_bb.Min, grab_bb.Max, ImGui::GetColorU32(color), 1.0f);
    }

    ImGui::PopStyleColor(2);

    return value_changed;
}


void SequenceWidget::OnEvent(Widgets* inWidgets, const SDL_Event& inEvent)
{
    if (inEvent.type == SDL_KEYDOWN && !inEvent.key.repeat)
    {
        switch (inEvent.key.keysym.sym)
        {
            case SDLK_DELETE:
            {
                if (m_Editor->GetActiveEntity() == Entity::Null)
                {
                    if (m_SelectedKeyframe != -1)
                    {
                        m_Sequences[GetActiveEntity()].RemoveKeyFrame(m_SelectedKeyframe);
                    }
                
                    m_SelectedKeyframe = -1;
                }
                break;
            }
        }
    }
}


} // raekor