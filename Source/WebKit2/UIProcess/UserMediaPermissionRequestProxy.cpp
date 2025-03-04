/*
 * Copyright (C) 2014 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "UserMediaPermissionRequestProxy.h"

#include "UserMediaPermissionRequestManagerProxy.h"
#include <WebCore/MediaStreamTrackSourcesRequestClient.h>
#include <WebCore/RealtimeMediaSourceCenter.h>
#include <wtf/text/StringHash.h>

namespace WebKit {

UserMediaPermissionRequestProxy::UserMediaPermissionRequestProxy(UserMediaPermissionRequestManagerProxy& manager, uint64_t userMediaID, const Vector<String>& audioDeviceUIDs, const Vector<String>& videoDeviceUIDs)
    : m_manager(manager)
    , m_userMediaID(userMediaID)
    , m_videoDeviceUIDs(videoDeviceUIDs)
    , m_audioDeviceUIDs(audioDeviceUIDs)
{
}

void UserMediaPermissionRequestProxy::allow(const String& audioDeviceUID, const String& videoDeviceUID)
{
    m_manager.didReceiveUserMediaPermissionDecision(m_userMediaID, true, audioDeviceUID, videoDeviceUID);
}

void UserMediaPermissionRequestProxy::deny()
{
    m_manager.didReceiveUserMediaPermissionDecision(m_userMediaID, false, emptyString(), emptyString());
}

void UserMediaPermissionRequestProxy::invalidate()
{
    m_manager.invalidateRequests();
}

} // namespace WebKit

