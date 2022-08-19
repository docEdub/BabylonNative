#import <MetalKit/MetalKit.h>
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include "NativeCamera.h"
#include <arcana/macros.h>
#include <arcana/threading/task.h>
#include <arcana/threading/dispatcher.h>
#include <Babylon/JsRuntimeScheduler.h>
#include <Babylon/Graphics/DeviceContext.h>
#include <arcana/threading/task_schedulers.h>
#include <memory>
#include <Foundation/Foundation.h>
#include <AVFoundation/AVFoundation.h>

@class CameraTextureDelegate;

#include "NativeCameraImpl.h"
#include <napi/napi.h>

@interface CameraTextureDelegate : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>{
    std::shared_ptr<Babylon::Plugins::Camera::Impl::ImplData> implData;
}

- (id)init:(std::shared_ptr<Babylon::Plugins::Camera::Impl::ImplData>)implData;

@end

namespace Babylon::Plugins
{
    namespace {
        typedef struct {
            vector_float2 position;
            vector_float2 uv;
            vector_float2 cameraUV;
        } XRVertex;

        static XRVertex vertices[] = {
            // 2D positions, UV,        camera UV
            { { -1, -1 },   { 0, 1 },   { 0, 1} },
            { { -1, 1 },    { 0, 0 },   { 0, 0} },
            { { 1, -1 },    { 1, 1 },   { 1, 1} },
            { { 1, 1 },     { 1, 0 },   { 1, 0} },
        };

        // This shader is used to render *either* the camera texture or the final composited texture.
        // It could be split into two shaders, but this is a bit simpler since they use the same structs
        // and have some common logic.
        // The shader is used in two passes:
        // 1. Render the camera texture to the color render texture (see GetNextFrame).
        // 2. Render the composited texture to the screen (see DrawFrame).
        //
        // NB: This is a copy/paste from Dependencies/xr/Source/ARKit/XR.mm:565
        //
        // NB: Use this shader as a reference for the BGRA to RGBA conversion.
        //      - The pixel channels might not need to be swapped at all. A straight copy from one texture to the other might do the swap automatically based on the texture formats.
        //      - Use Y CbCr camera format. (This shader does the conversion from Y CbCr to RGBA).
        //
        constexpr char shaderSource[] = R"(
            #include <metal_stdlib>
            #include <simd/simd.h>

            using namespace metal;

            #include <simd/simd.h>

            typedef struct
            {
                vector_float2 position;
                vector_float2 uv;
                vector_float2 cameraUV;
            } XRVertex;

            typedef struct
            {
                float4 position [[position]];
                float2 uv;
                float2 cameraUV;
            } RasterizerData;

            vertex RasterizerData
            vertexShader(uint vertexID [[vertex_id]],
                         constant XRVertex *vertices [[buffer(0)]])
            {
                RasterizerData out;
                out.position = vector_float4(vertices[vertexID].position.xy, 0.0, 1.0);
                out.uv = vertices[vertexID].uv;
                out.cameraUV = vertices[vertexID].cameraUV;
                return out;
            }

            fragment float4 fragmentShader(RasterizerData in [[stage_in]],
                texture2d<float, access::sample> cameraTextureY [[ texture(1) ]],
                texture2d<float, access::sample> cameraTextureCbCr [[ texture(2) ]])
            {
                constexpr sampler linearSampler(mip_filter::linear, mag_filter::linear, min_filter::linear);

                if (!is_null_texture(cameraTextureY) && !is_null_texture(cameraTextureCbCr))
                {
                    const float4 cameraSampleY = cameraTextureY.sample(linearSampler, in.cameraUV);
                    const float4 cameraSampleCbCr = cameraTextureCbCr.sample(linearSampler, in.cameraUV);

                    const float4x4 ycbcrToRGBTransform = float4x4(
                        float4(+1.0000f, +1.0000f, +1.0000f, +0.0000f),
                        float4(+0.0000f, -0.3441f, +1.7720f, +0.0000f),
                        float4(+1.4020f, -0.7141f, +0.0000f, +0.0000f),
                        float4(-0.7010f, +0.5291f, -0.8860f, +1.0000f)
                    );

                    float4 ycbcr = float4(cameraSampleY.r, cameraSampleCbCr.rg, 1.0);
                    float4 cameraSample = ycbcrToRGBTransform * ycbcr;
                    cameraSample.a = 1.0;

                    return cameraSample;
                }
                else
                {
                    return 0;
                }
            }
        )";

        id<MTLLibrary> CompileShader(id<MTLDevice> metalDevice, const char* source) {
            NSError* error;
            id<MTLLibrary> lib = [metalDevice newLibraryWithSource:@(source) options:nil error:&error];
            if(nil != error) {
                throw std::runtime_error{[error.localizedDescription cStringUsingEncoding:NSASCIIStringEncoding]};
            }
            return lib;
        }
    }

    struct Camera::Impl::ImplData
    {
        ~ImplData()
        {
            [avCaptureSession stopRunning];
            [avCaptureSession release];
            [cameraTextureDelegate release];
            if (textureCache)
            {
                CVMetalTextureCacheFlush(textureCache, 0);
                CFRelease(textureCache);
            }
        }
        
        CameraTextureDelegate* cameraTextureDelegate{};
        AVCaptureSession* avCaptureSession{};
        CVMetalTextureCacheRef textureCache{};
        id<MTLTexture> textureY{};
        id<MTLTexture> textureCbCr{};
        id<MTLTexture> textureRGBA{};
        id<MTLRenderPipelineState> cameraPipelineState{};
        size_t width = 0;
        size_t height = 0;
        id<MTLDevice> metalDevice{};
        id<MTLCommandQueue> commandQueue{};
        id<MTLCommandBuffer> currentCommandBuffer{};
    };
    Camera::Impl::Impl(Napi::Env env, bool overrideCameraTexture)
        : m_deviceContext{nullptr}
        , m_env{env}
        , m_implData{std::make_unique<ImplData>()}
        , m_overrideCameraTexture{overrideCameraTexture}
    {
    }

    Camera::Impl::~Impl()
    {
    }

    arcana::task<void, std::exception_ptr> Camera::Impl::Open(uint32_t /*width*/, uint32_t /*height*/, bool frontCamera)
    {
        m_implData->commandQueue = (id<MTLCommandQueue>)bgfx::getInternalData()->commandQueue;

        auto metalDevice = (id<MTLDevice>)bgfx::getInternalData()->context;
        m_implData->metalDevice = metalDevice;

        id<MTLLibrary> lib = CompileShader(metalDevice, shaderSource);
        id<MTLFunction> vertexFunction = [lib newFunctionWithName:@"vertexShader"];
        id<MTLFunction> fragmentFunction = [lib newFunctionWithName:@"fragmentShader"];
        (void)vertexFunction;
        (void)fragmentFunction;

        // Create a pipeline state for drawing the camera texture to the RGBA target texture.
        {
            MTLRenderPipelineDescriptor *pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
            pipelineStateDescriptor.label = @"Native Camera YCbCr to RGBA Pipeline";
            pipelineStateDescriptor.vertexFunction = vertexFunction;
            pipelineStateDescriptor.fragmentFunction = fragmentFunction;
            pipelineStateDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

            NSError* error;
            m_implData->cameraPipelineState = [metalDevice newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&error];
            if (!m_implData->cameraPipelineState) {
                NSLog(@"Failed to create camera pipeline state: %@", error);
            }
        }

        if (!m_deviceContext) {
            m_deviceContext = &Graphics::DeviceContext::GetFromJavaScript(m_env);
        }
        
        __block arcana::task_completion_source<void, std::exception_ptr> taskCompletionSource{};

        dispatch_sync(dispatch_get_main_queue(), ^{
            
            CVMetalTextureCacheCreate(NULL, NULL, metalDevice, NULL, &m_implData->textureCache);
            
            m_implData->cameraTextureDelegate = [[CameraTextureDelegate alloc]init:m_implData];
            
            m_implData->avCaptureSession = [[AVCaptureSession alloc] init];
            
            NSError *error;
#if (TARGET_OS_IPHONE)
            AVCaptureDevicePosition preferredPosition;
            AVCaptureDeviceType preferredDeviceType;
            
            if (frontCamera) {
                preferredPosition = AVCaptureDevicePositionFront;
                preferredDeviceType = AVCaptureDeviceTypeBuiltInTrueDepthCamera;
            } else {
                preferredPosition = AVCaptureDevicePositionBack;
                preferredDeviceType = AVCaptureDeviceTypeBuiltInDualCamera;
            }

            // Set camera capture device to default and the media type to video.
            AVCaptureDevice* captureDevice = [AVCaptureDevice defaultDeviceWithDeviceType:preferredDeviceType mediaType:AVMediaTypeVideo position:preferredPosition];
            if (!captureDevice) {
                // If a rear dual camera is not available, default to the rear wide angle camera.
                captureDevice = [AVCaptureDevice defaultDeviceWithDeviceType:AVCaptureDeviceTypeBuiltInWideAngleCamera mediaType:AVMediaTypeVideo position:AVCaptureDevicePositionBack];
                
                // In the event that the rear wide angle camera isn't available, default to the front wide angle camera.
                if (!captureDevice) {
                    captureDevice = [AVCaptureDevice defaultDeviceWithDeviceType:AVCaptureDeviceTypeBuiltInWideAngleCamera mediaType:AVMediaTypeVideo position:AVCaptureDevicePositionFront];
                }
            }
#else
            UNUSED(frontCamera);
            AVCaptureDevice* captureDevice = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
#endif
            // Set video capture input: If there a problem initialising the camera, it will give am error.
            AVCaptureDeviceInput *input = [AVCaptureDeviceInput deviceInputWithDevice:captureDevice error:&error];

            if (!input) {
                taskCompletionSource.complete(arcana::make_unexpected(std::make_exception_ptr(std::runtime_error{"Error Getting Camera Input"})));
                return;
            }
            // Adding input souce for capture session. i.e., Camera
            [m_implData->avCaptureSession addInput:input];

            dispatch_queue_t sampleBufferQueue = dispatch_queue_create("CameraMulticaster", DISPATCH_QUEUE_SERIAL);

            AVCaptureVideoDataOutput * dataOutput = [[AVCaptureVideoDataOutput alloc] init];
            [dataOutput setAlwaysDiscardsLateVideoFrames:YES];
            [dataOutput setVideoSettings:@{(id)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange)}];
            [dataOutput setSampleBufferDelegate:m_implData->cameraTextureDelegate queue:sampleBufferQueue];

            [m_implData->avCaptureSession addOutput:dataOutput];
            [m_implData->avCaptureSession commitConfiguration];
            [m_implData->avCaptureSession startRunning];
            
            taskCompletionSource.complete();
        });
        
        return taskCompletionSource.as_task();
    }

    void Camera::Impl::SetTextureOverride(void* /*texturePtr*/)
    {
        if (!m_overrideCameraTexture)
        {
            throw std::runtime_error{"Trying to override NativeCamera Texture."};
        }
        // stub
    }

    void Camera::Impl::UpdateCameraTexture(bgfx::TextureHandle textureHandle)
    {
        arcana::make_task(m_deviceContext->BeforeRenderScheduler(), arcana::cancellation::none(), [this, textureHandle] {
            if (m_implData->textureY && m_implData->textureCbCr && m_implData->textureRGBA)
            {
                // Do the shader operation here for YCrCb to RGBA.
                //      - The override internal call would then only need to be done once when the texture is first setup in bgfx.

                m_implData->currentCommandBuffer = [m_implData->commandQueue commandBuffer];
                m_implData->currentCommandBuffer.label = @"NativeCameraCommandBuffer";
                MTLRenderPassDescriptor *renderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];

                // TODO: Maybe synchronize this like at Dependencies/xr/Source/ARKit/XR.mm:907?
                id<MTLTexture> textureY = m_implData->textureY;
                id<MTLTexture> textureCbCr = m_implData->textureCbCr;

                if (renderPassDescriptor != nil) {
                    // Attach the color texture, on which we'll draw the camera texture (so no need to clear on load).
                    renderPassDescriptor.colorAttachments[0].texture = m_implData->textureRGBA;
                    renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionDontCare;
                    renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;

                    // Create and end the render encoder.
                    id<MTLRenderCommandEncoder> renderEncoder = [m_implData->currentCommandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
                    renderEncoder.label = @"NativeCameraEncoder";

                    // Set the shader pipeline.
                    [renderEncoder setRenderPipelineState:m_implData->cameraPipelineState];

                    // Set the vertex data.
                    [renderEncoder setVertexBytes:vertices length:sizeof(vertices) atIndex:0];

                    // Set the textures.
                    [renderEncoder setFragmentTexture:textureY atIndex:1];
                    [renderEncoder setFragmentTexture:textureCbCr atIndex:2];

                    // Draw the triangles.
                    [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];

                    [renderEncoder endEncoding];

                    [m_implData->currentCommandBuffer addCompletedHandler:^(id<MTLCommandBuffer>) {
                        if (textureY != nil) {
                            [textureY setPurgeableState:MTLPurgeableStateEmpty];
                        }

                        if (textureCbCr != nil) {
                            [textureCbCr setPurgeableState:MTLPurgeableStateEmpty];
                        }
                    }];
                }

                // Finalize rendering here & push the command buffer to the GPU.
                [m_implData->currentCommandBuffer commit];

                bgfx::overrideInternal(textureHandle, reinterpret_cast<uintptr_t>(m_implData->textureRGBA));
            }

        });
    }

    void Camera::Impl::Close()
    {
        m_implData.reset();
    }
}

@implementation CameraTextureDelegate

- (id)init:(std::shared_ptr<Babylon::Plugins::Camera::Impl::ImplData>)implData {
    self = [super init];
    self->implData = implData;

    id<MTLDevice> graphicsContext = (id<MTLDevice>)bgfx::getInternalData()->context;

    CVReturn err = CVMetalTextureCacheCreate(kCFAllocatorDefault, nil, graphicsContext, nil, &implData->textureCache);
    if (err) {
        throw std::runtime_error{"Unable to create Texture Cache"};
    }

    return self;
}

- (void)captureOutput:(AVCaptureOutput *)__unused captureOutput didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)__unused connection {
    CVPixelBufferRef pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);

    // Update both metal textures used by the renderer to display the camera image.
    id<MTLTexture> textureY = [self getCameraTexture:pixelBuffer plane:0];
    id<MTLTexture> textureCbCr = [self getCameraTexture:pixelBuffer plane:1];

    if(textureY != nil && textureCbCr != nil)
    {
        dispatch_async(dispatch_get_main_queue(), ^{
            implData->textureY = textureY;
            implData->textureCbCr = textureCbCr;
        });
    }

    size_t width = CVPixelBufferGetWidthOfPlane(pixelBuffer, 0);
    size_t height = CVPixelBufferGetHeightOfPlane(pixelBuffer, 0);
    if (implData->width != width || implData->height != height) {
        implData->width = width;
        implData->height = height;

        dispatch_async(dispatch_get_main_queue(), ^{
            // TODO: Figure out if and how this texture is supposed to be freed.
            MTLTextureDescriptor *textureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:width height:height mipmapped:NO];
            textureDescriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
            implData->textureRGBA = [implData->metalDevice newTextureWithDescriptor:textureDescriptor];
        });
    }
}

/**
 Updates the captured texture with the current pixel buffer.
*/
- (id<MTLTexture>)getCameraTexture:(CVPixelBufferRef)pixelBuffer plane:(int)planeIndex {
    CVReturn ret = CVPixelBufferLockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
    if (ret != kCVReturnSuccess) {
        return {};
    }

    @try {
        size_t planeWidth = CVPixelBufferGetWidthOfPlane(pixelBuffer, planeIndex);
        size_t planeHeight = CVPixelBufferGetHeightOfPlane(pixelBuffer, planeIndex);

        // Plane 0 is the Y plane, which is in R8Unorm format, and the second plane is the CBCR plane which is RG8Unorm format.
        auto pixelFormat = planeIndex ? MTLPixelFormatRG8Unorm : MTLPixelFormatR8Unorm;
        CVMetalTextureRef textureRef;

        // Create a texture from the corresponding plane.
        auto status = CVMetalTextureCacheCreateTextureFromImage(kCFAllocatorDefault, implData->textureCache, pixelBuffer, nil, pixelFormat, planeWidth, planeHeight, planeIndex, &textureRef);
        if (status != kCVReturnSuccess) {
            return nil;
        }

        id<MTLTexture> texture = CVMetalTextureGetTexture(textureRef);
        CFRelease(textureRef);

        return texture;
    }
    @finally {
        CVPixelBufferUnlockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
    }
}

@end
