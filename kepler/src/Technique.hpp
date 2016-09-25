#pragma once

#include "Base.hpp"
#include "AttributeSemantic.hpp"
#include "RenderState.hpp"

namespace kepler {

    using AttributeMap = std::map<std::string, AttributeSemantic>;

    /// A technique describes the shading used for a material.
    class Technique {
        ALLOW_MAKE_SHARED(Technique);
    public:
        virtual ~Technique() noexcept;
        static TechniqueRef create();
        static TechniqueRef create(EffectRef effect);

        EffectRef effect() const;
        const AttributeMap& attributes() const;
        const std::map<std::string, std::string>& uniforms() const;

        const std::map<std::string, MaterialParameterRef>& semantics() const;

        /// Returns the RenderState for this technique.
        RenderState& renderState();

        void setAttribute(const std::string& glslName, AttributeSemantic semantic);

        /// Sets the name of the parameter that is used to get the value for the given shader uniform.
        /// Material parameters can be defined in either the technique or the parent material. This method tells the technique which
        /// material parameter it should look for. If it is not set in the technique, it will look in the material.
        /// @param[in] glslName  The name of the shader uniform.
        /// @param[in] paramName The name of the material parameter.
        void setUniformName(const std::string& glslName, const std::string& paramName);

        void setUniform(const std::string& glslName, MaterialParameterRef param);
        void setSemanticUniform(const std::string& glslName, const std::string& paramName, MaterialParameter::Semantic semantic);
        void setEffect(EffectRef effect);
        void setMaterial(MaterialRef material);

        void bind(); // TODO remove and use a pass?
        void findValues(std::vector<MaterialParameterRef>& values);

        /// Finds the material parameter with the given name.
        MaterialParameterRef findValueParameter(const std::string& paramName);

    private:
        Technique();
        Technique(const Technique&) = delete;
        Technique& operator=(const Technique&) = delete;

        bool updateUniform(MaterialParameter& materialParam, const std::string& uniformName);
    private:
        std::string _name;
        EffectRef _effect;

        // attributes: <shaderAttribName, paramName>
        AttributeMap _attributes;

        // uniforms:   <shaderAttribName, paramName>
        std::map<std::string, std::string> _uniforms;

        // key = parameterName
        std::map<std::string, MaterialParameterRef> _semantics;
        // key = parameterName
        std::map<std::string, MaterialParameterRef> _values;

        std::weak_ptr<Material> _material;

        // states
        RenderState _renderState;
    };
}
