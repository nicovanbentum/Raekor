#pragma once

#include "widget.h"
#include "Raekor/json.h"

namespace Raekor {

struct GraphNode;

struct Pin {
    enum Type {
        INPUT, OUTPUT
    };

    Pin() = default;
    Pin(Type type, int id) : type(type), id(id) {}

    template<class Archive>
    void serialize(Archive& archive) {
        archive(CEREAL_NVP(type), CEREAL_NVP(id));
    }

    Type type;
    int id;
    std::string name;
    GraphNode* node;
};

class GraphNode {
public:
    friend class NodeGraphWidget;

    void Build();
    bool IsRootNode() const { return inputPins.empty(); }
    const Pin* FindPin(int id) const { return nullptr; }

    template<class Archive>
    void serialize(Archive& archive) {
        archive(CEREAL_NVP(inputPins), CEREAL_NVP(outputPins));
    }

    template<Pin::Type pinType>
    Pin& AddPin(int inPinID);

    template<> 
    Pin& AddPin<Pin::Type::INPUT>(int inPinID) {
        return inputPins.emplace_back(Pin::Type::INPUT, inPinID);
    }

    template<>
    Pin& AddPin<Pin::Type::OUTPUT>(int inPinID) {
        return outputPins.emplace_back(Pin::Type::OUTPUT, inPinID);
    }

protected:
    int mIndex;
    std::string mName;
    std::vector<Pin> inputPins;
    std::vector<Pin> outputPins;
};

struct Link {
    Link() = default;
    Link(int id, int start_id, int end_id) : id(id), start_id(start_id), end_id(end_id) {}

    template<class Archive>
    void serialize(Archive& archive) {
        archive(CEREAL_NVP(id), CEREAL_NVP(start_id), CEREAL_NVP(end_id));
    }

    int id;
    int start_id, end_id;
};

class NodeGraphWidget : public IWidget {
public:
    RTTI_CLASS_HEADER(NodeGraphWidget);

    NodeGraphWidget(Application* inApp);
    
    virtual void Draw(Widgets* inWidgets, float dt) override;
    virtual void OnEvent(Widgets* inWidgets, const SDL_Event& ev) override;
    const GraphNode* FindNode(int id) const;

    JSON::Object* GetSelectedObject() { return m_SelectedObject == -1 ? nullptr : &m_JSON.Get(m_SelectedObject); }

    template<class Archive>
    void serialize(Archive& archive) {
        archive(CEREAL_NVP(_pin_id), CEREAL_NVP(_node_id), CEREAL_NVP(_link_id), CEREAL_NVP(m_Links), CEREAL_NVP(m_Nodes));
    }

private:
    int GetNextPinID() { return _pin_id++; }
    int GetNextNodeID() { return _node_id++; }
    int GetNextLinkID() { return _link_id++; }

    int _pin_id = 1;
    int _node_id = 1;
    int _link_id = 1;
    std::vector<Link> m_Links;
    std::vector<GraphNode> m_Nodes;

    JSON::Parser m_JSON;
    int m_SelectedObject = -1;

    std::string m_OpenFilePath;
    bool m_WasRightClicked = false;
};

}