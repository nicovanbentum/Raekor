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
            auto json_data = JSON::JSONData(file_path);
            JSON::ReadArchive archive(json_data);
            archive >> m_Sequence;
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("+"))
        m_Sequence.AddKeyFrame(m_Editor->GetViewport().GetCamera(), m_Time);

    ImGui::SameLine();

    float duration = m_Sequence.GetDuration();

    if (ImGui::DragFloat("Duration", &duration, cIncrSequence, cMinSequenceLength, cMaxSequenceLength, "%.2f"))
        m_Sequence.SetDuration(duration);

   // if (ImGui::DragFloat("Time", &m_Time, cIncrSequence, cMinSequenceLength, duration, "%.2f"))
   //     m_Time = glm::min(m_Time, duration);
  
   // if (ImGui::Button("+"))
   //     m_Sequence.AddKeyFrame(m_Editor->GetViewport().GetCamera(), m_Time);

    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);

    float min = 0.0f;
    float max = glm::min(duration, cMaxSequenceLength);
    DrawTimeline("Timeline", ImGuiDataType_Float, &m_Time, &min, &max, "%.2f");


    if (m_State == SEQUENCE_PLAYING)
    {
        m_Time += inDeltaTime;

        if (m_Time > m_Sequence.GetDuration())
            m_Time = 0.0f;
    }

    ImGui::End();
}


bool SequenceWidget::DrawTimeline(const char* label, ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags)
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
        // Tabbing or CTRL-clicking on Slider turns it into an input box
        const bool clicked = hovered && ImGui::IsMouseClicked(0, id);
        const bool make_active = (clicked || g.NavActivateId == id);
        if (make_active && clicked)
            ImGui::SetKeyOwner(ImGuiKey_MouseLeft, id);
        if (make_active && temp_input_allowed)
            if ((clicked && g.IO.KeyCtrl) || (g.NavActivateId == id && (g.NavActivateFlags & ImGuiActivateFlags_PreferInput)))
                temp_input_is_active = true;

        if (make_active && !temp_input_is_active)
        {
            ImGui::SetActiveID(id, window);
            ImGui::SetFocusID(id, window);
            ImGui::FocusWindow(window);
            g.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
        }
    }

    if (temp_input_is_active)
    {
        // Only clamp CTRL+Click input when ImGuiSliderFlags_AlwaysClamp is set
        const bool is_clamp_input = (flags & ImGuiSliderFlags_AlwaysClamp) != 0;
        return ImGui::TempInputScalar(frame_bb, id, label, data_type, p_data, format, is_clamp_input ? p_min : NULL, is_clamp_input ? p_max : NULL);
    }

    // Draw frame
    const ImU32 frame_col = ImGui::GetColorU32(g.ActiveId == id ? ImGuiCol_FrameBgActive : ImGuiCol_FrameBg);
    ImGui::RenderFrame(frame_bb.Min, frame_bb.Max, frame_col, true, g.Style.FrameRounding);

    // Slider behavior
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 4.0f);

    ImRect grab_bb;
    const bool value_changed = ImGui::SliderBehavior(frame_bb, id, data_type, p_data, p_min, p_max, format, flags, &grab_bb);
    if (value_changed)
        ImGui::MarkItemEdited(id);

    ImGui::PopStyleVar();

    ImGui::PushClipRect(frame_bb.Min, frame_bb.Max, false);

    for (auto keyframe_index = 0u; keyframe_index < m_Sequence.GetKeyFrameCount(); keyframe_index++)
    {
        const auto& keyframe = m_Sequence.GetKeyFrame(keyframe_index);

        const auto radius = 8.0f;
        const auto segments = 4;

        const auto center_x = frame_bb.Min.x + frame_bb.GetWidth() * (keyframe.mTime / m_Sequence.GetDuration());
        const auto center_y = frame_bb.Min.y + 0.5f * frame_bb.GetHeight();

        const auto color = ImGui::GetColorU32(ImVec4(1.0f, 0.647, 0.0f, 1.0f));
        window->DrawList->AddNgonFilled(ImVec2(center_x, center_y), radius, color, segments);

        //window->DrawList->AddRectFilled(ImVec2(start_x, frame_bb.Min.y), ImVec2(start_x + 4.0f, frame_bb.Max.y), ImGui::GetColorU32(g.ActiveId == id ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab), style.GrabRounding);
    }

    ImGui::PopClipRect();

    // Render grab
    if (grab_bb.Max.x > grab_bb.Min.x)
        window->DrawList->AddRectFilled(grab_bb.Min, grab_bb.Max, ImGui::GetColorU32(g.ActiveId == id ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab), style.GrabRounding);

    // Display value using user-provided display format so user can add prefix/suffix/decorations to the value.
    char value_buf[64];
    const char* value_buf_end = value_buf + ImGui::DataTypeFormatString(value_buf, IM_ARRAYSIZE(value_buf), data_type, p_data, format);
    if (g.LogEnabled)
        ImGui::LogSetNextTextDecoration("{", "}");
    ImGui::RenderTextClipped(frame_bb.Min, frame_bb.Max, value_buf, value_buf_end, NULL, ImVec2(0.5f, 0.5f));

    if (label_size.x > 0.0f)
        ImGui::RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y), label);

    IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags | (temp_input_allowed ? ImGuiItemStatusFlags_Inputable : 0));
    return value_changed;
}


void SequenceWidget::OnEvent(Widgets* inWidgets, const SDL_Event& ev)
{

}


void SequenceWidget::ApplyToCamera(Camera& ioCamera)
{
    Vec2 angle = m_Sequence.GetAngle(ioCamera, m_Time);
    Vec3 position = m_Sequence.GetPosition(ioCamera, m_Time);

    ioCamera.SetAngle(angle);
    ioCamera.SetPosition(position);
}


} // raekor