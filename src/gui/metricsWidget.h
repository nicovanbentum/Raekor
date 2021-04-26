#pragma once

#include "widget.h"

namespace Raekor {

class Scene;

class MetricsWidget : public IWidget {
public:
    MetricsWidget(Editor* editor);
    virtual void draw() override;

private:
};

}