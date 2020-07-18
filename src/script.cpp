#include "pch.h"
#include "script.h"

#include "buffer.h"
#include "scene.h"

namespace Raekor {

    std::shared_ptr<chaiscript::Module> create_chaiscript_bindings() {
        auto module = std::make_shared<chaiscript::Module>();

        // add glm::vec2
        module->add(chaiscript::user_type<glm::vec<2, float, glm::packed_highp>>(), "Vector2");
        module->add(chaiscript::constructor<glm::vec<2, float, glm::packed_highp>(float x, float y)>(), "Vector2");
        module->add(chaiscript::fun(&glm::vec<2, float, glm::packed_highp>::x), "x");
        module->add(chaiscript::fun(&glm::vec<2, float, glm::packed_highp>::y), "y");

        // add glm::vec3
        module->add(chaiscript::user_type<glm::vec<2, float, glm::packed_highp>>(), "Vector3");
        module->add(chaiscript::constructor<glm::vec<3, float, glm::packed_highp>(float x, float y, float z)>(), "Vector3");
        module->add(chaiscript::fun(static_cast<glm::vec<3, float, glm::packed_highp> & (glm::vec<3, float, glm::packed_highp>::*)(const glm::vec<3, float, glm::packed_highp>&)>(&glm::vec<3, float, glm::packed_highp>::operator=)), "=");
        module->add(chaiscript::fun(static_cast<glm::vec<2, float, glm::packed_highp> & (glm::vec<2, float, glm::packed_highp>::*)(const glm::vec<2, float, glm::packed_highp>&)>(&glm::vec<2, float, glm::packed_highp>::operator=)), "=");
        module->add(chaiscript::fun(static_cast<glm::vec<3, float, glm::packed_highp> & (glm::vec<3, float, glm::packed_highp>::*)(const glm::vec<3, float, glm::packed_highp>&)>(&glm::vec<3, float, glm::packed_highp>::operator+=)), "+=");

        module->add(chaiscript::fun(&glm::vec<3, float, glm::packed_highp>::x), "x");
        module->add(chaiscript::fun(&glm::vec<3, float, glm::packed_highp>::y), "y");
        module->add(chaiscript::fun(&glm::vec<3, float, glm::packed_highp>::z), "z");

        // add Vertex
        module->add(chaiscript::user_type<Vertex>(), "Vertex");
        module->add(chaiscript::constructor<Vertex()>(), "Vertex");
        module->add(chaiscript::fun(&Vertex::pos), "pos");
        module->add(chaiscript::fun(&Vertex::uv), "uv");
        module->add(chaiscript::fun(&Vertex::normal), "normal");

        //add Face
        module->add(chaiscript::user_type<Triangle>(), "Triangle");
        module->add(chaiscript::constructor<Triangle()>(), "Triangle");
        module->add(chaiscript::constructor<Triangle(uint32_t _f1, uint32_t _f2, uint32_t _f3)>(), "Triangle");
        module->add(chaiscript::fun(&Triangle::p1), "p1");
        module->add(chaiscript::fun(&Triangle::p2), "p2");
        module->add(chaiscript::fun(&Triangle::p3), "p3");

        // add cross product for vec3's
        module->add(chaiscript::fun(static_cast<glm::vec<3, float, glm::packed_highp>(*)(const glm::vec<3, float, glm::packed_highp>&, const glm::vec<3, float, glm::packed_highp>&)>(&glm::cross<float, glm::packed_highp>)), "cross");

        // add normalize for vector 3
        module->add(chaiscript::fun(static_cast<glm::vec<3, float, glm::packed_highp>(*)(const glm::vec<3, float, glm::packed_highp>&)>(&glm::normalize<3, float, glm::packed_highp>)), "normalize");

        // add minus operator for vector 3's
        module->add(chaiscript::fun(static_cast<glm::vec<3, float, glm::packed_highp>(*)(const glm::vec<3, float, glm::packed_highp>&, const glm::vec<3, float, glm::packed_highp>&)>(&glm::operator-<float, glm::packed_highp>)), "-");

        // add glm::perlin for vec2's
        module->add(chaiscript::fun(static_cast<float(*)(glm::vec<2, float, glm::packed_highp> const&)>(&glm::perlin<float, glm::packed_highp>)), "perlin");
        module->add(chaiscript::fun(static_cast<float(*)(glm::vec<2, float, glm::packed_highp> const&)>(&glm::simplex<float, glm::packed_highp>)), "simplex");

        // add Scene methods

        module->add(chaiscript::fun(&ECS::ComponentManager<ECS::MeshComponent>::create), "create");
        module->add(chaiscript::fun(&ECS::MeshComponent::vertices), "vertices");
        module->add(chaiscript::fun(&ECS::MeshComponent::indices), "indices");
        module->add(chaiscript::fun(&ECS::MeshComponent::uploadVertices), "uploadVertices");
        module->add(chaiscript::fun(&ECS::MeshComponent::uploadIndices), "uploadIndices");
        module->add(chaiscript::fun(&ECS::MeshComponent::generateAABB), "generateAABB");

        return module;
    }

    std::shared_ptr<chaiscript::ChaiScript> create_chaiscript() {
        auto chai = std::shared_ptr<chaiscript::ChaiScript>(new chaiscript::ChaiScript({}, { "scripts\\" }));
        chai->add(create_chaiscript_bindings());
        return chai;
    }

} // raekor