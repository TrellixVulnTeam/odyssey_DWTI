add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBKIT_DIR}/WebKitVersion.h
    MAIN_DEPENDENCY ${WEBKIT_DIR}/scripts/generate-webkitversion.pl
    DEPENDS ${WEBKIT_DIR}/mac/Configurations/Version.xcconfig
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT_DIR}/scripts/generate-webkitversion.pl --config ${WEBKIT_DIR}/mac/Configurations/Version.xcconfig --outputDir ${DERIVED_SOURCES_WEBKIT_DIR}
    VERBATIM)
list(APPEND WebKit_SOURCES ${DERIVED_SOURCES_WEBKIT_DIR}/WebKitVersion.h)

list(APPEND WebKit_INCLUDE_DIRECTORIES
    "${CMAKE_SOURCE_DIR}/Source"
    "${JAVASCRIPTCORE_DIR}/ForwardingHeaders"
    "${WEBKIT_DIR}"
    "${WEBCORE_DIR}"
    "${WEBCORE_DIR}/Modules/webdatabase"
    "${WEBCORE_DIR}/accessibility"
    "${WEBCORE_DIR}/bindings/js"
    "${WEBCORE_DIR}/bridge"
    "${WEBCORE_DIR}/bridge/c"
    "${WEBCORE_DIR}/bridge/jsc"
    "${WEBCORE_DIR}/css"
    "${WEBCORE_DIR}/dom"
    "${WEBCORE_DIR}/editing"
    "${WEBCORE_DIR}/history"
    "${WEBCORE_DIR}/html"
    "${WEBCORE_DIR}/html/forms"
    "${WEBCORE_DIR}/html/track"
    "${WEBCORE_DIR}/inspector"
    "${WEBCORE_DIR}/loader"
    "${WEBCORE_DIR}/loader/appcache"
    "${WEBCORE_DIR}/loader/cache"
    "${WEBCORE_DIR}/loader/icon"
    "${WEBCORE_DIR}/page"
    "${WEBCORE_DIR}/page/animation"
    "${WEBCORE_DIR}/page/scrolling"
    "${WEBCORE_DIR}/platform"
    "${WEBCORE_DIR}/platform/animation"
    "${WEBCORE_DIR}/platform/audio"
    "${WEBCORE_DIR}/platform/graphics"
    "${WEBCORE_DIR}/platform/graphics/filters"
    "${WEBCORE_DIR}/platform/graphics/transforms"
    "${WEBCORE_DIR}/platform/network"
    "${WEBCORE_DIR}/platform/network/curl"
    "${WEBCORE_DIR}/platform/sql"
    "${WEBCORE_DIR}/platform/text"
    "${WEBCORE_DIR}/plugins"
    "${WEBCORE_DIR}/rendering"
    "${WEBCORE_DIR}/rendering/line"
    "${WEBCORE_DIR}/rendering/style"
    "${WEBCORE_DIR}/storage"
    "${WEBCORE_DIR}/style"
    "${WEBCORE_DIR}/svg"
    "${WEBCORE_DIR}/svg/properties"
    "${JAVASCRIPTCORE_DIR}"
    "${JAVASCRIPTCORE_DIR}/API"
    "${JAVASCRIPTCORE_DIR}/assembler"
    "${JAVASCRIPTCORE_DIR}/disassembler"
    "${JAVASCRIPTCORE_DIR}/bytecode"
    "${JAVASCRIPTCORE_DIR}/dfg"
    "${JAVASCRIPTCORE_DIR}/heap"
    "${JAVASCRIPTCORE_DIR}/interpreter"
    "${JAVASCRIPTCORE_DIR}/jit"
    "${JAVASCRIPTCORE_DIR}/llint"
    "${JAVASCRIPTCORE_DIR}/parser"
    "${JAVASCRIPTCORE_DIR}/profiler"
    "${JAVASCRIPTCORE_DIR}/runtime"
    "${WTF_DIR}"
    "${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}"
    "${DERIVED_SOURCES_WEBCORE_DIR}"
    "${DERIVED_SOURCES_WEBKIT_DIR}"
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders"
    "${CMAKE_BINARY_DIR}"
    mui
    mui/Api
    mui/Api/MorphOS
    mui/WebCoreSupport
    mui/Widgets
    mui/Plugins
    Storage
    WebCoreSupport
    mui/Api/AROS/include/
    "${WEBCORE_DIR}/fileapi"
    "${WEBCORE_DIR}/platform/mui"
    "${WEBCORE_DIR}/platform/bal"
    "${WEBCORE_DIR}/platform/graphics/cairo"
    "${WEBCORE_DIR}/platform/graphics/opentype"
    "${WEBCORE_DIR}/platform/linux"
    "${WEBCORE_DIR}/Modules/notifications"
    "${WEBCORE_DIR}/Modules/geolocation"
    "${CAIRO_INCLUDE_DIRS}"
)

list(APPEND WebKit_SOURCES
    mui/Api/MorphOS/addressbargroupclass.cpp
    mui/Api/MorphOS/appclass.cpp
    mui/Api/MorphOS/aros_stuff.c
    mui/Api/MorphOS/asl.cpp
    mui/Api/MorphOS/AutofillBackingStore.cpp
    mui/Api/MorphOS/AutofillManager.cpp
    mui/Api/MorphOS/autofillpopupclass.cpp
    mui/Api/MorphOS/autofillpopuplistclass.cpp
    mui/Api/MorphOS/blockmanagergroupclass.cpp
    mui/Api/MorphOS/blockmanagerlistclass.cpp
    mui/Api/MorphOS/blockmanagerwindowclass.cpp
    mui/Api/MorphOS/bookmarkgroupclass.cpp
    mui/Api/MorphOS/bookmarklisttreeclass.cpp
    mui/Api/MorphOS/bookmarkpanelgroupclass.cpp
    mui/Api/MorphOS/bookmarkwindowclass.cpp
    mui/Api/MorphOS/cattmp.cpp
    mui/Api/MorphOS/classes.cpp
    mui/Api/MorphOS/clipboard.cpp
    mui/Api/MorphOS/ColorChooserController.cpp
    mui/Api/MorphOS/colorchooserpopupclass.cpp
    mui/Api/MorphOS/consolelistclass.cpp
    mui/Api/MorphOS/consolewindowclass.cpp
    mui/Api/MorphOS/contextmenugroupclass.cpp
    mui/Api/MorphOS/contextmenulistclass.cpp
    mui/Api/MorphOS/cookiemanagerlistclass.cpp
    mui/Api/MorphOS/cookiemanagergroupclass.cpp
    mui/Api/MorphOS/cookiemanagerlistclass.cpp
    mui/Api/MorphOS/cookiemanagerwindowclass.cpp
    mui/Api/MorphOS/choosetitlegroupclass.cpp
    mui/Api/MorphOS/datetimechooserpopupclass.cpp
    mui/Api/MorphOS/DateTimeChooserController.cpp
    mui/Api/MorphOS/DownloadDelegateMorphOS.cpp
    mui/Api/MorphOS/downloadgroupclass.cpp
    mui/Api/MorphOS/downloadlistclass.cpp
    mui/Api/MorphOS/downloadwindowclass.cpp
    mui/Api/MorphOS/faviconclass.cpp
    mui/Api/MorphOS/findtextclass.cpp
    mui/Api/MorphOS/historybuttonclass.cpp
    mui/Api/MorphOS/historylistclass.cpp
    mui/Api/MorphOS/historylisttreeclass.cpp
    mui/Api/MorphOS/historypanelgroupclass.cpp
    mui/Api/MorphOS/historypopstringclass.cpp
    mui/Api/MorphOS/iconclass.cpp
    mui/Api/MorphOS/icufiles.cpp
    mui/Api/MorphOS/linklistclass.cpp
    mui/Api/MorphOS/locale.cpp
    mui/Api/MorphOS/loginwindowclass.cpp
    mui/Api/MorphOS/mediacontrolsgroupclass.cpp
    mui/Api/MorphOS/menuclass.cpp
    mui/Api/MorphOS/menuitemclass.cpp
    mui/Api/MorphOS/methodstack.c
    mui/Api/MorphOS/mimetypegroupclass.cpp
    mui/Api/MorphOS/mimetypelistclass.cpp
    mui/Api/MorphOS/mui.cpp
    mui/Api/MorphOS/navigationgroupclass.cpp
    mui/Api/MorphOS/networkledsgroupclass.cpp
    mui/Api/MorphOS/networklistclass.cpp
    mui/Api/MorphOS/networkwindowclass.cpp
    mui/Api/MorphOS/owbbrowserclass.cpp
    mui/Api/MorphOS/owbwindowclass.cpp
    mui/Api/MorphOS/passwordmanagergroupclass.cpp
    mui/Api/MorphOS/passwordmanagerlistclass.cpp
    mui/Api/MorphOS/passwordmanagerwindowclass.cpp
    mui/Api/MorphOS/popstringclass.cpp
    mui/Api/MorphOS/prefswindowclass.cpp
    mui/Api/MorphOS/owbgroupclass.cpp
    mui/Api/MorphOS/printerwindowclass.cpp
    mui/Api/MorphOS/quicklinkbuttongroupclass.cpp
    mui/Api/MorphOS/quicklinkgroupclass.cpp
    mui/Api/MorphOS/quicklinkparentgroupclass.cpp
    mui/Api/MorphOS/scriptmanagergroupclass.cpp
    mui/Api/MorphOS/scriptmanagerhostlistclass.cpp
    mui/Api/MorphOS/scriptmanagerlistclass.cpp
    mui/Api/MorphOS/scriptmanagerwindowclass.cpp
    mui/Api/MorphOS/seeksliderclass.cpp
    mui/Api/MorphOS/spacerclass.cpp
    mui/Api/MorphOS/searchbargroupclass.cpp
    mui/Api/MorphOS/searchmanagergroupclass.cpp
    mui/Api/MorphOS/searchmanagerlistclass.cpp
    mui/Api/MorphOS/searchmanagerwindowclass.cpp
    mui/Api/MorphOS/splashwindowclass.cpp
    mui/Api/MorphOS/suggestlistclass.cpp
    mui/Api/MorphOS/suggestpopstringclass.cpp
    mui/Api/MorphOS/tabthrobber.cpp
    mui/Api/MorphOS/tabtransferanimclass.cpp
    mui/Api/MorphOS/throbber.cpp
    mui/Api/MorphOS/titleclass.cpp
    mui/Api/MorphOS/titlelabelclass.cpp
    mui/Api/MorphOS/toolbutton_addbookmarkclass.cpp
    mui/Api/MorphOS/toolbutton_bookmarksclass.cpp
    mui/Api/MorphOS/toolbuttonclass.cpp
    mui/Api/MorphOS/toolbutton_newtabclass.cpp
    mui/Api/MorphOS/TopSitesManager.cpp
    mui/Api/MorphOS/transferanimclass.cpp
    mui/Api/MorphOS/urlprefsgroupclass.cpp
    mui/Api/MorphOS/urlprefslistclass.cpp
    mui/Api/MorphOS/urlprefswindowclass.cpp
    mui/Api/MorphOS/urlstringclass.cpp
    mui/Api/MorphOS/utils.cpp
    mui/Api/MorphOS/volumesliderclass.cpp
    mui/Api/MorphOS/WebViewPrivate.cpp

    mui/Api/DefaultPolicyDelegate.cpp
    mui/Api/DOMCoreClasses.cpp
    mui/Api/DOMCSSClasses.cpp
    mui/Api/DOMEventsClasses.cpp
    mui/Api/DOMHTMLClasses.cpp
    mui/Api/DOMRange.cpp
    mui/Api/HTTPHeaderPropertyBag.cpp
    mui/Api/WebBackForwardList.cpp
    mui/Api/WebDatabaseManager.cpp
    mui/Api/WebDataSource.cpp
    mui/Api/WebDragData.cpp
    mui/Api/WebDocumentLoader.cpp
    mui/Api/WebDownload.cpp
    mui/Api/WebError.cpp
    mui/Api/WebFramePolicyListener.cpp
    mui/Api/WebGeolocationPosition.cpp
    mui/Api/WebIconDatabase.cpp
    mui/Api/WebHistory.cpp
    mui/Api/WebHistoryItem.cpp
    mui/Api/WebHitTestResults.cpp
    mui/Api/WebInspector.cpp
    mui/Api/WebMutableURLRequest.cpp
    mui/Api/WebNavigationAction.cpp
    mui/Api/WebPasswordFormData.cpp
    mui/Api/WebPasswordFormUtils.cpp
    mui/Api/WebPreferences.cpp
    mui/Api/WebResource.cpp
    mui/Api/WebScriptObject.cpp
    mui/Api/WebSecurityOrigin.cpp
    mui/Api/WebFrame.cpp
    mui/Api/WebUtils.cpp
    mui/Api/WebView.cpp
    mui/Api/WebURLResponse.cpp
	
    mui/Api/AMIGAOS4/aos4funcs.cpp
	
    mui/Plugins/npapi.cpp
    mui/Plugins/PluginDatabase.cpp
    mui/Plugins/PluginDatabaseMUI.cpp
    mui/Plugins/PluginDataMorphOS.cpp
    mui/Plugins/PluginMainThreadScheduler.cpp
    mui/Plugins/PluginPackage.cpp
    mui/Plugins/PluginPackageMorphOS.cpp
    mui/Plugins/PluginStream.cpp
    mui/Plugins/PluginView.cpp
    mui/Plugins/PluginViewMorphOS.cpp

    mui/WebCoreSupport/WebChromeClient.cpp
    mui/WebCoreSupport/WebContextMenuClient.cpp
    mui/WebCoreSupport/WebDesktopNotificationsDelegate.cpp
    mui/WebCoreSupport/WebDragClient.cpp
    mui/WebCoreSupport/WebEditorClient.cpp
    mui/WebCoreSupport/WebGeolocationClient.cpp
    mui/WebCoreSupport/WebFrameLoaderClient.cpp
    mui/WebCoreSupport/WebFrameNetworkingContext.cpp
    mui/WebCoreSupport/WebInspectorClient.cpp
    mui/WebCoreSupport/WebPlatformStrategies.cpp
    mui/WebCoreSupport/WebProgressTrackerClient.cpp
    mui/WebCoreSupport/WebVisitedLinkStore.cpp

    mui/Widgets/WebDragDataMorphOS.cpp

    Storage/StorageAreaImpl.cpp
    Storage/StorageAreaSync.cpp
    Storage/StorageNamespaceImpl.cpp
    Storage/StorageSyncManager.cpp
    Storage/StorageThread.cpp
    Storage/StorageTracker.cpp
    Storage/WebDatabaseProvider.cpp
    Storage/WebStorageNamespaceProvider.cpp

    WebCoreSupport/WebViewGroup.cpp
)

set(WebKit_LIBRARY_TYPE STATIC)
