#pragma once

#include <BaseGL.hpp>
#include <OpenGL.hpp>

namespace kepler {
namespace gl {

/// Wrapper for VAO.
class VertexAttributeBinding final {
public:
    explicit VertexAttributeBinding(GLuint handle);
    ~VertexAttributeBinding() noexcept;

    static std::unique_ptr<VertexAttributeBinding> createUnique(const ref<MeshPrimitive>& meshPrim, const ref<Technique>& technique);

    void bind();
    void unbind();

private:
    VertexAttributeBinding(const VertexAttributeBinding&) = delete;
    VertexAttributeBinding& operator=(const VertexAttributeBinding&) = delete;
private:
    GLuint _handle;
};
}
}
