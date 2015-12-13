/*
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "CSSFilterImageValue.h"

#include "CSSImageValue.h"
#include "CachedImage.h"
#include "CachedResourceLoader.h"
#include "CrossfadeGeneratedImage.h"
#include "FilterEffectRenderer.h"
#include "ImageBuffer.h"
#include "RenderElement.h"
#include "StyleCachedImage.h"
#include "StyleGeneratedImage.h"
#include "StyleResolver.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

CSSFilterImageValue::~CSSFilterImageValue()
{
    if (m_cachedImage)
        m_cachedImage->removeClient(&m_filterSubimageObserver);
}

String CSSFilterImageValue::customCSSText() const
{
    StringBuilder result;
    result.appendLiteral("filter(");
    result.append(m_imageValue->cssText());
    result.appendLiteral(", ");
    result.append(m_filterValue->cssText());
    result.append(')');
    return result.toString();
}

FloatSize CSSFilterImageValue::fixedSize(const RenderElement* renderer)
{
    // FIXME: Skip Content Security Policy check when filter is applied to an element in a user agent shadow tree.
    // See <https://bugs.webkit.org/show_bug.cgi?id=146663>.
    ResourceLoaderOptions options = CachedResourceLoader::defaultCachedResourceOptions();

    CachedResourceLoader& cachedResourceLoader = renderer->document().cachedResourceLoader();
    CachedImage* cachedImage = cachedImageForCSSValue(m_imageValue.get(), cachedResourceLoader, options);

    if (!cachedImage)
        return FloatSize();

    return cachedImage->imageForRenderer(renderer)->size();
}

bool CSSFilterImageValue::isPending() const
{
    return CSSImageGeneratorValue::subimageIsPending(m_imageValue.get());
}

bool CSSFilterImageValue::knownToBeOpaque(const RenderElement*) const
{
    return false;
}

void CSSFilterImageValue::loadSubimages(CachedResourceLoader& cachedResourceLoader, const ResourceLoaderOptions& options)
{
    CachedResourceHandle<CachedImage> oldCachedImage = m_cachedImage;

    m_cachedImage = CSSImageGeneratorValue::cachedImageForCSSValue(m_imageValue.get(), cachedResourceLoader, options);

    if (m_cachedImage != oldCachedImage) {
        if (oldCachedImage)
            oldCachedImage->removeClient(&m_filterSubimageObserver);
        if (m_cachedImage)
            m_cachedImage->addClient(&m_filterSubimageObserver);
    }

    m_filterSubimageObserver.setReady(true);
}

RefPtr<Image> CSSFilterImageValue::image(RenderElement* renderer, const FloatSize& size)
{
    if (size.isEmpty())
        return nullptr;

    // FIXME: Skip Content Security Policy check when filter is applied to an element in a user agent shadow tree.
    // See <https://bugs.webkit.org/show_bug.cgi?id=146663>.
    ResourceLoaderOptions options = CachedResourceLoader::defaultCachedResourceOptions();

    CachedResourceLoader& cachedResourceLoader = renderer->document().cachedResourceLoader();
    CachedImage* cachedImage = cachedImageForCSSValue(m_imageValue.get(), cachedResourceLoader, options);

    if (!cachedImage)
        return Image::nullImage();

    Image* image = cachedImage->imageForRenderer(renderer);

    if (!image)
        return Image::nullImage();

    // Transform Image into ImageBuffer.
    // FIXME (149424): This buffer should not be unconditionally unaccelerated.
    std::unique_ptr<ImageBuffer> texture = ImageBuffer::create(size, Unaccelerated);
    if (!texture)
        return Image::nullImage();
    FloatRect imageRect = FloatRect(FloatPoint(), size);
    texture->context().drawImage(image, ColorSpaceDeviceRGB, imageRect);

    RefPtr<FilterEffectRenderer> filterRenderer = FilterEffectRenderer::create();
    filterRenderer->setSourceImage(WTF::move(texture));
    filterRenderer->setSourceImageRect(imageRect);
    filterRenderer->setFilterRegion(imageRect);
    if (!filterRenderer->build(renderer, m_filterOperations, FilterFunction))
        return Image::nullImage();
    filterRenderer->apply();

    m_generatedImage = filterRenderer->output()->copyImage();

    return m_generatedImage.release();
}

void CSSFilterImageValue::filterImageChanged(const IntRect&)
{
    for (auto it = clients().begin(), end = clients().end(); it != end; ++it)
        it->key->imageChanged(static_cast<WrappedImagePtr>(this));
}

void CSSFilterImageValue::createFilterOperations(StyleResolver* resolver)
{
    m_filterOperations.clear();
    if (m_filterValue)
        resolver->createFilterOperations(*m_filterValue, m_filterOperations);
}

void CSSFilterImageValue::FilterSubimageObserverProxy::imageChanged(CachedImage*, const IntRect* rect)
{
    if (m_ready)
        m_ownerValue->filterImageChanged(*rect);
}

bool CSSFilterImageValue::traverseSubresources(const std::function<bool (const CachedResource&)>& handler) const
{
    if (!m_cachedImage)
        return false;
    return handler(*m_cachedImage);
}

bool CSSFilterImageValue::equals(const CSSFilterImageValue& other) const
{
    return equalInputImages(other) && compareCSSValuePtr(m_filterValue, other.m_filterValue);
}

bool CSSFilterImageValue::equalInputImages(const CSSFilterImageValue& other) const
{
    return compareCSSValuePtr(m_imageValue, other.m_imageValue);
}

} // namespace WebCore
