#include "pch.h"
#include "consoleWidget.h"
#include "iter.h"
#include "application.h"

namespace RK {

RTTI_DEFINE_TYPE_NO_FACTORY(ConsoleWidget) {}


ConsoleWidget::ConsoleWidget(Application* inApp) : IWidget(inApp, reinterpret_cast<const char*>( ICON_FA_TERMINAL "  Console " )) {}


void ConsoleWidget::Draw(Widgets* inWidgets, float inDeltaTime)
{
	ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);

	if (!ImGui::Begin(m_Title.c_str(), &m_Open, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	{
		ImGui::End();
		return;
	}

	m_Visible = ImGui::IsWindowAppearing();

	const float footer_height = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();

	ImGui::BeginChild("##LOG", ImVec2(ImGui::GetContentRegionAvail().x, -footer_height), false, ImGuiWindowFlags_HorizontalScrollbar);

	if (ImGui::BeginPopupContextWindow())
	{
		if (ImGui::Selectable("clear")) m_Items.clear();
		ImGui::EndPopup();
	}

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
	for (const String& item : m_Items)
		ImGui::TextUnformatted(item.c_str());

	if (m_ShouldScrollToBottom || ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
	{
		ImGui::SetScrollHereY(1.0f);
		m_ShouldScrollToBottom = false;
	}

	ImGui::PopStyleVar();
	ImGui::EndChild();

	const ImVec2 cursor_after_log = ImGui::GetCursorScreenPos();

	ImGui::Separator();

	ImGui::SetItemDefaultFocus();

	ImGui::PushItemWidth(ImGui::GetWindowWidth());
	ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory;

	if (ImGui::InputText("##Input", &m_InputBuffer, flags, sEditCallback, (void*)this))
	{
		if (!m_InputBuffer.empty())
		{
			m_Items.push_back(m_InputBuffer);
			m_ShouldScrollToBottom = true;

			std::istringstream stream(m_InputBuffer);
			std::string name, value;
			stream >> name >> value;

			bool success = g_CVars.SetValue(name, value);
			if (!success)
			{
				if (!g_CVars.Exists(name))
					m_Items.emplace_back("cvar \"" + name + "\" does not exist.");

				else if (value.empty())
					m_Items.emplace_back("Please provide a value.");

				else
					m_Items.emplace_back("Failed to set cvar " + name + " to " + "\"" + value + "\"");
			}
			else
			{
				if (name.starts_with('r'))
					m_Editor->GetRenderInterface()->OnResize(m_Editor->GetViewport());
			}
		}

		m_InputBuffer.clear();
		ImGui::SetKeyboardFocusHere();
	}

	if (!m_InputBuffer.empty())
	{
		const int suggestion_count = int(ImGui::GetWindowHeight() * ( 2.0f / 3.0f ) / ImGui::GetTextLineHeightWithSpacing());
		const float suggestion_width = ImGui::GetItemRectSize().x - ImGui::GetStyle().FramePadding.x * 2;
		const float suggestion_height = ImGui::GetTextLineHeightWithSpacing() * suggestion_count;

		ImGui::SetNextWindowSize(ImVec2(suggestion_width, suggestion_height));
		ImGui::SetNextWindowPos(ImVec2(cursor_after_log.x, cursor_after_log.y - suggestion_height));
		ImGui::BeginTooltip();

		const ImGuiTextFilter filter = ImGuiTextFilter(m_InputBuffer.c_str());

		int first_cvar_index = -1;

		for (const auto& [index, mapping] : gEnumerate(g_CVars))
		{
			if (!filter.PassFilter(mapping.first.c_str()))
				continue;

			if (first_cvar_index == -1)
			{
				first_cvar_index = index;
				m_ActiveItem = glm::max(m_ActiveItem, first_cvar_index);
			}

			const String cvar_text = mapping.first + " " + g_CVars.GetValue(mapping.first) + '\n';

			if (index == m_ActiveItem)
			{
				ImGui::Selectable(cvar_text.c_str(), true);
			}
			else
				ImGui::TextUnformatted(cvar_text.c_str());
		}

		const size_t nr_of_cvars = g_CVars.GetCount();
		m_ActiveItem = m_ActiveItem > nr_of_cvars ? nr_of_cvars : m_ActiveItem;

		ImGui::EndTooltip();
	}

	ImGui::PopItemWidth();
	ImGui::Separator();
	ImGui::End();
}


void ConsoleWidget::LogMessage(const std::string& inMessage)
{
	std::scoped_lock lock(m_ItemsMutex);
	m_Items.push_back(inMessage);
}


int ConsoleWidget::sEditCallback(ImGuiInputTextCallbackData* data)
{
	ConsoleWidget* console = (ConsoleWidget*)data->UserData;

	if (data->EventKey == ImGuiKey_Tab && data->BufTextLen)
	{
		ImGuiTextFilter filter = ImGuiTextFilter(data->Buf);

		for (const auto& [index, cvar] : gEnumerate(g_CVars))
		{
			if (!filter.PassFilter(cvar.first.c_str()))
				continue;

			if (index == console->m_ActiveItem)
			{
				data->DeleteChars(0, data->BufTextLen);
				data->InsertChars(0, std::string(cvar.first + ' ').c_str());
				break;
			}
		}
	}

	auto GoToNextItem = [&]() -> int
	{
		bool found_active = false;
		ImGuiTextFilter filter = ImGuiTextFilter(data->Buf);

		for (const auto& [index, cvar] : gEnumerate(g_CVars))
		{
			if (index == console->m_ActiveItem)
			{
				found_active = true;
				continue;
			}

			if (!filter.PassFilter(cvar.first.c_str()))
				continue;

			if (found_active)
				return index;
		}

		return console->m_ActiveItem;
	};

	auto GoToPreviousItem = [&]() -> int
	{
		bool found_active = false;
		int previous_index = 0;
		ImGuiTextFilter filter = ImGuiTextFilter(data->Buf);

		for (const auto& [index, cvar] : gEnumerate(g_CVars))
		{
			if (!filter.PassFilter(cvar.first.c_str()))
				continue;

			if (index == console->m_ActiveItem)
				return previous_index;

			previous_index = index;
		}

		return console->m_ActiveItem;
	};

	if (data->EventKey == ImGuiKey_DownArrow)
		console->m_ActiveItem = GoToNextItem();

	if (data->EventKey == ImGuiKey_UpArrow && console->m_ActiveItem)
		console->m_ActiveItem = GoToPreviousItem();

    if (data->EventKey == ImGuiKey_UpArrow && data->BufTextLen == 0)
        data->InsertChars(0, console->m_Items.back().c_str());

	return 0;
}

}