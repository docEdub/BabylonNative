#if ! __has_feature(objc_arc)
#error "ARC is off"
#endif

#include "../../Include/XR.h"
#include "../../Include/XRHelpers.h"

#import <UIKit/UIKit.h>
#import <CompositorServices/CompositorServices.h>
#import <MetalKit/MetalKit.h>
#import <Foundation/Foundation.h>
#include <stdexcept>

#include "Include/IXrContextVisionOS.h"

namespace {
    typedef struct {
        vector_float2 position;
        vector_float2 uv;
    } XRVertex;

    /**
     Defines the 2D positions and mapping UVs for rendering in VisionOS immersive mode.
     */
    static XRVertex vertices[] = {
        // 2D positions, UV
        { { -1, -1 },   { 0, 1 } },
        { { -1, 1 },    { 0, 0 } },
        { { 1, -1 },    { 1, 1 } },
        { { 1, 1 },     { 1, 0 } },
    };

    /**
     Helper function to convert a transform into an xr::pose.
     */
    static xr::Pose TransformToPose(simd_float4x4 transform) {
        xr::Pose pose{};
        auto orientation = simd_quaternion(transform);
        pose.Orientation = { orientation.vector.x,
                           orientation.vector.y,
                           orientation.vector.z,
                           orientation.vector.w };

        pose.Position = { transform.columns[3][0],
                        transform.columns[3][1],
                        transform.columns[3][2] };

        return pose;
    }

    /**
     Helper function to convert an xr pose into a transform.
     */
    static simd_float4x4 PoseToTransform(xr::Pose pose) {
        auto poseQuaternion = simd_quaternion(pose.Orientation.X, pose.Orientation.Y, pose.Orientation.Z, pose.Orientation.W);
        auto poseTransform = simd_matrix4x4(poseQuaternion);
        poseTransform.columns[3][0] = pose.Position.X;
        poseTransform.columns[3][1] = pose.Position.Y;
        poseTransform.columns[3][2] = pose.Position.Z;

        return poseTransform;
    }
}

namespace xr {
    namespace {
        // VisionOS immersive rendering shader
        constexpr char shaderSource[] = R"(
            #include <metal_stdlib>
            #include <simd/simd.h>

            using namespace metal;

            typedef struct
            {
                vector_float2 position;
                vector_float2 uv;
            } XRVertex;

            typedef struct
            {
                float4 position [[position]];
                float2 uv;
            } RasterizerData;

            vertex RasterizerData
            vertexShader(uint vertexID [[vertex_id]],
                         constant XRVertex *vertices [[buffer(0)]])
            {
                RasterizerData out;
                out.position = vector_float4(vertices[vertexID].position.xy, 0.0, 1.0);
                out.uv = vertices[vertexID].uv;
                return out;
            }

            fragment float4 fragmentShader(RasterizerData in [[stage_in]],
                texture2d<float, access::sample> babylonTexture [[ texture(0) ]])
            {
                constexpr sampler linearSampler(mip_filter::linear, mag_filter::linear, min_filter::linear);

                if (!is_null_texture(babylonTexture))
                {
                    return babylonTexture.sample(linearSampler, in.uv);
                }
                else
                {
                    return float4(0.0, 0.0, 0.0, 1.0);
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

    struct XrContextVisionOS : public IXrContextVisionOS {
        bool Initialized{true};
        bool ImmersiveSession{false};
        void* CompositorLayer{nullptr};

        bool IsInitialized() const override
        {
            return Initialized;
        }

        bool IsImmersiveSessionActive() const override
        {
            return ImmersiveSession;
        }

        void SetImmersiveSessionActive(bool active) override
        {
            ImmersiveSession = active;
            if (active) {
                StartImmersiveSession();
            } else {
                StopImmersiveSession();
            }
        }

        void* GetCompositorLayer() const override
        {
            return CompositorLayer;
        }

        void SetCompositorLayer(void* layer) override
        {
            CompositorLayer = layer;
        }

        void StartImmersiveSession() {
            NSLog(@"Starting immersive session");
            // Post notification to Swift app to hide main window
            [[NSNotificationCenter defaultCenter] postNotificationName:@"immersiveModeChanged" object:@YES];
        }

        void StopImmersiveSession() {
            NSLog(@"Stopping immersive session");
            // Post notification to Swift app to show main window
            [[NSNotificationCenter defaultCenter] postNotificationName:@"immersiveModeChanged" object:@NO];
        }

        virtual ~XrContextVisionOS() = default;
    };

    struct System::Impl {
    public:
        std::unique_ptr<XrContextVisionOS> XrContext{std::make_unique<XrContextVisionOS>()};

        Impl(const std::string&) {}

        bool IsInitialized() const {
            return XrContext->IsInitialized();
        }

        bool TryInitialize() {
            return true;
        }

        uintptr_t GetNativeXrContext() const {
            return reinterpret_cast<uintptr_t>(XrContext.get());
        }

        std::string GetNativeXrContextType() const {
            return "VisionOS";
        }
    };

    struct System::Session::Impl {
    public:
        const System::Impl& SystemImpl;
        std::vector<Frame::View> ActiveFrameViews{ {} };
        std::vector<Frame::InputSource> InputSources;
        std::vector<Frame::Plane> Planes{};
        std::vector<Frame::Mesh> Meshes{};
        std::vector<std::unique_ptr<Frame::ImageTrackingResult>> ImageTrackingResults{};
        std::vector<FeaturePoint> FeaturePointCloud{};
        std::optional<Space> EyeTrackerSpace{};
        float DepthNearZ{ DEFAULT_DEPTH_NEAR_Z };
        float DepthFarZ{ DEFAULT_DEPTH_FAR_Z };
        bool FeaturePointCloudEnabled{ false };

        Impl(System::Impl& systemImpl, void* graphicsContext, void* commandQueue, std::function<void*()> windowProvider)
            : SystemImpl{ systemImpl }
            , getXRView{ [windowProvider{ std::move(windowProvider) }] { return (__bridge MTKView*)windowProvider(); } }
            , metalDevice{ (__bridge id<MTLDevice>)graphicsContext }
            , commandQueue{ (__bridge id<MTLCommandQueue>)commandQueue } {

            // Initialize immersive rendering components
            id<MTLLibrary> lib = CompileShader(metalDevice, shaderSource);
            id<MTLFunction> vertexFunction = [lib newFunctionWithName:@"vertexShader"];
            id<MTLFunction> fragmentFunction = [lib newFunctionWithName:@"fragmentShader"];

            // Create a pipeline state for immersive rendering
            {
                MTLRenderPipelineDescriptor *pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
                pipelineStateDescriptor.label = @"VisionOS Immersive Pipeline";
                pipelineStateDescriptor.vertexFunction = vertexFunction;
                pipelineStateDescriptor.fragmentFunction = fragmentFunction;
                pipelineStateDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
                pipelineStateDescriptor.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float_Stencil8;

                NSError *error = nil;
                immersivePipelineState = [metalDevice newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&error];
                if (error) {
                    throw std::runtime_error{[error.localizedDescription cStringUsingEncoding:NSASCIIStringEncoding]};
                }
            }

            // Create vertex buffer
            vertexBuffer = [metalDevice newBufferWithBytes:vertices length:sizeof(vertices) options:MTLResourceStorageModeShared];
        }

        bool IsSessionSupportedForType(SessionType sessionType) const {
            switch (sessionType) {
                case SessionType::IMMERSIVE_VR:
                    return true;
                case SessionType::IMMERSIVE_AR:
                    return false; // VisionOS AR is not implemented yet
                case SessionType::INLINE:
                    return false; // Inline sessions not supported
                default:
                    return false;
            }
        }

        void SetDepthsNearFar(float depthNear, float depthFar) {
            DepthNearZ = depthNear;
            DepthFarZ = depthFar;
        }

        void SetPlaneDetectionEnabled(bool enabled) {
            // VisionOS plane detection not implemented yet
        }

        bool TrySetFeaturePointCloudEnabled(bool enabled) {
            // VisionOS feature point cloud not implemented yet
            return false;
        }

        bool TrySetPreferredPlaneDetectorOptions(const xr::GeometryDetectorOptions& options) {
            // VisionOS plane detection not implemented yet
            return false;
        }

        bool TrySetMeshDetectorEnabled(const bool enabled) {
            // VisionOS mesh detection not implemented yet
            return false;
        }

        bool TrySetPreferredMeshDetectorOptions(const xr::GeometryDetectorOptions& options) {
            // VisionOS mesh detection not implemented yet
            return false;
        }

        std::vector<xr::ImageTrackingScore>* GetImageTrackingScores() {
            // VisionOS image tracking not implemented yet
            return nullptr;
        }

        void CreateAugmentedImageDatabase(const std::vector<xr::System::Session::ImageTrackingRequest>& requests) {
            // VisionOS image tracking not implemented yet
        }

        void RequestEndSession() {
            SystemImpl.XrContext->SetImmersiveSessionActive(false);
        }

        std::unique_ptr<System::Session::Frame> GetNextFrame(bool& shouldEndSession, bool& shouldRestartSession, std::function<arcana::task<void, std::exception_ptr>(void*)> deletedTextureAsyncCallback) {
            shouldEndSession = false;
            shouldRestartSession = false;

            if (!SystemImpl.XrContext->IsImmersiveSessionActive()) {
                shouldEndSession = true;
                return nullptr;
            }

            // Create a frame for immersive rendering
            auto frame = std::make_unique<System::Session::Frame>(*this);
            
            // Set up basic immersive view
            if (ActiveFrameViews.empty()) {
                ActiveFrameViews.resize(1);
                ActiveFrameViews[0].Space = Space::STAGE;
                ActiveFrameViews[0].FieldOfView = { 0.785f, 0.785f, 0.785f, 0.785f }; // 45 degrees
                ActiveFrameViews[0].Size = { 1024, 1024 };
                
                // Set up view transform for immersive space
                simd_float4x4 viewTransform = matrix_identity_float4x4;
                viewTransform.columns[3][2] = 2.0f; // Move back 2 units from origin
                ActiveFrameViews[0].Pose = TransformToPose(viewTransform);
            }

            return frame;
        }

    private:
        std::function<MTKView*()> getXRView;
        id<MTLDevice> metalDevice;
        id<MTLCommandQueue> commandQueue;
        id<MTLRenderPipelineState> immersivePipelineState;
        id<MTLBuffer> vertexBuffer;
        
        static constexpr float DEFAULT_DEPTH_NEAR_Z = 0.01f;
        static constexpr float DEFAULT_DEPTH_FAR_Z = 1000.0f;
    };

    struct System::Session::Frame::Impl {
    public:
        Impl(Session::Impl& sessionImpl)
            : sessionImpl{sessionImpl} { }

        Session::Impl& sessionImpl;
    };

    System::Session::Frame::Frame(Session::Impl& sessionImpl)
        : Views{ sessionImpl.ActiveFrameViews }
        , InputSources{ sessionImpl.InputSources}
        , FeaturePointCloud{ sessionImpl.FeaturePointCloud }
        , EyeTrackerSpace{ sessionImpl.EyeTrackerSpace }
        , UpdatedPlanes{}
        , RemovedPlanes{}
        , UpdatedMeshes{}
        , RemovedMeshes{}
        , UpdatedImageTrackingResults{}
        , IsTracking{true}
        , m_impl{ std::make_unique<System::Session::Frame::Impl>(sessionImpl) } {
        if (!Views.empty()) {
            Views[0].DepthNearZ = sessionImpl.DepthNearZ;
            Views[0].DepthFarZ = sessionImpl.DepthFarZ;
        }
    }

    System::Session::Frame::~Frame() {
    }

    void System::Session::Frame::Render() {
        // VisionOS immersive rendering implementation
    }

    void System::Session::Frame::GetHitTestResults(std::vector<HitResult>& filteredResults, xr::Ray offsetRay, xr::HitTestTrackableType trackableTypes) const {
        // VisionOS hit testing not implemented yet
    }

    Anchor System::Session::Frame::CreateAnchor(Pose pose, NativeTrackablePtr) const {
        // VisionOS anchor creation not implemented yet
        return Anchor{};
    }

    Anchor System::Session::Frame::DeclareAnchor(NativeTrackablePtr) const {
        // VisionOS anchor declaration not implemented yet
        return Anchor{};
    }

    void System::Session::Frame::UpdateAnchor(Anchor&) const {
        // VisionOS anchor update not implemented yet
    }

    void System::Session::Frame::DeleteAnchor(Anchor&) const {
        // VisionOS anchor deletion not implemented yet
    }

    SceneObject& System::Session::Frame::GetSceneObjectByID(SceneObject::Identifier) const {
        // VisionOS scene object retrieval not implemented yet
        static SceneObject dummyObject{};
        return dummyObject;
    }

    Plane& System::Session::Frame::GetPlaneByID(Plane::Identifier) const {
        // VisionOS plane retrieval not implemented yet
        static Plane dummyPlane{};
        return dummyPlane;
    }

    Mesh& System::Session::Frame::GetMeshByID(Mesh::Identifier) const {
        // VisionOS mesh retrieval not implemented yet
        static Mesh dummyMesh{};
        return dummyMesh;
    }

    ImageTrackingResult& System::Session::Frame::GetImageTrackingResultByID(ImageTrackingResult::Identifier) const {
        // VisionOS image tracking result retrieval not implemented yet
        static ImageTrackingResult dummyResult{};
        return dummyResult;
    }

    System::System(const char* appName)
        : m_impl{ std::make_unique<System::Impl>(appName) } {}

    System::~System() {}

    bool System::IsInitialized() const {
        return m_impl->IsInitialized();
    }

    bool System::TryInitialize() {
        return m_impl->TryInitialize();
    }

    arcana::task<bool, std::exception_ptr> System::IsSessionSupportedAsync(SessionType sessionType) {
        // Only IMMERSIVE_VR is supported for visionOS for now.
        return arcana::task_from_result<std::exception_ptr>(sessionType == SessionType::IMMERSIVE_VR);
    }

    uintptr_t System::GetNativeXrContext() {
        return reinterpret_cast<uintptr_t>(m_impl->XrContext.get());
    }

    std::string System::GetNativeXrContextType() {
        return "VisionOS";
    }

    arcana::task<std::shared_ptr<System::Session>, std::exception_ptr> System::Session::CreateAsync(System& system, void* graphicsDevice, void* commandQueue, std::function<void*()> windowProvider) {
        auto session = std::make_shared<System::Session>(system, graphicsDevice, commandQueue, std::move(windowProvider));
        return arcana::task_from_result<std::exception_ptr>(session);
    }

    System::Session::Session(System& system, void* graphicsDevice, void* commandQueue, std::function<void*()> windowProvider)
        : m_impl{ std::make_unique<System::Session::Impl>(*system.m_impl, graphicsDevice, commandQueue, std::move(windowProvider)) } {}

    System::Session::~Session() {}

    std::unique_ptr<System::Session::Frame> System::Session::GetNextFrame(bool& shouldEndSession, bool& shouldRestartSession, std::function<arcana::task<void, std::exception_ptr>(void*)> deletedTextureAsyncCallback) {
        return m_impl->GetNextFrame(shouldEndSession, shouldRestartSession, deletedTextureAsyncCallback);
    }

    void System::Session::RequestEndSession() {
        m_impl->RequestEndSession();
    }

    void System::Session::SetDepthsNearFar(float depthNear, float depthFar) {
        m_impl->SetDepthsNearFar(depthNear, depthFar);
    }

    void System::Session::SetPlaneDetectionEnabled(bool enabled) const {
        m_impl->SetPlaneDetectionEnabled(enabled);
    }

    bool System::Session::TrySetFeaturePointCloudEnabled(bool enabled) const {
        return m_impl->TrySetFeaturePointCloudEnabled(enabled);
    }

    bool System::Session::TrySetPreferredPlaneDetectorOptions(const GeometryDetectorOptions& options) {
        return m_impl->TrySetPreferredPlaneDetectorOptions(options);
    }

    bool System::Session::TrySetMeshDetectorEnabled(const bool enabled) {
        return m_impl->TrySetMeshDetectorEnabled(enabled);
    }

    bool System::Session::TrySetPreferredMeshDetectorOptions(const GeometryDetectorOptions& options) {
        return m_impl->TrySetPreferredMeshDetectorOptions(options);
    }

    std::vector<ImageTrackingScore>* System::Session::GetImageTrackingScores() const {
        return m_impl->GetImageTrackingScores();
    }

    void System::Session::CreateAugmentedImageDatabase(const std::vector<ImageTrackingRequest>& requests) const {
        m_impl->CreateAugmentedImageDatabase(requests);
    }
}
