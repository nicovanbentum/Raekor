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
    IWidget(inApp, (const char*)( ICON_FA_FILM "  Sequencer " )) {}


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

    if (!m_LockedToCamera)
    {
        ImGui::PopItemFlag();
        ImGui::PopStyleColor();
    }

    ImGui::SameLine();

    if (ImGui::Button((const char*)ICON_FA_SAVE))
    {
        const auto file_path = OS::sSaveFileDialog("JSON File (*.json)\0", "json");

        if (!file_path.empty())
        {
            JSON::WriteArchive archive(file_path);
            archive << m_Sequence;
        }
    }

    ImGui::SameLine();

    if (ImGui::Button((const char*)ICON_FA_FOLDER_OPEN))
    {
        const auto file_path = OS::sOpenFileDialog("JSON File (*.json)\0");

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

    const auto text_width = ImGui::CalcTextSize("Duration: 60.000 seconds").x;
    ImGui::SetNextItemWidth(text_width);

    auto cursor_pos_x = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - text_width;

    ImGui::SetCursorPosX(cursor_pos_x);

    auto duration_value = m_Sequence.GetDuration();
    auto duration_text = duration_value > 1 ? "Duration: %.1f seconds" : "Duration: %.1f second";

    if (m_State == SEQUENCE_PLAYING)
    {
        duration_value = m_Time;
        duration_text = duration_value > 1 ? "Playing: %.1f seconds" : "Playing: %.1f second";
    }

    if (ImGui::DragFloat("##sequenceduration", &duration_value, cIncrSequence, cMinSequenceLength, cMaxSequenceLength, duration_text))
    {
        if (m_State != SEQUENCE_PLAYING)
            m_Sequence.SetDuration(duration_value);
    }

    const auto avail = ImGui::GetContentRegionAvail();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));

    ImGui::VSliderFloat("##sequencespeed", ImVec2(avail.x * 0.01f, avail.y), &m_Speed, 0.0f, 1.0f, "%", ImGuiSliderFlags_NoRoundToFormat);

    ImGui::SameLine();

    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);

    // don't pass m_Time directly as DrawTimeline's return value indicates if we should update the value or not
    auto time = m_Time;
    
    if (DrawTimeline("Timeline", time, 0.0f, glm::min(m_Sequence.GetDuration(), cMaxSequenceLength)))
        m_Time = time;

    ImGui::PopStyleVar();

    ImGui::End();
}


bool SequenceWidget::DrawTimeline(const char* inLabel, float& inTime, const float inMinTime, const float inMaxTime)
{
    auto window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    auto& imgui = *GImGui;
    const auto& style = imgui.Style;
    const auto id = window->GetID(inLabel);

    auto frame_bb = ImRect(window->DC.CursorPos, window->DC.CursorPos + ImGui::GetContentRegionAvail());

    const auto label_size = ImGui::CalcTextSize(inLabel, NULL, true);
    const auto total_bb   = ImRect(frame_bb.Min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0.0f));

    const auto flags = 0u;
    const auto temp_input_allowed = (flags & ImGuiSliderFlags_NoInput) == 0;

    ImGui::ItemSize(total_bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(total_bb, id, &frame_bb, temp_input_allowed ? ImGuiItemFlags_Inputable : 0))
        return false;

    const auto is_hovered  = ImGui::ItemHoverable(frame_bb, id);
    const auto is_clicked  = is_hovered && ImGui::IsMouseClicked(0, id);
    const auto make_active = (is_clicked || imgui.NavActivateId == id);

    if (make_active)
    {
        ImGui::SetActiveID(id, window);
        ImGui::SetFocusID(id, window);
        ImGui::FocusWindow(window);
        imgui.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
    }

    // Draw frame
    const auto frame_col = ImGui::GetColorU32(imgui.ActiveId == id ? ImGuiCol_FrameBgActive : ImGuiCol_FrameBg);
    ImGui::RenderFrame(frame_bb.Min, frame_bb.Max, frame_col, true, imgui.Style.FrameRounding);

    ImGui::PushClipRect(frame_bb.Min, frame_bb.Max, false);

    auto line_nr = 0u;

    for (float f = 0.0f; f < m_Sequence.GetDuration(); f += cIncrSequence)
    {
        const auto x = frame_bb.Min.x + frame_bb.GetWidth() * (f / m_Sequence.GetDuration());

        const auto odd_color  = ImVec4(0.2f, 0.2f, 0.2f, 0.5f);
        const auto even_color = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
        
        auto line_color = line_nr % 2 != 0 ? odd_color : even_color;

        window->DrawList->AddLine(ImVec2(x, frame_bb.Min.y), ImVec2(x, frame_bb.Max.y), ImGui::GetColorU32(line_color), 2.0f);

        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));

        char value_buf[64];
        const auto value_buf_end = value_buf + ImGui::DataTypeFormatString(value_buf, IM_ARRAYSIZE(value_buf), ImGuiDataType_Float, &f, "%.1f");

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

        auto ngon_center_x = padded_bb.Min.x + padded_bb.GetWidth() * (keyframe.mTime / m_Sequence.GetDuration());
        auto ngon_center_y = padded_bb.Min.y + 0.5f * padded_bb.GetHeight();

        const auto ngon_bb_min = ImVec2(ngon_center_x - radius, ngon_center_y - radius);
        const auto ngon_bb_max = ImVec2(ngon_center_x + radius, ngon_center_y + radius);
        const auto is_hovering_keyframe = ImGui::IsMouseHoveringRect(ngon_bb_min, ngon_bb_max);
        
        if (m_SelectedKeyframe == keyframe_index && ImGui::IsMouseDragging(0) && m_IsDraggingKeyframe)
        {
            ngon_center_x = ImGui::GetMousePos().x; // move the keyframe to the cursor, horizontal only
            keyframe.mTime = (ngon_center_x - padded_bb.Min.x) / padded_bb.GetWidth() * m_Sequence.GetDuration();
            keyframe.mTime = glm::clamp(keyframe.mTime, 0.0f, m_Sequence.GetDuration());

            if (m_SelectedKeyframe - 1 >= 0)
            {
                auto& prev_keyframe = m_Sequence.GetKeyFrame(m_SelectedKeyframe - 1);
                // if we moved backwards in time past a previous keyframe, swap
                if (keyframe.mTime < prev_keyframe.mTime)
                {
                    std::swap(prev_keyframe, keyframe);
                    m_SelectedKeyframe = m_SelectedKeyframe - 1;
                }
            }

            if (m_SelectedKeyframe + 1 < m_Sequence.GetKeyFrameCount())
            {
                auto& next_keyframe = m_Sequence.GetKeyFrame(m_SelectedKeyframe + 1);
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
            const auto color = ImGui::GetColorU32(ImVec4(1, 0.9, 0, 1));
            window->DrawList->AddNgonFilled(ImVec2(ngon_center_x, ngon_center_y), radius + 2.0f, color, segments);
        }
        
        const auto regular_color  = ImGui::GetColorU32(ImVec4(1.0f, 0.647, 0.0f, 1.0f));
        const auto selected_color = ImGui::GetColorU32(ImVec4(1.0f, 0.2f, 0.0f, 1.0f));
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
                m_Sequence.RemoveKeyFrame(m_SelectedKeyframe);

            m_SelectedKeyframe = -1;
        }

        if (ImGui::MenuItem("Duplicate"))
        {
            auto key_frame = m_Sequence.GetKeyFrame(m_SelectedKeyframe);
            // slightly offset the time so we can actually see the duplicate and it doesn't perfectly overlap
            m_Sequence.AddKeyFrame(m_Editor->GetViewport().GetCamera(), key_frame.mTime + 0.05f, key_frame.mPosition, key_frame.mAngle);
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
    const auto can_click_timeline = frame_bb.Contains(ImGui::GetMousePos()) && 
                                    !was_right_clicked && !was_left_clicked && 
                                    !ImGui::IsPopupOpen("##keyframerightclicked");

    if (can_click_timeline && ImGui::IsMouseClicked(0))
        m_SelectedKeyframe = -1;

    if (can_click_timeline && ImGui::IsMouseClicked(1))
        ImGui::OpenPopup("##timelinerightclicked");

    ImGui::PopClipRect();

    // Slider behavior
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 4.0f);

    const auto v_min = inMinTime;
    const auto v_max = inMaxTime;

    const auto axis = ImGuiAxis_X;
    const auto is_floating_point = true;
    const auto v_range_f = (float)(v_min < v_max ? v_max - v_min : v_min - v_max); // We don't need high precision for what we do with it.

    // Calculate bounds
    const auto bb = frame_bb;
    const auto grab_padding = 2.0f; // FIXME: Should be part of style.
    const auto slider_sz = (bb.Max[axis] - bb.Min[axis]) - grab_padding * 2.0f;

    auto grab_sz = style.GrabMinSize;
    if (!is_floating_point && v_range_f >= 0.0f)                         // v_range_f < 0 may happen on integer overflows
        grab_sz = ImMax(slider_sz / (v_range_f + 1), style.GrabMinSize); // For integer sliders: if possible have the grab size represent 1 unit

    grab_sz = ImMin(grab_sz, slider_sz);
    const auto slider_usable_sz = slider_sz - grab_sz;
    const auto slider_usable_pos_min = bb.Min[axis] + grab_padding + grab_sz * 0.5f;
    const auto slider_usable_pos_max = bb.Max[axis] - grab_padding - grab_sz * 0.5f;

    const auto is_logarithmic = false;
    auto logarithmic_zero_epsilon = 0.0f; // Only valid when is_logarithmic is true
    auto zero_deadzone_halfsize = 0.0f; // Only valid when is_logarithmic is true

    // Process interacting with the slider
    auto value_changed = false;
    const auto allow_slider_input = !was_right_clicked && !was_left_clicked && !m_IsDraggingKeyframe;

    if (imgui.ActiveId == id && allow_slider_input)
    {
        auto clicked_t = 0.0f;
        auto set_new_value = false;

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
                    auto grab_t = ImGui::ScaleRatioFromValueT<float, float, float>(ImGuiDataType_Float, inTime, v_min, v_max, is_logarithmic, logarithmic_zero_epsilon, zero_deadzone_halfsize);

                    const auto grab_pos = ImLerp(slider_usable_pos_min, slider_usable_pos_max, grab_t);
                    const auto clicked_around_grab = (mouse_abs_pos >= grab_pos - grab_sz * 0.5f - 1.0f) && (mouse_abs_pos <= grab_pos + grab_sz * 0.5f + 1.0f); // No harm being extra generous here.
                    imgui.SliderGrabClickOffset = (clicked_around_grab && is_floating_point) ? mouse_abs_pos - grab_pos : 0.0f;
                }

                if (slider_usable_sz > 0.0f)
                    clicked_t = ImSaturate((mouse_abs_pos - imgui.SliderGrabClickOffset - slider_usable_pos_min) / slider_usable_sz);

                set_new_value = true;
            }
        }

        if (set_new_value)
        {
            auto v_new = ImGui::ScaleValueFromRatioT<float, float, float>(ImGuiDataType_Float, clicked_t, v_min, v_max, is_logarithmic, logarithmic_zero_epsilon, zero_deadzone_halfsize);

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

    auto grab_bb = ImRect(ImVec2(FLT_MAX, FLT_MAX), ImVec2(FLT_MIN, FLT_MIN));

    if (slider_sz < 1.0f)
        grab_bb = ImRect(bb.Min, bb.Min);
    else
    {
        // Output grab position so it can be displayed by the caller
        auto grab_t = ImGui::ScaleRatioFromValueT<float, float, float>(ImGuiDataType_Float, inTime, v_min, v_max, is_logarithmic, logarithmic_zero_epsilon, zero_deadzone_halfsize);

        const auto grab_pos = ImLerp(slider_usable_pos_min, slider_usable_pos_max, grab_t);

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

        auto color = (imgui.ActiveId == id && allow_slider_input) ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab;

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
                        m_Sequence.RemoveKeyFrame(m_SelectedKeyframe);
                
                    m_SelectedKeyframe = -1;
                }
                break;
            }
        }
    }
}


void SequenceWidget::ApplyToCamera(Camera& ioCamera, float inDeltaTime)
{
    if (m_State == SEQUENCE_PLAYING)
    {
        m_Time += inDeltaTime * m_Speed;

        if (m_Time > m_Sequence.GetDuration())
            m_Time = 0.0f;
    }

    const auto angle = m_Sequence.GetAngle(ioCamera, m_Time);
    const auto position = m_Sequence.GetPosition(ioCamera, m_Time);

    ioCamera.SetAngle(angle);
    ioCamera.SetPosition(position);
}


} // raekor