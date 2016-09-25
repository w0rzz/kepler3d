#pragma once

#include "Base.hpp"
#include "BaseGL.hpp"

namespace kepler {

    /// Wrapper for VAO.
    class VertexAttributeBinding final {
    public:
        explicit VertexAttributeBinding(GLuint handle);
        ~VertexAttributeBinding() noexcept;

        static std::unique_ptr<VertexAttributeBinding> createUnique(const MeshPrimitiveRef& meshPrim, const TechniqueRef& technique);

        void bind();
        void unbind();

    private:
        VertexAttributeBinding(const VertexAttributeBinding&) = delete;
        VertexAttributeBinding& operator=(const VertexAttributeBinding&) = delete;
    private:
        GLuint _handle;
    };
}
