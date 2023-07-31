#pragma once

#include "widget.h"
#include "json.h"

namespace Raekor {

struct Link {
    Link() = default;
    Link(int id, int start_id, int end_id) : id(id), start_id(start_id), end_id(end_id) {}

    int id;
    int start_id, end_id;
};

class NodeGraphWidget : public IWidget {
public:
    RTTI_CLASS_HEADER(NodeGraphWidget);

    NodeGraphWidget(Application* inApp);
    
    virtual void Draw(Widgets* inWidgets, float dt) override;
    virtual void OnEvent(Widgets* inWidgets, const SDL_Event& ev) override;

    int GetSelectedObjectIndex() { return m_SelectedObject; }
    void SelectObject(int inIndex) { m_SelectedObject = inIndex; ImNodes::ClearNodeSelection(); ImNodes::SelectNode(m_SelectedObject); }

private:
    int m_SelectedObject = -1;
    int m_LinkPinDropped = -1;
    std::string m_OpenFilePath;
    bool m_WasRightClicked = false;
    bool m_WasLinkConnected = false;
    int start_node_id, start_pin_id, end_node_id, end_pin_id;

};

}