#pragma once

#include "widget.h"

namespace Raekor {
    
class AssetsWidget : public IWidget {
public:
    AssetsWidget(Editor* editor);
    virtual void draw() override;
};

}
