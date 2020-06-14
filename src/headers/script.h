#pragma once

namespace Raekor {

    std::shared_ptr<chaiscript::Module> create_chaiscript_bindings();

    std::shared_ptr<chaiscript::ChaiScript> create_chaiscript();

} // raekor
