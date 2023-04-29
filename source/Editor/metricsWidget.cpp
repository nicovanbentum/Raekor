#include "pch.h"
#include "metricsWidget.h"
#include "editor.h"

namespace Raekor {

RTTI_CLASS_CPP_NO_FACTORY(MetricsWidget) {}

MetricsWidget::MetricsWidget(Editor* editor) : IWidget(editor, ICON_FA_CHART_BAR " Metrics ") {}

void MetricsWidget::draw(float dt) {
    if (!IsOpen()) 
        return;

    if(m_Times.empty())
        for (const auto& [name, timer] : GetRenderer().m_Timings)
            m_Times[name] = timer->getMilliseconds();

    m_UpdateInterval += dt;

    ImGui::Begin(m_Title.c_str(), &m_Open);
    m_Visible = ImGui::IsWindowAppearing();
    
    if (m_UpdateInterval >= 0.1f /* Update timings every 1/10th of a second */) {
        m_UpdateInterval = 0.0f;
     
        for (const auto& [name, timer] : GetRenderer().m_Timings)
            m_Times[name] = timer->getMilliseconds();
    }

    // Draw all render pass metrics using the cached timings
    for (const auto& [name, TimeOpenGL] : m_Times)
        ImGui::Text(std::string(name + ": %.3f ms").c_str(), TimeOpenGL);

    ImGui::End();
}

} // raekor