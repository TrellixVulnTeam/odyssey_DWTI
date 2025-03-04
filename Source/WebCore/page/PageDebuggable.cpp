/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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

#include "config.h"
#include "PageDebuggable.h"

#if ENABLE(REMOTE_INSPECTOR)

#include "Document.h"
#include "InspectorController.h"
#include "MainFrame.h"
#include "Page.h"
#include "Settings.h"
#include <inspector/InspectorAgentBase.h>

using namespace Inspector;

namespace WebCore {

PageDebuggable::PageDebuggable(Page& page)
    : m_page(page)
    , m_forcedDeveloperExtrasEnabled(false)
{
}

String PageDebuggable::name() const
{
    if (!m_page.mainFrame().document())
        return String();

    return m_page.mainFrame().document()->title();
}

String PageDebuggable::url() const
{
    if (!m_page.mainFrame().document())
        return String();

    String url = m_page.mainFrame().document()->url().string();
    return url.isEmpty() ? ASCIILiteral("about:blank") : url;
}

bool PageDebuggable::hasLocalDebugger() const
{
    return m_page.inspectorController().hasLocalFrontend();
}

void PageDebuggable::connect(Inspector::FrontendChannel* channel, bool isAutomaticInspection)
{
    if (!m_page.settings().developerExtrasEnabled()) {
        m_forcedDeveloperExtrasEnabled = true;
        m_page.settings().setDeveloperExtrasEnabled(true);
    } else
        m_forcedDeveloperExtrasEnabled = false;

    InspectorController& inspectorController = m_page.inspectorController();
    inspectorController.connectFrontend(channel, isAutomaticInspection);
}

void PageDebuggable::disconnect(Inspector::FrontendChannel* channel)
{
    InspectorController& inspectorController = m_page.inspectorController();
    inspectorController.disconnectFrontend(channel);

    if (m_forcedDeveloperExtrasEnabled) {
        m_forcedDeveloperExtrasEnabled = false;
        m_page.settings().setDeveloperExtrasEnabled(false);
    }
}

void PageDebuggable::dispatchMessageFromRemoteFrontend(const String& message)
{
    m_page.inspectorController().dispatchMessageFromFrontend(message);
}

void PageDebuggable::setIndicating(bool indicating)
{
    m_page.inspectorController().setIndicating(indicating);
}

} // namespace WebCore

#endif // ENABLE(REMOTE_INSPECTOR)
