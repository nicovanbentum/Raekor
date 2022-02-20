#include "pch.h"
#include "metricsWidget.h"
#include "editor.h"

namespace Raekor {

MetricsWidget::MetricsWidget(Editor* editor) : IWidget(editor, "Metrics") {}

void MetricsWidget::draw(float dt) {
    if (!visible) return;

    accumTime += dt;

    ImGui::Begin(title.c_str(), &visible);
    
    if (accumTime >= 100.0f /* update every 1/10th of a second */) {
        accumTime = 0.0f;
    }

    for (const auto& kv : GetRenderer().timings) {
        ImGui::Text(std::string(kv.first + ": %.3f ms").c_str(), kv.second->getMilliseconds());
    }

    ImGui::End();
}

} // raekor