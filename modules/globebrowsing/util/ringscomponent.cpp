/*****************************************************************************************
*                                                                                       *
* OpenSpace                                                                             *
*                                                                                       *
* Copyright (c) 2014-2018                                                               *
*                                                                                       *
* Permission is hereby granted, free of charge, to any person obtaining a copy of this  *
* software and associated documentation files (the "Software"), to deal in the Software *
* without restriction, including without limitation the rights to use, copy, modify,    *
* merge, publish, distribute, sublicense, and/or sell copies of the Software, and to    *
* permit persons to whom the Software is furnished to do so, subject to the following   *
* conditions:                                                                           *
*                                                                                       *
* The above copyright notice and this permission notice shall be included in all copies *
* or substantial portions of the Software.                                              *
*                                                                                       *
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,   *
* INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A         *
* PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT    *
* HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF  *
* CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE  *
* OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                         *
****************************************************************************************/

#include <modules/globebrowsing/util/ringscomponent.h>
#include <modules/globebrowsing/globebrowsingmodule.h>
#include <modules/globebrowsing/globes/renderableglobe.h>

#include <openspace/documentation/documentation.h>
#include <openspace/documentation/verifier.h>
#include <openspace/util/updatestructures.h>

#include <openspace/engine/globals.h>
#include <openspace/engine/openspaceengine.h>
#include <openspace/engine/moduleengine.h>

#include <openspace/rendering/renderengine.h>
#include <openspace/scene/scene.h>

#include <ghoul/filesystem/cachemanager.h>
#include <ghoul/filesystem/filesystem.h>
#include <ghoul/logging/logmanager.h>

#include <ghoul/filesystem/filesystem.h>
#include <ghoul/misc/dictionary.h>
#include <ghoul/opengl/programobject.h>
#include <ghoul/io/texture/texturereader.h>
#include <ghoul/opengl/texture.h>
#include <ghoul/opengl/textureunit.h>

#include <ghoul/font/fontmanager.h>
#include <ghoul/font/fontrenderer.h>

#include <fstream>
#include <cstdlib>
#include <locale>

namespace {
    constexpr const std::array<const char*, 7> UniformNames = {
        "modelViewMatrix", "projectionMatrix", "textureOffset", 
        "transparency", "_nightFactor", "sunPosition", "texture1"
    };

    constexpr const std::array<const char*, 3> GeomUniformNames = {
        "modelViewMatrix", "projectionMatrix", "textureOffset"
    };

    constexpr openspace::properties::Property::PropertyInfo TextureInfo = {
        "Texture",
        "Texture",
        "This value is the path to a texture on disk that contains a one-dimensional "
        "texture which is used for these rings."
    };

    constexpr openspace::properties::Property::PropertyInfo SizeInfo = {
        "Size",
        "Size",
        "This value specifies the radius of the rings in meter."
    };

    constexpr openspace::properties::Property::PropertyInfo OffsetInfo = {
        "Offset",
        "Offset",
        "This value is used to limit the width of the rings.Each of the two values is a "
        "value between 0 and 1, where 0 is the center of the ring and 1 is the maximum "
        "extent at the radius. If this value is, for example {0.5, 1.0}, the ring is "
        "only shown between radius/2 and radius. It defaults to {0.0, 1.0}."
    };

    constexpr openspace::properties::Property::PropertyInfo NightFactorInfo = {
        "NightFactor",
        "Night Factor",
        "This value is a multiplicative factor that is applied to the side of the rings "
        "that is facing away from the Sun. If this value is equal to '1', no darkening "
        "of the night side occurs."
    };

    constexpr openspace::properties::Property::PropertyInfo TransparencyInfo = {
        "Transparency",
        "Transparency",
        "This value determines the transparency of part of the rings depending on the "
        "color values. For this value v, the transparency is equal to length(color) / v."
    };
} // namespace

namespace openspace {

    documentation::Documentation RingsComponent::Documentation() {
        using namespace documentation;
        return {
            "Rings Component",
            "globebrowsing_rings_component",
            {
                {
                    TextureInfo.identifier,
                    new StringVerifier,
                    Optional::Yes,
                    TextureInfo.description
                },
                {
                    SizeInfo.identifier,
                    new DoubleVerifier,
                    Optional::Yes,
                    SizeInfo.description
                },
                {
                    OffsetInfo.identifier,
                    new DoubleVector2Verifier,
                    Optional::Yes,
                    OffsetInfo.description
                },
                {
                    NightFactorInfo.identifier,
                    new DoubleVerifier,
                    Optional::Yes,
                    NightFactorInfo.description
                },
                {
                    TransparencyInfo.identifier,
                    new DoubleVerifier,
                    Optional::Yes,
                    TransparencyInfo.description
                }
            }
        };
    }

    RingsComponent::RingsComponent(const ghoul::Dictionary& dictionary)
        : properties::PropertyOwner({ "Rings" })
        , _texturePath(TextureInfo)
        , _size(SizeInfo, 1.f, 0.f, 1e25f)
        , _offset(OffsetInfo, glm::vec2(0.f, 1.f), glm::vec2(0.f), glm::vec2(1.f))
        , _nightFactor(NightFactorInfo, 0.33f, 0.f, 1.f)
        , _transparency(TransparencyInfo, 0.15f, 0.f, 1.f)
        , _enabled({ "Enabled", "Enabled", "Enable/Disable Rings" }, true)
        , _ringsDictionary(dictionary)
    {
        using ghoul::filesystem::File;
        
        if (dictionary.hasKey("Rings")) {
            dictionary.getValue("Rings", _ringsDictionary);
        }
        
        documentation::testSpecificationAndThrow(
            Documentation(),
            _ringsDictionary,
            "RingsComponent"
        );
    }

    void RingsComponent::initialize() 
    {
        using ghoul::filesystem::File;

        addProperty(_enabled);

        _size = static_cast<float>(_ringsDictionary.value<double>(SizeInfo.identifier));
        //setBoundingSphere(_size);
        _size.onChange([&]() { _planeIsDirty = true; });
        addProperty(_size);

        _texturePath = absPath(_ringsDictionary.value<std::string>(TextureInfo.identifier));
        _textureFile = std::make_unique<File>(_texturePath);

        if (_ringsDictionary.hasKeyAndValue<glm::vec2>(OffsetInfo.identifier)) {
            _offset = _ringsDictionary.value<glm::vec2>(OffsetInfo.identifier);
        }
        addProperty(_offset);

        _texturePath.onChange([&]() { loadTexture(); });
        addProperty(_texturePath);

        _textureFile->setCallback([&](const File&) { _textureIsDirty = true; });

        if (_ringsDictionary.hasKeyAndValue<double>(NightFactorInfo.identifier)) {
            _nightFactor = static_cast<float>(
                _ringsDictionary.value<double>(NightFactorInfo.identifier)
                );
        }
        addProperty(_nightFactor);

        if (_ringsDictionary.hasKeyAndValue<double>(TransparencyInfo.identifier)) {
            _transparency = static_cast<float>(
                _ringsDictionary.value<double>(TransparencyInfo.identifier)
                );
        }
        addProperty(_transparency);
    }

    bool RingsComponent::isReady() const {
        return (_shader || _geometryOnlyShader) && _texture;
    }

    void RingsComponent::initializeGL() {
        _shader = global::renderEngine.buildRenderProgram(
            "RingsProgram",
            absPath("${MODULE_GLOBEBROWSING}/shaders/rings_vs.glsl"),
            absPath("${MODULE_GLOBEBROWSING}/shaders/rings_fs.glsl")
        );

        _geometryOnlyShader = global::renderEngine.buildRenderProgram(
            "RingsGeomOnlyProgram",
            absPath("${MODULE_GLOBEBROWSING}/shaders/rings_geom_vs.glsl"),
            absPath("${MODULE_GLOBEBROWSING}/shaders/rings_geom_fs.glsl")
        );

        ghoul::opengl::updateUniformLocations(*_shader, _uniformCache, UniformNames);
        ghoul::opengl::updateUniformLocations(
            *_geometryOnlyShader, 
            _geomUniformCache, 
            GeomUniformNames
        );

        glGenVertexArrays(1, &_quad);
        glGenBuffers(1, &_vertexPositionBuffer);

        createPlane();
        loadTexture();
    }

    void RingsComponent::deinitializeGL() {
        glDeleteVertexArrays(1, &_quad);
        _quad = 0;

        glDeleteBuffers(1, &_vertexPositionBuffer);
        _vertexPositionBuffer = 0;

        _textureFile = nullptr;
        _texture = nullptr;

        global::renderEngine.removeRenderProgram(_shader.get());
        _shader = nullptr;

        global::renderEngine.removeRenderProgram(_geometryOnlyShader.get());
        _geometryOnlyShader = nullptr;
    }

    void RingsComponent::draw(const RenderData& data, const RingsComponent::RenderPass renderPass) {
        if (renderPass == GeometryAndShading) {
            _shader->activate();
        }
        else if (renderPass == GeometryOnly) {
            _geometryOnlyShader->activate();
        }
        
        glm::dmat4 modelTransform =
            glm::translate(glm::dmat4(1.0), data.modelTransform.translation) *
            glm::dmat4(data.modelTransform.rotation) *
            glm::scale(glm::dmat4(1.0), glm::dvec3(data.modelTransform.scale));

        glm::dmat4 modelViewTransform = data.camera.combinedViewMatrix() * modelTransform;

        ghoul::opengl::TextureUnit unit;
        if (renderPass == GeometryAndShading) {
            _shader->setUniform(_uniformCache.modelViewMatrix, modelViewTransform);
            _shader->setUniform(_uniformCache.projectionMatrix, glm::dmat4(data.camera.projectionMatrix()));
            _shader->setUniform(_uniformCache.textureOffset, _offset);
            _shader->setUniform(_uniformCache.transparency, _transparency);

            _shader->setUniform(_uniformCache.nightFactor, _nightFactor);
            _shader->setUniform(_uniformCache.sunPosition, _sunPosition);

            unit.activate();
            _texture->bind();
            _shader->setUniform(_uniformCache.texture, unit);
        }
        else if (renderPass == GeometryOnly) {
            _geometryOnlyShader->setUniform(
                _geomUniformCache.modelViewMatrix,
                modelViewTransform
            );
            _geometryOnlyShader->setUniform(
                _geomUniformCache.projectionMatrix,
                glm::dmat4(data.camera.projectionMatrix())
            );
            _geometryOnlyShader->setUniform(
                _geomUniformCache.textureOffset,
                _offset
            );
        }

        glDisable(GL_CULL_FACE);

        glBindVertexArray(_quad);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glEnable(GL_CULL_FACE);
       

        if (renderPass == GeometryAndShading) {
            _shader->deactivate();
        }
        else if (renderPass == GeometryOnly) {
            _geometryOnlyShader->deactivate();
        }
    }

    void RingsComponent::draw(
        const RenderData& data,
        const RingsComponent::RenderPass renderPass,
        const ShadowComponent::ShadowMapData& shadowData
    ) {
        _shader->activate();
        
        glm::dmat4 modelTransform =
            glm::translate(glm::dmat4(1.0), data.modelTransform.translation) *
            glm::dmat4(data.modelTransform.rotation) *
            glm::scale(glm::dmat4(1.0), glm::dvec3(data.modelTransform.scale));

        glm::dmat4 modelViewTransform = data.camera.combinedViewMatrix() * modelTransform;

        _shader->setUniform("modelMatrix", modelTransform);

        _shader->setUniform(_uniformCache.modelViewMatrix, modelViewTransform);
        _shader->setUniform(_uniformCache.projectionMatrix, glm::dmat4(data.camera.projectionMatrix()));
        _shader->setUniform(_uniformCache.textureOffset, _offset);
        _shader->setUniform(_uniformCache.transparency, _transparency);

        _shader->setUniform(_uniformCache.nightFactor, _nightFactor);
        _shader->setUniform(_uniformCache.sunPosition, _sunPosition);
        
        ghoul::opengl::TextureUnit unit;
        unit.activate();
        _texture->bind();
        _shader->setUniform(_uniformCache.texture, unit);
        
        _shader->setUniform("shadowMatrix", shadowData.shadowMatrix);
        ghoul::opengl::TextureUnit shadowMapUnit;
        shadowMapUnit.activate();
        glBindTexture(GL_TEXTURE_2D, shadowData.shadowDepthTexture);
        _shader->setUniform("shadowMap", shadowMapUnit);


        glDisable(GL_CULL_FACE);

        glBindVertexArray(_quad);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glEnable(GL_CULL_FACE);


        _shader->deactivate();
    }

    void RingsComponent::update(const UpdateData& data) {
        if (_shader->isDirty()) {
            _shader->rebuildFromFile();
            ghoul::opengl::updateUniformLocations(*_shader, _uniformCache, UniformNames);
        }

        if (_geometryOnlyShader->isDirty()) {
            _geometryOnlyShader->rebuildFromFile();
            ghoul::opengl::updateUniformLocations(
                *_geometryOnlyShader, 
                _geomUniformCache, 
                GeomUniformNames
            );
        }

        if (_planeIsDirty) {
            createPlane();
            _planeIsDirty = false;
        }

        if (_textureIsDirty) {
            loadTexture();
            _textureIsDirty = false;
        }

        _sunPosition = glm::normalize(
            global::renderEngine.scene()->sceneGraphNode("Sun")->worldPosition() -
            data.modelTransform.translation
        );
    }

    void RingsComponent::loadTexture() {
        if (!_texturePath.value().empty()) {
            using namespace ghoul::io;
            using namespace ghoul::opengl;
            std::unique_ptr<Texture> texture = TextureReader::ref().loadTexture(
                absPath(_texturePath)
            );

            if (texture) {
                LDEBUGC(
                    "RingsComponent",
                    fmt::format("Loaded texture from '{}'", absPath(_texturePath))
                );
                _texture = std::move(texture);

                _texture->uploadTexture();
                _texture->setFilter(ghoul::opengl::Texture::FilterMode::AnisotropicMipMap);

                _textureFile = std::make_unique<ghoul::filesystem::File>(_texturePath);
                _textureFile->setCallback(
                    [&](const ghoul::filesystem::File&) { _textureIsDirty = true; }
                );
            }
        }
    }

    void RingsComponent::createPlane() {
        const GLfloat size = _size;

        struct VertexData {
            GLfloat x;
            GLfloat y;
            GLfloat s;
            GLfloat t;
        };

        VertexData data[] = {
            { -size, -size, 0.f, 0.f },
            {  size,  size, 1.f, 1.f },
            { -size,  size, 0.f, 1.f },
            { -size, -size, 0.f, 0.f },
            {  size, -size, 1.f, 0.f },
            {  size,  size, 1.f, 1.f },
        };

        glBindVertexArray(_quad);
        glBindBuffer(GL_ARRAY_BUFFER, _vertexPositionBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(
            0,
            2,
            GL_FLOAT,
            GL_FALSE,
            sizeof(VertexData),
            nullptr
        );
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(
            1,
            2,
            GL_FLOAT,
            GL_FALSE,
            sizeof(VertexData),
            reinterpret_cast<void*>(offsetof(VertexData, s)) // NOLINT
        );
    }

    bool RingsComponent::isEnabled() const {
        return _enabled;
    }
} // namespace openspace
