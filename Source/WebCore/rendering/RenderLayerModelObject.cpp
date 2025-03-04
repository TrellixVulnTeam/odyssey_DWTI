/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 *           (C) 2005, 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010, 2012 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "RenderLayerModelObject.h"

#include "RenderLayer.h"
#include "RenderLayerCompositor.h"
#include "RenderView.h"

namespace WebCore {

bool RenderLayerModelObject::s_wasFloating = false;
bool RenderLayerModelObject::s_hadLayer = false;
bool RenderLayerModelObject::s_hadTransform = false;
bool RenderLayerModelObject::s_layerWasSelfPainting = false;

RenderLayerModelObject::RenderLayerModelObject(Element& element, Ref<RenderStyle>&& style, unsigned baseTypeFlags)
    : RenderElement(element, WTF::move(style), baseTypeFlags | RenderLayerModelObjectFlag)
{
}

RenderLayerModelObject::RenderLayerModelObject(Document& document, Ref<RenderStyle>&& style, unsigned baseTypeFlags)
    : RenderElement(document, WTF::move(style), baseTypeFlags | RenderLayerModelObjectFlag)
{
}

RenderLayerModelObject::~RenderLayerModelObject()
{
    if (isPositioned()) {
        if (style().hasViewportConstrainedPosition())
            view().frameView().removeViewportConstrainedObject(this);
    }

    // Our layer should have been destroyed and cleared by now
    ASSERT(!hasLayer());
    ASSERT(!m_layer);
}

void RenderLayerModelObject::destroyLayer()
{
    ASSERT(!hasLayer()); // Callers should have already called setHasLayer(false)
    ASSERT(m_layer);
    m_layer = nullptr;
}

void RenderLayerModelObject::createLayer()
{
    ASSERT(!m_layer);
    m_layer = std::make_unique<RenderLayer>(*this);
    setHasLayer(true);
    m_layer->insertOnlyThisLayer();
}

bool RenderLayerModelObject::hasSelfPaintingLayer() const
{
    return m_layer && m_layer->isSelfPaintingLayer();
}

void RenderLayerModelObject::styleWillChange(StyleDifference diff, const RenderStyle& newStyle)
{
    s_wasFloating = isFloating();
    s_hadLayer = hasLayer();
    s_hadTransform = hasTransform();
    if (s_hadLayer)
        s_layerWasSelfPainting = layer()->isSelfPaintingLayer();

    // If our z-index changes value or our visibility changes,
    // we need to dirty our stacking context's z-order list.
    const RenderStyle* oldStyle = hasInitializedStyle() ? &style() : nullptr;
    if (oldStyle) {
        if (parent()) {
            // Do a repaint with the old style first, e.g., for example if we go from
            // having an outline to not having an outline.
            if (diff == StyleDifferenceRepaintLayer) {
                layer()->repaintIncludingDescendants();
                if (!(oldStyle->clip() == newStyle.clip()))
                    layer()->clearClipRectsIncludingDescendants();
            } else if (diff == StyleDifferenceRepaint || newStyle.outlineSize() < oldStyle->outlineSize())
                repaint();
        }

        if (diff == StyleDifferenceLayout || diff == StyleDifferenceSimplifiedLayout) {
            // When a layout hint happens, we do a repaint of the layer, since the layer could end up being destroyed.
            if (hasLayer()) {
                if (oldStyle->position() != newStyle.position()
                    || oldStyle->zIndex() != newStyle.zIndex()
                    || oldStyle->hasAutoZIndex() != newStyle.hasAutoZIndex()
                    || !(oldStyle->clip() == newStyle.clip())
                    || oldStyle->hasClip() != newStyle.hasClip()
                    || oldStyle->opacity() != newStyle.opacity()
                    || oldStyle->transform() != newStyle.transform()
                    || oldStyle->filter() != newStyle.filter()
                    )
                layer()->repaintIncludingDescendants();
            } else if (newStyle.hasTransform() || newStyle.opacity() < 1 || newStyle.hasFilter() || newStyle.hasBackdropFilter()) {
                // If we don't have a layer yet, but we are going to get one because of transform or opacity,
                //  then we need to repaint the old position of the object.
                repaint();
            }
        }
    }

    RenderElement::styleWillChange(diff, newStyle);
}

#if ENABLE(CSS_SCROLL_SNAP)
static bool scrollSnapContainerRequiresUpdateForStyleUpdate(const RenderStyle& oldStyle, const RenderStyle& newStyle)
{
    return !(oldStyle.scrollSnapType() == newStyle.scrollSnapType()
        && oldStyle.scrollSnapPointsX() == newStyle.scrollSnapPointsX()
        && oldStyle.scrollSnapPointsY() == newStyle.scrollSnapPointsY()
        && oldStyle.scrollSnapDestination() == newStyle.scrollSnapDestination());
}
#endif

void RenderLayerModelObject::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderElement::styleDidChange(diff, oldStyle);
    updateFromStyle();

    if (requiresLayer()) {
        if (!layer() && layerCreationAllowedForSubtree()) {
            if (s_wasFloating && isFloating())
                setChildNeedsLayout();
            createLayer();
            if (parent() && !needsLayout() && containingBlock()) {
                layer()->setRepaintStatus(NeedsFullRepaint);
                // There is only one layer to update, it is not worth using |cachedOffset| since
                // we are not sure the value will be used.
                layer()->updateLayerPositions(0);
            }
        }
    } else if (layer() && layer()->parent()) {
#if ENABLE(CSS_COMPOSITING)
        if (oldStyle->hasBlendMode())
            layer()->parent()->dirtyAncestorChainHasBlendingDescendants();
#endif
        setHasTransformRelatedProperty(false); // All transform-related propeties force layers, so we know we don't have one or the object doesn't support them.
        setHasReflection(false);
        layer()->removeOnlyThisLayer(); // calls destroyLayer() which clears m_layer
        if (s_wasFloating && isFloating())
            setChildNeedsLayout();
        if (s_hadTransform)
            setNeedsLayoutAndPrefWidthsRecalc();
    }

    if (layer()) {
        layer()->styleChanged(diff, oldStyle);
        if (s_hadLayer && layer()->isSelfPaintingLayer() != s_layerWasSelfPainting)
            setChildNeedsLayout();
    }

    bool newStyleIsViewportConstrained = style().hasViewportConstrainedPosition();
    bool oldStyleIsViewportConstrained = oldStyle && oldStyle->hasViewportConstrainedPosition();
    if (newStyleIsViewportConstrained != oldStyleIsViewportConstrained) {
        if (newStyleIsViewportConstrained && layer())
            view().frameView().addViewportConstrainedObject(this);
        else
            view().frameView().removeViewportConstrainedObject(this);
    }

#if ENABLE(CSS_SCROLL_SNAP)
    const RenderStyle& newStyle = style();
    if (oldStyle && scrollSnapContainerRequiresUpdateForStyleUpdate(*oldStyle, newStyle)) {
        if (RenderLayer* renderLayer = layer()) {
            renderLayer->updateSnapOffsets();
            renderLayer->updateScrollSnapState();
        } else if (isBody() || isRoot()) {
            FrameView& frameView = view().frameView();
            frameView.updateSnapOffsets();
            frameView.updateScrollSnapState();
            frameView.updateScrollingCoordinatorScrollSnapProperties();
        }
    }
    if (oldStyle && oldStyle->scrollSnapCoordinates() != newStyle.scrollSnapCoordinates()) {
        const RenderBox* scrollSnapBox = enclosingBox().findEnclosingScrollableContainer();
        if (scrollSnapBox && scrollSnapBox->layer()) {
            const RenderStyle& style = scrollSnapBox->style();
            if (style.scrollSnapType() != ScrollSnapType::None) {
                scrollSnapBox->layer()->updateSnapOffsets();
                scrollSnapBox->layer()->updateScrollSnapState();
                if (scrollSnapBox->isBody() || scrollSnapBox->isRoot())
                    scrollSnapBox->view().frameView().updateScrollingCoordinatorScrollSnapProperties();
            }
        }
    }
#endif
}

} // namespace WebCore

