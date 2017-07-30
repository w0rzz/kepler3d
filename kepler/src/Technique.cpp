#include "stdafx.h"
#include "Technique.hpp"
#include "Effect.hpp"
#include "Material.hpp"

namespace kepler {

    Technique::Technique() {
    }

    Technique::~Technique() noexcept {
    }

    ref<Technique> Technique::create() {
        return std::make_shared<Technique>();
    }

    ref<Technique> Technique::create(ref<Effect> effect) {
        auto tech = create();
        tech->setEffect(effect);
        return tech;
    }

    ref<Effect> Technique::effect() const {
        return _effect;
    }

    const AttributeMap& Technique::attributes() const {
        return _attributes;
    }

    const std::map<std::string, std::string>& Technique::uniforms() const {
        return _uniforms;
    }

    const std::map<std::string, ref<MaterialParameter>>& Technique::semantics() const {
        return _semantics;
    }

    RenderState& Technique::renderState() {
        return _renderState;
    }

    void Technique::setAttribute(const std::string& glslName, AttributeSemantic semantic) {
        _attributes[glslName] = semantic;
    }

    void Technique::setUniformName(const std::string& glslName, const std::string& paramName) {
        _uniforms[glslName] = paramName;
    }

    void Technique::setUniform(const std::string& glslName, ref<MaterialParameter> param) {
        setUniformName(glslName, param->name());
        _values[param->name()] = param;
        updateUniform(*param, glslName);
    }

    void Technique::setSemanticUniform(const std::string& glslName, MaterialParameter::Semantic semantic) {
        auto p = MaterialParameter::create(glslName);
        p->setSemantic(semantic);
        _semantics[glslName] = p;
        updateUniform(*p, glslName);
    }

    void Technique::setEffect(ref<Effect> effect) {
        _effect = effect;
    }

    void Technique::setMaterial(ref<Material> material) {
        _material = material;
    }

    void Technique::bind() {
        if (_effect) {
            _effect->bind();
            _renderState.bind();
        }
    }

    void Technique::findValues(std::vector<ref<MaterialParameter>>& values) {
        values.clear();
        if (_effect) {
            auto material = _material.lock();
            for (auto& uniformIt : _uniforms) {
                const std::string& uniformName = uniformIt.first;
                const auto& paramName = uniformIt.second;

                // TODO don't do this. This shouldn't be necessary.

                // TODO This is a mess. Needs to be improved.

                ref<MaterialParameter> materialParam = findValueParameter(paramName);

                if (materialParam == nullptr && material) {
                    auto param = material->param(paramName);
                    if (param) {
                        materialParam = param;
                    }
                }

                if (materialParam) {
                    if (updateUniform(*materialParam, uniformName)) {
                        values.push_back(materialParam);
                    }
                }
            }
        }
    }

    ref<MaterialParameter> Technique::findValueParameter(const std::string& paramName) {
        auto p = _values.find(paramName);
        if (p != _values.end()) {
            return p->second;
        }
        // TODO semantics and look in the parent material?
        return nullptr;
    }

    bool Technique::updateUniform(MaterialParameter& materialParam, const std::string& uniformName) {
        if (_effect && (materialParam.uniform() == nullptr || materialParam.uniform()->effect() != _effect)) {
            Uniform* uniform = _effect->uniform(uniformName);
            if (uniform == nullptr) {
                return false;
            }
            materialParam.setUniform(uniform);
        }
        return true;
    }
}
