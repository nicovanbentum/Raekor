#pragma once

#include "widget.h"

namespace Raekor {

class GLRenderer;

class RandomWidget : public IWidget {
public:
    RandomWidget(Editor* editor);
    virtual void draw() override;

private:
    GLRenderer& renderer;
};

} // raekor
