#include "pch.h"
#include "assetsWidget.h"

#include "iter.h"
#include "scene.h"
#include "components.h"
#include "Application.h"
#include "IconsFontAwesome5.h"

namespace RK {

RTTI_DEFINE_TYPE_NO_FACTORY(ComponentsWidget) {}

ComponentsWidget::ComponentsWidget(Application* inApp) : IWidget(inApp, reinterpret_cast<const char*>( ICON_FA_PALETTE "  Components " )) {}

void ComponentsWidget::UpdateLayoutSizes(float avail_width)
{
    // Layout: when not stretching: allow extending into right-most spacing.
    m_LayoutItemSpacing = (float)m_IconSpacing;
    if (m_StretchSpacing == false)
        avail_width += floorf(m_LayoutItemSpacing * 0.5f);

    const uint32_t item_count = GetScene().Count<Material>();

    // Layout: calculate number of icon per line and number of lines
    m_LayoutItemSize = ImVec2(floorf(m_IconSize), floorf(m_IconSize));
    m_LayoutColumnCount = glm::max((int)( avail_width / ( m_LayoutItemSize.x + m_LayoutItemSpacing ) ), 1);
    m_LayoutLineCount = ( item_count + m_LayoutColumnCount - 1 ) / m_LayoutColumnCount;

    // Layout: when stretching: allocate remaining space to more spacing. Round before division, so item_spacing may be non-integer.
    if (m_StretchSpacing && m_LayoutColumnCount > 1)
        m_LayoutItemSpacing = floorf(avail_width - m_LayoutItemSize.x * m_LayoutColumnCount) / m_LayoutColumnCount;

    m_LayoutItemStep = ImVec2(m_LayoutItemSize.x + m_LayoutItemSpacing, m_LayoutItemSize.y + m_LayoutItemSpacing);
    m_LayoutSelectableSpacing = glm::max(floorf(m_LayoutItemSpacing) - m_IconHitSpacing, 0.0f);
    m_LayoutOuterPadding = floorf(m_LayoutItemSpacing * 0.5f);
}

void ComponentsWidget::Draw(Widgets* inWidgets, float dt)
{
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(0, 0, 0, 0));

	ImGui::Begin(m_Title.c_str(), &m_Open, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_MenuBar);
	m_Visible = ImGui::IsWindowAppearing();
    
    ImGui::PopStyleColor(2);

	Scene& scene = GetScene();

    struct ComponentTypeInfo
    {
        const char* name;
        bool enabled;
    };

    static std::array component_type_infos = { 
        ComponentTypeInfo{"Material", true}, 
        ComponentTypeInfo{"Animation", false} 
    };

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Components"))
        {
            for (ComponentTypeInfo& info : component_type_infos)
            {
                ImGui::MenuItem(info.name, "", &info.enabled);
            }

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }


    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowContentSize(ImVec2(0.0f, m_LayoutOuterPadding + m_LayoutLineCount * ( m_LayoutItemSize.x + m_LayoutItemSpacing )));

    if (ImGui::BeginChild("Assets", ImVec2(0.0f, -1), ImGuiChildFlags_Border, ImGuiWindowFlags_NoMove))
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        const float avail_width = ImGui::GetContentRegionAvail().x;
        UpdateLayoutSizes(avail_width);

        ImVec2 start_pos = ImGui::GetCursorScreenPos();
        start_pos = ImVec2(start_pos.x + m_LayoutOuterPadding, start_pos.y + m_LayoutOuterPadding);
        ImGui::SetCursorScreenPos(start_pos);

        const Array<Entity>& entities = GetScene().GetEntities<Material>();
        const Array<Material>& materials = GetScene().GetStorage<Material>();

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(m_LayoutSelectableSpacing, m_LayoutSelectableSpacing));

        const int item_curr_idx_to_focus = -1;
        const int column_count = m_LayoutColumnCount;
        
        ImGuiListClipper clipper;
        clipper.Begin(m_LayoutLineCount, m_LayoutItemStep.y);
        
        if (item_curr_idx_to_focus != -1)
            clipper.IncludeItemByIndex(item_curr_idx_to_focus / column_count); // Ensure focused item line is not clipped.

        while (clipper.Step())
        {
            for (int line_idx = clipper.DisplayStart; line_idx < clipper.DisplayEnd; line_idx++)
            {
                const int item_min_idx_for_current_line = line_idx * column_count;
                const int item_max_idx_for_current_line = glm::min(( line_idx + 1 ) * column_count, int(materials.size()));

                for (int item_idx = item_min_idx_for_current_line; item_idx < item_max_idx_for_current_line; ++item_idx)
                {
                    const Entity& entity = entities[item_idx];
                    const Material& material = materials[item_idx];

                    ImGui::PushID(entity);

                    ImVec2 pos = ImVec2(start_pos.x + ( item_idx % column_count ) * m_LayoutItemStep.x, start_pos.y + line_idx * m_LayoutItemStep.y);
                    ImGui::SetCursorScreenPos(pos);
                    ImGui::SetNextItemSelectionUserData(item_idx);
                    
                    bool item_is_selected = entity == GetActiveEntity(); 
                    bool item_is_visible = ImGui::IsRectVisible(m_LayoutItemSize);
                    
                    if (ImGui::Selectable("", item_is_selected, ImGuiSelectableFlags_None, m_LayoutItemSize))
                        SetActiveEntity(entity);

                    if (item_curr_idx_to_focus == item_idx)
                        ImGui::SetKeyboardFocusHere(-1);

                    if (ImGui::BeginDragDropSource())
                    {
                        ImGui::EndDragDropSource();
                    }

                    if (item_is_visible)
                    {
                        ImVec2 box_min(pos.x - 1, pos.y - 1);
                        ImVec2 box_max(box_min.x + m_LayoutItemSize.x + 2, box_min.y + m_LayoutItemSize.y + 2); // dubious
                        draw_list->AddRectFilled(box_min, box_max, ImGui::GetColorU32(ImGuiCol_MenuBarBg)); 

                        if (material.gpuAlbedoMap && !material.albedoFile.empty())
                            draw_list->AddImage((void*)((intptr_t)m_Editor->GetRenderInterface()->GetImGuiTextureID(material.gpuAlbedoMap)), box_min + ImVec2(1, 1), box_max - ImVec2(1, 1));

                        if (m_LayoutItemSize.x >= ImGui::CalcTextSize("999").x)
                        {
                            ImU32 label_col = ImGui::GetColorU32(item_is_selected ? ImGuiCol_Text : ImGuiCol_TextDisabled);
                            char label[32];
                            sprintf(label, "%d", entity);
                            draw_list->AddText(ImVec2(box_min.x, box_max.y - ImGui::GetFontSize()), label_col, label);
                        }
                    }

                    ImGui::PopID();
                }
            }
        }
        clipper.End();
        ImGui::PopStyleVar();

        if (ImGui::IsWindowAppearing())
            m_ZoomWheelAccum = 0.0f;

        if (ImGui::IsWindowHovered() && io.MouseWheel != 0.0f && ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsAnyItemActive() == false)
        {
            m_ZoomWheelAccum += io.MouseWheel;
            if (fabsf(m_ZoomWheelAccum) >= 1.0f)
            {
                const float hovered_item_nx = ( io.MousePos.x - start_pos.x + m_LayoutItemSpacing * 0.5f ) / m_LayoutItemStep.x;
                const float hovered_item_ny = ( io.MousePos.y - start_pos.y + m_LayoutItemSpacing * 0.5f ) / m_LayoutItemStep.y;
                const int hovered_item_idx = ( (int)hovered_item_ny * m_LayoutColumnCount ) + (int)hovered_item_nx;

                m_IconSize *= powf(1.1f, (float)(int)m_ZoomWheelAccum);
                m_IconSize = glm::clamp(m_IconSize, 16.0f, 128.0f);
                m_ZoomWheelAccum -= (int)m_ZoomWheelAccum;
                UpdateLayoutSizes(avail_width);

                float hovered_item_rel_pos_y = ( (float)( hovered_item_idx / m_LayoutColumnCount ) + fmodf(hovered_item_ny, 1.0f) ) * m_LayoutItemStep.y;
                hovered_item_rel_pos_y += ImGui::GetStyle().WindowPadding.y;
                float mouse_local_y = io.MousePos.y - ImGui::GetWindowPos().y;
                ImGui::SetScrollY(hovered_item_rel_pos_y - mouse_local_y);
            }
        }
    }
    ImGui::EndChild();

	ImGui::End();
}

} // raekor