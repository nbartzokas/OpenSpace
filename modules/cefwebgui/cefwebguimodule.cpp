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

#include <modules/webbrowser/webbrowsermodule.h>

#include <modules/cefwebgui/cefwebguimodule.h>
#include <modules/cefwebgui/include/guirenderhandler.h>
#include <modules/cefwebgui/include/guikeyboardhandler.h>
#include <modules/webbrowser/include/browserinstance.h>
#include <openspace/engine/globals.h>
#include <openspace/engine/globalscallbacks.h>
#include <openspace/engine/moduleengine.h>
#include <openspace/engine/windowdelegate.h>
#include <ghoul/fmt.h>
#include <ghoul/logging/logmanager.h>
#include <ghoul/misc/dictionary.h>

namespace {
    constexpr const char* _loggerCat = "CefWebGui";

    constexpr openspace::properties::Property::PropertyInfo EnabledInfo = {
        "Enabled",
        "Is Enabled",
        "This setting determines whether the browser should be enabled or not."
    };

    constexpr openspace::properties::Property::PropertyInfo VisibleInfo = {
        "Visible",
        "Is Visible",
        "This setting determines whether the browser should be visible or not."
    };

    constexpr openspace::properties::Property::PropertyInfo GuiUrlInfo = {
        "GuiUrl",
        "GUI URL",
        "The URL of the webpage that is used to load the WebGUI from."
    };
} // namespace

namespace openspace {

CefWebGuiModule::CefWebGuiModule()
    : OpenSpaceModule(CefWebGuiModule::Name)
    , _enabled(EnabledInfo, true)
    , _visible(VisibleInfo, true)
    , _url(GuiUrlInfo, "")
{
    addProperty(_enabled);
    addProperty(_visible);
    addProperty(_url);
}

void CefWebGuiModule::startOrStopGui() {
    WebBrowserModule* webBrowserModule = global::moduleEngine.module<WebBrowserModule>();

    const bool isGuiWindow =
        global::windowDelegate.hasGuiWindow() ?
        global::windowDelegate.isGuiWindow() :
        true;
    const bool isMaster = global::windowDelegate.isMaster();

    if (_enabled && isGuiWindow && isMaster) {
        LDEBUGC("WebBrowser", fmt::format("Loading GUI from {}", _url));

        if (!_instance) {
            _instance = std::make_unique<BrowserInstance>(
                new GUIRenderHandler,
                new GUIKeyboardHandler
            );
            _instance->initialize();
            _instance->loadUrl(_url);
        }
        if (_visible) {
            webBrowserModule->attachEventHandler(_instance.get());
        }
        webBrowserModule->addBrowser(_instance.get());
    } else if (_instance) {
        _instance->close(true);
        webBrowserModule->removeBrowser(_instance.get());
        webBrowserModule->detachEventHandler();
        _instance.reset();
    }
}

void CefWebGuiModule::internalInitialize(const ghoul::Dictionary& configuration) {
    WebBrowserModule* webBrowserModule =
        global::moduleEngine.module<WebBrowserModule>();

    bool available = webBrowserModule && webBrowserModule->isEnabled();

    if (!available) {
        return;
    }

    _enabled.onChange([this]() {
        startOrStopGui();
    });

    _url.onChange([this]() {
        if (_instance) {
            _instance->loadUrl(_url);
        }
    });

    _visible.onChange([this, webBrowserModule]() {
        if (_visible && _instance) {
            webBrowserModule->attachEventHandler(_instance.get());
        } else {
            webBrowserModule->detachEventHandler();
        }
    });

    _url = configuration.value<std::string>(GuiUrlInfo.identifier);

    _enabled = configuration.hasValue<bool>(EnabledInfo.identifier) &&
               configuration.value<bool>(EnabledInfo.identifier);

    _visible = configuration.hasValue<bool>(VisibleInfo.identifier) &&
               configuration.value<bool>(VisibleInfo.identifier);

    global::callback::initializeGL.push_back([this]() {
        startOrStopGui();
    });

    global::callback::draw2D.push_back([this](){
        const bool isGuiWindow =
            global::windowDelegate.hasGuiWindow() ?
            global::windowDelegate.isGuiWindow() :
            true;
        const bool isMaster = global::windowDelegate.isMaster();

        if (isGuiWindow && isMaster && _instance) {
            if (global::windowDelegate.windowHasResized()) {
                _instance->reshape(global::windowDelegate.currentWindowSize());
            }
            if (_visible) {
                _instance->draw();
            }
        }
    });

    global::callback::deinitializeGL.push_back([this]() {
        _enabled = false;
        startOrStopGui();
    });
}

} // namespace openspace
