/*
   Copyright (C) 2011 Samsung Electronics
   Copyright (C) 2012 Intel Corporation. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "EwkView.h"

#include "ContextMenuClientEfl.h"
#include "EflScreenUtilities.h"
#include "EvasGLContext.h"
#include "EvasGLSurface.h"
#include "EwkDebug.h"
#include "FindClientEfl.h"
#include "FormClientEfl.h"
#include "InputMethodContextEfl.h"
#include "NativeWebKeyboardEvent.h"
#include "NativeWebMouseEvent.h"
#include "NativeWebWheelEvent.h"
#include "PageLoadClientEfl.h"
#include "PagePolicyClientEfl.h"
#include "PageUIClientEfl.h"
#include "SnapshotImageGL.h"
#include "ViewClientEfl.h"
#include "WKArray.h"
#include "WKDictionary.h"
#include "WKEventEfl.h"
#include "WKGeometry.h"
#include "WKNumber.h"
#include "WKPageGroup.h"
#include "WKPopupItem.h"
#include "WKString.h"
#include "WKView.h"
#include "WKViewEfl.h"
#include "WebImage.h"
#include "WebPageGroup.h"
#include "WebPageProxy.h"
#include "WebPreferences.h"
#include "WebProcessPool.h"
#include "ewk_back_forward_list_private.h"
#include "ewk_color_picker_private.h"
#include "ewk_context_menu_item_private.h"
#include "ewk_context_menu_private.h"
#include "ewk_context_private.h"
#include "ewk_page_group_private.h"
#include "ewk_popup_menu_item_private.h"
#include "ewk_popup_menu_private.h"
#include "ewk_security_origin_private.h"
#include "ewk_settings_private.h"
#include "ewk_window_features_private.h"
#include <Ecore_Evas.h>
#include <Edje.h>
#include <Evas_GL.h>
#include <WebCore/CairoUtilitiesEfl.h>
#include <WebCore/Cursor.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/PlatformContextCairo.h>
#include <WebKit/WKImageCairo.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>

#if ENABLE(VIBRATION)
#include "VibrationClientEfl.h"
#endif

#if ENABLE(FULLSCREEN_API)
#include "WebFullScreenManagerProxy.h"
#endif

#if HAVE(ACCESSIBILITY) && defined(HAVE_ECORE_X)
#include "WebAccessibility.h"
#endif

using namespace EwkViewCallbacks;
using namespace WebCore;
using namespace WebKit;

// Auxiliary functions.

const char EwkView::smartClassName[] = "EWK2_View";

static inline void smartDataChanged(Ewk_View_Smart_Data* smartData)
{
    ASSERT(smartData);

    if (smartData->changed.any)
        return;

    smartData->changed.any = true;
    evas_object_smart_changed(smartData->self);
}

static Evas_Smart* defaultSmartClassInstance()
{
    static Ewk_View_Smart_Class api = EWK_VIEW_SMART_CLASS_INIT_NAME_VERSION(EwkView::smartClassName);
    static Evas_Smart* smart = 0;

    if (!smart) {
        EwkView::initSmartClassInterface(api);
        smart = evas_smart_class_new(&api.sc);
    }

    return smart;
}

static inline Ewk_View_Smart_Data* toSmartData(Evas_Object* evasObject)
{
    ASSERT(evasObject);
    ASSERT(isEwkViewEvasObject(evasObject));

    return static_cast<Ewk_View_Smart_Data*>(evas_object_smart_data_get(evasObject));
}

static inline EwkView* toEwkView(const Ewk_View_Smart_Data* smartData)
{
    ASSERT(smartData);
    ASSERT(smartData->priv);

    return smartData->priv;
}

static inline EwkView* toEwkView(const Evas_Object* evasObject)
{
    ASSERT(evasObject);
    ASSERT(isEwkViewEvasObject(evasObject));

    return toEwkView(static_cast<Ewk_View_Smart_Data*>(evas_object_smart_data_get(evasObject)));
}

static inline void showEvasObjectsIfNeeded(const Ewk_View_Smart_Data* smartData)
{
    ASSERT(smartData);

    if (evas_object_clipees_get(smartData->base.clipper))
        evas_object_show(smartData->base.clipper);
    evas_object_show(smartData->image);
}

// EwkViewEventHandler implementation.

template <Evas_Callback_Type EventType>
class EwkViewEventHandler {
public:
    static void subscribe(Evas_Object* evasObject)
    {
        evas_object_event_callback_add(evasObject, EventType, handleEvent, toSmartData(evasObject));
    }

    static void unsubscribe(Evas_Object* evasObject)
    {
        evas_object_event_callback_del(evasObject, EventType, handleEvent);
    }

    static void handleEvent(void* data, Evas*, Evas_Object*, void* eventInfo);
};

template <>
void EwkViewEventHandler<EVAS_CALLBACK_MOUSE_DOWN>::handleEvent(void* data, Evas*, Evas_Object*, void* eventInfo)
{
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    if (smartData->api->mouse_down)
        smartData->api->mouse_down(smartData, static_cast<Evas_Event_Mouse_Down*>(eventInfo));
}

template <>
void EwkViewEventHandler<EVAS_CALLBACK_MOUSE_UP>::handleEvent(void* data, Evas*, Evas_Object*, void* eventInfo)
{
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    if (smartData->api->mouse_up)
        smartData->api->mouse_up(smartData, static_cast<Evas_Event_Mouse_Up*>(eventInfo));
}

template <>
void EwkViewEventHandler<EVAS_CALLBACK_MOUSE_MOVE>::handleEvent(void* data, Evas*, Evas_Object*, void* eventInfo)
{
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    if (smartData->api->mouse_move)
        smartData->api->mouse_move(smartData, static_cast<Evas_Event_Mouse_Move*>(eventInfo));
}

template <>
void EwkViewEventHandler<EVAS_CALLBACK_FOCUS_IN>::handleEvent(void* data, Evas*, Evas_Object*, void*)
{
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    if (smartData->api->focus_in)
        smartData->api->focus_in(smartData);
}

template <>
void EwkViewEventHandler<EVAS_CALLBACK_FOCUS_OUT>::handleEvent(void* data, Evas*, Evas_Object*, void*)
{
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    if (smartData->api->focus_out)
        smartData->api->focus_out(smartData);
}

template <>
void EwkViewEventHandler<EVAS_CALLBACK_MOUSE_WHEEL>::handleEvent(void* data, Evas*, Evas_Object*, void* eventInfo)
{
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    if (smartData->api->mouse_wheel)
        smartData->api->mouse_wheel(smartData, static_cast<Evas_Event_Mouse_Wheel*>(eventInfo));
}

template <>
void EwkViewEventHandler<EVAS_CALLBACK_MOUSE_IN>::handleEvent(void* data, Evas* evas, Evas_Object*, void*)
{
#ifdef HAVE_ECORE_X
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EwkView* self = toEwkView(smartData);

    Ecore_Evas* ecoreEvas = ecore_evas_ecore_evas_get(evas);
    Ecore_X_Window window = getEcoreXWindow(ecoreEvas);

    self->updateCursor(window);
#endif
}

template <>
void EwkViewEventHandler<EVAS_CALLBACK_KEY_DOWN>::handleEvent(void* data, Evas*, Evas_Object*, void* eventInfo)
{
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    if (smartData->api->key_down)
        smartData->api->key_down(smartData, static_cast<Evas_Event_Key_Down*>(eventInfo));
}

template <>
void EwkViewEventHandler<EVAS_CALLBACK_KEY_UP>::handleEvent(void* data, Evas*, Evas_Object*, void* eventInfo)
{
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    if (smartData->api->key_up)
        smartData->api->key_up(smartData, static_cast<Evas_Event_Key_Up*>(eventInfo));
}

template <>
void EwkViewEventHandler<EVAS_CALLBACK_SHOW>::handleEvent(void* data, Evas*, Evas_Object*, void*)
{
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    WKViewSetIsVisible(toEwkView(smartData)->wkView(), true);
}

template <>
void EwkViewEventHandler<EVAS_CALLBACK_HIDE>::handleEvent(void* data, Evas*, Evas_Object*, void*)
{
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    WKViewSetIsVisible(toEwkView(smartData)->wkView(), false);
}

typedef HashMap<WKPageRef, Evas_Object*> WKPageToEvasObjectMap;

static inline WKPageToEvasObjectMap& wkPageToEvasObjectMap()
{
    static NeverDestroyed<WKPageToEvasObjectMap> map;
    return map;
}

// EwkView implementation.

EwkView::EwkView(WKViewRef view, Evas_Object* evasObject)
    : m_webView(view)
    , m_evasObject(evasObject)
    , m_context(EwkContext::findOrCreateWrapper(WKPageGetContext(wkPage())))
    , m_pageGroup(EwkPageGroup::findOrCreateWrapper(WKPageGetPageGroup(wkPage())))
    , m_pageLoadClient(std::make_unique<PageLoadClientEfl>(this))
    , m_pagePolicyClient(std::make_unique<PagePolicyClientEfl>(this))
    , m_contextMenuClient(std::make_unique<ContextMenuClientEfl>(this))
    , m_findClient(std::make_unique<FindClientEfl>(this))
    , m_formClient(std::make_unique<FormClientEfl>(this))
    , m_viewClient(std::make_unique<ViewClientEfl>(this))
#if ENABLE(VIBRATION)
    , m_vibrationClient(std::make_unique<VibrationClientEfl>(this))
#endif
    , m_backForwardList(std::make_unique<EwkBackForwardList>(WKPageGetBackForwardList(wkPage())))
#ifdef HAVE_ECORE_X
    , m_customCursor(ECORE_X_CURSOR_X)
#endif
    , m_userAgent(WKEinaSharedString(AdoptWK, WKPageCopyUserAgent(wkPage())))
    , m_applicationNameForUserAgent(WKEinaSharedString(AdoptWK, WKPageCopyApplicationNameForUserAgent(wkPage())))
    , m_mouseEventsEnabled(false)
#if ENABLE(TOUCH_EVENTS)
    , m_touchEventsEnabled(false)
    , m_gestureRecognizer(std::make_unique<GestureRecognizer>(this))
#endif
    , m_displayTimer(*this, &EwkView::displayTimerFired)
    , m_inputMethodContext(InputMethodContextEfl::create(this, smartData()->base.evas))
#if HAVE(ACCESSIBILITY) && defined(HAVE_ECORE_X)
    , m_webAccessibility(std::make_unique<WebAccessibility>(this))
#endif
    , m_pageViewportControllerClient(this)
    , m_pageViewportController(page(), m_pageViewportControllerClient)
    , m_isAccelerated(true)
    , m_isWaitingForNewPage(false)
{
    ASSERT(m_evasObject);
    ASSERT(m_context);

    // FIXME: Remove when possible.
    static_cast<WebViewEfl*>(webView())->setEwkView(this);

    // FIXME: Consider it to move into EvasGLContext.
    m_evasGL = evas_gl_new(evas_object_evas_get(m_evasObject));
    if (m_evasGL) {
        Evas_GL_Context* context = evas_gl_context_create(m_evasGL, 0);
        if (!context) {
            WARN("Failed to create Evas_GL_Context. EwkView is not created.");
            return;
        }

        m_evasGLContext = std::make_unique<EvasGLContext>(m_evasGL, context);
    }

    if (!m_evasGLContext) {
        WARN("Failed to create Evas_GL, falling back to software mode.");
        m_isAccelerated = false;
    }

    m_pendingSurfaceResize = m_isAccelerated;
    WKViewInitialize(wkView());

    m_pageUIClient = std::make_unique<PageUIClientEfl>(this);

    WKPageGroupRef wkPageGroup = WKPageGetPageGroup(wkPage());
    WKPreferencesRef wkPreferences = WKPageGroupGetPreferences(wkPageGroup);
    WKPreferencesSetWebGLEnabled(wkPreferences, true);
    WKPreferencesSetFullScreenEnabled(wkPreferences, true);
    WKPreferencesSetWebAudioEnabled(wkPreferences, true);
    WKPreferencesSetOfflineWebApplicationCacheEnabled(wkPreferences, true);
#if ENABLE(SPELLCHECK)
    WKPreferencesSetAsynchronousSpellCheckingEnabled(wkPreferences, true);
#endif
    WKPreferencesSetInteractiveFormValidationEnabled(wkPreferences, true);

    WKPageSetBackgroundExtendsBeyondPage(wkPage(), true);

    // Enable mouse events by default
    setMouseEventsEnabled(true);

    WKPageToEvasObjectMap::AddResult result = wkPageToEvasObjectMap().add(wkPage(), m_evasObject);
    ASSERT_UNUSED(result, result.isNewEntry);
}

EwkView::~EwkView()
{
    ASSERT(wkPageToEvasObjectMap().get(wkPage()) == m_evasObject);
    wkPageToEvasObjectMap().remove(wkPage());

    m_evasGLSurface = nullptr;
    m_evasGLContext = nullptr;

    if (m_evasGL)
        evas_gl_free(m_evasGL);
}

EwkView* EwkView::create(WKViewRef webView, Evas* canvas, Evas_Smart* smart)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(canvas, nullptr);

    Evas_Object* evasObject = evas_object_smart_add(canvas, smart ? smart : defaultSmartClassInstance());
    EINA_SAFETY_ON_NULL_RETURN_VAL(evasObject, nullptr);

    Ewk_View_Smart_Data* smartData = toSmartData(evasObject);
    if (!smartData) {
        evas_object_del(evasObject);
        return 0;
    }

    ASSERT(!smartData->priv);

    smartData->priv = new EwkView(webView, evasObject);

    return smartData->priv;
}

bool EwkView::initSmartClassInterface(Ewk_View_Smart_Class& api)
{
    if (api.version != EWK_VIEW_SMART_CLASS_VERSION) {
        EINA_LOG_CRIT("Ewk_View_Smart_Class %p is version %lu while %lu was expected.",
            &api, api.version, EWK_VIEW_SMART_CLASS_VERSION);
        return false;
    }

    if (!parentSmartClass.add)
        evas_object_smart_clipped_smart_set(&parentSmartClass);

    evas_object_smart_clipped_smart_set(&api.sc);

    // Set Evas_Smart_Class callbacks.
    api.sc.add = handleEvasObjectAdd;
    api.sc.del = handleEvasObjectDelete;
    api.sc.move = handleEvasObjectMove;
    api.sc.resize = handleEvasObjectResize;
    api.sc.show = handleEvasObjectShow;
    api.sc.hide = handleEvasObjectHide;
    api.sc.color_set = handleEvasObjectColorSet;
    api.sc.calculate = handleEvasObjectCalculate;
    api.sc.data = smartClassName; // It is used for type checking.

    // Set Ewk_View_Smart_Class callbacks.
    api.focus_in = handleEwkViewFocusIn;
    api.focus_out = handleEwkViewFocusOut;
    api.mouse_wheel = handleEwkViewMouseWheel;
    api.mouse_down = handleEwkViewMouseDown;
    api.mouse_up = handleEwkViewMouseUp;
    api.mouse_move = handleEwkViewMouseMove;
    api.key_down = handleEwkViewKeyDown;
    api.key_up = handleEwkViewKeyUp;

    return true;
}

Evas_Object* EwkView::toEvasObject(WKPageRef page)
{
    ASSERT(page);
    return wkPageToEvasObjectMap().get(page);
}

WKPageRef EwkView::wkPage() const
{
    return WKViewGetPage(wkView());
}

#ifdef HAVE_ECORE_X
void EwkView::updateCursor(Ecore_X_Window window)
{
    if (m_customCursor) {
        ecore_x_window_cursor_set(window, m_customCursor);
        return;
    }

    const char* group = static_cast<const char*>(m_cursorIdentifier.group);
    if (!group)
        return;

    applyCursorFromEcoreX(window, group);
}
#endif

void EwkView::setCursor(const Cursor& cursor)
{
#ifdef HAVE_ECORE_X
    Ecore_Evas* ecoreEvas = ecore_evas_ecore_evas_get(smartData()->base.evas);
    Ecore_X_Window window = getEcoreXWindow(ecoreEvas);

    if (Image* cursorImage = cursor.image()) {
        // Custom cursor.
        if (cursorImage == m_cursorIdentifier.image)
            return;

        IntPoint hotSpot;
        cursorImage->getHotSpot(hotSpot);

        Ecore_X_Cursor customCursor = createCustomCursor(window, cursorImage, IntSize(cursorImage->size()), hotSpot);
        if (!customCursor)
            return;

        if (m_customCursor)
            ecore_x_cursor_free(m_customCursor);

        m_cursorIdentifier.image = cursorImage;
        m_customCursor = customCursor;

    } else {
        if (m_customCursor) {
            ecore_x_cursor_free(m_customCursor);
            m_customCursor = ECORE_X_CURSOR_X;
        }

        // Standard cursor.
        const char* group = cursor.platformCursor();
        if (!group || group == m_cursorIdentifier.group)
            return;

        m_cursorIdentifier.group = group;
    }

    updateCursor(window);
#endif
}

void EwkView::setDeviceScaleFactor(float scale)
{
    const WKSize& deviceSize = WKViewGetSize(wkView());
    WKPageSetCustomBackingScaleFactor(wkPage(), scale);

    // Update internal viewport size after device-scale change.
    WKViewSetSize(wkView(), deviceSize);
}

float EwkView::deviceScaleFactor() const
{
    return WKPageGetBackingScaleFactor(wkPage());
}

AffineTransform EwkView::transformToScreen() const
{
    AffineTransform transform;

    int windowGlobalX = 0;
    int windowGlobalY = 0;

    Ewk_View_Smart_Data* sd = smartData();

#ifdef HAVE_ECORE_X
    Ecore_Evas* ecoreEvas = ecore_evas_ecore_evas_get(sd->base.evas);

    Ecore_X_Window window = getEcoreXWindow(ecoreEvas);

    int x, y; // x, y are relative to parent (in a reparenting window manager).
    while (window) {
        ecore_x_window_geometry_get(window, &x, &y, 0, 0);
        windowGlobalX += x;
        windowGlobalY += y;
        window = ecore_x_window_parent_get(window);
    }
#endif

    transform.translate(-sd->view.x, -sd->view.y);
    transform.translate(windowGlobalX, windowGlobalY);

    return transform;
}

inline Ewk_View_Smart_Data* EwkView::smartData() const
{
    return toSmartData(m_evasObject);
}

inline IntSize EwkView::size() const
{
    // WebPage expects a size in UI units, and not raw device units.
    FloatSize uiSize = deviceSize();
    uiSize.scale(1 / deviceScaleFactor());
    return roundedIntSize(uiSize);
}

inline IntSize EwkView::deviceSize() const
{
    return toIntSize(WKViewGetSize(wkView()));
}

void EwkView::displayTimerFired()
{
    Ewk_View_Smart_Data* sd = smartData();

    if (m_pendingSurfaceResize) {
        // Create a GL surface here so that Evas has no chance of painting to an empty GL surface.
        if (!createGLSurface())
            return;
        // Make Evas objects visible here in order not to paint empty Evas objects with black color.
        showEvasObjectsIfNeeded(sd);

        m_pendingSurfaceResize = false;
    }

    if (!m_isAccelerated) {
        RefPtr<cairo_surface_t> surface = createSurfaceForImage(sd->image);
        if (!surface)
            return;

        WKViewPaintToCairoSurface(wkView(), surface.get());
        evas_object_image_data_update_add(sd->image, 0, 0, sd->view.w, sd->view.h);
        return;
    }

    evas_gl_make_current(m_evasGL, m_evasGLSurface->surface(), m_evasGLContext->context());

    WKViewPaintToCurrentGLContext(wkView());

    // sd->image should be updated from (0,0) when using the evasGL for graphics backend.
    evas_object_image_data_update_add(sd->image, 0, 0, sd->view.w, sd->view.h);
}

void EwkView::scheduleUpdateDisplay()
{
    if (deviceSize().isEmpty())
        return;

    if (!m_displayTimer.isActive())
        m_displayTimer.startOneShot(0);
}

void EwkView::setViewportPosition(const FloatPoint& contentsPosition)
{
    WKViewSetContentPosition(wkView(), WKPointMake(contentsPosition.x(), contentsPosition.y()));
    m_pageViewportController.didChangeContentsVisibility(contentsPosition, m_pageViewportController.currentScale());
}

#if ENABLE(FULLSCREEN_API)
/**
 * @internal
 * Calls fullscreen_enter callback or falls back to default behavior and enables fullscreen mode.
 */
void EwkView::enterFullScreen()
{
    Ewk_View_Smart_Data* sd = smartData();

    RefPtr<EwkSecurityOrigin> origin = EwkSecurityOrigin::create(m_url);

    if (!sd->api->fullscreen_enter || !sd->api->fullscreen_enter(sd, origin.get())) {
        Ecore_Evas* ecoreEvas = ecore_evas_ecore_evas_get(sd->base.evas);
        ecore_evas_fullscreen_set(ecoreEvas, true);
    }
}

/**
 * @internal
 * Calls fullscreen_exit callback or falls back to default behavior and disables fullscreen mode.
 */
void EwkView::exitFullScreen()
{
    Ewk_View_Smart_Data* sd = smartData();

    if (!sd->api->fullscreen_exit || !sd->api->fullscreen_exit(sd)) {
        Ecore_Evas* ecoreEvas = ecore_evas_ecore_evas_get(sd->base.evas);
        ecore_evas_fullscreen_set(ecoreEvas, false);
    }
}
#endif

WKRect EwkView::windowGeometry() const
{
    Evas_Coord x, y, width, height;
    Ewk_View_Smart_Data* sd = smartData();

    if (!sd->api->window_geometry_get || !sd->api->window_geometry_get(sd, &x, &y, &width, &height)) {
        Ecore_Evas* ee = ecore_evas_ecore_evas_get(sd->base.evas);
        ecore_evas_request_geometry_get(ee, &x, &y, &width, &height);
    }

    return WKRectMake(x, y, width, height);
}

void EwkView::setWindowGeometry(const WKRect& rect)
{
    Ewk_View_Smart_Data* sd = smartData();

    if (!sd->api->window_geometry_set || !sd->api->window_geometry_set(sd, rect.origin.x, rect.origin.y, rect.size.width, rect.size.height)) {
        Ecore_Evas* ee = ecore_evas_ecore_evas_get(sd->base.evas);
        ecore_evas_move_resize(ee, rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);
    }
}

const char* EwkView::title() const
{
    m_title = WKEinaSharedString(AdoptWK, WKPageCopyTitle(wkPage()));

    return m_title;
}

/**
 * @internal
 * This function may return @c NULL.
 */
InputMethodContextEfl* EwkView::inputMethodContext()
{
    return m_inputMethodContext.get();
}

const char* EwkView::themePath() const
{
    return m_theme;
}

void EwkView::setThemePath(const char* theme)
{
    if (m_theme != theme) {
        m_theme = theme;
        WKRetainPtr<WKStringRef> wkTheme = adoptWK(WKStringCreateWithUTF8CString(theme));
        WKViewSetThemePath(wkView(), wkTheme.get());
    }
}

void EwkView::setCustomTextEncodingName(const char* customEncoding)
{
    if (m_customEncoding == customEncoding)
        return;

    m_customEncoding = customEncoding;
    WKRetainPtr<WKStringRef> wkCustomEncoding = adoptWK(WKStringCreateWithUTF8CString(customEncoding));
    WKPageSetCustomTextEncodingName(wkPage(), wkCustomEncoding.get());
}

void EwkView::setUserAgent(const char* userAgent)
{
    if (m_userAgent == userAgent)
        return;

    WKRetainPtr<WKStringRef> wkUserAgent = adoptWK(WKStringCreateWithUTF8CString(userAgent));
    WKPageSetCustomUserAgent(wkPage(), wkUserAgent.get());

    // When 'userAgent' is 0, user agent is set as a standard user agent by WKPageSetCustomUserAgent()
    // so m_userAgent needs to be updated using WKPageCopyUserAgent().
    m_userAgent = WKEinaSharedString(AdoptWK, WKPageCopyUserAgent(wkPage()));
}

void EwkView::setApplicationNameForUserAgent(const char* applicationNameForUserAgent)
{
    if (m_applicationNameForUserAgent == applicationNameForUserAgent)
        return;

    m_applicationNameForUserAgent = applicationNameForUserAgent;

    WKRetainPtr<WKStringRef> wkApplicationName = adoptWK(WKStringCreateWithUTF8CString(applicationNameForUserAgent));
    WKPageSetApplicationNameForUserAgent(wkPage(), wkApplicationName.get());

    // WKPageSetApplicationNameForUserAgent also changes user agent.
    m_userAgent = WKEinaSharedString(AdoptWK, WKPageCopyUserAgent(wkPage()));
}

void EwkView::setMouseEventsEnabled(bool enabled)
{
    if (m_mouseEventsEnabled == enabled)
        return;

    m_mouseEventsEnabled = enabled;
    if (enabled) {
        EwkViewEventHandler<EVAS_CALLBACK_MOUSE_DOWN>::subscribe(m_evasObject);
        EwkViewEventHandler<EVAS_CALLBACK_MOUSE_UP>::subscribe(m_evasObject);
        EwkViewEventHandler<EVAS_CALLBACK_MOUSE_MOVE>::subscribe(m_evasObject);
    } else {
        EwkViewEventHandler<EVAS_CALLBACK_MOUSE_DOWN>::unsubscribe(m_evasObject);
        EwkViewEventHandler<EVAS_CALLBACK_MOUSE_UP>::unsubscribe(m_evasObject);
        EwkViewEventHandler<EVAS_CALLBACK_MOUSE_MOVE>::unsubscribe(m_evasObject);
    }
}

#if ENABLE(TOUCH_EVENTS)
static WKTouchPointState toWKTouchPointState(Evas_Touch_Point_State state)
{
    switch (state) {
    case EVAS_TOUCH_POINT_UP:
        return kWKTouchPointStateTouchReleased;
    case EVAS_TOUCH_POINT_MOVE:
        return kWKTouchPointStateTouchMoved;
    case EVAS_TOUCH_POINT_DOWN:
        return kWKTouchPointStateTouchPressed;
    case EVAS_TOUCH_POINT_STILL:
        return kWKTouchPointStateTouchStationary;
    case EVAS_TOUCH_POINT_CANCEL:
    default:
        return kWKTouchPointStateTouchCancelled;
    }
}

static WKEventModifiers toWKEventModifiers(const Evas_Modifier* modifiers)
{
    WKEventModifiers wkModifiers = 0;
    if (evas_key_modifier_is_set(modifiers, "Shift"))
        wkModifiers |= kWKEventModifiersShiftKey;
    if (evas_key_modifier_is_set(modifiers, "Control"))
        wkModifiers |= kWKEventModifiersControlKey;
    if (evas_key_modifier_is_set(modifiers, "Alt"))
        wkModifiers |= kWKEventModifiersAltKey;
    if (evas_key_modifier_is_set(modifiers, "Meta"))
        wkModifiers |= kWKEventModifiersMetaKey;

    return wkModifiers;
}

void EwkView::feedTouchEvent(Ewk_Touch_Event_Type type, const Eina_List* points, const Evas_Modifier* modifiers)
{
    unsigned length = eina_list_count(points);
    auto touchPoints = std::make_unique<WKTypeRef[]>(length);
    for (unsigned i = 0; i < length; ++i) {
        Ewk_Touch_Point* point = static_cast<Ewk_Touch_Point*>(eina_list_nth(points, i));
        ASSERT(point);
        IntPoint position(point->x, point->y);
        touchPoints[i] = WKTouchPointCreate(point->id, toAPI(IntPoint(position)), toAPI(transformToScreen().mapPoint(position)), toWKTouchPointState(point->state), WKSizeMake(0, 0), 0, 1);
    }
    WKRetainPtr<WKArrayRef> wkTouchPoints(AdoptWK, WKArrayCreateAdoptingValues(touchPoints.get(), length));

    WKViewSendTouchEvent(wkView(), adoptWK(WKTouchEventCreate(static_cast<WKEventType>(type), wkTouchPoints.get(), toWKEventModifiers(modifiers), ecore_time_get())).get());
}

void EwkView::setTouchEventsEnabled(bool enabled)
{
    if (m_touchEventsEnabled == enabled)
        return;

    m_touchEventsEnabled = enabled;

    if (enabled) {
        // FIXME: We have to connect touch callbacks with mouse and multi events
        // because the Evas creates mouse events for first touch and multi events
        // for second and third touches. Below codes should be fixed when the Evas
        // supports the touch events.
        // See https://bugs.webkit.org/show_bug.cgi?id=97785 for details.
        Ewk_View_Smart_Data* sd = smartData();
        evas_object_event_callback_add(m_evasObject, EVAS_CALLBACK_MOUSE_DOWN, handleMouseDownForTouch, sd);
        evas_object_event_callback_add(m_evasObject, EVAS_CALLBACK_MOUSE_UP, handleMouseUpForTouch, sd);
        evas_object_event_callback_add(m_evasObject, EVAS_CALLBACK_MOUSE_MOVE, handleMouseMoveForTouch, sd);
        evas_object_event_callback_add(m_evasObject, EVAS_CALLBACK_MULTI_DOWN, handleMultiDownForTouch, sd);
        evas_object_event_callback_add(m_evasObject, EVAS_CALLBACK_MULTI_UP, handleMultiUpForTouch, sd);
        evas_object_event_callback_add(m_evasObject, EVAS_CALLBACK_MULTI_MOVE, handleMultiMoveForTouch, sd);
    } else {
        evas_object_event_callback_del(m_evasObject, EVAS_CALLBACK_MOUSE_DOWN, handleMouseDownForTouch);
        evas_object_event_callback_del(m_evasObject, EVAS_CALLBACK_MOUSE_UP, handleMouseUpForTouch);
        evas_object_event_callback_del(m_evasObject, EVAS_CALLBACK_MOUSE_MOVE, handleMouseMoveForTouch);
        evas_object_event_callback_del(m_evasObject, EVAS_CALLBACK_MULTI_DOWN, handleMultiDownForTouch);
        evas_object_event_callback_del(m_evasObject, EVAS_CALLBACK_MULTI_UP, handleMultiUpForTouch);
        evas_object_event_callback_del(m_evasObject, EVAS_CALLBACK_MULTI_MOVE, handleMultiMoveForTouch);
    }
}

void EwkView::doneWithTouchEvent(WKTouchEventRef event, bool wasEventHandled)
{
    if (wasEventHandled) {
        m_gestureRecognizer->reset();
        return;
    }

    m_gestureRecognizer->processTouchEvent(event);
}
#endif

bool EwkView::createGLSurface()
{
    if (!m_isAccelerated)
        return true;

    static Evas_GL_Config evasGLConfig = {
        EVAS_GL_RGBA_8888,
        EVAS_GL_DEPTH_BIT_8,
        EVAS_GL_STENCIL_NONE,
        EVAS_GL_OPTIONS_NONE,
        EVAS_GL_MULTISAMPLE_NONE,
#if defined(EVAS_GL_API_VERSION) && EVAS_GL_API_VERSION >= 2
        EVAS_GL_GLES_2_X
#endif
    };

    // Recreate to current size: Replaces if non-null, and frees existing surface after (OwnPtr).
    if (deviceSize().width() && deviceSize().height()) {
        Evas_GL_Surface* surface = nullptr;
        surface = evas_gl_surface_create(m_evasGL, &evasGLConfig, deviceSize().width(), deviceSize().height());
        if (!surface)
            return false;

        m_evasGLSurface = std::make_unique<EvasGLSurface>(m_evasGL, surface);
    }

    if (!m_evasGLSurface)
        return false;

    Evas_Native_Surface nativeSurface;
    evas_gl_native_surface_get(m_evasGL, m_evasGLSurface->surface(), &nativeSurface);
    evas_object_image_native_surface_set(smartData()->image, &nativeSurface);

    evas_gl_make_current(m_evasGL, m_evasGLSurface->surface(), m_evasGLContext->context());

    Evas_GL_API* gl = evas_gl_api_get(m_evasGL);

    WKPoint boundsEnd = WKViewUserViewportToScene(wkView(), WKPointMake(deviceSize().width(), deviceSize().height()));
    gl->glViewport(0, 0, boundsEnd.x, boundsEnd.y);
    gl->glClearColor(1.0, 1.0, 1.0, 0);
    gl->glClear(GL_COLOR_BUFFER_BIT);

    return true;
}

#if ENABLE(INPUT_TYPE_COLOR)
/**
 * @internal
 * Requests to show external color picker.
 */
void EwkView::requestColorPicker(WKColorPickerResultListenerRef listener, const WebCore::Color& color)
{
    Ewk_View_Smart_Data* sd = smartData();
    EINA_SAFETY_ON_NULL_RETURN(sd->api->input_picker_color_request);

    if (!sd->api->input_picker_color_request)
        return;

    if (m_colorPicker)
        dismissColorPicker();

    m_colorPicker = std::make_unique<EwkColorPicker>(listener, color);

    sd->api->input_picker_color_request(sd, m_colorPicker.get());
}

/**
 * @internal
 * Requests to hide external color picker.
 */
void EwkView::dismissColorPicker()
{
    if (!m_colorPicker)
        return;

    Ewk_View_Smart_Data* sd = smartData();
    EINA_SAFETY_ON_NULL_RETURN(sd->api->input_picker_color_dismiss);

    if (sd->api->input_picker_color_dismiss)
        sd->api->input_picker_color_dismiss(sd);

    m_colorPicker = nullptr;
}
#endif

void EwkView::customContextMenuItemSelected(WKContextMenuItemRef contextMenuItem)
{
    Ewk_View_Smart_Data* sd = smartData();
    ASSERT(sd->api);

    if (!sd->api->custom_item_selected)
        return;

    std::unique_ptr<EwkContextMenuItem> item = std::make_unique<EwkContextMenuItem>(contextMenuItem, nullptr);

    sd->api->custom_item_selected(sd, item.get());
}

void EwkView::showContextMenu(WKPoint position, WKArrayRef items)
{
    Ewk_View_Smart_Data* sd = smartData();
    ASSERT(sd->api);

    if (!sd->api->context_menu_show)
        return;

    if (m_contextMenu)
        hideContextMenu();

    m_contextMenu = EwkContextMenu::create(this, items);

    position = WKViewContentsToUserViewport(wkView(), position);

    sd->api->context_menu_show(sd, position.x, position.y, m_contextMenu.get());
}

void EwkView::hideContextMenu()
{
    if (!m_contextMenu)
        return;

    Ewk_View_Smart_Data* sd = smartData();
    ASSERT(sd->api);

    if (sd->api->context_menu_hide)
        sd->api->context_menu_hide(sd);

    m_contextMenu = nullptr;
}

void EwkView::requestPopupMenu(WKPopupMenuListenerRef popupMenuListener, const WKRect& rect, WKPopupItemTextDirection textDirection, double pageScaleFactor, WKArrayRef items, int32_t selectedIndex)
{
    Ewk_View_Smart_Data* sd = smartData();
    ASSERT(sd->api);

    ASSERT(popupMenuListener);

    if (!sd->api->popup_menu_show)
        return;

    if (m_popupMenu)
        closePopupMenu();

    m_popupMenu = std::make_unique<EwkPopupMenu>(this, popupMenuListener, items, selectedIndex);

    WKPoint popupMenuPosition = WKViewContentsToUserViewport(wkView(), rect.origin);

    Eina_Rectangle einaRect;
    EINA_RECTANGLE_SET(&einaRect, popupMenuPosition.x, popupMenuPosition.y, rect.size.width, rect.size.height);

    switch (textDirection) {
    case kWKPopupItemTextDirectionRTL:
        sd->api->popup_menu_show(sd, einaRect, EWK_TEXT_DIRECTION_RIGHT_TO_LEFT, pageScaleFactor, m_popupMenu.get());
        break;
    case EWK_TEXT_DIRECTION_LEFT_TO_RIGHT:
        sd->api->popup_menu_show(sd, einaRect, EWK_TEXT_DIRECTION_LEFT_TO_RIGHT, pageScaleFactor, m_popupMenu.get());
        break;
    }
}

void EwkView::closePopupMenu()
{
    if (!m_popupMenu)
        return;

    Ewk_View_Smart_Data* sd = smartData();
    ASSERT(sd->api);

    if (sd->api->popup_menu_hide)
        sd->api->popup_menu_hide(sd);

    m_popupMenu = nullptr;
}

/**
 * @internal
 * Calls a smart member function for javascript alert().
 */
void EwkView::requestJSAlertPopup(const WKEinaSharedString& message)
{
    Ewk_View_Smart_Data* sd = smartData();
    ASSERT(sd->api);

    if (!sd->api->run_javascript_alert)
        return;

    sd->api->run_javascript_alert(sd, message);
}

/**
 * @internal
 * Calls a smart member function for javascript confirm() and returns a value from the function. Returns false by default.
 */
bool EwkView::requestJSConfirmPopup(const WKEinaSharedString& message)
{
    Ewk_View_Smart_Data* sd = smartData();
    ASSERT(sd->api);

    if (!sd->api->run_javascript_confirm)
        return false;

    return sd->api->run_javascript_confirm(sd, message);
}

/**
 * @internal
 * Calls a smart member function for javascript prompt() and returns a value from the function. Returns null string by default.
 */
WKEinaSharedString EwkView::requestJSPromptPopup(const WKEinaSharedString& message, const WKEinaSharedString& defaultValue)
{
    Ewk_View_Smart_Data* sd = smartData();
    ASSERT(sd->api);

    if (!sd->api->run_javascript_prompt)
        return WKEinaSharedString();

    return WKEinaSharedString::adopt(sd->api->run_javascript_prompt(sd, message, defaultValue));
}

/**
 * @internal
 * Calls exceeded_database_quota callback or falls back to default behavior returns default database quota.
 */
unsigned long long EwkView::informDatabaseQuotaReached(const String& databaseName, const String& displayName, unsigned long long currentQuota, unsigned long long currentOriginUsage, unsigned long long currentDatabaseUsage, unsigned long long expectedUsage)
{
    Ewk_View_Smart_Data* sd = smartData();
    ASSERT(sd->api);

    static const unsigned long long defaultQuota = 5 * 1024 * 1204; // 5 MB
    if (sd->api->exceeded_database_quota)
        return sd->api->exceeded_database_quota(sd, databaseName.utf8().data(), displayName.utf8().data(), currentQuota, currentOriginUsage, currentDatabaseUsage, expectedUsage);

    return defaultQuota;
}

WebView* EwkView::webView()
{
    return toImpl(m_webView.get());
}

/**
 * @internal
 * The url of view was changed by the frame loader.
 *
 * Emits signal: "url,changed" with pointer to new url string.
 */
void EwkView::informURLChange()
{
    WKRetainPtr<WKURLRef> wkActiveURL = adoptWK(WKPageCopyActiveURL(wkPage()));
    WKRetainPtr<WKStringRef> wkURLString = wkActiveURL ? adoptWK(WKURLCopyString(wkActiveURL.get())) : adoptWK(WKStringCreateWithUTF8CString(""));

    if (WKStringIsEqualToUTF8CString(wkURLString.get(), m_url))
        return;

    m_url = WKEinaSharedString(wkURLString.get());
    smartCallback<URLChanged>().call(m_url);
}

/**
 * @internal
 * Update new scale factor to PageViewportController.
 *
 * ewk_view_scale_set() had only updated a scale factor of WebPageProxy. It had caused unsynchronized scale factor
 * between WebPageProxy and PageViewportController. To be sync between WebPageProxy and PageViewportController,
 * ewk_view_scale_set() needs to update the scale factor in PageViewportController as well.
 */
void EwkView::updateScaleToPageViewportController(double scaleFactor, int x, int y)
{
    m_pageViewportController.setInitiallyFitToViewport(false);
    m_pageViewportController.didChangeContentsVisibility(WebCore::FloatPoint(x, y), scaleFactor);
}

EwkWindowFeatures* EwkView::windowFeatures()
{
    if (!m_windowFeatures)
        m_windowFeatures = EwkWindowFeatures::create(0, this);

    return m_windowFeatures.get();
}

WKPageRef EwkView::createNewPage(PassRefPtr<EwkUrlRequest>, WKDictionaryRef windowFeatures)
{
    Ewk_View_Smart_Data* sd = smartData();
    ASSERT(sd->api);

    if (!sd->api->window_create)
        return 0;

    RefPtr<EwkWindowFeatures> ewkWindowFeatures = EwkWindowFeatures::create(windowFeatures, this);

    Evas_Object* newEwkView = sd->api->window_create(sd, ewkWindowFeatures.get());
    if (!newEwkView)
        return 0;

    EwkView* newViewImpl = toEwkView(newEwkView);
    ASSERT(newViewImpl);

    newViewImpl->m_windowFeatures = ewkWindowFeatures;

    return static_cast<WKPageRef>(WKRetain(newViewImpl->page()));
}

void EwkView::close()
{
    Ewk_View_Smart_Data* sd = smartData();
    ASSERT(sd->api);

    if (!sd->api->window_close)
        return;

    sd->api->window_close(sd);
}

void EwkView::handleEvasObjectAdd(Evas_Object* evasObject)
{
    const Evas_Smart* smart = evas_object_smart_smart_get(evasObject);
    const Evas_Smart_Class* smartClass = evas_smart_class_get(smart);
    const Ewk_View_Smart_Class* api = reinterpret_cast<const Ewk_View_Smart_Class*>(smartClass);
    ASSERT(api);

    Ewk_View_Smart_Data* smartData = toSmartData(evasObject);

    if (!smartData) {
        // Allocating with 'calloc' as the API contract is that it should be deleted with 'free()'.
        smartData = static_cast<Ewk_View_Smart_Data*>(calloc(1, sizeof(Ewk_View_Smart_Data)));
        evas_object_smart_data_set(evasObject, smartData);
    }

    smartData->self = evasObject;
    smartData->api = api;

    parentSmartClass.add(evasObject);

    smartData->priv = 0; // Will be initialized further.

    // Create evas_object_image to draw web contents.
    smartData->image = evas_object_image_add(smartData->base.evas);
    evas_object_image_alpha_set(smartData->image, false);
    evas_object_image_filled_set(smartData->image, true);
    evas_object_smart_member_add(smartData->image, evasObject);
    evas_object_show(smartData->image);

    EwkViewEventHandler<EVAS_CALLBACK_FOCUS_IN>::subscribe(evasObject);
    EwkViewEventHandler<EVAS_CALLBACK_FOCUS_OUT>::subscribe(evasObject);
    EwkViewEventHandler<EVAS_CALLBACK_MOUSE_IN>::subscribe(evasObject);
    EwkViewEventHandler<EVAS_CALLBACK_MOUSE_WHEEL>::subscribe(evasObject);
    EwkViewEventHandler<EVAS_CALLBACK_KEY_DOWN>::subscribe(evasObject);
    EwkViewEventHandler<EVAS_CALLBACK_KEY_UP>::subscribe(evasObject);
    EwkViewEventHandler<EVAS_CALLBACK_SHOW>::subscribe(evasObject);
    EwkViewEventHandler<EVAS_CALLBACK_HIDE>::subscribe(evasObject);
}

void EwkView::handleEvasObjectDelete(Evas_Object* evasObject)
{
    Ewk_View_Smart_Data* smartData = toSmartData(evasObject);
    if (smartData) {
        ASSERT(smartData->priv); // smartData->priv is EwkView instance.
        delete smartData->priv;
    }

    parentSmartClass.del(evasObject);
}

void EwkView::handleEvasObjectResize(Evas_Object* evasObject, Evas_Coord width, Evas_Coord height)
{
    Ewk_View_Smart_Data* smartData = toSmartData(evasObject);
    ASSERT(smartData);

    evas_object_resize(smartData->image, width, height);
    evas_object_image_size_set(smartData->image, width, height);
    evas_object_image_fill_set(smartData->image, 0, 0, width, height);

    smartData->changed.size = true;
    smartDataChanged(smartData);
}

void EwkView::handleEvasObjectMove(Evas_Object* evasObject, Evas_Coord /*x*/, Evas_Coord /*y*/)
{
    Ewk_View_Smart_Data* smartData = toSmartData(evasObject);
    ASSERT(smartData);

    smartData->changed.position = true;
    smartDataChanged(smartData);
}

void EwkView::handleEvasObjectCalculate(Evas_Object* evasObject)
{
    Ewk_View_Smart_Data* smartData = toSmartData(evasObject);
    ASSERT(smartData);

    EwkView* self = toEwkView(smartData);

    smartData->changed.any = false;

    Evas_Coord x, y, width, height;
    evas_object_geometry_get(evasObject, &x, &y, &width, &height);

    if (smartData->changed.position) {
        smartData->changed.position = false;
        smartData->view.x = x;
        smartData->view.y = y;
        evas_object_move(smartData->image, x, y);
        WKViewSetUserViewportTranslation(self->wkView(), x, y);
    }

    if (smartData->changed.size) {
        smartData->changed.size = false;
        smartData->view.w = width;
        smartData->view.h = height;

        WKViewSetSize(self->wkView(), WKSizeMake(width, height));
        if (WKPageUseFixedLayout(self->wkPage()))
            self->pageViewportController().didChangeViewportSize(self->size());

        self->setNeedsSurfaceResize();
    }
}

void EwkView::handleEvasObjectShow(Evas_Object* evasObject)
{
    Ewk_View_Smart_Data* smartData = toSmartData(evasObject);
    ASSERT(smartData);

    if (!toEwkView(smartData)->m_pendingSurfaceResize)
        showEvasObjectsIfNeeded(smartData);
}

void EwkView::handleEvasObjectHide(Evas_Object* evasObject)
{
    Ewk_View_Smart_Data* smartData = toSmartData(evasObject);
    ASSERT(smartData);

    evas_object_hide(smartData->base.clipper);
    evas_object_hide(smartData->image);
}

void EwkView::handleEvasObjectColorSet(Evas_Object* evasObject, int red, int green, int blue, int alpha)
{
    Ewk_View_Smart_Data* smartData = toSmartData(evasObject);
    ASSERT(smartData);

    int backgroundAlpha;
    WKViewGetBackgroundColor(toEwkView(smartData)->wkView(), nullptr, nullptr, nullptr, &backgroundAlpha);
    evas_object_image_alpha_set(smartData->image, alpha < 255 || backgroundAlpha < 255);
    parentSmartClass.color_set(evasObject, red, green, blue, alpha);
}

Eina_Bool EwkView::handleEwkViewFocusIn(Ewk_View_Smart_Data* smartData)
{
    WKViewSetIsFocused(toEwkView(smartData)->wkView(), true);
    return true;
}

Eina_Bool EwkView::handleEwkViewFocusOut(Ewk_View_Smart_Data* smartData)
{
    WKViewSetIsFocused(toEwkView(smartData)->wkView(), false);
    return true;
}

Eina_Bool EwkView::handleEwkViewMouseWheel(Ewk_View_Smart_Data* smartData, const Evas_Event_Mouse_Wheel* wheelEvent)
{
    EwkView* self = toEwkView(smartData);
    self->page()->handleWheelEvent(NativeWebWheelEvent(wheelEvent, self->webView()->transformFromScene(), self->transformToScreen()));
    return true;
}

Eina_Bool EwkView::handleEwkViewMouseDown(Ewk_View_Smart_Data* smartData, const Evas_Event_Mouse_Down* downEvent)
{
    EwkView* self = toEwkView(smartData);
    self->page()->handleMouseEvent(NativeWebMouseEvent(downEvent, self->webView()->transformFromScene(), self->transformToScreen()));
    return true;
}

Eina_Bool EwkView::handleEwkViewMouseUp(Ewk_View_Smart_Data* smartData, const Evas_Event_Mouse_Up* upEvent)
{
    EwkView* self = toEwkView(smartData);
    self->page()->handleMouseEvent(NativeWebMouseEvent(upEvent, self->webView()->transformFromScene(), self->transformToScreen()));

    if (InputMethodContextEfl* inputMethodContext = self->inputMethodContext())
        inputMethodContext->handleMouseUpEvent(upEvent);

    return true;
}

Eina_Bool EwkView::handleEwkViewMouseMove(Ewk_View_Smart_Data* smartData, const Evas_Event_Mouse_Move* moveEvent)
{
    EwkView* self = toEwkView(smartData);
    self->page()->handleMouseEvent(NativeWebMouseEvent(moveEvent, self->webView()->transformFromScene(), self->transformToScreen()));
    return true;
}

Eina_Bool EwkView::handleEwkViewKeyDown(Ewk_View_Smart_Data* smartData, const Evas_Event_Key_Down* downEvent)
{
    bool isFiltered = false;
    EwkView* self = toEwkView(smartData);
    if (InputMethodContextEfl* inputMethodContext = self->inputMethodContext())
        inputMethodContext->handleKeyDownEvent(downEvent, &isFiltered);

    self->page()->handleKeyboardEvent(NativeWebKeyboardEvent(downEvent, isFiltered));
    return true;
}

Eina_Bool EwkView::handleEwkViewKeyUp(Ewk_View_Smart_Data* smartData, const Evas_Event_Key_Up* upEvent)
{
    toEwkView(smartData)->page()->handleKeyboardEvent(NativeWebKeyboardEvent(upEvent));
    return true;
}

#if ENABLE(TOUCH_EVENTS)
void EwkView::feedTouchEvents(Ewk_Touch_Event_Type type, double timestamp)
{
    Ewk_View_Smart_Data* sd = smartData();

    unsigned length = evas_touch_point_list_count(sd->base.evas);
    if (!length)
        return;

    auto touchPoints = std::make_unique<WKTypeRef[]>(length);
    for (unsigned i = 0; i < length; ++i) {
        int x, y;
        evas_touch_point_list_nth_xy_get(sd->base.evas, i, &x, &y);
        IntPoint position(x, y);
        Evas_Touch_Point_State state = evas_touch_point_list_nth_state_get(sd->base.evas, i);
        int id = evas_touch_point_list_nth_id_get(sd->base.evas, i);
        touchPoints[i] = WKTouchPointCreate(id, toAPI(IntPoint(position)), toAPI(transformToScreen().mapPoint(position)), toWKTouchPointState(state), WKSizeMake(0, 0), 0, 1);
    }
    WKRetainPtr<WKArrayRef> wkTouchPoints(AdoptWK, WKArrayCreateAdoptingValues(touchPoints.get(), length));

    WKViewSendTouchEvent(wkView(), adoptWK(WKTouchEventCreate(static_cast<WKEventType>(type), wkTouchPoints.get(), toWKEventModifiers(evas_key_modifier_get(sd->base.evas)), timestamp)).get());
}

void EwkView::handleMouseDownForTouch(void*, Evas*, Evas_Object* ewkView, void* eventInfo)
{
    toEwkView(ewkView)->feedTouchEvents(EWK_TOUCH_START, static_cast<Evas_Event_Mouse_Down*>(eventInfo)->timestamp / 1000.0);
}

void EwkView::handleMouseUpForTouch(void*, Evas*, Evas_Object* ewkView, void* eventInfo)
{
    toEwkView(ewkView)->feedTouchEvents(EWK_TOUCH_END, static_cast<Evas_Event_Mouse_Up*>(eventInfo)->timestamp / 1000.0);
}

void EwkView::handleMouseMoveForTouch(void*, Evas*, Evas_Object* ewkView, void* eventInfo)
{
    toEwkView(ewkView)->feedTouchEvents(EWK_TOUCH_MOVE, static_cast<Evas_Event_Mouse_Move*>(eventInfo)->timestamp / 1000.0);
}

void EwkView::handleMultiDownForTouch(void*, Evas*, Evas_Object* ewkView, void* eventInfo)
{
    toEwkView(ewkView)->feedTouchEvents(EWK_TOUCH_START, static_cast<Evas_Event_Multi_Down*>(eventInfo)->timestamp / 1000.0);
}

void EwkView::handleMultiUpForTouch(void*, Evas*, Evas_Object* ewkView, void* eventInfo)
{
    toEwkView(ewkView)->feedTouchEvents(EWK_TOUCH_END, static_cast<Evas_Event_Multi_Up*>(eventInfo)->timestamp / 1000.0);
}

void EwkView::handleMultiMoveForTouch(void*, Evas*, Evas_Object* ewkView, void* eventInfo)
{
    toEwkView(ewkView)->feedTouchEvents(EWK_TOUCH_MOVE, static_cast<Evas_Event_Multi_Move*>(eventInfo)->timestamp / 1000.0);
}
#endif

void EwkView::handleFaviconChanged(const char* pageURL, void* eventInfo)
{
    EwkView* view = static_cast<EwkView*>(eventInfo);

    if (!view->url() || strcasecmp(view->url(), pageURL))
        return;
}

PassRefPtr<cairo_surface_t> EwkView::takeSnapshot()
{
    // Suspend all animations before taking the snapshot.
    WKViewSuspendActiveDOMObjectsAndAnimations(wkView());

    // Wait for the pending repaint events to be processed.
    while (m_displayTimer.isActive())
        ecore_main_loop_iterate();

    Ewk_View_Smart_Data* sd = smartData();
    if (m_isAccelerated) {
        RefPtr<cairo_surface_t> snapshot = getImageSurfaceFromFrameBuffer(0, 0, sd->view.w, sd->view.h);
        // Resume all animations.
        WKViewResumeActiveDOMObjectsAndAnimations(wkView());

        return snapshot.release();
    }

    RefPtr<cairo_surface_t> snapshot = createSurfaceForImage(sd->image);
    // Resume all animations.
    WKViewResumeActiveDOMObjectsAndAnimations(wkView());

    return snapshot.release();
}

void EwkView::didFindZoomableArea(const WKPoint& point, const WKRect& area)
{
    notImplemented();
    UNUSED_PARAM(point);
    UNUSED_PARAM(area);
}

bool EwkView::scrollBy(const IntSize& offset)
{
    WKPoint oldPosition = WKViewGetContentPosition(wkView());
    float contentScale = WKViewGetContentScaleFactor(wkView());

    float effectiveScale = contentScale * deviceScaleFactor();
    FloatPoint newPosition(oldPosition.x + offset.width() / effectiveScale, oldPosition.y + offset.height() / effectiveScale);

    // Update new position to the PageViewportController.
    newPosition = m_pageViewportController.boundContentsPositionAtScale(newPosition, contentScale);
    m_pageViewportController.didChangeContentsVisibility(newPosition, contentScale);

    // Update new position to the WKView.
    WKPoint position = WKPointMake(newPosition.x(), newPosition.y());
    WKViewSetContentPosition(wkView(), position);

    // If the page position has not changed, notify the caller using the return value.
    return !(oldPosition == position);
}

void EwkView::setBackgroundColor(int red, int green, int blue, int alpha)
{
    if (red == 255 && green == 255 && blue == 255 && alpha == 255) {
        WKViewSetDrawsBackground(wkView(), true);
        WKPageSetBackgroundExtendsBeyondPage(wkPage(), true);
    } else {
        WKViewSetDrawsBackground(wkView(), false);
        WKPageSetBackgroundExtendsBeyondPage(wkPage(), false);
    }

    int objectAlpha;
    Evas_Object* image = smartData()->image;
    evas_object_color_get(image, nullptr, nullptr, nullptr, &objectAlpha);
    evas_object_image_alpha_set(image, alpha < 255 || objectAlpha < 255);

    WKViewSetBackgroundColor(wkView(), red, green, blue, alpha);
}

Evas_Smart_Class EwkView::parentSmartClass = EVAS_SMART_CLASS_INIT_NULL;

