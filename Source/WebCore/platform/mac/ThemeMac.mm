/*
 * Copyright (C) 2008, 2010, 2011, 2012 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"
#import "ThemeMac.h"

#import "AXObjectCache.h"
#import "BlockExceptions.h"
#import "GraphicsContext.h"
#import "ImageBuffer.h"
#import "LocalCurrentGraphicsContext.h"
#import "NSButtonCellSPI.h"
#import "ScrollView.h"
#import "WebCoreSystemInterface.h"
#import <Carbon/Carbon.h>
#import <wtf/StdLibExtras.h>

static NSRect focusRingClipRect;
static BOOL themeWindowHasKeyAppearance;

@interface WebCoreThemeWindow : NSWindow
@end

@implementation WebCoreThemeWindow

- (BOOL)hasKeyAppearance
{
    return themeWindowHasKeyAppearance;
}

- (BOOL)isKeyWindow
{
    return themeWindowHasKeyAppearance;
}

@end

@interface WebCoreThemeView : NSControl
@end

@implementation WebCoreThemeView

- (NSWindow *)window
{
    // Using defer:YES prevents us from wasting any window server resources for this window, since we're not actually
    // going to draw into it. The other arguments match what you get when calling -[NSWindow init].
    static WebCoreThemeWindow *window = [[WebCoreThemeWindow alloc] initWithContentRect:NSMakeRect(100, 100, 100, 100)
        styleMask:NSTitledWindowMask backing:NSBackingStoreBuffered defer:YES];
    return window;
}

- (BOOL)isFlipped
{
    return YES;
}

- (NSText *)currentEditor
{
    return nil;
}

- (BOOL)_automaticFocusRingDisabled
{
    return YES;
}

- (NSRect)_focusRingVisibleRect
{
    if (NSIsEmptyRect(focusRingClipRect))
        return [self visibleRect];
    return focusRingClipRect;
}

- (NSView *)_focusRingClipAncestor
{
    return self;
}

- (void)addSubview:(NSView*)subview
{
    // By doing nothing in this method we forbid controls from adding subviews.
    // This tells AppKit to not use Layer-backed animation for control rendering.
    UNUSED_PARAM(subview);
}

@end

@implementation NSFont (WebCoreTheme)

- (NSString*)webCoreFamilyName
{
    if ([[self familyName] hasPrefix:@"."])
        return [self fontName];

    return [self familyName];
}

@end

// FIXME: Default buttons really should be more like push buttons and not like buttons.

namespace WebCore {

enum {
    topMargin,
    rightMargin,
    bottomMargin,
    leftMargin
};

Theme* platformTheme()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(ThemeMac, themeMac, ());
    return &themeMac;
}

// Helper functions used by a bunch of different control parts.

static NSControlSize controlSizeForFont(const FontCascade& font)
{
    int fontSize = font.pixelSize();
    if (fontSize >= 16)
        return NSRegularControlSize;
    if (fontSize >= 11)
        return NSSmallControlSize;
    return NSMiniControlSize;
}

static LengthSize sizeFromNSControlSize(NSControlSize nsControlSize, const LengthSize& zoomedSize, float zoomFactor, const std::array<IntSize, 3>& sizes)
{
    IntSize controlSize = sizes[nsControlSize];
    if (zoomFactor != 1.0f)
        controlSize = IntSize(controlSize.width() * zoomFactor, controlSize.height() * zoomFactor);
    LengthSize result = zoomedSize;
    if (zoomedSize.width().isIntrinsicOrAuto() && controlSize.width() > 0)
        result.setWidth(Length(controlSize.width(), Fixed));
    if (zoomedSize.height().isIntrinsicOrAuto() && controlSize.height() > 0)
        result.setHeight(Length(controlSize.height(), Fixed));
    return result;
}

static LengthSize sizeFromFont(const FontCascade& font, const LengthSize& zoomedSize, float zoomFactor, const std::array<IntSize, 3>& sizes)
{
    return sizeFromNSControlSize(controlSizeForFont(font), zoomedSize, zoomFactor, sizes);
}

static ControlSize controlSizeFromPixelSize(const std::array<IntSize, 3>& sizes, const IntSize& minZoomedSize, float zoomFactor)
{
    if (minZoomedSize.width() >= static_cast<int>(sizes[NSRegularControlSize].width() * zoomFactor) &&
        minZoomedSize.height() >= static_cast<int>(sizes[NSRegularControlSize].height() * zoomFactor))
        return NSRegularControlSize;
    if (minZoomedSize.width() >= static_cast<int>(sizes[NSSmallControlSize].width() * zoomFactor) &&
        minZoomedSize.height() >= static_cast<int>(sizes[NSSmallControlSize].height() * zoomFactor))
        return NSSmallControlSize;
    return NSMiniControlSize;
}

static void setControlSize(NSCell* cell, const std::array<IntSize, 3>& sizes, const IntSize& minZoomedSize, float zoomFactor)
{
    ControlSize size = controlSizeFromPixelSize(sizes, minZoomedSize, zoomFactor);
    if (size != [cell controlSize]) // Only update if we have to, since AppKit does work even if the size is the same.
        [cell setControlSize:(NSControlSize)size];
}

static void updateStates(NSCell* cell, const ControlStates& controlStates, bool useAnimation = false)
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED < 101000
    UNUSED_PARAM(useAnimation);
#endif

    // The animated state cause this thread to start and stop repeatedly on CoreAnimation synchronize calls.
    // This short burts of activity in between are not long enough for VoiceOver to retrieve accessibility attributes and makes the process appear unresponsive.
    if (AXObjectCache::accessibilityEnhancedUserInterfaceEnabled())
        useAnimation = false;
    
    ControlStates::States states = controlStates.states();

    // Hover state is not supported by Aqua.
    
    // Pressed state
    bool oldPressed = [cell isHighlighted];
    bool pressed = states & ControlStates::PressedState;
    if (pressed != oldPressed) {
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000
        [(NSButtonCell*)cell _setHighlighted:pressed animated:useAnimation];
#else
        [cell setHighlighted:pressed];
#endif
    }
    
    // Enabled state
    bool oldEnabled = [cell isEnabled];
    bool enabled = states & ControlStates::EnabledState;
    if (enabled != oldEnabled)
        [cell setEnabled:enabled];

    // Checked and Indeterminate
    bool oldIndeterminate = [cell state] == NSMixedState;
    bool indeterminate = (states & ControlStates::IndeterminateState);
    bool checked = states & ControlStates::CheckedState;
    bool oldChecked = [cell state] == NSOnState;
    if (oldIndeterminate != indeterminate || checked != oldChecked) {
        NSCellStateValue newState = indeterminate ? NSMixedState : (checked ? NSOnState : NSOffState);
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000
        [(NSButtonCell*)cell _setState:newState animated:useAnimation];
#else
        [cell setState:newState];
#endif
    }

    // Window inactive state does not need to be checked explicitly, since we paint parented to 
    // a view in a window whose key state can be detected.
}

static ThemeDrawState convertControlStatesToThemeDrawState(ThemeButtonKind kind, const ControlStates& controlStates)
{
    ControlStates::States states = controlStates.states();

    if (!(states & ControlStates::EnabledState))
        return kThemeStateUnavailableInactive;

    // Do not process PressedState if !EnabledState.
    if (states & ControlStates::PressedState) {
        if (kind == kThemeIncDecButton || kind == kThemeIncDecButtonSmall || kind == kThemeIncDecButtonMini)
            return states & ControlStates::SpinUpState ? kThemeStatePressedUp : kThemeStatePressedDown;
        return kThemeStatePressed;
    }
    return kThemeStateActive;
}

static FloatRect inflateRect(const FloatRect& zoomedRect, const IntSize& zoomedSize, const int* margins, float zoomFactor)
{
    // Only do the inflation if the available width/height are too small.  Otherwise try to
    // fit the glow/check space into the available box's width/height.
    int widthDelta = zoomedRect.width() - (zoomedSize.width() + margins[leftMargin] * zoomFactor + margins[rightMargin] * zoomFactor);
    int heightDelta = zoomedRect.height() - (zoomedSize.height() + margins[topMargin] * zoomFactor + margins[bottomMargin] * zoomFactor);
    FloatRect result(zoomedRect);
    if (widthDelta < 0) {
        result.setX(result.x() - margins[leftMargin] * zoomFactor);
        result.setWidth(result.width() - widthDelta);
    }
    if (heightDelta < 0) {
        result.setY(result.y() - margins[topMargin] * zoomFactor);
        result.setHeight(result.height() - heightDelta);
    }
    return result;
}

// Checkboxes and radio buttons

static const std::array<IntSize, 3>& checkboxSizes()
{
    static const std::array<IntSize, 3> sizes = { { IntSize(14, 14), IntSize(12, 12), IntSize(10, 10) } };
    return sizes;
}

static const int* checkboxMargins(NSControlSize controlSize)
{
    static const int margins[3][4] =
    {
        // top right bottom left
        { 2, 2, 2, 2 },
        { 2, 1, 2, 1 },
        { 0, 0, 1, 0 },
    };
    return margins[controlSize];
}

static LengthSize checkboxSize(const FontCascade& font, const LengthSize& zoomedSize, float zoomFactor)
{
    // If the width and height are both specified, then we have nothing to do.
    if (!zoomedSize.width().isIntrinsicOrAuto() && !zoomedSize.height().isIntrinsicOrAuto())
        return zoomedSize;

    // Use the font size to determine the intrinsic width of the control.
    return sizeFromFont(font, zoomedSize, zoomFactor, checkboxSizes());
}

// Radio Buttons

static const std::array<IntSize, 3>& radioSizes()
{
    static const std::array<IntSize, 3> sizes = { { IntSize(16, 16), IntSize(12, 12), IntSize(10, 10) } };
    return sizes;
}

static const int* radioMargins(NSControlSize controlSize)
{
    static const int margins[3][4] =
    {
        // top right bottom left
        { 1, 0, 1, 2 },
        { 1, 1, 2, 1 },
        { 0, 0, 1, 1 },
    };
    return margins[controlSize];
}

static LengthSize radioSize(const FontCascade& font, const LengthSize& zoomedSize, float zoomFactor)
{
    // If the width and height are both specified, then we have nothing to do.
    if (!zoomedSize.width().isIntrinsicOrAuto() && !zoomedSize.height().isIntrinsicOrAuto())
        return zoomedSize;

    // Use the font size to determine the intrinsic width of the control.
    return sizeFromFont(font, zoomedSize, zoomFactor, radioSizes());
}
    
static void configureToggleButton(NSCell* cell, ControlPart buttonType, const ControlStates& states, const IntSize& zoomedSize, float zoomFactor, bool isStateChange)
{
    // Set the control size based off the rectangle we're painting into.
    setControlSize(cell, buttonType == CheckboxPart ? checkboxSizes() : radioSizes(), zoomedSize, zoomFactor);

    // Update the various states we respond to.
    updateStates(cell, states, isStateChange);
}
    
static RetainPtr<NSButtonCell> createToggleButtonCell(ControlPart buttonType)
{
    RetainPtr<NSButtonCell> toggleButtonCell = adoptNS([[NSButtonCell alloc] init]);
    
    if (buttonType == CheckboxPart) {
        [toggleButtonCell setButtonType:NSSwitchButton];
        [toggleButtonCell setAllowsMixedState:YES];
    } else {
        ASSERT(buttonType == RadioPart);
        [toggleButtonCell setButtonType:NSRadioButton];
    }
    
    [toggleButtonCell setTitle:nil];
    [toggleButtonCell setFocusRingType:NSFocusRingTypeExterior];
    return toggleButtonCell;
}
    
static NSButtonCell *sharedRadioCell(const ControlStates& states, const IntSize& zoomedSize, float zoomFactor)
{
    static NSButtonCell *radioCell = createToggleButtonCell(RadioPart).leakRef();

    configureToggleButton(radioCell, RadioPart, states, zoomedSize, zoomFactor, false);
    return radioCell;
}
    
static NSButtonCell *sharedCheckboxCell(const ControlStates& states, const IntSize& zoomedSize, float zoomFactor)
{
    static NSButtonCell *checkboxCell = createToggleButtonCell(CheckboxPart).leakRef();

    configureToggleButton(checkboxCell, CheckboxPart, states, zoomedSize, zoomFactor, false);
    return checkboxCell;
}

static bool drawCellFocusRing(NSCell *cell, NSRect cellFrame, NSView *controlView)
{
    wkDrawCellFocusRingWithFrameAtTime(cell, cellFrame, controlView, std::numeric_limits<double>::max());
    return false;
}

static void paintToggleButton(ControlPart buttonType, ControlStates& controlStates, GraphicsContext& context, const FloatRect& zoomedRect, float zoomFactor, ScrollView* scrollView, float deviceScaleFactor, float pageScaleFactor)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    RetainPtr<NSButtonCell> toggleButtonCell = static_cast<NSButtonCell *>(controlStates.platformControl());
    IntSize zoomedRectSize = IntSize(zoomedRect.size());

    if (controlStates.isDirty()) {
        if (!toggleButtonCell)
            toggleButtonCell = createToggleButtonCell(buttonType);
        configureToggleButton(toggleButtonCell.get(), buttonType, controlStates, zoomedRectSize, zoomFactor, true);
    } else {
        if (!toggleButtonCell) {
            if (buttonType == CheckboxPart)
                toggleButtonCell = sharedCheckboxCell(controlStates, zoomedRectSize, zoomFactor);
            else {
                ASSERT(buttonType == RadioPart);
                toggleButtonCell = sharedRadioCell(controlStates, zoomedRectSize, zoomFactor);
            }
        }
        configureToggleButton(toggleButtonCell.get(), buttonType, controlStates, zoomedRectSize, zoomFactor, false);
    }
    controlStates.setDirty(false);

    GraphicsContextStateSaver stateSaver(context);

    NSControlSize controlSize = [toggleButtonCell controlSize];
    IntSize zoomedSize = buttonType == CheckboxPart ? checkboxSizes()[controlSize] : radioSizes()[controlSize];
    zoomedSize.setWidth(zoomedSize.width() * zoomFactor);
    zoomedSize.setHeight(zoomedSize.height() * zoomFactor);
    const int* controlMargins = buttonType == CheckboxPart ? checkboxMargins(controlSize) : radioMargins(controlSize);
    FloatRect inflatedRect = inflateRect(zoomedRect, zoomedSize, controlMargins, zoomFactor);

    if (zoomFactor != 1.0f) {
        inflatedRect.setWidth(inflatedRect.width() / zoomFactor);
        inflatedRect.setHeight(inflatedRect.height() / zoomFactor);
        context.translate(inflatedRect.x(), inflatedRect.y());
        context.scale(FloatSize(zoomFactor, zoomFactor));
        context.translate(-inflatedRect.x(), -inflatedRect.y());
    }

    LocalCurrentGraphicsContext localContext(context);

    bool useUnparentedView = false;
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000
    useUnparentedView = true;
#endif
    NSView *view = ThemeMac::ensuredView(scrollView, controlStates, useUnparentedView);

    bool needsRepaint = false;
    bool useImageBuffer = pageScaleFactor != 1.0f || zoomFactor != 1.0f;
    bool isCellFocused = controlStates.states() & ControlStates::FocusState;

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000
    if ([toggleButtonCell _stateAnimationRunning]) {
        context.translate(inflatedRect.x(), inflatedRect.y());
        context.scale(FloatSize(1, -1));
        context.translate(0, -inflatedRect.height());

        [toggleButtonCell _renderCurrentAnimationFrameInContext:context.platformContext() atLocation:NSMakePoint(0, 0)];
        if (![toggleButtonCell _stateAnimationRunning] && isCellFocused)
            needsRepaint = ThemeMac::drawCellOrFocusRingWithViewIntoContext(toggleButtonCell.get(), context, inflatedRect, view, false, true, useImageBuffer, deviceScaleFactor);
    } else
        needsRepaint = ThemeMac::drawCellOrFocusRingWithViewIntoContext(toggleButtonCell.get(), context, inflatedRect, view, true, isCellFocused, useImageBuffer, deviceScaleFactor);
#else
    needsRepaint = ThemeMac::drawCellOrFocusRingWithViewIntoContext(toggleButtonCell.get(), context, inflatedRect, view, true, isCellFocused, useImageBuffer, deviceScaleFactor);
#endif

    [toggleButtonCell setControlView:nil];

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000
    needsRepaint |= [toggleButtonCell _stateAnimationRunning];
#endif
    controlStates.setNeedsRepaint(needsRepaint);
    if (needsRepaint)
        controlStates.setPlatformControl(toggleButtonCell.get());

    END_BLOCK_OBJC_EXCEPTIONS
}

// Buttons

// Buttons really only constrain height. They respect width.
static const std::array<IntSize, 3>& buttonSizes()
{
    static const std::array<IntSize, 3> sizes = { { IntSize(0, 21), IntSize(0, 18), IntSize(0, 15) } };
    return sizes;
}

static const int* buttonMargins(NSControlSize controlSize)
{
    static const int margins[3][4] =
    {
        { 4, 6, 7, 6 },
        { 4, 5, 6, 5 },
        { 0, 1, 1, 1 },
    };
    return margins[controlSize];
}

enum ButtonCellType { NormalButtonCell, DefaultButtonCell };

static NSButtonCell *leakButtonCell(ButtonCellType type)
{
    NSButtonCell *cell = [[NSButtonCell alloc] init];
    [cell setTitle:nil];
    [cell setButtonType:NSMomentaryPushInButton];
    if (type == DefaultButtonCell)
        [cell setKeyEquivalent:@"\r"];
    return cell;
}

static void setUpButtonCell(NSButtonCell *cell, ControlPart part, const ControlStates& states, const IntSize& zoomedSize, float zoomFactor)
{
    // Set the control size based off the rectangle we're painting into.
    const std::array<IntSize, 3>& sizes = buttonSizes();
    if (part == SquareButtonPart || zoomedSize.height() > buttonSizes()[NSRegularControlSize].height() * zoomFactor) {
        // Use the square button
        if ([cell bezelStyle] != NSShadowlessSquareBezelStyle)
            [cell setBezelStyle:NSShadowlessSquareBezelStyle];
    } else if ([cell bezelStyle] != NSRoundedBezelStyle)
        [cell setBezelStyle:NSRoundedBezelStyle];

    setControlSize(cell, sizes, zoomedSize, zoomFactor);

    // Update the various states we respond to.
    updateStates(cell, states);
}

static NSButtonCell *button(ControlPart part, const ControlStates& controlStates, const IntSize& zoomedSize, float zoomFactor)
{
    ControlStates::States states = controlStates.states();
    NSButtonCell *cell;
    if (states & ControlStates::DefaultState) {
        static NSButtonCell *defaultCell = leakButtonCell(DefaultButtonCell);
        cell = defaultCell;
    } else {
        static NSButtonCell *normalCell = leakButtonCell(NormalButtonCell);
        cell = normalCell;
    }
    setUpButtonCell(cell, part, controlStates, zoomedSize, zoomFactor);
    return cell;
}
    
static void paintButton(ControlPart part, ControlStates& controlStates, GraphicsContext& context, const FloatRect& zoomedRect, float zoomFactor, ScrollView* scrollView, float deviceScaleFactor, float pageScaleFactor)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    
    // Determine the width and height needed for the control and prepare the cell for painting.
    ControlStates::States states = controlStates.states();
    NSButtonCell *buttonCell = button(part, controlStates, IntSize(zoomedRect.size()), zoomFactor);
    GraphicsContextStateSaver stateSaver(context);

    NSControlSize controlSize = [buttonCell controlSize];
    IntSize zoomedSize = buttonSizes()[controlSize];
    zoomedSize.setWidth(zoomedRect.width()); // Buttons don't ever constrain width, so the zoomed width can just be honored.
    zoomedSize.setHeight(zoomedSize.height() * zoomFactor);
    FloatRect inflatedRect = zoomedRect;
    if ([buttonCell bezelStyle] == NSRoundedBezelStyle) {
        // Center the button within the available space.
        if (inflatedRect.height() > zoomedSize.height()) {
            inflatedRect.setY(inflatedRect.y() + (inflatedRect.height() - zoomedSize.height()) / 2);
            inflatedRect.setHeight(zoomedSize.height());
        }

        // Now inflate it to account for the shadow.
        inflatedRect = inflateRect(inflatedRect, zoomedSize, buttonMargins(controlSize), zoomFactor);

        if (zoomFactor != 1.0f) {
            inflatedRect.setWidth(inflatedRect.width() / zoomFactor);
            inflatedRect.setHeight(inflatedRect.height() / zoomFactor);
            context.translate(inflatedRect.x(), inflatedRect.y());
            context.scale(FloatSize(zoomFactor, zoomFactor));
            context.translate(-inflatedRect.x(), -inflatedRect.y());
        }
    }
    
    LocalCurrentGraphicsContext localContext(context);
    
    NSView *view = ThemeMac::ensuredView(scrollView, controlStates);
    NSWindow *window = [view window];
    NSButtonCell *previousDefaultButtonCell = [window defaultButtonCell];

    bool useImageBuffer = pageScaleFactor != 1.0f || zoomFactor != 1.0f;
    bool needsRepaint = ThemeMac::drawCellOrFocusRingWithViewIntoContext(buttonCell, context, inflatedRect, view, true, states & ControlStates::FocusState, useImageBuffer, deviceScaleFactor);
    if (states & ControlStates::DefaultState) {
        [window setDefaultButtonCell:buttonCell];
#if __MAC_OS_X_VERSION_MIN_REQUIRED < 101000
        wkAdvanceDefaultButtonPulseAnimation(buttonCell);
#endif
    } else if ([previousDefaultButtonCell isEqual:buttonCell])
        [window setDefaultButtonCell:nil];
    
    controlStates.setNeedsRepaint(needsRepaint);

    [buttonCell setControlView:nil];

    if (![previousDefaultButtonCell isEqual:buttonCell])
        [window setDefaultButtonCell:previousDefaultButtonCell];

    END_BLOCK_OBJC_EXCEPTIONS
}

// Stepper

static const std::array<IntSize, 3>& stepperSizes()
{
    static const std::array<IntSize, 3> sizes = { { IntSize(19, 27), IntSize(15, 22), IntSize(13, 15) } };
    return sizes;
}

// We don't use controlSizeForFont() for steppers because the stepper height
// should be equal to or less than the corresponding text field height,
static NSControlSize stepperControlSizeForFont(const FontCascade& font)
{
    int fontSize = font.pixelSize();
    if (fontSize >= 18)
        return NSRegularControlSize;
    if (fontSize >= 13)
        return NSSmallControlSize;
    return NSMiniControlSize;
}

static void paintStepper(ControlStates& states, GraphicsContext& context, const FloatRect& zoomedRect, float zoomFactor, ScrollView*)
{
    // We don't use NSStepperCell because there are no ways to draw an
    // NSStepperCell with the up button highlighted.

    HIThemeButtonDrawInfo drawInfo;
    drawInfo.version = 0;
    drawInfo.state = convertControlStatesToThemeDrawState(kThemeIncDecButton, states);
    drawInfo.adornment = kThemeAdornmentDefault;
    ControlSize controlSize = controlSizeFromPixelSize(stepperSizes(), IntSize(zoomedRect.size()), zoomFactor);
    if (controlSize == NSSmallControlSize)
        drawInfo.kind = kThemeIncDecButtonSmall;
    else if (controlSize == NSMiniControlSize)
        drawInfo.kind = kThemeIncDecButtonMini;
    else
        drawInfo.kind = kThemeIncDecButton;

    IntRect rect(zoomedRect);
    GraphicsContextStateSaver stateSaver(context);
    if (zoomFactor != 1.0f) {
        rect.setWidth(rect.width() / zoomFactor);
        rect.setHeight(rect.height() / zoomFactor);
        context.translate(rect.x(), rect.y());
        context.scale(FloatSize(zoomFactor, zoomFactor));
        context.translate(-rect.x(), -rect.y());
    }
    CGRect bounds(rect);
    CGRect backgroundBounds;
    HIThemeGetButtonBackgroundBounds(&bounds, &drawInfo, &backgroundBounds);
    // Center the stepper rectangle in the specified area.
    backgroundBounds.origin.x = bounds.origin.x + (bounds.size.width - backgroundBounds.size.width) / 2;
    if (backgroundBounds.size.height < bounds.size.height) {
        int heightDiff = clampToInteger(bounds.size.height - backgroundBounds.size.height);
        backgroundBounds.origin.y = bounds.origin.y + (heightDiff / 2) + 1;
    }

    LocalCurrentGraphicsContext localContext(context);
    HIThemeDrawButton(&backgroundBounds, &drawInfo, localContext.cgContext(), kHIThemeOrientationNormal, 0);
}

// This will ensure that we always return a valid NSView, even if ScrollView doesn't have an associated document NSView.
// If the ScrollView doesn't have an NSView, we will return a fake NSView set up in the way AppKit expects.
NSView *ThemeMac::ensuredView(ScrollView* scrollView, const ControlStates& controlStates, bool useUnparentedView)
{
    if (!useUnparentedView) {
        if (NSView *documentView = scrollView->documentView())
            return documentView;
    }

    // Use a fake view.
    static WebCoreThemeView *themeView = [[WebCoreThemeView alloc] init];
    [themeView setFrameSize:NSSizeFromCGSize(scrollView->totalContentsSize())];

    themeWindowHasKeyAppearance = !(controlStates.states() & ControlStates::WindowInactiveState);

    return themeView;
}

void ThemeMac::setFocusRingClipRect(const FloatRect& rect)
{
    focusRingClipRect = rect;
}

const float buttonFocusRectOutlineWidth = 3.0f;

bool ThemeMac::drawCellOrFocusRingWithViewIntoContext(NSCell* cell, GraphicsContext& context, const FloatRect& inflatedRect, NSView* view, bool drawButtonCell, bool drawFocusRing, bool useImageBuffer, float deviceScaleFactor)
{
    ASSERT(drawButtonCell || drawFocusRing);
    bool needsRepaint = false;
    if (useImageBuffer) {
        NSRect imageBufferDrawRect = NSRect(FloatRect(buttonFocusRectOutlineWidth, buttonFocusRectOutlineWidth, inflatedRect.width(), inflatedRect.height()));
        std::unique_ptr<ImageBuffer> imageBuffer = ImageBuffer::createCompatibleBuffer(inflatedRect.size() + 2 * FloatSize(buttonFocusRectOutlineWidth, buttonFocusRectOutlineWidth), deviceScaleFactor, ColorSpaceSRGB, context, false);
        {
            LocalCurrentGraphicsContext localContext(imageBuffer->context());
            if (drawButtonCell)
                [cell drawWithFrame:imageBufferDrawRect inView:view];
            
            if (drawFocusRing)
                needsRepaint = drawCellFocusRing(cell, imageBufferDrawRect, view);
        }
        context.drawImageBuffer(imageBuffer.get(), ColorSpaceSRGB, inflatedRect.location() - FloatSize(buttonFocusRectOutlineWidth, buttonFocusRectOutlineWidth));
        return needsRepaint;
    }
    if (drawButtonCell)
        [cell drawWithFrame:NSRect(inflatedRect) inView:view];
    
    if (drawFocusRing)
        needsRepaint = drawCellFocusRing(cell, NSRect(inflatedRect), view);
    
    return needsRepaint;
}

// Theme overrides

int ThemeMac::baselinePositionAdjustment(ControlPart part) const
{
    if (part == CheckboxPart || part == RadioPart)
        return -2;
    return Theme::baselinePositionAdjustment(part);
}

Optional<FontCascadeDescription> ThemeMac::controlFont(ControlPart part, const FontCascade& font, float zoomFactor) const
{
    switch (part) {
        case PushButtonPart: {
            FontCascadeDescription fontDescription;
            fontDescription.setIsAbsoluteSize(true);

            NSFont* nsFont = [NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:controlSizeForFont(font)]];
            fontDescription.setOneFamily(AtomicString("-apple-system", AtomicString::ConstructFromLiteral));
            fontDescription.setComputedSize([nsFont pointSize] * zoomFactor);
            fontDescription.setSpecifiedSize([nsFont pointSize] * zoomFactor);
            return fontDescription;
        }
        default:
            return Theme::controlFont(part, font, zoomFactor);
    }
}

LengthSize ThemeMac::controlSize(ControlPart part, const FontCascade& font, const LengthSize& zoomedSize, float zoomFactor) const
{
    switch (part) {
        case CheckboxPart:
            return checkboxSize(font, zoomedSize, zoomFactor);
        case RadioPart:
            return radioSize(font, zoomedSize, zoomFactor);
        case PushButtonPart:
            // Height is reset to auto so that specified heights can be ignored.
            return sizeFromFont(font, LengthSize(zoomedSize.width(), Length()), zoomFactor, buttonSizes());
        case InnerSpinButtonPart:
            if (!zoomedSize.width().isIntrinsicOrAuto() && !zoomedSize.height().isIntrinsicOrAuto())
                return zoomedSize;
            return sizeFromNSControlSize(stepperControlSizeForFont(font), zoomedSize, zoomFactor, stepperSizes());
        default:
            return zoomedSize;
    }
}

LengthSize ThemeMac::minimumControlSize(ControlPart part, const FontCascade& font, float zoomFactor) const
{
    switch (part) {
        case SquareButtonPart:
        case DefaultButtonPart:
        case ButtonPart:
            return LengthSize(Length(0, Fixed), Length(static_cast<int>(15 * zoomFactor), Fixed));
        case InnerSpinButtonPart:{
            IntSize base = stepperSizes()[NSMiniControlSize];
            return LengthSize(Length(static_cast<int>(base.width() * zoomFactor), Fixed),
                              Length(static_cast<int>(base.height() * zoomFactor), Fixed));
        }
        default:
            return Theme::minimumControlSize(part, font, zoomFactor);
    }
}

LengthBox ThemeMac::controlBorder(ControlPart part, const FontCascade& font, const LengthBox& zoomedBox, float zoomFactor) const
{
    switch (part) {
        case SquareButtonPart:
        case DefaultButtonPart:
        case ButtonPart:
            return LengthBox(0, zoomedBox.right().value(), 0, zoomedBox.left().value());
        default:
            return Theme::controlBorder(part, font, zoomedBox, zoomFactor);
    }
}

LengthBox ThemeMac::controlPadding(ControlPart part, const FontCascade& font, const LengthBox& zoomedBox, float zoomFactor) const
{
    switch (part) {
        case PushButtonPart: {
            // Just use 8px.  AppKit wants to use 11px for mini buttons, but that padding is just too large
            // for real-world Web sites (creating a huge necessary minimum width for buttons whose space is
            // by definition constrained, since we select mini only for small cramped environments.
            // This also guarantees the HTML <button> will match our rendering by default, since we're using a consistent
            // padding.
            const int padding = 8 * zoomFactor;
            return LengthBox(2, padding, 3, padding);
        }
        default:
            return Theme::controlPadding(part, font, zoomedBox, zoomFactor);
    }
}

void ThemeMac::inflateControlPaintRect(ControlPart part, const ControlStates& states, FloatRect& zoomedRect, float zoomFactor) const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    IntSize zoomRectSize = IntSize(zoomedRect.size());
    switch (part) {
        case CheckboxPart: {
            // We inflate the rect as needed to account for padding included in the cell to accommodate the checkbox
            // shadow" and the check.  We don't consider this part of the bounds of the control in WebKit.
            NSCell *cell = sharedCheckboxCell(states, zoomRectSize, zoomFactor);
            NSControlSize controlSize = [cell controlSize];
            IntSize zoomedSize = checkboxSizes()[controlSize];
            zoomedSize.setHeight(zoomedSize.height() * zoomFactor);
            zoomedSize.setWidth(zoomedSize.width() * zoomFactor);
            zoomedRect = inflateRect(zoomedRect, zoomedSize, checkboxMargins(controlSize), zoomFactor);
            break;
        }
        case RadioPart: {
            // We inflate the rect as needed to account for padding included in the cell to accommodate the radio button
            // shadow".  We don't consider this part of the bounds of the control in WebKit.
            NSCell *cell = sharedRadioCell(states, zoomRectSize, zoomFactor);
            NSControlSize controlSize = [cell controlSize];
            IntSize zoomedSize = radioSizes()[controlSize];
            zoomedSize.setHeight(zoomedSize.height() * zoomFactor);
            zoomedSize.setWidth(zoomedSize.width() * zoomFactor);
            zoomedRect = inflateRect(zoomedRect, zoomedSize, radioMargins(controlSize), zoomFactor);
            break;
        }
        case PushButtonPart:
        case DefaultButtonPart:
        case ButtonPart: {
            NSButtonCell *cell = button(part, states, zoomRectSize, zoomFactor);
            NSControlSize controlSize = [cell controlSize];

            // We inflate the rect as needed to account for the Aqua button's shadow.
            if ([cell bezelStyle] == NSRoundedBezelStyle) {
                IntSize zoomedSize = buttonSizes()[controlSize];
                zoomedSize.setHeight(zoomedSize.height() * zoomFactor);
                zoomedSize.setWidth(zoomedRect.width()); // Buttons don't ever constrain width, so the zoomed width can just be honored.
                zoomedRect = inflateRect(zoomedRect, zoomedSize, buttonMargins(controlSize), zoomFactor);
            }
            break;
        }
        case InnerSpinButtonPart: {
            static const int stepperMargin[4] = { 0, 0, 0, 0 };
            ControlSize controlSize = controlSizeFromPixelSize(stepperSizes(), zoomRectSize, zoomFactor);
            IntSize zoomedSize = stepperSizes()[controlSize];
            zoomedSize.setHeight(zoomedSize.height() * zoomFactor);
            zoomedSize.setWidth(zoomedSize.width() * zoomFactor);
            zoomedRect = inflateRect(zoomedRect, zoomedSize, stepperMargin, zoomFactor);
            break;
        }
        default:
            break;
    }
    END_BLOCK_OBJC_EXCEPTIONS
}

void ThemeMac::paint(ControlPart part, ControlStates& states, GraphicsContext& context, const FloatRect& zoomedRect, float zoomFactor, ScrollView* scrollView, float deviceScaleFactor, float pageScaleFactor)
{
    switch (part) {
        case CheckboxPart:
            paintToggleButton(part, states, context, zoomedRect, zoomFactor, scrollView, deviceScaleFactor, pageScaleFactor);
            break;
        case RadioPart:
            paintToggleButton(part, states, context, zoomedRect, zoomFactor, scrollView, deviceScaleFactor, pageScaleFactor);
            break;
        case PushButtonPart:
        case DefaultButtonPart:
        case ButtonPart:
        case SquareButtonPart:
            paintButton(part, states, context, zoomedRect, zoomFactor, scrollView, deviceScaleFactor, pageScaleFactor);
            break;
        case InnerSpinButtonPart:
            paintStepper(states, context, zoomedRect, zoomFactor, scrollView);
            break;
        default:
            break;
    }
}

}
