#include "pch.h"
#include "metricsWidget.h"
#include "editor.h"

namespace Raekor {

MetricsWidget::MetricsWidget(Editor* editor) : IWidget(editor, "Metrics") {

}

void MetricsWidget::draw() {
    if (!visible) return;

    ImGui::Begin(title.c_str(), &visible);


    ImGui::Text("Deferred shading: %.3f", editor->renderer.deferredPass->getTimeMs());

    ImGui::End();
}

} // raekor