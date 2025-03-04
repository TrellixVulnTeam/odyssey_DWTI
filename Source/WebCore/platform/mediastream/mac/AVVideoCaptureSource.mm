/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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
#import "AVVideoCaptureSource.h"

#if ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)

#import "AVCaptureDeviceManager.h"
#import "BlockExceptions.h"
#import "GraphicsContextCG.h"
#import "IntRect.h"
#import "Logging.h"
#import "MediaConstraints.h"
#import "NotImplemented.h"
#import "RealtimeMediaSourceStates.h"
#import <AVFoundation/AVFoundation.h>
#import <objc/runtime.h>

#import "CoreMediaSoftLink.h"

typedef AVCaptureConnection AVCaptureConnectionType;
typedef AVCaptureDevice AVCaptureDeviceType;
typedef AVCaptureDeviceInput AVCaptureDeviceInputType;
typedef AVCaptureOutput AVCaptureOutputType;
typedef AVCaptureVideoDataOutput AVCaptureVideoDataOutputType;
typedef AVCaptureVideoPreviewLayer AVCaptureVideoPreviewLayerType;

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)
SOFT_LINK_FRAMEWORK_OPTIONAL(CoreVideo)

SOFT_LINK_CLASS(AVFoundation, AVCaptureConnection)
SOFT_LINK_CLASS(AVFoundation, AVCaptureDevice)
SOFT_LINK_CLASS(AVFoundation, AVCaptureDeviceInput)
SOFT_LINK_CLASS(AVFoundation, AVCaptureOutput)
SOFT_LINK_CLASS(AVFoundation, AVCaptureVideoDataOutput)
SOFT_LINK_CLASS(AVFoundation, AVCaptureVideoPreviewLayer)

#define AVCaptureConnection getAVCaptureConnectionClass()
#define AVCaptureDevice getAVCaptureDeviceClass()
#define AVCaptureDeviceInput getAVCaptureDeviceInputClass()
#define AVCaptureOutput getAVCaptureOutputClass()
#define AVCaptureVideoDataOutput getAVCaptureVideoDataOutputClass()
#define AVCaptureVideoPreviewLayer getAVCaptureVideoPreviewLayerClass()

SOFT_LINK_POINTER(AVFoundation, AVMediaTypeAudio, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVMediaTypeVideo, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVCaptureSessionPreset1280x720, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVCaptureSessionPreset640x480, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVCaptureSessionPreset352x288, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVCaptureSessionPresetLow, NSString *)

#define AVMediaTypeVideo getAVMediaTypeVideo()
#define AVCaptureSessionPreset1280x720 getAVCaptureSessionPreset1280x720()
#define AVCaptureSessionPreset640x480 getAVCaptureSessionPreset640x480()
#define AVCaptureSessionPreset352x288 getAVCaptureSessionPreset352x288()
#define AVCaptureSessionPresetLow getAVCaptureSessionPresetLow()

SOFT_LINK(CoreVideo, CVPixelBufferGetWidth, size_t, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
SOFT_LINK(CoreVideo, CVPixelBufferGetHeight, size_t, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
SOFT_LINK(CoreVideo, CVPixelBufferGetBaseAddress, void*, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
SOFT_LINK(CoreVideo, CVPixelBufferGetBytesPerRow, size_t, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
SOFT_LINK(CoreVideo, CVPixelBufferGetPixelFormatType, OSType, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
SOFT_LINK(CoreVideo, CVPixelBufferLockBaseAddress, CVReturn, (CVPixelBufferRef pixelBuffer, CVOptionFlags lockFlags), (pixelBuffer, lockFlags))
SOFT_LINK(CoreVideo, CVPixelBufferUnlockBaseAddress, CVReturn, (CVPixelBufferRef pixelBuffer, CVOptionFlags lockFlags), (pixelBuffer, lockFlags))

SOFT_LINK_POINTER(CoreVideo, kCVPixelBufferPixelFormatTypeKey, NSString *)
#define kCVPixelBufferPixelFormatTypeKey getkCVPixelBufferPixelFormatTypeKey()

namespace WebCore {

RefPtr<AVMediaCaptureSource> AVVideoCaptureSource::create(AVCaptureDeviceType* device, const AtomicString& id, PassRefPtr<MediaConstraints> constraint)
{
    return adoptRef(new AVVideoCaptureSource(device, id, constraint));
}

AVVideoCaptureSource::AVVideoCaptureSource(AVCaptureDeviceType* device, const AtomicString& id, PassRefPtr<MediaConstraints> constraint)
    : AVMediaCaptureSource(device, id, RealtimeMediaSource::Video, constraint)
    , m_frameRate(0)
    , m_width(0)
    , m_height(0)
{
    currentStates()->setSourceId(id);
    currentStates()->setSourceType(RealtimeMediaSourceStates::Camera);
}

AVVideoCaptureSource::~AVVideoCaptureSource()
{
}

RefPtr<RealtimeMediaSourceCapabilities> AVVideoCaptureSource::capabilities() const
{
    notImplemented();
    return 0;
}

void AVVideoCaptureSource::updateStates()
{
    RealtimeMediaSourceStates* states = currentStates();

    if ([device() position] == AVCaptureDevicePositionFront)
        states->setFacingMode(RealtimeMediaSourceStates::User);
    else if ([device() position] == AVCaptureDevicePositionBack)
        states->setFacingMode(RealtimeMediaSourceStates::Environment);
    else
        states->setFacingMode(RealtimeMediaSourceStates::Unknown);
    
    states->setFrameRate(m_frameRate);
    states->setWidth(m_width);
    states->setHeight(m_height);
    states->setAspectRatio(static_cast<float>(m_width) / m_height);
}

bool AVVideoCaptureSource::setFrameRateConstraint(float minFrameRate, float maxFrameRate)
{
    AVFrameRateRange *bestFrameRateRange = 0;

    for (AVFrameRateRange *frameRateRange in [[device() activeFormat] videoSupportedFrameRateRanges]) {
        if (!maxFrameRate) {
            if (minFrameRate == [frameRateRange minFrameRate])
                bestFrameRateRange = frameRateRange;
        } else if (minFrameRate >= [frameRateRange minFrameRate] && maxFrameRate <= [frameRateRange maxFrameRate]) {
            if (CMTIME_COMPARE_INLINE([frameRateRange minFrameDuration], >, [bestFrameRateRange minFrameDuration]))
                bestFrameRateRange = frameRateRange;
        }
    }
    
    if (!bestFrameRateRange) {
        LOG(Media, "AVVideoCaptureSource::setFrameRateConstraint(%p), frame rate range %f..%f not supported by video device", this, minFrameRate, maxFrameRate);
        return false;
    }
    
    NSError *error = nil;
    @try {
        if ([device() lockForConfiguration:&error]) {
            [device() setActiveVideoMinFrameDuration:[bestFrameRateRange minFrameDuration]];
            if (maxFrameRate)
                [device() setActiveVideoMaxFrameDuration:[bestFrameRateRange maxFrameDuration]];
            [device() unlockForConfiguration];
        }
    } @catch(NSException *exception) {
        LOG(Media, "AVVideoCaptureSource::setFrameRateConstraint(%p), exception thrown configuring device: <%s> %s", this, [[exception name] UTF8String], [[exception reason] UTF8String]);
        return false;
    }
    
    if (error) {
        LOG(Media, "AVVideoCaptureSource::setFrameRateConstraint(%p), failed to lock video device for configuration: %s", this, [[error localizedDescription] UTF8String]);
        return false;
    }

    LOG(Media, "AVVideoCaptureSource::setFrameRateConstraint(%p) - set frame rate range to %f..%f", this, minFrameRate, maxFrameRate);
    return true;
}

bool AVVideoCaptureSource::applyConstraints(MediaConstraints* constraints)
{
    ASSERT(constraints);

    const Vector<AtomicString>& constraintNames = AVCaptureDeviceManager::validConstraintNames();
    String widthConstraint;
    String heightConstraint;

    constraints->getMandatoryConstraintValue(constraintNames[AVCaptureDeviceManager::Width], widthConstraint);
    constraints->getMandatoryConstraintValue(constraintNames[AVCaptureDeviceManager::Height], heightConstraint);

    int width = widthConstraint.toInt();
    int height = heightConstraint.toInt();
    if (!width && !height) {
        constraints->getOptionalConstraintValue(constraintNames[AVCaptureDeviceManager::Width], widthConstraint);
        constraints->getOptionalConstraintValue(constraintNames[AVCaptureDeviceManager::Height], heightConstraint);
        width = widthConstraint.toInt();
        height = heightConstraint.toInt();
    }
    
    if (width || height) {
        NSString *preset = AVCaptureDeviceManager::bestSessionPresetForVideoSize(session(), width, height);
        if (!preset || ![session() canSetSessionPreset:preset])
            return false;
        
        [session() setSessionPreset:preset];
    }

    String frameRateConstraint;
    constraints->getMandatoryConstraintValue(constraintNames[AVCaptureDeviceManager::FrameRate], frameRateConstraint);
    float frameRate = frameRateConstraint.toFloat();
    if (!frameRate) {
        constraints->getOptionalConstraintValue(constraintNames[AVCaptureDeviceManager::FrameRate], frameRateConstraint);
        frameRate = frameRateConstraint.toFloat();
    }
    if (frameRate && !setFrameRateConstraint(frameRate, 0))
        return false;

    return true;
}

void AVVideoCaptureSource::setupCaptureSession()
{
    RetainPtr<AVCaptureDeviceInputType> videoIn = adoptNS([allocAVCaptureDeviceInputInstance() initWithDevice:device() error:nil]);
    ASSERT([session() canAddInput:videoIn.get()]);
    if ([session() canAddInput:videoIn.get()])
        [session() addInput:videoIn.get()];

    if (constraints())
        applyConstraints(constraints());

    RetainPtr<AVCaptureVideoDataOutputType> videoOutput = adoptNS([allocAVCaptureVideoDataOutputInstance() init]);
    RetainPtr<NSDictionary> settingsDictionary = adoptNS([[NSDictionary alloc] initWithObjectsAndKeys:
                                                         [NSNumber numberWithInt:kCVPixelFormatType_32BGRA], kCVPixelBufferPixelFormatTypeKey
                                                         , nil]);
    [videoOutput setVideoSettings:settingsDictionary.get()];
    setVideoSampleBufferDelegate(videoOutput.get());
    ASSERT([session() canAddOutput:videoOutput.get()]);
    if ([session() canAddOutput:videoOutput.get()])
        [session() addOutput:videoOutput.get()];

    m_videoConnection = adoptNS([videoOutput.get() connectionWithMediaType:AVMediaTypeVideo]);
    m_videoPreviewLayer = adoptNS([[AVCaptureVideoPreviewLayer alloc] initWithSession:session()]);
}

bool AVVideoCaptureSource::calculateFramerate(CMSampleBufferRef sampleBuffer)
{
    CMTime sampleTime = CMSampleBufferGetPresentationTimeStamp(sampleBuffer);
    if (!CMTIME_IS_NUMERIC(sampleTime))
        return false;

    Float64 frameTime = CMTimeGetSeconds(sampleTime);
    Float64 oneSecondAgo = frameTime - 1;
    
    m_videoFrameTimeStamps.append(frameTime);
    
    while (m_videoFrameTimeStamps[0] < oneSecondAgo)
        m_videoFrameTimeStamps.remove(0);

    Float64 frameRate = m_frameRate;
    m_frameRate = (m_frameRate + m_videoFrameTimeStamps.size()) / 2;

    return frameRate != m_frameRate;
}

void AVVideoCaptureSource::processNewFrame(RetainPtr<CMSampleBufferRef> sampleBuffer)
{
    CMFormatDescriptionRef formatDescription = CMSampleBufferGetFormatDescription(sampleBuffer.get());
    if (!formatDescription)
        return;

    bool statesChanged = false;

    statesChanged = calculateFramerate(sampleBuffer.get());
    m_buffer = sampleBuffer;
    m_lastImage = nullptr;

    CMVideoDimensions dimensions = CMVideoFormatDescriptionGetDimensions(formatDescription);
    if (dimensions.width != m_width || dimensions.height != m_height) {
        m_width = dimensions.width;
        m_height = dimensions.height;
        statesChanged = true;
    }

    if (statesChanged)
        this->statesDidChanged();
}

void AVVideoCaptureSource::captureOutputDidOutputSampleBufferFromConnection(AVCaptureOutputType*, CMSampleBufferRef sampleBuffer, AVCaptureConnectionType*)
{
    RetainPtr<CMSampleBufferRef> buffer = sampleBuffer;

    scheduleDeferredTask([this, buffer] {
        this->processNewFrame(buffer);
    });
}

RetainPtr<CGImageRef> AVVideoCaptureSource::currentFrameImage()
{
    if (m_lastImage)
        return m_lastImage;

    if (!m_buffer)
        return nullptr;

    CVPixelBufferRef pixelBuffer = static_cast<CVPixelBufferRef>(CMSampleBufferGetImageBuffer(m_buffer.get()));
    ASSERT(CVPixelBufferGetPixelFormatType(pixelBuffer) == kCVPixelFormatType_32BGRA);

    CVPixelBufferLockBaseAddress(pixelBuffer, 0);
    void *baseAddress = CVPixelBufferGetBaseAddress(pixelBuffer);
    size_t bytesPerRow = CVPixelBufferGetBytesPerRow(pixelBuffer);
    size_t width = CVPixelBufferGetWidth(pixelBuffer);
    size_t height = CVPixelBufferGetHeight(pixelBuffer);

    RetainPtr<CGDataProviderRef> provider = adoptCF(CGDataProviderCreateWithData(NULL, baseAddress, bytesPerRow * height, NULL));
    m_lastImage = adoptCF(CGImageCreate(width, height, 8, 32, bytesPerRow, deviceRGBColorSpaceRef(), kCGBitmapByteOrder32Little | kCGImageAlphaPremultipliedFirst, provider.get(), NULL, true, kCGRenderingIntentDefault));

    CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);

    return m_lastImage;
}

void AVVideoCaptureSource::paintCurrentFrameInContext(GraphicsContext& context, const FloatRect& rect)
{
    if (context.paintingDisabled() || !currentFrameImage())
        return;

    GraphicsContextStateSaver stateSaver(context);
    context.translate(rect.x(), rect.y() + rect.height());
    context.scale(FloatSize(1, -1));
    context.setImageInterpolationQuality(InterpolationLow);
    IntRect paintRect(IntPoint(0, 0), IntSize(rect.width(), rect.height()));
    CGContextDrawImage(context.platformContext(), CGRectMake(0, 0, paintRect.width(), paintRect.height()), m_lastImage.get());
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
