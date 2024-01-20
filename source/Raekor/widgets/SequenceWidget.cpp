#include "pch.h"
#include "SequenceWidget.h"

#include "OS.h"
#include "json.h"
#include "camera.h"
#include "archive.h"
#include "application.h"

namespace Raekor {

RTTI_DEFINE_TYPE_NO_FACTORY(SequenceWidget) {}

SequenceWidget::SequenceWidget(Application* inApp) : 
    IWidget(inApp, reinterpret_cast<const char*>( ICON_FA_FILM "  Sequencer " )) {}


void SequenceWidget::Draw(Widgets* inWidgets, float inDeltaTime)
{
    m_Visible = ImGui::Begin(m_Title.c_str(), &m_Open);

    ImGui::SameLine();

    if (ImGui::Button((const char*)(m_LockedToCamera ? ICON_FA_LOCK : ICON_FA_LOCK_OPEN)))
        m_LockedToCamera = !m_LockedToCamera;

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
            case SEQUENCE_PLAYING:
            {
                m_State = SEQUENCE_PAUSED;
            } break;

            case SEQUENCE_PAUSED:
            {
                m_State = SEQUENCE_PLAYING;
            } break;

            case SEQUENCE_STOPPED:
            {
                m_State = SEQUENCE_PLAYING;
            } break;
        }
    }

    ImGui::SameLine();

    if (ImGui::Button((const char*)ICON_FA_STOP))
    {
        m_State = SEQUENCE_STOPPED;
        m_Time = 0.0f;
    }

    if (!m_LockedToCamera)
    {
        ImGui::PopItemFlag();
        ImGui::PopStyleColor();
    }

    ImGui::SameLine();

    if (ImGui::Button((const char*)ICON_FA_SAVE))
    {
        std::string file_path = OS::sSaveFileDialog("JSON File (*.json)\0", "json");

        if (!file_path.empty())
        {
            JSON::WriteArchive archive(file_path);
            archive << m_Sequence;
        }
    }

    ImGui::SameLine();

    if (ImGui::Button((const char*)ICON_FA_FOLDER_OPEN))
    {
        std::string file_path = OS::sOpenFileDialog("JSON File (*.json)\0");

        if (!file_path.empty())
        {
            JSON::ReadArchive archive(file_path);
            archive >> m_Sequence;

            m_OpenFile = fs::relative(file_path).string();
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("+"))
        m_Sequence.AddKeyFrame(m_Editor->GetViewport().GetCamera(), m_Time);

    ImGui::SameLine();

    if (ImGui::Button("-"))
    {
        if (m_SelectedKeyframe != -1)
            m_Sequence.RemoveKeyFrame(m_SelectedKeyframe);

        m_SelectedKeyframe = -1;
    }

    ImGui::SameLine();

    if (!m_OpenFile.empty())
    {
        ImGui::Text(m_OpenFile.c_str());
        ImGui::SameLine();
    }

    float duration = m_Sequence.GetDuration();

    const auto text_width = ImGui::CalcTextSize("Duration: 60.000 seconds").x;
    ImGui::SetNextItemWidth(text_width);

    auto cursor_pos_x = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - text_width;

    ImGui::SetCursorPosX(cursor_pos_x);

    auto duration_value = duration;
    auto duration_text = duration > 1 ? "Duration: %.1f seconds" : "Duration: %.1f second";

    if (m_State == SEQUENCE_PLAYING)
    {
        duration_value = m_Time;
        duration_text = duration > 1 ? "Playing: %.1f seconds" : "Playing: %.1f second";
    }

    if (ImGui::DragFloat("##sequenceduration", &duration_value, cIncrSequence, cMinSequenceLength, cMaxSequenceLength, duration_text))
    {
        if (m_State != SEQUENCE_PLAYING)
            m_Sequence.SetDuration(duration_value);
    }

   // if (ImGui::DragFloat("Time", &m_Time, cIncrSequence, cMinSequenceLength, duration, "%.2f"))
   //     m_Time = glm::min(m_Time, duration);
  
   // if (ImGui::Button("+"))
   //     m_Sequence.AddKeyFrame(m_Editor->GetViewport().GetCamera(), m_Time);

    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);

    float time = m_Time;
    
    if (DrawTimeline("Timeline", ImGuiDataType_Float, time, 0.0f, glm::min(duration, cMaxSequenceLength), "%.2f"))
        m_Time = time;

    ImGui::End();
}


bool SequenceWidget::DrawTimeline(const char* label, ImGuiDataType data_type, float& inTime, const float p_min, const float p_max, const char* format, ImGuiSliderFlags flags)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const float w = ImGui::CalcItemWidth();

    const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
    // ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(w, label_size.y + style.FramePadding.y * 2.0f));
    ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + ImGui::GetContentRegionAvail());

    const ImRect total_bb(frame_bb.Min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0.0f));

    const bool temp_input_allowed = (flags & ImGuiSliderFlags_NoInput) == 0;
    ImGui::ItemSize(total_bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(total_bb, id, &frame_bb, temp_input_allowed ? ImGuiItemFlags_Inputable : 0))
        return false;

    // Default format string when passing NULL
    if (format == NULL)
        format = ImGui::DataTypeGetInfo(data_type)->PrintFmt;

    const bool hovered = ImGui::ItemHoverable(frame_bb, id);
    bool temp_input_is_active = temp_input_allowed && ImGui::TempInputIsActive(id);
    if (!temp_input_is_active)
    {
        const bool clicked = hovered && ImGui::IsMouseClicked(0, id);
        const bool make_active = (clicked || g.NavActivateId == id);

        if (make_active && !temp_input_is_active)
        {
            ImGui::SetActiveID(id, window);
            ImGui::SetFocusID(id, window);
            ImGui::FocusWindow(window);
            g.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
        }
    }

    // Draw frame
    const ImU32 frame_col = ImGui::GetColorU32(g.ActiveId == id ? ImGuiCol_FrameBgActive : ImGuiCol_FrameBg);
    ImGui::RenderFrame(frame_bb.Min, frame_bb.Max, frame_col, true, g.Style.FrameRounding);

    ImGui::PushClipRect(frame_bb.Min, frame_bb.Max, false);

    auto line_nr = 0u;

    for (float f = 0.0f; f < m_Sequence.GetDuration(); f += cIncrSequence)
    {
        const auto x = frame_bb.Min.x + frame_bb.GetWidth() * (f / m_Sequence.GetDuration());

        const auto half_color = ImVec4(0.2f, 0.2f, 0.2f, 0.5f);
        const auto full_color = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
        
        auto line_color = line_nr % 2 != 0 ? full_color : half_color;

        window->DrawList->AddLine(ImVec2(x, frame_bb.Min.y), ImVec2(x, frame_bb.Max.y), ImGui::GetColorU32(line_color), 2.0f);

        // Display value using user-provided display format so user can add prefix/suffix/decorations to the value.
        char value_buf[64];
        const char* value_buf_end = value_buf + ImGui::DataTypeFormatString(value_buf, IM_ARRAYSIZE(value_buf), data_type, &f, "%.1f");

        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));

        ImGui::RenderText(ImVec2(x + 3.0f, frame_bb.Min.y), value_buf, value_buf_end);

        ImGui::PopStyleColor();

        line_nr++;
    }

    auto was_left_clicked = false;
    auto was_right_clicked = false;
    auto hovering_any_keyframe = false;

    for (auto keyframe_index = 0u; keyframe_index < m_Sequence.GetKeyFrameCount(); keyframe_index++)
    {
        auto& keyframe = m_Sequence.GetKeyFrame(keyframe_index);

        const auto radius = 8.0f;
        const auto segments = 4;

        auto padded_bb = frame_bb;
        padded_bb.Min += style.FramePadding;
        padded_bb.Max -= style.FramePadding;

        auto center_x = padded_bb.Min.x + padded_bb.GetWidth() * (keyframe.mTime / m_Sequence.GetDuration());
        auto center_y = padded_bb.Min.y + 0.5f * padded_bb.GetHeight();

        const auto is_hovering_keyframe = ImGui::IsMouseHoveringRect(ImVec2(center_x - radius, center_y - radius), ImVec2(center_x + radius, center_y + radius));
        
        if (m_SelectedKeyframe == keyframe_index && ImGui::IsMouseDragging(0) && m_IsDraggingKeyframe)
        {
            center_x = ImGui::GetMousePos().x;
            keyframe.mTime = (center_x - padded_bb.Min.x) / padded_bb.GetWidth() * m_Sequence.GetDuration();
            keyframe.mTime = glm::clamp(keyframe.mTime, 0.0f, m_Sequence.GetDuration());

            if (m_SelectedKeyframe - 1 >= 0)
            {
                auto& prev_keyframe = m_Sequence.GetKeyFrame(m_SelectedKeyframe - 1);
                if (keyframe.mTime < prev_keyframe.mTime)
                {
                    std::swap(prev_keyframe, keyframe);
                    m_SelectedKeyframe = m_SelectedKeyframe - 1;
                }
            }

            if (m_SelectedKeyframe + 1 < m_Sequence.GetKeyFrameCount())
            {
                auto& next_keyframe = m_Sequence.GetKeyFrame(m_SelectedKeyframe + 1);
                if (keyframe.mTime > next_keyframe.mTime)
                {
                    std::swap(keyframe, next_keyframe);
                    m_SelectedKeyframe = m_SelectedKeyframe + 1;
                }
            }
        }

        if (keyframe_index == m_SelectedKeyframe)
        {
            const auto color = ImGui::GetColorU32(ImVec4(1, 0.9, 0, 1));
            window->DrawList->AddNgonFilled(ImVec2(center_x, center_y), radius + 2.0f, color, segments);
        }
        

        const auto color = ImGui::GetColorU32(ImVec4(1.0f, 0.647, 0.0f, 1.0f));
        const auto selected_color = ImGui::GetColorU32(ImVec4(1.0f, 0.2f, 0.0f, 1.0f));
        window->DrawList->AddNgonFilled(ImVec2(center_x, center_y), radius, keyframe_index == m_SelectedKeyframe ? selected_color : color, segments);


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
            m_Editor->SetActiveEntity(NULL_ENTITY);

        if (ImGui::MenuItem("Delete"))
        {
            if (m_SelectedKeyframe != -1)
                m_Sequence.RemoveKeyFrame(m_SelectedKeyframe);

            m_SelectedKeyframe = -1;
        }

        if (ImGui::MenuItem("Duplicate"))
        {
            const auto& key_frame = m_Sequence.GetKeyFrame(m_SelectedKeyframe);
            m_Sequence.AddKeyFrame(m_Editor->GetViewport().GetCamera(), key_frame.mTime + 0.05f);
        }

        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("##timelinerightclicked"))
    {

        if (ImGui::MenuItem("Clear"))
        {
            const auto duration = m_Sequence.GetDuration();
            m_Sequence = {};
            m_Sequence.SetDuration(duration);
        }

        ImGui::EndPopup();
    }

    if (was_right_clicked)
        ImGui::OpenPopup("##keyframerightclicked");

    // clicked somewhere inside the timeline, but not on any keyframes or popups
    if (ImGui::IsMouseClicked(0) 
        && frame_bb.Contains(ImGui::GetMousePos()) 
        && !was_right_clicked && !was_left_clicked 
        && !ImGui::IsPopupOpen("##keyframerightclicked"))
    {
        m_SelectedKeyframe = -1;
    }


    if (ImGui::IsMouseClicked(1) 
        && frame_bb.Contains(ImGui::GetMousePos()) 
        && !was_right_clicked && !was_left_clicked 
        && !ImGui::IsPopupOpen("##keyframerightclicked"))
    {
        ImGui::OpenPopup("##timelinerightclicked");
    }

    ImGui::PopClipRect();

    // Slider behavior
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 4.0f);

    ImRect grab_bb(ImVec2(FLT_MAX, FLT_MAX), ImVec2(FLT_MIN, FLT_MIN));

    const auto v_min = p_min;
    const auto v_max = p_max;

    const ImGuiAxis axis = (flags & ImGuiSliderFlags_Vertical) ? ImGuiAxis_Y : ImGuiAxis_X;
    const bool is_floating_point = (data_type == ImGuiDataType_Float) || (data_type == ImGuiDataType_Double);
    const float v_range_f = (float)(v_min < v_max ? v_max - v_min : v_min - v_max); // We don't need high precision for what we do with it.

    // Calculate bounds
    const auto bb = frame_bb;
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
    bool allow_slider_input = !was_right_clicked && !was_left_clicked && !m_IsDraggingKeyframe;
    if (g.ActiveId == id && allow_slider_input)
    {
        bool set_new_value = false;
        float clicked_t = 0.0f;
        if (g.ActiveIdSource == ImGuiInputSource_Mouse)
        {
            if (!g.IO.MouseDown[0])
            {
                ImGui::ClearActiveID();
            }
            else
            {
                const float mouse_abs_pos = g.IO.MousePos[axis];
                if (g.ActiveIdIsJustActivated)
                {
                    float grab_t = ImGui::ScaleRatioFromValueT<float, float, float>(data_type, inTime, v_min, v_max, is_logarithmic, logarithmic_zero_epsilon, zero_deadzone_halfsize);
                    if (axis == ImGuiAxis_Y)
                        grab_t = 1.0f - grab_t;
                    const float grab_pos = ImLerp(slider_usable_pos_min, slider_usable_pos_max, grab_t);
                    const bool clicked_around_grab = (mouse_abs_pos >= grab_pos - grab_sz * 0.5f - 1.0f) && (mouse_abs_pos <= grab_pos + grab_sz * 0.5f + 1.0f); // No harm being extra generous here.
                    g.SliderGrabClickOffset = (clicked_around_grab && is_floating_point) ? mouse_abs_pos - grab_pos : 0.0f;
                }
                if (slider_usable_sz > 0.0f)
                    clicked_t = ImSaturate((mouse_abs_pos - g.SliderGrabClickOffset - slider_usable_pos_min) / slider_usable_sz);
                if (axis == ImGuiAxis_Y)
                    clicked_t = 1.0f - clicked_t;
                set_new_value = true;
            }
        }

        if (set_new_value)
        {
            float v_new = ImGui::ScaleValueFromRatioT<float, float, float>(data_type, clicked_t, v_min, v_max, is_logarithmic, logarithmic_zero_epsilon, zero_deadzone_halfsize);

            // Round to user desired precision based on format string
            if (is_floating_point && !(flags & ImGuiSliderFlags_NoRoundToFormat))
                v_new = ImGui::RoundScalarWithFormatT<float>(format, data_type, v_new);

            // Apply result
            if (inTime != v_new)
            {
                inTime = v_new;
                value_changed = true;
            }
        }
    }

    if (slider_sz < 1.0f)
    {
        grab_bb = ImRect(bb.Min, bb.Min);
    }
    else
    {
        // Output grab position so it can be displayed by the caller
        float grab_t = ImGui::ScaleRatioFromValueT<float, float, float>(data_type, inTime, v_min, v_max, is_logarithmic, logarithmic_zero_epsilon, zero_deadzone_halfsize);
        if (axis == ImGuiAxis_Y)
            grab_t = 1.0f - grab_t;
        const float grab_pos = ImLerp(slider_usable_pos_min, slider_usable_pos_max, grab_t);
        if (axis == ImGuiAxis_X)
            grab_bb = ImRect(grab_pos - grab_sz * 0.5f, bb.Min.y + grab_padding, grab_pos + grab_sz * 0.5f, bb.Max.y - grab_padding);
        else
            grab_bb = ImRect(bb.Min.x + grab_padding, grab_pos - grab_sz * 0.5f, bb.Max.x - grab_padding, grab_pos + grab_sz * 0.5f);
    }

    if (value_changed)
        ImGui::MarkItemEdited(id);

    ImGui::PopStyleVar();
    
    if (was_right_clicked || was_left_clicked || hovering_any_keyframe)
        value_changed = false;

    ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImGui::GetStyleColorVec4(ImGuiCol_Text));

    // Render grab
    if (grab_bb.Max.x > grab_bb.Min.x)
    {
        grab_bb.Min.y -= style.FrameRounding * 0.30f;
        grab_bb.Max.y += style.FrameRounding * 0.30f;

        ImGuiCol color = (g.ActiveId == id && allow_slider_input) ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab;

        if (m_State == SEQUENCE_PLAYING)
            color = ImGuiCol_SliderGrabActive;

        window->DrawList->AddRectFilled(grab_bb.Min, grab_bb.Max, ImGui::GetColorU32(color), 1.0f);
    }

    ImGui::PopStyleColor(2);

    IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags | (temp_input_allowed ? ImGuiItemStatusFlags_Inputable : 0));

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
                if (m_SelectedKeyframe != -1)
                    m_Sequence.RemoveKeyFrame(m_SelectedKeyframe);
                
                m_SelectedKeyframe = -1;
                break;
            }
        }
    }
}


void SequenceWidget::ApplyToCamera(Camera& ioCamera, float inDeltaTime)
{
    if (m_State == SEQUENCE_PLAYING)
    {
        m_Time += inDeltaTime;

        if (m_Time > m_Sequence.GetDuration())
            m_Time = 0.0f;
    }

    Vec2 angle = m_Sequence.GetAngle(ioCamera, m_Time);
    Vec3 position = m_Sequence.GetPosition(ioCamera, m_Time);

    ioCamera.SetAngle(angle);
    ioCamera.SetPosition(position);
}


} // raekor