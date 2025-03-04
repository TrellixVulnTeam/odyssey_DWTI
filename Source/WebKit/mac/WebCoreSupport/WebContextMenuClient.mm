/*
 * Copyright (C) 2006, 2007, 2008, 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if !PLATFORM(IOS)

#import "WebContextMenuClient.h"

#import "WebDelegateImplementationCaching.h"
#import "WebElementDictionary.h"
#import "WebFrame.h"
#import "WebFrameInternal.h"
#import "WebHTMLView.h"
#import "WebHTMLViewInternal.h"
#import "WebKitVersionChecks.h"
#import "WebNSPasteboardExtras.h"
#import "WebSharingServicePickerController.h"
#import "WebUIDelegate.h"
#import "WebUIDelegatePrivate.h"
#import "WebView.h"
#import "WebViewInternal.h"
#import <WebCore/BitmapImage.h>
#import <WebCore/ContextMenu.h>
#import <WebCore/ContextMenuController.h>
#import <WebCore/Document.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameView.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/ImageBuffer.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/NSSharingServicePickerSPI.h>
#import <WebCore/Page.h>
#import <WebCore/RenderBox.h>
#import <WebCore/RenderObject.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebCore/URL.h>
#import <WebKitLegacy/DOMPrivate.h>

using namespace WebCore;

@interface NSApplication (AppKitSecretsIKnowAbout)
- (void)speakString:(NSString *)string;
@end

WebContextMenuClient::WebContextMenuClient(WebView *webView)
#if ENABLE(SERVICE_CONTROLS)
    : WebSharingServicePickerClient(webView)
#else
    : m_webView(webView)
#endif
{
}

WebContextMenuClient::~WebContextMenuClient()
{
#if ENABLE(SERVICE_CONTROLS)
    if (m_sharingServicePickerController)
        [m_sharingServicePickerController clear];
#endif
}

void WebContextMenuClient::contextMenuDestroyed()
{
    delete this;
}

static BOOL isPreVersion3Client(void)
{
    static BOOL preVersion3Client = !WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITH_3_0_CONTEXT_MENU_TAGS);
    return preVersion3Client;
}

static BOOL isPreInspectElementTagClient(void)
{
    static BOOL preInspectElementTagClient = !WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITH_INSPECT_ELEMENT_MENU_TAG);
    return preInspectElementTagClient;
}

static NSMutableArray *fixMenusToSendToOldClients(NSMutableArray *defaultMenuItems)
{
    NSMutableArray *savedItems = nil;

    unsigned defaultItemsCount = [defaultMenuItems count];

    if (isPreInspectElementTagClient() && defaultItemsCount >= 2) {
        NSMenuItem *secondToLastItem = [defaultMenuItems objectAtIndex:defaultItemsCount - 2];
        NSMenuItem *lastItem = [defaultMenuItems objectAtIndex:defaultItemsCount - 1];

        if ([secondToLastItem isSeparatorItem] && [lastItem tag] == WebMenuItemTagInspectElement) {
            savedItems = [NSMutableArray arrayWithCapacity:2];
            [savedItems addObject:secondToLastItem];
            [savedItems addObject:lastItem];

            [defaultMenuItems removeObject:secondToLastItem];
            [defaultMenuItems removeObject:lastItem];
            defaultItemsCount -= 2;
        }
    }

    BOOL preVersion3Client = isPreVersion3Client();
    if (!preVersion3Client)
        return savedItems;
        
    for (unsigned i = 0; i < defaultItemsCount; ++i) {
        NSMenuItem *item = [defaultMenuItems objectAtIndex:i];
        int tag = [item tag];
        int oldStyleTag = tag;

        if (tag >= WEBMENUITEMTAG_WEBKIT_3_0_SPI_START) {
            // Change all editing-related SPI tags listed in WebUIDelegatePrivate.h to WebMenuItemTagOther
            // to match our old WebKit context menu behavior.
            oldStyleTag = WebMenuItemTagOther;
        } else {
            // All items are expected to have useful tags coming into this method.
            ASSERT(tag != WebMenuItemTagOther);
            
            // Use the pre-3.0 tags for the few items that changed tags as they moved from SPI to API. We
            // do this only for old clients; new Mail already expects the new symbols in this case.
            if (preVersion3Client) {
                switch (tag) {
                    case WebMenuItemTagSearchInSpotlight:
                        oldStyleTag = OldWebMenuItemTagSearchInSpotlight;
                        break;
                    case WebMenuItemTagSearchWeb:
                        oldStyleTag = OldWebMenuItemTagSearchWeb;
                        break;
                    case WebMenuItemTagLookUpInDictionary:
                        oldStyleTag = OldWebMenuItemTagLookUpInDictionary;
                        break;
                    default:
                        break;
                }
            }
        }

        if (oldStyleTag != tag)
            [item setTag:oldStyleTag];
    }

    return savedItems;
}

static void fixMenusReceivedFromOldClients(NSMutableArray *newMenuItems, NSMutableArray *savedItems)
{   
    if (savedItems)
        [newMenuItems addObjectsFromArray:savedItems];

    BOOL preVersion3Client = isPreVersion3Client();
    if (!preVersion3Client)
        return;
    
    // Restore the modern tags to the menu items whose tags we altered in fixMenusToSendToOldClients. 
    unsigned newItemsCount = [newMenuItems count];
    for (unsigned i = 0; i < newItemsCount; ++i) {
        NSMenuItem *item = [newMenuItems objectAtIndex:i];
        
        int tag = [item tag];
        int modernTag = tag;
        
        if (tag == WebMenuItemTagOther) {
            // Restore the specific tag for items on which we temporarily set WebMenuItemTagOther to match old behavior.
            NSString *title = [item title];
            if ([title isEqualToString:contextMenuItemTagOpenLink()])
                modernTag = WebMenuItemTagOpenLink;
            else if ([title isEqualToString:contextMenuItemTagIgnoreGrammar()])
                modernTag = WebMenuItemTagIgnoreGrammar;
            else if ([title isEqualToString:contextMenuItemTagSpellingMenu()])
                modernTag = WebMenuItemTagSpellingMenu;
            else if ([title isEqualToString:contextMenuItemTagShowSpellingPanel(true)]
                     || [title isEqualToString:contextMenuItemTagShowSpellingPanel(false)])
                modernTag = WebMenuItemTagShowSpellingPanel;
            else if ([title isEqualToString:contextMenuItemTagCheckSpelling()])
                modernTag = WebMenuItemTagCheckSpelling;
            else if ([title isEqualToString:contextMenuItemTagCheckSpellingWhileTyping()])
                modernTag = WebMenuItemTagCheckSpellingWhileTyping;
            else if ([title isEqualToString:contextMenuItemTagCheckGrammarWithSpelling()])
                modernTag = WebMenuItemTagCheckGrammarWithSpelling;
            else if ([title isEqualToString:contextMenuItemTagFontMenu()])
                modernTag = WebMenuItemTagFontMenu;
            else if ([title isEqualToString:contextMenuItemTagShowFonts()])
                modernTag = WebMenuItemTagShowFonts;
            else if ([title isEqualToString:contextMenuItemTagBold()])
                modernTag = WebMenuItemTagBold;
            else if ([title isEqualToString:contextMenuItemTagItalic()])
                modernTag = WebMenuItemTagItalic;
            else if ([title isEqualToString:contextMenuItemTagUnderline()])
                modernTag = WebMenuItemTagUnderline;
            else if ([title isEqualToString:contextMenuItemTagOutline()])
                modernTag = WebMenuItemTagOutline;
            else if ([title isEqualToString:contextMenuItemTagStyles()])
                modernTag = WebMenuItemTagStyles;
            else if ([title isEqualToString:contextMenuItemTagShowColors()])
                modernTag = WebMenuItemTagShowColors;
            else if ([title isEqualToString:contextMenuItemTagSpeechMenu()])
                modernTag = WebMenuItemTagSpeechMenu;
            else if ([title isEqualToString:contextMenuItemTagStartSpeaking()])
                modernTag = WebMenuItemTagStartSpeaking;
            else if ([title isEqualToString:contextMenuItemTagStopSpeaking()])
                modernTag = WebMenuItemTagStopSpeaking;
            else if ([title isEqualToString:contextMenuItemTagWritingDirectionMenu()])
                modernTag = WebMenuItemTagWritingDirectionMenu;
            else if ([title isEqualToString:contextMenuItemTagDefaultDirection()])
                modernTag = WebMenuItemTagDefaultDirection;
            else if ([title isEqualToString:contextMenuItemTagLeftToRight()])
                modernTag = WebMenuItemTagLeftToRight;
            else if ([title isEqualToString:contextMenuItemTagRightToLeft()])
                modernTag = WebMenuItemTagRightToLeft;
            else if ([title isEqualToString:contextMenuItemTagInspectElement()])
                modernTag = WebMenuItemTagInspectElement;
            else if ([title isEqualToString:contextMenuItemTagCorrectSpellingAutomatically()])
                modernTag = WebMenuItemTagCorrectSpellingAutomatically;
            else if ([title isEqualToString:contextMenuItemTagSubstitutionsMenu()])
                modernTag = WebMenuItemTagSubstitutionsMenu;
            else if ([title isEqualToString:contextMenuItemTagShowSubstitutions(true)]
                     || [title isEqualToString:contextMenuItemTagShowSubstitutions(false)])
                modernTag = WebMenuItemTagShowSubstitutions;
            else if ([title isEqualToString:contextMenuItemTagSmartCopyPaste()])
                modernTag = WebMenuItemTagSmartCopyPaste;
            else if ([title isEqualToString:contextMenuItemTagSmartQuotes()])
                modernTag = WebMenuItemTagSmartQuotes;
            else if ([title isEqualToString:contextMenuItemTagSmartDashes()])
                modernTag = WebMenuItemTagSmartDashes;
            else if ([title isEqualToString:contextMenuItemTagSmartLinks()])
                modernTag = WebMenuItemTagSmartLinks;
            else if ([title isEqualToString:contextMenuItemTagTextReplacement()])
                modernTag = WebMenuItemTagTextReplacement;
            else if ([title isEqualToString:contextMenuItemTagTransformationsMenu()])
                modernTag = WebMenuItemTagTransformationsMenu;
            else if ([title isEqualToString:contextMenuItemTagMakeUpperCase()])
                modernTag = WebMenuItemTagMakeUpperCase;
            else if ([title isEqualToString:contextMenuItemTagMakeLowerCase()])
                modernTag = WebMenuItemTagMakeLowerCase;
            else if ([title isEqualToString:contextMenuItemTagCapitalize()])
                modernTag = WebMenuItemTagCapitalize;
            else {
            // We don't expect WebMenuItemTagOther for any items other than the ones we explicitly handle.
            // There's nothing to prevent an app from applying this tag, but they are supposed to only
            // use tags in the range starting with WebMenuItemBaseApplicationTag=10000
                ASSERT_NOT_REACHED();
            }
        } else if (preVersion3Client) {
            // Restore the new API tag for items on which we temporarily set the old SPI tag. The old SPI tag was
            // needed to avoid confusing clients linked against earlier WebKits; the new API tag is needed for
            // WebCore to handle the menu items appropriately (without needing to know about the old SPI tags).
            switch (tag) {
                case OldWebMenuItemTagSearchInSpotlight:
                    modernTag = WebMenuItemTagSearchInSpotlight;
                    break;
                case OldWebMenuItemTagSearchWeb:
                    modernTag = WebMenuItemTagSearchWeb;
                    break;
                case OldWebMenuItemTagLookUpInDictionary:
                    modernTag = WebMenuItemTagLookUpInDictionary;
                    break;
                default:
                    break;
            }
        }
        
        if (modernTag != tag)
            [item setTag:modernTag];        
    }
}

NSMutableArray* WebContextMenuClient::getCustomMenuFromDefaultItems(ContextMenu* defaultMenu)
{
    id delegate = [m_webView UIDelegate];
    SEL selector = @selector(webView:contextMenuItemsForElement:defaultMenuItems:);
    if (![delegate respondsToSelector:selector])
        return defaultMenu->platformDescription();

    NSDictionary *element = [[[WebElementDictionary alloc] initWithHitTestResult:[m_webView page]->contextMenuController().hitTestResult()] autorelease];

    BOOL preVersion3Client = isPreVersion3Client();
    if (preVersion3Client) {
        DOMNode *node = [element objectForKey:WebElementDOMNodeKey];
        if ([node isKindOfClass:[DOMHTMLInputElement class]] && [(DOMHTMLInputElement *)node _isTextField])
            return defaultMenu->platformDescription();
        if ([node isKindOfClass:[DOMHTMLTextAreaElement class]])
            return defaultMenu->platformDescription();
    }

    NSMutableArray *defaultMenuItems = defaultMenu->platformDescription();

    unsigned defaultItemsCount = [defaultMenuItems count];
    for (unsigned i = 0; i < defaultItemsCount; ++i)
        [[defaultMenuItems objectAtIndex:i] setRepresentedObject:element];

    NSMutableArray *savedItems = [fixMenusToSendToOldClients(defaultMenuItems) retain];
    NSArray *delegateSuppliedItems = CallUIDelegate(m_webView, selector, element, defaultMenuItems);
    NSMutableArray *newMenuItems = [delegateSuppliedItems mutableCopy];
    fixMenusReceivedFromOldClients(newMenuItems, savedItems);
    [savedItems release];
    return [newMenuItems autorelease];
}

void WebContextMenuClient::contextMenuItemSelected(ContextMenuItem* item, const ContextMenu* parentMenu)
{
    id delegate = [m_webView UIDelegate];
    SEL selector = @selector(webView:contextMenuItemSelected:forElement:);
    if ([delegate respondsToSelector:selector]) {
        NSDictionary *element = [[WebElementDictionary alloc] initWithHitTestResult:[m_webView page]->contextMenuController().hitTestResult()];

        CallUIDelegate(m_webView, selector, item->platformDescription(), element);

        [element release];
    }
}

void WebContextMenuClient::downloadURL(const URL& url)
{
    [m_webView _downloadURL:url];
}

void WebContextMenuClient::searchWithSpotlight()
{
    [m_webView _searchWithSpotlightFromMenu:nil];
}

void WebContextMenuClient::searchWithGoogle(const Frame*)
{
    [m_webView _searchWithGoogleFromMenu:nil];
}

void WebContextMenuClient::lookUpInDictionary(Frame* frame)
{
    WebHTMLView* htmlView = (WebHTMLView*)[[kit(frame) frameView] documentView];
    if(![htmlView isKindOfClass:[WebHTMLView class]])
        return;
    [htmlView _lookUpInDictionaryFromMenu:nil];
}

bool WebContextMenuClient::isSpeaking()
{
    return [NSApp isSpeaking];
}

void WebContextMenuClient::speak(const String& string)
{
    [NSApp speakString:[[(NSString*)string copy] autorelease]];
}

void WebContextMenuClient::stopSpeaking()
{
    [NSApp stopSpeaking:nil];
}

ContextMenuItem WebContextMenuClient::shareMenuItem(const HitTestResult& hitTestResult)
{
    if (![[NSMenuItem class] respondsToSelector:@selector(standardShareMenuItemWithItems:)])
        return ContextMenuItem();

    Node* node = hitTestResult.innerNonSharedNode();
    if (!node)
        return ContextMenuItem();

    Frame* frame = node->document().frame();
    if (!frame)
        return ContextMenuItem();

    URL downloadableMediaURL;
    if (!hitTestResult.absoluteMediaURL().isEmpty() && hitTestResult.isDownloadableMedia())
        downloadableMediaURL = hitTestResult.absoluteMediaURL();

    RetainPtr<NSImage> nsImage;
    if (Image* image = hitTestResult.image()) {
        if (RefPtr<SharedBuffer> buffer = image->data())
            nsImage = adoptNS([[NSImage alloc] initWithData:[NSData dataWithBytes:buffer->data() length:buffer->size()]]);
    }

    return ContextMenuItem::shareMenuItem(hitTestResult.absoluteLinkURL(), downloadableMediaURL, nsImage.get(), hitTestResult.selectedText());
}

bool WebContextMenuClient::clientFloatRectForNode(Node& node, FloatRect& rect) const
{
    RenderObject* renderer = node.renderer();
    if (!renderer) {
        // This method shouldn't be called in cases where the controlled node hasn't rendered.
        ASSERT_NOT_REACHED();
        return false;
    }

    if (!is<RenderBox>(*renderer))
        return false;
    auto& renderBox = downcast<RenderBox>(*renderer);

    LayoutRect layoutRect = renderBox.clientBoxRect();
    FloatQuad floatQuad = renderBox.localToAbsoluteQuad(FloatQuad(layoutRect));
    rect = floatQuad.boundingBox();

    return true;
}

#if ENABLE(SERVICE_CONTROLS)
void WebContextMenuClient::sharingServicePickerWillBeDestroyed(WebSharingServicePickerController &)
{
    m_sharingServicePickerController = nil;
}

WebCore::FloatRect WebContextMenuClient::screenRectForCurrentSharingServicePickerItem(WebSharingServicePickerController &)
{
    Page* page = [m_webView page];
    if (!page)
        return NSZeroRect;

    Node* node = page->contextMenuController().context().hitTestResult().innerNode();
    if (!node)
        return NSZeroRect;

    FrameView* frameView = node->document().view();
    if (!frameView) {
        // This method shouldn't be called in cases where the controlled node isn't in a rendered view.
        ASSERT_NOT_REACHED();
        return NSZeroRect;
    }

    FloatRect rect;
    if (!clientFloatRectForNode(*node, rect))
        return NSZeroRect;

    // FIXME: https://webkit.org/b/132915
    // Ideally we'd like to convert the content rect to screen coordinates without the lossy float -> int conversion.
    // Creating a rounded int rect works well in practice, but might still lead to off-by-one-pixel problems in edge cases.
    IntRect intRect = roundedIntRect(rect);
    return frameView->contentsToScreen(intRect);
}

RetainPtr<NSImage> WebContextMenuClient::imageForCurrentSharingServicePickerItem(WebSharingServicePickerController &)
{
    Page* page = [m_webView page];
    if (!page)
        return nil;

    Node* node = page->contextMenuController().context().hitTestResult().innerNode();
    if (!node)
        return nil;

    FrameView* frameView = node->document().view();
    if (!frameView) {
        // This method shouldn't be called in cases where the controlled node isn't in a rendered view.
        ASSERT_NOT_REACHED();
        return nil;
    }

    FloatRect rect;
    if (!clientFloatRectForNode(*node, rect))
        return nil;

    // This is effectively a snapshot, and will be painted in an unaccelerated fashion in line with FrameSnapshotting.
    std::unique_ptr<ImageBuffer> buffer = ImageBuffer::create(rect.size(), Unaccelerated);
    if (!buffer)
        return nil;

    VisibleSelection oldSelection = frameView->frame().selection().selection();
    RefPtr<Range> range = Range::create(node->document(), Position(node, Position::PositionIsBeforeAnchor), Position(node, Position::PositionIsAfterAnchor));
    frameView->frame().selection().setSelection(VisibleSelection(*range), FrameSelection::DoNotSetFocus);

    PaintBehavior oldPaintBehavior = frameView->paintBehavior();
    frameView->setPaintBehavior(PaintBehaviorSelectionOnly);

    buffer->context().translate(-toFloatSize(rect.location()));
    frameView->paintContents(buffer->context(), roundedIntRect(rect));

    frameView->frame().selection().setSelection(oldSelection);
    frameView->setPaintBehavior(oldPaintBehavior);

    RefPtr<Image> image = buffer->copyImage(DontCopyBackingStore);
    if (!image)
        return nil;

    return [[image->getNSImage() retain] autorelease];
}
#endif

NSMenu *WebContextMenuClient::contextMenuForEvent(NSEvent *event, NSView *view, bool& isServicesMenu)
{
    isServicesMenu = false;

    Page* page = [m_webView page];
    if (!page)
        return nil;

#if ENABLE(SERVICE_CONTROLS) && defined(__LP64__)
    if (Image* image = page->contextMenuController().context().controlledImage()) {
        ASSERT(page->contextMenuController().context().hitTestResult().innerNode());

        RetainPtr<NSItemProvider> itemProvider = adoptNS([[NSItemProvider alloc] initWithItem:image->getNSImage() typeIdentifier:@"public.image"]);

        bool isContentEditable = page->contextMenuController().context().hitTestResult().innerNode()->isContentEditable();
        m_sharingServicePickerController = adoptNS([[WebSharingServicePickerController alloc] initWithItems:@[ itemProvider.get() ] includeEditorServices:isContentEditable client:this style:NSSharingServicePickerStyleRollover]);

        isServicesMenu = true;
        return [m_sharingServicePickerController menu];
    }
#endif

    return [view menuForEvent:event];
}

void WebContextMenuClient::showContextMenu()
{
    Page* page = [m_webView page];
    if (!page)
        return;
    Frame* frame = page->contextMenuController().hitTestResult().innerNodeFrame();
    if (!frame)
        return;
    FrameView* frameView = frame->view();
    if (!frameView)
        return;

    NSView* view = frameView->documentView();
    IntPoint point = frameView->contentsToWindow(page->contextMenuController().hitTestResult().roundedPointInInnerNodeFrame());
    NSEvent* event = [NSEvent mouseEventWithType:NSRightMouseDown location:point modifierFlags:0 timestamp:0 windowNumber:[[view window] windowNumber] context:0 eventNumber:0 clickCount:1 pressure:1];

    // Show the contextual menu for this event.
    bool isServicesMenu;
    if (NSMenu *menu = contextMenuForEvent(event, view, isServicesMenu)) {
        if (isServicesMenu)
            [menu popUpMenuPositioningItem:nil atLocation:[view convertPoint:point toView:nil] inView:view];
        else
            [NSMenu popUpContextMenu:menu withEvent:event forView:view];
    }
}

#endif // !PLATFORM(IOS)
