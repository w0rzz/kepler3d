﻿#include "stdafx.h"
#include "GLTF2Loader.hpp"

#include "lazy_gltf2.hpp"

#include "Scene.hpp"
#include "Camera.hpp"
#include "Node.hpp"
#include "Mesh.hpp"
#include "MeshPrimitive.hpp"
#include "MeshRenderer.hpp"
#include "VertexAttributeAccessor.hpp"
#include "VertexBuffer.hpp"
#include "IndexBuffer.hpp"
#include "IndexAccessor.hpp"
#include "Material.hpp"
#include "MaterialParameter.hpp"
#include "Effect.hpp"
#include "Technique.hpp"
#include "Sampler.hpp"
#include "Image.hpp"
#include "FileSystem.hpp"
#include "StringUtils.hpp"
#include "Logging.hpp"

#include <iostream>
#include <iomanip> // setprecision
#include <chrono>
#include <array>

using std::string;
using std::shared_ptr;

#define RETURN_IF_FOUND(map, key) \
    { \
        auto i = (map).find(key); \
        if (i != (map).end()) { \
            return i->second; \
        } \
    }

static constexpr GLchar* DEFAULT_VERT_SHADER = "precision highp float;\n"
"\n"
"uniform mat4 u_modelViewMatrix;\n"
"uniform mat3 u_normalMatrix;\n"
"uniform mat4 u_projectionMatrix;\n"
"\n"
"attribute vec3 a_normal;\n"
"attribute vec3 a_position;\n"
"\n"
"void main(void)\n"
"{\n"
"    gl_Position = u_projectionMatrix * u_modelViewMatrix * vec4(a_position,1.0);\n"
"}\n";

static constexpr GLchar* DEFAULT_FRAG_SHADER = "precision highp float;\n"
"\n"
"uniform vec4 u_emission;\n"
"\n"
"void main(void)\n"
"{\n"
"    gl_FragColor = u_emission;\n"
"}\n";

static constexpr const GLchar* BASIC_VERT_PATH = "../kepler/res/shaders/basic.vert";
static constexpr const GLchar* BASIC_FRAG_PATH = "../kepler/res/shaders/basic.frag";

namespace kepler {

    using ubyte = unsigned char;

    static constexpr const char* HAS_UV = "HAS_UV";

    static constexpr int DEFAULT_FORMAT = GL_RGBA;

    // functions
    static MeshPrimitive::Mode toMode(gltf2::Primitive::Mode mode);
    static MaterialParameter::Semantic toSemantic(const string& semantic);
    static AttributeSemantic toAttributeSemantic(const string& semantic);
    static Sampler::Wrap toWrapMode(gltf2::Sampler::Wrap wrap);
    static Sampler::MinFilter toMinFilterMode(gltf2::Sampler::MinFilter filter);
    static Sampler::MagFilter toMagFilterMode(gltf2::Sampler::MagFilter filter);
    static GLint numComponentsOfType(const string& type);
    static void setState(int state, RenderState& block);

    using time_type = std::chrono::nanoseconds;
    static time_type __totalTime;

    // Private implementation
    /// @Internal
    class GLTF2Loader::Impl final {
        friend class GLTF2Loader;
    public:
        Impl();
        ~Impl() noexcept;

        bool loadJson(const char* path);

        ref<Scene> loadSceneFromFile(const char* path);

        ref<Scene> loadDefaultScene();

        ref<Scene> loadScene(size_t index);

        ref<Node> loadNode(size_t index);
        ref<Node> loadNode(const gltf2::Node& gNode);

        ref<Mesh> loadMesh(size_t index);
        ref<Camera> loadCamera(size_t index);

        shared_ptr<std::vector<ubyte>> loadBuffer(size_t index);

        ref<VertexBuffer> loadVertexBuffer(size_t index);
        ref<IndexBuffer> loadIndexBuffer(size_t index);
        ref<IndexAccessor> loadIndexAccessor(size_t index);
        ref<VertexAttributeAccessor> loadVertexAttributeAccessor(size_t index);

        ref<Material> loadMaterial(size_t index, MeshPrimitive& primitive);

        ref<Texture> loadTexture(size_t index);
        ref<Sampler> loadSampler(size_t index);
        ref<Image> loadImage(size_t index);

        ref<Material> loadDefaultMaterial();
        ref<Technique> loadDefaultTechnique();

        string uriToPath(const string& uri) const;

        void useDefaultMaterial(bool value);
        void setAutoLoadMaterials(bool value);

    private:
        void loadTransform(const gltf2::Node& gNode, const ref<Node>& node);

    private:
        gltf2::Gltf _gltf;

        std::map <size_t, std::shared_ptr<std::vector<ubyte>>> _buffers;

        std::map<size_t, ref<Node>> _nodes;
        std::map<size_t, ref<VertexBuffer>> _vbos;
        std::map<size_t, ref<IndexBuffer>> _indexBuffers;
        std::map<size_t, ref<IndexAccessor>> _indexAccessors;
        std::map<size_t, ref<VertexAttributeAccessor>> _vertexAttributeAccessors;

        std::map<size_t, ref<Material>> _materials;
        std::map<size_t, ref<Technique>> _techniques;
        std::map<size_t, ref<Effect>> _effects;
        std::map<size_t, ref<Texture>> _textures;
        std::map<size_t, ref<Sampler>> _samplers;
        std::map<size_t, ref<Image>> _images;

        ref<Material> _defaultMaterial;
        ref<Technique> _defaultTechnique;

        string _baseDir;

        bool _loaded;
        bool _useDefaultMaterial;
        bool _autoLoadMaterials;
        float _aspectRatio;

        time_type _jsonLoadTime;
    };

    ////////////////////////////////////////////////////////////////

    GLTF2Loader::GLTF2Loader() : _impl(std::make_unique<Impl>()) {
    }

    GLTF2Loader::GLTF2Loader(const char * path) : _impl(std::make_unique<Impl>()) {
        _impl->loadJson(path);
    }

    GLTF2Loader::~GLTF2Loader() noexcept {
    }

    GLTF2Loader::operator bool() const {
        return _impl->_loaded;
    }

    bool GLTF2Loader::load(const char* path) {
        return _impl->loadJson(path);
    }

    ref<Scene> GLTF2Loader::loadSceneFromFile(const char* path) {
        return _impl->loadSceneFromFile(path);
    }

    ref<Mesh> GLTF2Loader::findMeshByIndex(size_t index) {
        return _impl->loadMesh(index);
    }

    void GLTF2Loader::clear() {
        _impl.reset();
        _impl = std::make_unique<Impl>();
    }

    void GLTF2Loader::useDefaultMaterial(bool value) {
        _impl->useDefaultMaterial(value);
    }

    void GLTF2Loader::setAutoLoadMaterials(bool value) {
        _impl->setAutoLoadMaterials(value);
    }

    void GLTF2Loader::setCameraAspectRatio(float aspectRatio) {
        _impl->_aspectRatio = aspectRatio;
    }

    void GLTF2Loader::printTotalTime() {
        std::clog << "Total: " << std::chrono::duration_cast<std::chrono::milliseconds>(__totalTime).count() << " ms" << std::endl;
    }

    ////////////////////////////////////////////////////////////////

    GLTF2Loader::Impl::Impl()
        : _loaded(false), _useDefaultMaterial(false), _autoLoadMaterials(true), _aspectRatio(0.0f) {
    }

    GLTF2Loader::Impl::~Impl() noexcept {
    }

    bool GLTF2Loader::Impl::loadJson(const char* path) {
        _baseDir = directoryName(path);

        auto start = std::chrono::high_resolution_clock::now();

        _loaded = _gltf.load(path);

        auto end = std::chrono::high_resolution_clock::now();
        _jsonLoadTime = std::chrono::duration_cast<time_type>(end - start);

        return _loaded;
    }

    ref<Scene> GLTF2Loader::Impl::loadSceneFromFile(const char* path) {
        // TODO call clear() first?
        auto start = std::chrono::high_resolution_clock::now();

        if (!loadJson(path)) {
            loge("LOAD_SCENE_FROM_FILE ", path);
            return nullptr;
        }

        auto scene = loadDefaultScene();

        auto end = std::chrono::high_resolution_clock::now();
        auto t = std::chrono::duration_cast<time_type>(end - start);
        __totalTime += t;
        auto clogWidth = std::clog.width();
        std::clog.width(8);
        std::clog << std::chrono::duration_cast<std::chrono::milliseconds>(t).count() << " ms to load ";

        std::clog << "(";
        double percent = (double)_jsonLoadTime.count() / (double)t.count() * 100.0;
        std::clog.width(2);
        std::clog << std::fixed;
        std::clog << std::setprecision(0);
        std::clog << percent << "% json) ";
        std::clog << path << std::endl;
        std::clog.unsetf(std::ios_base::floatfield);
        std::clog.width(clogWidth);
        return scene;
    }

    ref<Scene> GLTF2Loader::Impl::loadDefaultScene() {
        size_t index;
        if (_gltf.defaultScene(index)) {
            return loadScene(index);
        }
        else if (_gltf.sceneCount() > 0) {
            // load the first scene if there was no default scene
            return loadScene(0);
        }
        return nullptr;
    }

    ref<Scene> GLTF2Loader::Impl::loadScene(size_t index) {
        if (auto gScene = _gltf.scene(index)) {
            auto scene = Scene::create();
            for (const auto& index : gScene.nodes()) {
                scene->addNode(loadNode(index));
            }
            // TODO active camera?
            return scene;
        }
        return nullptr;
    }

    ref<Node> GLTF2Loader::Impl::loadNode(size_t index) {
        RETURN_IF_FOUND(_nodes, index);
        if (auto gNode = _gltf.node(index)) {
            auto node = loadNode(gNode);
            _nodes[index] = node;
            return node;
        }
        return nullptr;
    }

    ref<Node> GLTF2Loader::Impl::loadNode(const gltf2::Node& gNode) {
        auto node = Node::create(gNode.name());

        loadTransform(gNode, node);

        // load mesh
        size_t meshIndex;
        if (gNode.mesh(meshIndex)) {
            if (auto mesh = loadMesh(meshIndex)) {
                node->addComponent(MeshRenderer::create(mesh));
            }
        }

        // load camera
        size_t cameraIndex;
        if (gNode.camera(cameraIndex)) {
            node->addComponent(loadCamera(cameraIndex));
        }

        // load children
        for (const auto& index : gNode.children()) {
            node->addNode(loadNode(index));
        }
        return node;
    }

    ref<Mesh> GLTF2Loader::Impl::loadMesh(size_t index) {
        if (auto gMesh = _gltf.mesh(index)) {
            auto mesh = Mesh::create();
            mesh->setName(gMesh.name());
            const size_t primCount = gMesh.primitiveCount();
            for (size_t i = 0; i < primCount; ++i) {
                if (auto gPrim = gMesh.primitive(i)) {
                    auto prim = MeshPrimitive::create(toMode(gPrim.mode()));
                    for (const auto& attrib : gPrim.attributes()) {
                        auto vertexAttributeAccessor = loadVertexAttributeAccessor(attrib.second);
                        if (vertexAttributeAccessor) {
                            prim->setAttribute(toAttributeSemantic(attrib.first), vertexAttributeAccessor);
                        }
                    }
                    // load indices
                    size_t indicesIndex;
                    if (gPrim.indices(indicesIndex)) {
                        if (auto indexAccessor = loadIndexAccessor(indicesIndex)) {
                            prim->setIndices(indexAccessor);
                        }
                    }
                    if (_autoLoadMaterials) {
                        // load material
                        ref<Material> material = nullptr;
                        size_t materialIndex;
                        if (gPrim.material(materialIndex)) {
                            if (material = loadMaterial(materialIndex, *prim)) {
                                prim->setMaterial(material);
                            }
                        }
                        if (material == nullptr) {
                            prim->setMaterial(loadDefaultMaterial());
                        }
                    }
                    mesh->addMeshPrimitive(prim);
                }
            }
            if (mesh->primitiveCount() > 0) {
                return mesh;
            }
            else {
                loge("MESH::ZERO_PRIMITIVES_LOADED");
            }
        }
        return nullptr;
    }

    ref<Camera> GLTF2Loader::Impl::loadCamera(size_t index) {
        if (auto gCamera = _gltf.camera(index)) {
            switch (gCamera.type()) {
            case gltf2::Camera::Type::PERSPECTIVE:
                if (auto gPers = gCamera.perspective()) {
                    // _aspectRatio can replace the aspect ratio found in the file.
                    float aspectRatio = _aspectRatio > 0.0f ? _aspectRatio : gPers.aspectRatio();
                    return Camera::createPerspective(glm::degrees(gPers.yfov()), aspectRatio, gPers.znear(), gPers.zfar());
                }
                break;
            case gltf2::Camera::Type::ORTHOGRAPHIC:
                if (auto gOrtho = gCamera.orthographic()) {
                    return Camera::createOrthographic(gOrtho.xmag(), gOrtho.ymag(), 1.0f, gOrtho.znear(), gOrtho.zfar());
                }
                break;
            }
        }
        return nullptr;
    }

    shared_ptr<std::vector<ubyte>> GLTF2Loader::Impl::loadBuffer(size_t index) {
        RETURN_IF_FOUND(_buffers, index);
        if (auto gBuffer = _gltf.buffer(index)) {
            auto byteLength = gBuffer.byteLength();
            auto buffer = std::make_shared<std::vector<ubyte>>();
            if (gBuffer.load(*buffer)) {
                _buffers[index] = buffer;
                return buffer;
            }
        }
        return nullptr;
    }

    ref<VertexBuffer> GLTF2Loader::Impl::loadVertexBuffer(size_t index) {
        RETURN_IF_FOUND(_vbos, index);
        if (auto gBufferView = _gltf.bufferView(index)) {
            size_t bufferIndex;
            if (gBufferView.buffer(bufferIndex)) {
                if (auto buffer = loadBuffer(bufferIndex)) {
                    auto byteLength = gBufferView.byteLength();
                    auto byteOffset = gBufferView.byteOffset();
                    //auto byteStride = gBufferView.byteStride(); // TODO is this needed?
                    auto vbo = VertexBuffer::create(byteLength, &(*buffer)[0] + byteOffset, GL_STATIC_DRAW);
                    _vbos[index] = vbo;
                    return vbo;
                }
            }
        }
        return nullptr;
    }

    ref<IndexBuffer> GLTF2Loader::Impl::loadIndexBuffer(size_t index) {
        // TODO is it possible to have more than 1 of the same index buffer?
        RETURN_IF_FOUND(_indexBuffers, index);

        // TODO this is very similar to loadVertexBuffer. Move to common function.

        if (auto gBufferView = _gltf.bufferView(index)) {
            size_t bufferIndex;
            if (gBufferView.buffer(bufferIndex)) {
                if (auto buffer = loadBuffer(bufferIndex)) {
                    auto byteLength = gBufferView.byteLength();
                    auto byteOffset = gBufferView.byteOffset();
                    //auto byteStride = gBufferView.byteStride(); // TODO is this needed?
                    auto indexBuffer = IndexBuffer::create(byteLength, &(*buffer)[0] + byteOffset, GL_STATIC_DRAW);
                    _indexBuffers[index] = indexBuffer;
                    return indexBuffer;
                }
            }
        }
        return nullptr;
    }

    ref<IndexAccessor> GLTF2Loader::Impl::loadIndexAccessor(size_t index) {
        RETURN_IF_FOUND(_indexAccessors, index);
        if (auto gAccessor = _gltf.accessor(index)) {
            size_t bufferViewIndex;
            if (gAccessor.bufferView(bufferViewIndex)) {
                if (auto indexBuffer = loadIndexBuffer(bufferViewIndex)) {
                    GLenum componentType = static_cast<GLenum>(gAccessor.componentType());
                    GLsizei count = static_cast<GLsizei>(gAccessor.count());
                    GLintptr byteOffset = gAccessor.byteOffset();
                    auto indexAccessor = IndexAccessor::create(indexBuffer, count, componentType, byteOffset);
                    _indexAccessors[index] = indexAccessor;
                    return indexAccessor;
                }
            }
        }
        return nullptr;
    }

    ref<VertexAttributeAccessor> GLTF2Loader::Impl::loadVertexAttributeAccessor(size_t index) {
        RETURN_IF_FOUND(_vertexAttributeAccessors, index);
        if (auto gAccessor = _gltf.accessor(index)) {
            size_t bufferViewIndex;
            if (gAccessor.bufferView(bufferViewIndex)) {
                auto vbo = loadVertexBuffer(bufferViewIndex);
                if (vbo == nullptr) {
                    return nullptr;
                }
                GLenum componentType = static_cast<GLenum>(gAccessor.componentType());
                GLint componentSize = gltf2::numberOfComponents<GLint>(gAccessor.type());
                auto byteStride = static_cast<GLsizei>(gAccessor.bufferView().byteStride());
                GLintptr byteOffset = gAccessor.byteOffset();
                auto count = static_cast<GLsizei>(gAccessor.count());
                auto vertexAttributeAccessor = VertexAttributeAccessor::create(vbo, componentSize, componentType, false, byteStride, byteOffset, count);
                _vertexAttributeAccessors[index] = vertexAttributeAccessor;
                return vertexAttributeAccessor;
            }
        }
        return nullptr;
    }

    ref<Material> GLTF2Loader::Impl::loadMaterial(size_t index, MeshPrimitive& primitive) {
        RETURN_IF_FOUND(_materials, index);
        if (_useDefaultMaterial) {
            return loadDefaultMaterial();
        }
        if (auto gMaterial = _gltf.material(index)) {
            auto material = Material::create();
            if (const char* name = gMaterial.name()) {
                material->setName(name);
            }
            // load basic shader

            std::vector<const char*> defines;
            if (primitive.hasAttribute(AttributeSemantic::TEXCOORD_0)) {
                defines.push_back(HAS_UV);
            }

            auto effect = Effect::createFromFile(BASIC_VERT_PATH, BASIC_FRAG_PATH, defines.data(), defines.size());
            if (!effect) {
                return nullptr;
            }
            auto tech = Technique::create(effect);
            glm::vec4 baseColorFactor(1);
            if (auto gPbr = gMaterial.pbrMetallicRoughness()) {
                size_t textureIndex;
                if (gPbr.baseColorTexture().index(textureIndex)) {
                    if (auto texture = loadTexture(textureIndex)) {
                        tech->setUniform("s_baseMap", MaterialParameter::create("s_baseMap", texture));
                    }
                }                
                gPbr.baseColorFactor(glm::value_ptr(baseColorFactor));
            }
            tech->setAttribute("a_position", AttributeSemantic::POSITION);
            tech->setAttribute("a_normal", AttributeSemantic::NORMAL);
            tech->setAttribute("a_texcoord0", AttributeSemantic::TEXCOORD_0);

            tech->setSemanticUniform("mvp", "mvp", MaterialParameter::Semantic::MODELVIEWPROJECTION);
            tech->setSemanticUniform("modelView", "modelView", MaterialParameter::Semantic::MODELVIEW);
            tech->setSemanticUniform("normalMatrix", "normalMatrix", MaterialParameter::Semantic::MODELVIEWINVERSETRANSPOSE);

            tech->setUniform("lightPos", MaterialParameter::create("lightPos", glm::vec3(1, 1, 1)));
            tech->setUniform("lightColor", MaterialParameter::create("lightColor", glm::vec3(1, 1, 1)));
            tech->setUniform("shininess", MaterialParameter::create("shininess", 64.0f));
            tech->setUniform("specularStrength", MaterialParameter::create("specularStrength", 0.2f));
            tech->setUniform("ambient", MaterialParameter::create("ambient", glm::vec3(0.2f)));

            tech->setUniform("constantAttenuation", MaterialParameter::create("constantAttenuation", 1.f));
            tech->setUniform("linearAttenuation", MaterialParameter::create("linearAttenuation", 0.f));
            tech->setUniform("quadraticAttenuation", MaterialParameter::create("quadraticAttenuation", 0.0025f));

            // gltf 2.0
            tech->setUniform("baseColorFactor", MaterialParameter::create("baseColorFactor", baseColorFactor));

            auto& state = tech->renderState();
            state.setDepthTest(true);
            state.setCulling(!gMaterial.doubleSided());

            material->setTechnique(tech);
            _materials[index] = material;
            return material;
        }
        return nullptr;
    }

    ref<Texture> GLTF2Loader::Impl::loadTexture(size_t index) {
        RETURN_IF_FOUND(_textures, index);
        if (auto gTexture = _gltf.texture(index)) {
            size_t imageIndex;
            if (gTexture.source(imageIndex)) {
                if (auto image = loadImage(imageIndex)) {
                    auto texture = Texture::create2D(image.get(), DEFAULT_FORMAT, true);
                    size_t samplerIndex;
                    if (gTexture.sampler(samplerIndex)) {
                        if (auto sampler = loadSampler(samplerIndex)) {
                            texture->setSampler(sampler);
                        }
                    }
                    _textures[index] = texture;
                    return texture;
                }
            }
        }
        return nullptr;
    }

    ref<Sampler> GLTF2Loader::Impl::loadSampler(size_t index) {
        RETURN_IF_FOUND(_samplers, index);
        if (auto gSampler = _gltf.sampler(index)) {
            auto magFilter = toMagFilterMode(gSampler.magFilter());
            auto minFilter = toMinFilterMode(gSampler.minFilter());
            auto wrapS = toWrapMode(gSampler.wrapS());
            auto wrapT = toWrapMode(gSampler.wrapT());
            // GLTF doesn't have WRAP_R
            auto sampler = Sampler::create();
            // Use trilinear filtering instead of what was specified in the gltf file.
            minFilter = Sampler::MinFilter::LINEAR_MIPMAP_LINEAR;
            sampler->setFilterMode(minFilter, magFilter);
            sampler->setWrapMode(wrapS, wrapT);
            return sampler;
        }
        return nullptr;
    }

    ref<Image> GLTF2Loader::Impl::loadImage(size_t index) {
        RETURN_IF_FOUND(_images, index);
        ref<Image> image = nullptr;
        if (auto gImage = _gltf.image(index)) {
            if (const char* uri = gImage.uri()) {
                if (gImage.isBase64()) {
                    std::vector<unsigned char> imageData;
                    if (gImage.loadBase64(imageData)) {
                        image = Image::createFromFileMemory(imageData.data(), static_cast<int>(imageData.size()));
                    }
                }
                else {
                    std::string imagePath = _gltf.baseDir() + gImage.uri();
                    image = Image::createFromFile(imagePath.c_str());
                }
            }
            else if (auto gBufferView = gImage.bufferView()) {
                size_t bufferIndex;
                if (gBufferView.buffer(bufferIndex)) {
                    if (auto buffer = loadBuffer(bufferIndex)) {
                        int bufferLength = static_cast<int>(gBufferView.byteLength());
                        image = Image::createFromFileMemory(buffer->data() + gBufferView.byteOffset(), bufferLength);
                    }
                }
            }
            if (image) {
                _images[index] = image;
            }
        }
        return image;
    }

    ref<Material> GLTF2Loader::Impl::loadDefaultMaterial() {
        if (_defaultMaterial) {
            return _defaultMaterial;
        }
        auto material = Material::create();

        auto emission = MaterialParameter::create("emission");
        emission->setValue(glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
        material->addParam(emission);

        auto technique = loadDefaultTechnique();
        material->setTechnique(technique);
        return material;
    }

    ref<Technique> GLTF2Loader::Impl::loadDefaultTechnique() {
        if (_defaultTechnique) {
            return _defaultTechnique;
        }

        auto effect = Effect::createFromSource(DEFAULT_VERT_SHADER, DEFAULT_FRAG_SHADER);
        if (effect == nullptr) {
            loge("LOAD_DEFAULT_EFFECT");
            return nullptr;
        }
        auto tech = Technique::create();
        tech->setEffect(effect);

        tech->setAttribute("a_normal", AttributeSemantic::NORMAL);
        tech->setAttribute("a_position", AttributeSemantic::POSITION);

        tech->setUniformName("u_emission", "emission");

        tech->setSemanticUniform("u_modelViewMatrix", "modelViewMatrix", MaterialParameter::Semantic::MODELVIEW);
        tech->setSemanticUniform("u_normalMatrix", "normalMatrix", MaterialParameter::Semantic::MODELVIEWINVERSETRANSPOSE);
        tech->setSemanticUniform("u_projectionMatrix", "projectionMatrix", MaterialParameter::Semantic::PROJECTION);

        auto& state = tech->renderState();
        state.setCulling(true);
        state.setDepthTest(true);

        _defaultTechnique = tech;
        return _defaultTechnique;
    }

    string GLTF2Loader::Impl::uriToPath(const string& uri) const {
        // TODO handle absolute paths for uri
        return joinPath(_baseDir, uri);
    }

    void GLTF2Loader::Impl::useDefaultMaterial(bool value) {
        _useDefaultMaterial = value;
    }

    void GLTF2Loader::Impl::setAutoLoadMaterials(bool value) {
        _autoLoadMaterials = value;
    }

    void GLTF2Loader::Impl::loadTransform(const gltf2::Node& gNode, const ref<Node>& node) {
        std::array<float, 16> matrix;
        if (gNode.matrix(matrix.data())) {
            node->setLocalTransform(glm::make_mat4(matrix.data()));
        }
        else {
            glm::vec3 trans;
            glm::vec3 scale(1.0f);
            glm::quat rot;
            gNode.translation(glm::value_ptr(trans));
            gNode.scale(glm::value_ptr(scale));
            gNode.rotation(glm::value_ptr(rot));
            node->setLocalTransform(trans, rot, scale);
        }
    }

    ////////////////////////////////////////////////////////////////

    MeshPrimitive::Mode toMode(gltf2::Primitive::Mode mode) {
        switch (mode) {
        case gltf2::Primitive::Mode::POINTS:         return MeshPrimitive::POINTS;
        case gltf2::Primitive::Mode::LINES:          return MeshPrimitive::LINES;
        case gltf2::Primitive::Mode::LINE_LOOP:      return MeshPrimitive::LINE_LOOP;
        case gltf2::Primitive::Mode::LINE_STRIP:     return MeshPrimitive::LINE_STRIP;
        case gltf2::Primitive::Mode::TRIANGLES:      return MeshPrimitive::TRIANGLES;
        case gltf2::Primitive::Mode::TRIANGLE_STRIP: return MeshPrimitive::TRIANGLE_STRIP;
        case gltf2::Primitive::Mode::TRIANGLE_FAN:   return MeshPrimitive::TRIANGLE_FAN;
        default:
            return MeshPrimitive::TRIANGLES;
        }
    }

    MaterialParameter::Semantic toSemantic(const string& semantic) {
        switch (semantic.length()) {
        case 4: if (semantic == "VIEW") return MaterialParameter::Semantic::VIEW;
            break;
        case 5:
            if (semantic == "LOCAL") return MaterialParameter::Semantic::LOCAL;
            if (semantic == "MODEL") return MaterialParameter::Semantic::MODEL;
            break;
        case 8: if (semantic == "VIEWPORT") return MaterialParameter::Semantic::VIEWPORT;
            break;
        case 9: if (semantic == "MODELVIEW") return MaterialParameter::Semantic::MODELVIEW;
            break;
        case 10: if (semantic == "PROJECTION") return MaterialParameter::Semantic::PROJECTION;
            break;
        case 11: if (semantic == "VIEWINVERSE") return MaterialParameter::Semantic::VIEWINVERSE;
            break;
        case 12: if (semantic == "MODELINVERSE") return MaterialParameter::Semantic::MODELINVERSE;
            break;
        case 16: if (semantic == "MODELVIEWINVERSE") return MaterialParameter::Semantic::MODELVIEWINVERSE;
            break;
        case 17: if (semantic == "PROJECTIONINVERSE") return MaterialParameter::Semantic::PROJECTIONINVERSE;
            break;
        case 19: if (semantic == "MODELVIEWPROJECTION") return MaterialParameter::Semantic::MODELVIEWPROJECTION;
            break;
        case 21: if (semantic == "MODELINVERSETRANSPOSE") return MaterialParameter::Semantic::MODELINVERSETRANSPOSE;
            break;
        case 25: if (semantic == "MODELVIEWINVERSETRANSPOSE") return MaterialParameter::Semantic::MODELVIEWINVERSETRANSPOSE;
            break;
        case 26: if (semantic == "MODELVIEWPROJECTIONINVERSE") return MaterialParameter::Semantic::MODELVIEWPROJECTIONINVERSE;
            break;
        default: break;
        }
        return MaterialParameter::Semantic::NONE;
    }

    AttributeSemantic toAttributeSemantic(const string& semantic) {
        switch (semantic[0]) {
        case 'C':
            if (semantic == "COLOR_0") return AttributeSemantic::COLOR_0;
            if (semantic == "COLOR") return AttributeSemantic::COLOR;
            break;
        case 'J':
            if (semantic == "JOINT") return AttributeSemantic::JOINT;
            if (semantic == "JOINTMATRIX") return AttributeSemantic::JOINTMATRIX; // TODO remove?
            break;
        case 'N': if (semantic == "NORMAL") return AttributeSemantic::NORMAL;
            break;
        case 'P': if (semantic == "POSITION") return AttributeSemantic::POSITION;
            break;
        case 'W': if (semantic == "WEIGHT") return AttributeSemantic::WEIGHT;
            break;
        case 'T':
            if (semantic == "TANGENT") {
                return AttributeSemantic::TANGENT;
            }
            if (semantic.length() >= 10 && semantic[1] == 'E') {
                int n = semantic[9] - '0';
                if (semantic.length() == 11) {
                    n = n * 10 + (semantic[10] - '0');
                }
                switch (n) {
                case 0: return AttributeSemantic::TEXCOORD_0;
                case 1: return AttributeSemantic::TEXCOORD_1;
                case 2: return AttributeSemantic::TEXCOORD_2;
                case 3: return AttributeSemantic::TEXCOORD_3;
                case 4: return AttributeSemantic::TEXCOORD_4;
                case 5: return AttributeSemantic::TEXCOORD_5;
                case 6: return AttributeSemantic::TEXCOORD_6;
                case 7: return AttributeSemantic::TEXCOORD_7;
                case 8: return AttributeSemantic::TEXCOORD_8;
                case 9: return AttributeSemantic::TEXCOORD_9;
                case 10: return AttributeSemantic::TEXCOORD_10;
                case 11: return AttributeSemantic::TEXCOORD_11;
                case 12: return AttributeSemantic::TEXCOORD_12;
                case 13: return AttributeSemantic::TEXCOORD_13;
                case 14: return AttributeSemantic::TEXCOORD_14;
                case 15: return AttributeSemantic::TEXCOORD_15;
                case 16: return AttributeSemantic::TEXCOORD_16;
                case 17: return AttributeSemantic::TEXCOORD_17;
                case 18: return AttributeSemantic::TEXCOORD_18;
                case 19: return AttributeSemantic::TEXCOORD_19;
                case 20: return AttributeSemantic::TEXCOORD_20;
                case 21: return AttributeSemantic::TEXCOORD_21;
                case 22: return AttributeSemantic::TEXCOORD_22;
                case 23: return AttributeSemantic::TEXCOORD_23;
                case 24: return AttributeSemantic::TEXCOORD_24;
                case 25: return AttributeSemantic::TEXCOORD_25;
                case 26: return AttributeSemantic::TEXCOORD_26;
                case 27: return AttributeSemantic::TEXCOORD_27;
                case 28: return AttributeSemantic::TEXCOORD_28;
                case 29: return AttributeSemantic::TEXCOORD_29;
                case 30: return AttributeSemantic::TEXCOORD_30;
                case 31: return AttributeSemantic::TEXCOORD_31;
                default: break;
                }
            }
            break;
        default:
            break;
        }
        throw std::domain_error("Invalid attribute semantic type");
    }

    Sampler::Wrap toWrapMode(gltf2::Sampler::Wrap wrap) {
        switch (wrap) {
        case gltf2::Sampler::Wrap::REPEAT:          return Sampler::Wrap::REPEAT;
        case gltf2::Sampler::Wrap::CLAMP_TO_EDGE:   return Sampler::Wrap::CLAMP_TO_EDGE;
        case gltf2::Sampler::Wrap::MIRRORED_REPEAT: return Sampler::Wrap::MIRRORED_REPEAT;
        default:
            throw std::domain_error("Invalid wrap mode");
        }
    }

    Sampler::MinFilter toMinFilterMode(gltf2::Sampler::MinFilter filter) {
        switch (filter) {
        case gltf2::Sampler::MinFilter::NEAREST: return Sampler::MinFilter::NEAREST;
        case gltf2::Sampler::MinFilter::LINEAR:  return Sampler::MinFilter::LINEAR;
        case gltf2::Sampler::MinFilter::NEAREST_MIPMAP_NEAREST: return Sampler::MinFilter::NEAREST_MIPMAP_NEAREST;
        case gltf2::Sampler::MinFilter::LINEAR_MIPMAP_NEAREST:  return Sampler::MinFilter::LINEAR_MIPMAP_NEAREST;
        case gltf2::Sampler::MinFilter::NEAREST_MIPMAP_LINEAR:  return Sampler::MinFilter::NEAREST_MIPMAP_LINEAR;
        case gltf2::Sampler::MinFilter::LINEAR_MIPMAP_LINEAR:   return Sampler::MinFilter::LINEAR_MIPMAP_LINEAR;
        default:
            throw std::domain_error("Invalid filter mode");
        }
    }

    Sampler::MagFilter toMagFilterMode(gltf2::Sampler::MagFilter filter) {
        switch (filter) {
        case gltf2::Sampler::MagFilter::NEAREST: return Sampler::MagFilter::NEAREST;
        case gltf2::Sampler::MagFilter::LINEAR:  return Sampler::MagFilter::LINEAR;
        default:
            throw std::domain_error("Invalid filter mode");
        }
    }

    GLint numComponentsOfType(const string& type) {
        if (type.length() == 4) {
            char c0 = type[0];
            char c3 = type[3];
            if (c0 == 'V') {
                switch (c3) {
                case '2': return 2;
                case '3': return 3;
                case '4': return 4;
                }
            }
            else if (c0 == 'M') {
                switch (c3) {
                case '2': return 4;
                case '3': return 9;
                case '4': return 16;
                }
            }
        }
        return 1;
    }

    void setState(int state, RenderState& block) {
        switch (state) {
        case GL_BLEND:
            block.setBlend(true);
            break;
        case GL_CULL_FACE:
            block.setCulling(true);
            break;
        case GL_DEPTH_TEST:
            block.setDepthTest(true);
            break;
        case GL_POLYGON_OFFSET_FILL:
            block.setPolygonOffsetFill(true);
            break;
        case GL_PRIMITIVE_RESTART_FIXED_INDEX:
            block.setPrimitiveRestartFixedIndex(true);
            break;
        case GL_RASTERIZER_DISCARD:
            block.setRasterizerDiscard(true);
            break;
        case GL_SAMPLE_ALPHA_TO_COVERAGE:
            block.setSampleAlphaToCoverage(true);
            break;
        case GL_SAMPLE_COVERAGE:
            block.setSampleCoverage(true);
            break;
        case GL_SAMPLE_MASK:
            block.setSampleMask(true);
            break;
        case GL_SCISSOR_TEST:
            block.setScissorTest(true);
            break;
        case GL_STENCIL_TEST:
            block.setStencilTest(true);
            break;
        default:
            logw("Unknown enable state");
            break;
        }
    }
}