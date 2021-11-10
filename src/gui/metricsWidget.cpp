#include "pch.h"
#include "metricsWidget.h"
#include "editor.h"

namespace Raekor {

MetricsWidget::MetricsWidget(Editor* editor) : IWidget(editor, "Metrics") {

}

void MetricsWidget::draw(float dt) {
    if (!visible) return;

    accumTime += dt;

    ImGui::Begin(title.c_str(), &visible);
    
    if (accumTime >= 100.0f /* every half second */) {
        time = renderer().deferShading->getTimeMs();
        accumTime = 0.0f;
    }
    
    ImGui::Text("Deferred shading: %.3f", time);

    ImGui::End();
}

} // raekor