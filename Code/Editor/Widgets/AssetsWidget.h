#pragma once

#include "Widget.h"
#include "CVars.h"

namespace RK {

class Editor;
class Material;
class Animation;

class MaterialsWidget : public IWidget
{
	RTTI_DECLARE_VIRTUAL_TYPE(MaterialsWidget);

public:
	MaterialsWidget(Editor* inEditor);
	
	virtual void Draw(Widgets* inWidgets, float dt) override;
	virtual void OnEvent(Widgets* inWidgets, const SDL_Event& ev) override {}

    void UpdateLayoutSizes(float inAvailableWidth);

private:
    float m_IconSize = 56.0f;
    int m_IconSpacing = 10;
    int m_IconHitSpacing = 4;
    bool m_StretchSpacing = true;
    float m_ZoomWheelAccum = 0.0f;
    ImVec2 m_LayoutItemSize;
    ImVec2 m_LayoutItemStep;
    int m_LayoutLineCount = 0;
    int m_LayoutColumnCount = 0;
    float m_LayoutItemSpacing = 0.0f;
    float m_LayoutOuterPadding = 0.0f;
    float m_LayoutSelectableSpacing = 0.0f;
};

}
