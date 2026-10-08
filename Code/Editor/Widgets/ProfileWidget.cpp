#include "PCH.h"
#include "ProfileWidget.h"

#include "Iter.h"
#include "Hash.h"
#include "Timer.h"
#include "Editor.h"
#include "Profiler.h"
#include "Application.h"

namespace RK {

RTTI_DEFINE_TYPE_NO_FACTORY(ProfileWidget) {}


ProfileWidget::ProfileWidget(Editor* inEditor) : IWidget(inEditor, reinterpret_cast<const char*>( ICON_FA_RULER " Profiler " )) {}


void ProfileWidget::Draw(Widgets* inWidgets, float inDeltaTime)
{
	ImGui::Begin(m_Title.c_str(), &m_Open);
	m_Visible = ImGui::IsWindowAppearing();

	ImGui::AlignTextToFramePadding();
	ImGui::Text("Filter:");
	ImGui::SameLine();

	ImGui::SetNextItemWidth(ImGui::CalcTextSize("RK::DX12::RenderGraph::Execute").x);
	ImGui::InputText("##profilerfilterinput", &m_FilterInputBuffer);
	ImGui::SameLine();

	ImGuiTextFilter filter = ImGuiTextFilter(m_FilterInputBuffer.c_str());

	bool enabled = g_Profiler->IsEnabled();
	if (ImGui::Checkbox(enabled ? "Running" : "Paused", &enabled))
		g_Profiler->SetEnabled(enabled);

	ImGui::SameLine();

	ImGui::SetNextItemWidth(100.0f);
	ImGui::SliderFloat("Zoom##profilerzoom", &m_Zoom, 1.0f, 10.0f, "%.1f");

	const Array<ProfileSection>& cpu_sections = g_Profiler->GetCPUSections();
	const Array<ProfileSection>& gpu_sections = g_Profiler->GetGPUSections();

	const TrackBounds cpu_bounds = sGetTrackBounds(cpu_sections);
	const TrackBounds gpu_bounds = sGetTrackBounds(gpu_sections);

	uint32_t row_count = 0;
	uint32_t track_count = 0;
	uint64_t total_ticks = 1;

	if (!cpu_sections.empty())
	{
		track_count++;
		row_count += cpu_bounds.mMaxDepth + 1;
		total_ticks = glm::max(total_ticks, cpu_bounds.mHighestTick - cpu_bounds.mLowestTick);
	}

	if (!gpu_sections.empty())
	{
		track_count++;
		row_count += gpu_bounds.mMaxDepth + 1;
		total_ticks = glm::max(total_ticks, gpu_bounds.mHighestTick - gpu_bounds.mLowestTick);
	}

	if (track_count == 0)
	{
		ImGui::TextDisabled("No profiling data available.");
		ImGui::End();
		return;
	}

	ImVec2 avail_size = ImGui::GetContentRegionAvail();
	avail_size.x = glm::max(avail_size.x, 1.0f);

	const float track_label_height = ImGui::GetTextLineHeightWithSpacing() * track_count;
	const float bar_height = glm::max(( avail_size.y - track_label_height ) / row_count, 1.0f) * m_Zoom;
	const float pixels_per_tick = avail_size.x / total_ticks;

	if (!cpu_sections.empty())
		DrawTrack(0, "CPU", cpu_sections, pixels_per_tick, bar_height, filter);

	if (!gpu_sections.empty())
		DrawTrack(1, "GPU", gpu_sections, pixels_per_tick, bar_height, filter);

	ImGui::End();
}


ProfileWidget::TrackBounds ProfileWidget::sGetTrackBounds(const Array<ProfileSection>& inSections)
{
	TrackBounds bounds;

	for (const ProfileSection& section : inSections)
	{
		bounds.mMaxDepth = glm::max(bounds.mMaxDepth, section.mDepth);
		bounds.mLowestTick = glm::min(bounds.mLowestTick, section.mStartTick);
		bounds.mHighestTick = glm::max(bounds.mHighestTick, section.mEndTick);
	}

	return bounds;
}


void ProfileWidget::DrawTrack(int inTrack, const char* inLabel, const Array<ProfileSection>& inSections, float inPixelsPerTick, float inBarHeight, const ImGuiTextFilter& inFilter)
{
	const TrackBounds bounds = sGetTrackBounds(inSections);
	const float track_time = Timer::sGetTicksToSeconds(bounds.mHighestTick - bounds.mLowestTick);

	ImGui::Text("%s (%.2f ms)", inLabel, Timer::sToMilliseconds(track_time));

	ImGui::PushID(inTrack);

	const ImVec2 start_pos = ImGui::GetCursorScreenPos();
	const float track_width = ImGui::GetContentRegionAvail().x;

	for (const auto& [index, section] : gEnumerate(inSections))
	{
		const float start_pos_x = start_pos.x + ( section.mStartTick - bounds.mLowestTick ) * inPixelsPerTick;
		const float end_pos_x = start_pos.x + ( section.mEndTick - bounds.mLowestTick ) * inPixelsPerTick;

		// skip sections that are too small to see
		if (end_pos_x - start_pos_x < 1.0f)
			continue;

		const float start_pos_y = start_pos.y + ( section.mDepth + 0 ) * inBarHeight;
		const float end_pos_y = start_pos.y + ( section.mDepth + 1 ) * inBarHeight;

		const ImVec2 pad = ImVec2(1.0f, 1.0f);
		const ImRect bbox = ImRect(ImVec2(start_pos_x, start_pos_y), ImVec2(end_pos_x, end_pos_y));

		char text_buffer[255];
		ImFormatString(text_buffer, std::size(text_buffer), "%s (%.2f ms)", section.mName, Timer::sToMilliseconds(section.GetSeconds()));

		ImGui::PushID(index);

		ImGui::SetCursorScreenPos(bbox.Min);
		ImGui::InvisibleButton(section.mName, bbox.GetSize(), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

		ImGui::PopID();

		const bool is_selected = m_SelectedTrack == inTrack && m_SelectedSectionIndex == index;

		if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
		{
			m_SelectedTrack = is_selected ? -1 : inTrack;
			m_SelectedSectionIndex = is_selected ? -1 : index;
		}

		const bool is_hovered = ImGui::IsItemHovered();

		if (is_hovered)
			ImGui::SetTooltip(text_buffer);

		const float label_hash = float(gHash32Bit(section.mName)) / UINT32_MAX;
		const bool passes_filter = inFilter.PassFilter(section.mName);

		float upper_v = is_hovered ? 0.6f : 0.5f;
		float lower_v = is_hovered ? 0.5f : 0.4f;

		if (!passes_filter)
		{
			upper_v = 0.2f;
			lower_v = 0.2f;
		}

		ImVec4 upper_hsv = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
		ImVec4 lower_hsv = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
		ImGui::ColorConvertHSVtoRGB(label_hash, 0.5f, upper_v, upper_hsv.x, upper_hsv.y, upper_hsv.z);
		ImGui::ColorConvertHSVtoRGB(label_hash, 0.5f, lower_v, lower_hsv.x, lower_hsv.y, lower_hsv.z);

		const ImU32 upper_gradient = ImGui::ColorConvertFloat4ToU32(upper_hsv);
		const ImU32 lower_gradient = ImGui::ColorConvertFloat4ToU32(lower_hsv);

		ImGui::GetWindowDrawList()->AddRectFilled(bbox.Min, bbox.Max, is_selected ? IM_COL32_WHITE : IM_COL32_BLACK);
		ImGui::GetWindowDrawList()->AddRectFilledMultiColor(bbox.Min + pad, bbox.Max - pad, upper_gradient, lower_gradient, lower_gradient, upper_gradient);

		// dont render labels for tiny bars
		if (bbox.GetWidth() > track_width * 0.01f)
		{
			const ImVec2 label_size = ImGui::CalcTextSize(text_buffer, NULL, true);
			const ImRect text_clip_rect = ImRect(bbox.Min + pad, bbox.Max - pad * 2.0f);
			const ImColor text_color = passes_filter ? ImGui::GetStyleColorVec4(ImGuiCol_Text) : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(text_color));
			ImGui::RenderTextClipped(bbox.Min + pad * 2.0f, bbox.Max - pad * 2.0f, text_buffer, NULL, &label_size, ImGui::GetStyle().ButtonTextAlign, &text_clip_rect);
			ImGui::PopStyleColor();
		}
	}

	ImGui::PopID();

	ImGui::SetCursorScreenPos(start_pos);
	ImGui::Dummy(ImVec2(track_width, ( bounds.mMaxDepth + 1 ) * inBarHeight));
}


void ProfileWidget::OnEvent(Widgets* inWidgets, const SDL_Event& inEvent)
{
	if (inEvent.type == SDL_EVENT_KEY_DOWN && !inEvent.key.repeat)
	{
		switch (inEvent.key.key)
		{
			case SDLK_SPACE:
			{
				g_Profiler->SetEnabled(!g_Profiler->IsEnabled());
			} break;
		}
	}
}

} // raekor
