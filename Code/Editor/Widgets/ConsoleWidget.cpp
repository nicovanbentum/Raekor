#include "PCH.h"
#include "ConsoleWidget.h"
#include "Iter.h"
#include "Editor.h"

namespace RK {

RTTI_DEFINE_TYPE_NO_FACTORY(ConsoleWidget) {}


ConsoleWidget::ConsoleWidget(Editor* inEditor) : IWidget(inEditor, reinterpret_cast<const char*>( ICON_FA_TERMINAL "  Console " ))
{
	m_LogSink = g_Logger.AddSink([this](const LogMessage& inMessage)
	{
		std::scoped_lock lock(m_ItemsMutex);

		if (m_Items.size() >= cMaxItems)
			m_Items.erase(m_Items.begin(), m_Items.begin() + cMaxItems / 4);

		m_Items.push_back(inMessage);
	}, true);
}


ConsoleWidget::~ConsoleWidget()
{
	g_Logger.RemoveSink(m_LogSink);
}


void ConsoleWidget::Draw(Widgets* inWidgets, float inDeltaTime)
{
	ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);

	if (!ImGui::Begin(m_Title.c_str(), &m_Open, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	{
		ImGui::End();
		return;
	}

	m_Visible = ImGui::IsWindowAppearing();

	constexpr StaticArray level_colors =
	{
		ImVec4(0.55f, 0.55f, 0.55f, 1.0f),
		ImVec4(0.90f, 0.90f, 0.90f, 1.0f),
		ImVec4(1.00f, 0.80f, 0.30f, 1.0f),
		ImVec4(1.00f, 0.40f, 0.40f, 1.0f)
	};

	StaticArray<uint32_t, LOG_LEVEL_COUNT> level_counts = {};

	{
		std::scoped_lock lock(m_ItemsMutex);

		for (const LogMessage& item : m_Items)
			level_counts[item.mLevel]++;
	}

	bool clear_items = false;

	if (ImGui::Button((const char*)ICON_FA_TRASH))
		clear_items = true;

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Clear the console");

	ImGui::SameLine();

	for (int level = 0; level < LOG_LEVEL_COUNT; level++)
	{
		const String label = std::format("{} {}##ConsoleLevel{}", gToString(ELogLevel(level)), level_counts[level], level);

		ImGui::PushStyleColor(ImGuiCol_Text, m_ShowLevel[level] ? level_colors[level] : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		ImGui::PushStyleColor(ImGuiCol_Button, m_ShowLevel[level] ? ImGui::GetStyleColorVec4(ImGuiCol_FrameBg) : ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

		if (ImGui::Button(label.c_str()))
			m_ShowLevel[level] = !m_ShowLevel[level];

		ImGui::PopStyleColor(2);

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Show or hide %s messages", gToString(ELogLevel(level)));

		ImGui::SameLine();
	}

	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
	ImGui::InputTextWithHint("##ConsoleFilter", (const char*)ICON_FA_SEARCH "  Filter messages..", m_Filter.InputBuf, IM_ARRAYSIZE(m_Filter.InputBuf));

	if (ImGui::IsItemEdited())
		m_Filter.Build();

	const float footer_height = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();

	ImGui::BeginChild("##LOG", ImVec2(ImGui::GetContentRegionAvail().x, -footer_height), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);

	if (ImGui::BeginPopupContextWindow())
	{
		if (ImGui::Selectable("Clear"))
			clear_items = true;

		ImGui::EndPopup();
	}

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));

	{
		std::scoped_lock lock(m_ItemsMutex);

		if (clear_items)
			m_Items.clear();

		for (const LogMessage& item : m_Items)
		{
			if (!m_ShowLevel[item.mLevel])
				continue;

			const String line = std::format("[{}] {}", item.mCategory, item.mText);

			if (!m_Filter.PassFilter(line.c_str()))
				continue;

			ImGui::PushStyleColor(ImGuiCol_Text, level_colors[item.mLevel]);
			ImGui::TextUnformatted(line.c_str());
			ImGui::PopStyleColor();
		}
	}

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

	ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
	ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory;

	if (ImGui::InputTextWithHint("##Input", "Enter a console variable and value, Tab completes, Up/Down selects", &m_InputBuffer, flags, sEditCallback, (void*)this))
	{
		if (!m_InputBuffer.empty())
			ExecuteCommand(m_InputBuffer);

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

		for (const auto& [index, mapping] : gEnumerate(g_CVariables->GetCVars()))
		{
			if (!filter.PassFilter(mapping.first.c_str()))
				continue;

			if (first_cvar_index == -1)
			{
				first_cvar_index = index;
				m_ActiveItem = glm::max(m_ActiveItem, first_cvar_index);
			}

			const String cvar_text = mapping.first + " " + g_CVariables->GetValue(mapping.first) + '\n';

			if (index == m_ActiveItem)
			{
				ImGui::Selectable(cvar_text.c_str(), true);
			}
			else
				ImGui::TextUnformatted(cvar_text.c_str());
		}

		const size_t nr_of_cvars = g_CVariables->GetCount();
		m_ActiveItem = m_ActiveItem > nr_of_cvars ? nr_of_cvars : m_ActiveItem;

		ImGui::EndTooltip();
	}

	ImGui::PopItemWidth();
	ImGui::End();
}


void ConsoleWidget::ExecuteCommand(const String& inCommand)
{
	m_CommandHistory.push_back(inCommand);
	m_ShouldScrollToBottom = true;

	gLogInfo("Console", "> {}", inCommand);

	std::istringstream stream(inCommand);
	std::string name, value;
	stream >> name >> value;

	if (g_CVariables->SetValue(name, value))
		return;

	if (!g_CVariables->Exists(name))
		gLogWarning("Console", "cvar \"{}\" does not exist.", name);

	else if (value.empty())
		gLogWarning("Console", "Please provide a value for cvar \"{}\".", name);

	else
		gLogWarning("Console", "Failed to set cvar \"{}\" to \"{}\".", name, value);
}


int ConsoleWidget::sEditCallback(ImGuiInputTextCallbackData* data)
{
	ConsoleWidget* console = (ConsoleWidget*)data->UserData;

	if (data->EventKey == ImGuiKey_Tab && data->BufTextLen)
	{
		ImGuiTextFilter filter = ImGuiTextFilter(data->Buf);

		for (const auto& [index, cvar] : gEnumerate(g_CVariables->GetCVars()))
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

		for (const auto& [index, cvar] : gEnumerate(g_CVariables->GetCVars()))
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

		for (const auto& [index, cvar] : gEnumerate(g_CVariables->GetCVars()))
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

    if (data->EventKey == ImGuiKey_UpArrow && data->BufTextLen == 0 && !console->m_CommandHistory.empty())
        data->InsertChars(0, console->m_CommandHistory.back().c_str());

	return 0;
}

}