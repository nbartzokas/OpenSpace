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

#include <modules/marsrover/marsrovermodule.h>

#include <openspace/documentation/documentation.h>
#include <openspace/rendering/renderable.h>
#include <openspace/rendering/screenspacerenderable.h>
#include <openspace/util/factorymanager.h>

#include <ghoul/misc/assert.h>

#include <modules/marsrover/rendering/screenspacedashboard.h>
#include <modules/marsrover/rendering/renderablemarsrover.h>

#include <modules/marsrover/rotation/fixedrotation.h>
#include <modules/marsrover/rotation/luarotation.h>
#include <modules/marsrover/rotation/staticrotation.h>

#include <modules/marsrover/scale/luascale.h>
#include <modules/marsrover/scale/staticscale.h>
#include <ghoul/filesystem/filesystem>

namespace openspace {

MarsroverModule::MarsroverModule() : OpenSpaceModule("Marsrover") {}    //changed by kristin: original was MarsRoverModule::Name instead of "Marsrover"

void MarsroverModule::internalInitialize(const ghoul::Dictionary&) {

    auto fRenderable = FactoryManager::ref().factory<Renderable>();
    ghoul_assert(fRenderable, "Renderable factory was not created");

    fRenderable->registerClass<RenderableMarsrover>("RenderableMarsrover");

}

std::vector<documentation::Documentation> MarsroverModule::documentations() const {
    return {
        RenderableMarsrover::Documentation(),
    };
}


std::vector<scripting::LuaLibrary> MarsroverModule::luaLibraries() const {
    return {
        ScreenSpaceDashboard::luaLibrary()
    };
}

} // namespace openspace