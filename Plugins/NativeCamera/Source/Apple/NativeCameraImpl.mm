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
        // constexpr char shaderSource[] = R"(
        //     #include <metal_stdlib>
        //     #include <simd/simd.h>

        //     using namespace metal;

        //     #include <simd/simd.h>

        //     typedef struct
        //     {
        //         vector_float2 position;
        //         vector_float2 uv;
        //         vector_float2 cameraUV;
        //     } XRVertex;

        //     typedef struct
        //     {
        //         float4 position [[position]];
        //         float2 uv;
        //         float2 cameraUV;
        //     } RasterizerData;

        //     vertex RasterizerData
        //     vertexShader(uint vertexID [[vertex_id]],
        //                  constant XRVertex *vertices [[buffer(0)]])
        //     {
        //         RasterizerData out;
        //         out.position = vector_float4(vertices[vertexID].position.xy, 0.0, 1.0);
        //         out.uv = vertices[vertexID].uv;
        //         out.cameraUV = vertices[vertexID].cameraUV;
        //         return out;
        //     }

        //     fragment float4 fragmentShader(RasterizerData in [[stage_in]],
        //         texture2d<float, access::sample> babylonTexture [[ texture(0) ]],
        //         texture2d<float, access::sample> cameraTextureY [[ texture(1) ]],
        //         texture2d<float, access::sample> cameraTextureCbCr [[ texture(2) ]])
        //     {
        //         constexpr sampler linearSampler(mip_filter::linear, mag_filter::linear, min_filter::linear);

        //         if (!is_null_texture(babylonTexture))
        //         {
        //             return babylonTexture.sample(linearSampler, in.uv);
        //         }
        //         else if (!is_null_texture(cameraTextureY) && !is_null_texture(cameraTextureCbCr))
        //         {
        //             const float4 cameraSampleY = cameraTextureY.sample(linearSampler, in.cameraUV);
        //             const float4 cameraSampleCbCr = cameraTextureCbCr.sample(linearSampler, in.cameraUV);

        //             const float4x4 ycbcrToRGBTransform = float4x4(
        //                 float4(+1.0000f, +1.0000f, +1.0000f, +0.0000f),
        //                 float4(+0.0000f, -0.3441f, +1.7720f, +0.0000f),
        //                 float4(+1.4020f, -0.7141f, +0.0000f, +0.0000f),
        //                 float4(-0.7010f, +0.5291f, -0.8860f, +1.0000f)
        //             );

        //             float4 ycbcr = float4(cameraSampleY.r, cameraSampleCbCr.rg, 1.0);
        //             float4 cameraSample = ycbcrToRGBTransform * ycbcr;
        //             cameraSample.a = 1.0;

        //             return cameraSample;
        //         }
        //         else
        //         {
        //             return 0;
        //         }
        //     }
        // )";

        // id<MTLLibrary> CompileShader(id<MTLDevice> metalDevice, const char* source) {
        //     NSError* error;
        //     id<MTLLibrary> lib = [metalDevice newLibraryWithSource:@(source) options:nil error:&error];
        //     if(nil != error) {
        //         throw std::runtime_error{[error.localizedDescription cStringUsingEncoding:NSASCIIStringEncoding]};
        //     }
        //     return lib;
        // }
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
        id<MTLRenderPipelineState> cameraPipelineState{};
        size_t width = 0;
        size_t height = 0;
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
        auto metalDevice = (id<MTLDevice>)bgfx::getInternalData()->context;
        // auto commandQueue = (id<MTLCommandQueue>)bgfx::getInternalData()->commandQueue;
        // (void)commandQueue;

        // id<MTLLibrary> lib = CompileShader(metalDevice, shaderSource);
        // id<MTLFunction> vertexFunction = [lib newFunctionWithName:@"vertexShader"];
        // id<MTLFunction> fragmentFunction = [lib newFunctionWithName:@"fragmentShader"];
        // (void)vertexFunction;
        // (void)fragmentFunction;

        // Create a pipeline state for drawing the camera texture to the RGBA target texture.
        // {
        //     MTLRenderPipelineDescriptor *pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
        //     pipelineStateDescriptor.label = @"Native Camera YCbCr to RGBA Pipeline";
        //     pipelineStateDescriptor.vertexFunction = vertexFunction;
        //     pipelineStateDescriptor.fragmentFunction = fragmentFunction;
        //     pipelineStateDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

        //     NSError* error;
        //     m_implData->cameraPipelineState = [metalDevice newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&error];
        //     if (!m_implData->cameraPipelineState) {
        //         NSLog(@"Failed to create camera pipeline state: %@", error);
        //     }
        // }

        NSDictionary *formats = [NSDictionary dictionaryWithObjectsAndKeys:
                @"kCVPixelFormatType_1Monochrome", [NSNumber numberWithInt:kCVPixelFormatType_1Monochrome],
                @"kCVPixelFormatType_2Indexed", [NSNumber numberWithInt:kCVPixelFormatType_2Indexed],
                @"kCVPixelFormatType_4Indexed", [NSNumber numberWithInt:kCVPixelFormatType_4Indexed],
                @"kCVPixelFormatType_8Indexed", [NSNumber numberWithInt:kCVPixelFormatType_8Indexed],
                @"kCVPixelFormatType_1IndexedGray_WhiteIsZero", [NSNumber numberWithInt:kCVPixelFormatType_1IndexedGray_WhiteIsZero],
                @"kCVPixelFormatType_2IndexedGray_WhiteIsZero", [NSNumber numberWithInt:kCVPixelFormatType_2IndexedGray_WhiteIsZero],
                @"kCVPixelFormatType_4IndexedGray_WhiteIsZero", [NSNumber numberWithInt:kCVPixelFormatType_4IndexedGray_WhiteIsZero],
                @"kCVPixelFormatType_8IndexedGray_WhiteIsZero", [NSNumber numberWithInt:kCVPixelFormatType_8IndexedGray_WhiteIsZero],
                @"kCVPixelFormatType_16BE555", [NSNumber numberWithInt:kCVPixelFormatType_16BE555],
                @"kCVPixelFormatType_16LE555", [NSNumber numberWithInt:kCVPixelFormatType_16LE555],
                @"kCVPixelFormatType_16LE5551", [NSNumber numberWithInt:kCVPixelFormatType_16LE5551],
                @"kCVPixelFormatType_16BE565", [NSNumber numberWithInt:kCVPixelFormatType_16BE565],
                @"kCVPixelFormatType_16LE565", [NSNumber numberWithInt:kCVPixelFormatType_16LE565],
                @"kCVPixelFormatType_24RGB", [NSNumber numberWithInt:kCVPixelFormatType_24RGB],
                @"kCVPixelFormatType_24BGR", [NSNumber numberWithInt:kCVPixelFormatType_24BGR],
                @"kCVPixelFormatType_32ARGB", [NSNumber numberWithInt:kCVPixelFormatType_32ARGB],
                @"kCVPixelFormatType_32BGRA", [NSNumber numberWithInt:kCVPixelFormatType_32BGRA],
                @"kCVPixelFormatType_32ABGR", [NSNumber numberWithInt:kCVPixelFormatType_32ABGR],
                @"kCVPixelFormatType_32RGBA", [NSNumber numberWithInt:kCVPixelFormatType_32RGBA],
                @"kCVPixelFormatType_64ARGB", [NSNumber numberWithInt:kCVPixelFormatType_64ARGB],
                @"kCVPixelFormatType_48RGB", [NSNumber numberWithInt:kCVPixelFormatType_48RGB],
                @"kCVPixelFormatType_32AlphaGray", [NSNumber numberWithInt:kCVPixelFormatType_32AlphaGray],
                @"kCVPixelFormatType_16Gray", [NSNumber numberWithInt:kCVPixelFormatType_16Gray],
                @"kCVPixelFormatType_422YpCbCr8", [NSNumber numberWithInt:kCVPixelFormatType_422YpCbCr8],
                @"kCVPixelFormatType_4444YpCbCrA8", [NSNumber numberWithInt:kCVPixelFormatType_4444YpCbCrA8],
                @"kCVPixelFormatType_4444YpCbCrA8R", [NSNumber numberWithInt:kCVPixelFormatType_4444YpCbCrA8R],
                @"kCVPixelFormatType_444YpCbCr8", [NSNumber numberWithInt:kCVPixelFormatType_444YpCbCr8],
                @"kCVPixelFormatType_422YpCbCr16", [NSNumber numberWithInt:kCVPixelFormatType_422YpCbCr16],
                @"kCVPixelFormatType_422YpCbCr10", [NSNumber numberWithInt:kCVPixelFormatType_422YpCbCr10],
                @"kCVPixelFormatType_444YpCbCr10", [NSNumber numberWithInt:kCVPixelFormatType_444YpCbCr10],
                @"kCVPixelFormatType_420YpCbCr8Planar", [NSNumber numberWithInt:kCVPixelFormatType_420YpCbCr8Planar],
                @"kCVPixelFormatType_420YpCbCr8PlanarFullRange", [NSNumber numberWithInt:kCVPixelFormatType_420YpCbCr8PlanarFullRange],
                @"kCVPixelFormatType_422YpCbCr_4A_8BiPlanar", [NSNumber numberWithInt:kCVPixelFormatType_422YpCbCr_4A_8BiPlanar],
                @"kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange", [NSNumber numberWithInt:kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange],
                @"kCVPixelFormatType_420YpCbCr8BiPlanarFullRange", [NSNumber numberWithInt:kCVPixelFormatType_420YpCbCr8BiPlanarFullRange],
                @"kCVPixelFormatType_422YpCbCr8_yuvs", [NSNumber numberWithInt:kCVPixelFormatType_422YpCbCr8_yuvs],
                @"kCVPixelFormatType_422YpCbCr8FullRange", [NSNumber numberWithInt:kCVPixelFormatType_422YpCbCr8FullRange],
            nil];
        AVCaptureVideoDataOutput* dataOutput = [[AVCaptureVideoDataOutput alloc] init];
        NSArray<NSNumber*>* availableFormats = [dataOutput availableVideoCVPixelFormatTypes];
        for (NSNumber* format : availableFormats) {
            NSLog(@"%@", [formats objectForKey:format]);
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
            if (m_implData->textureY && m_implData->textureCbCr)
            {
                // Do the shader operation here for BGRA to RGBA.
                //      - The override internal call would then only need to be done once when the texture is first setup in bgfx.
                bgfx::overrideInternal(textureHandle, reinterpret_cast<uintptr_t>(m_implData->textureY));
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

    implData->width = CVPixelBufferGetWidthOfPlane(pixelBuffer, 0);
    implData->height = CVPixelBufferGetHeightOfPlane(pixelBuffer, 0);

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
