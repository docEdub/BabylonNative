#import "LibNativeBridge.h"
#import <Babylon/AppRuntime.h>
#import <Babylon/Graphics/Device.h>
#import <Babylon/ScriptLoader.h>
#import <Babylon/Plugins/NativeEngine.h>
#import <Babylon/Plugins/NativeInput.h>
#import <Babylon/Plugins/NativeOptimizations.h>
#import <Babylon/Polyfills/Canvas.h>
#import <Babylon/Polyfills/Console.h>
#import <Babylon/Polyfills/Window.h>
#import <Babylon/Polyfills/XMLHttpRequest.h>
#import <Metal/Metal.h>
#import <simd/simd.h>

@implementation LibNativeBridge {
  std::optional<Babylon::Graphics::Device> _device;
  std::optional<Babylon::Graphics::DeviceUpdate> _update;
  std::optional<Babylon::AppRuntime> _runtime;
  std::optional<Babylon::Polyfills::Canvas> _nativeCanvas;
  Babylon::Plugins::NativeInput* _nativeInput;
  bool _isXrActive;
  CADisplayLink *_displayLink;
  
  // Immersive mode properties
  cp_layer_renderer_t _layerRenderer;
  cp_frame_t _frame;
  cp_drawable_t _drawable;
  NSInteger _viewportWidth;
  NSInteger _viewportHeight;
}

+ (instancetype)sharedInstance {
  static LibNativeBridge *sharedInstance = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    sharedInstance = [[self alloc] init];
  });
  return sharedInstance;
}

- (bool)initializeWithWidth:(NSInteger)width height:(NSInteger)height {
    if (self.initialized) {
        return YES;
    }
  
    _displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(render)];
    [_displayLink addToRunLoop:NSRunLoop.mainRunLoop forMode:NSDefaultRunLoopMode];
    
    Babylon::Graphics::Configuration graphicsConfig{};
    if (self.metalLayer) {
        graphicsConfig.Window = self.metalLayer;
    }
    graphicsConfig.Width = static_cast<size_t>(width);
    graphicsConfig.Height = static_cast<size_t>(height);

    _device.emplace(graphicsConfig);
    _update.emplace(_device->GetUpdate("update"));

    _device->StartRenderingCurrentFrame();
    _update->Start();

    _runtime.emplace();

    _runtime->Dispatch([self](Napi::Env env) {
        self->_device->AddToJavaScript(env);

        Babylon::Polyfills::Console::Initialize(env, [](const char* message, auto) {
            NSLog(@"%s", message);
        });

        self->_nativeCanvas.emplace(Babylon::Polyfills::Canvas::Initialize(env));

        Babylon::Polyfills::Window::Initialize(env);

        Babylon::Polyfills::XMLHttpRequest::Initialize(env);

        Babylon::Plugins::NativeEngine::Initialize(env);

        Babylon::Plugins::NativeOptimizations::Initialize(env);
     
        _nativeInput = &Babylon::Plugins::NativeInput::CreateForJavaScript(env);
    });

    Babylon::ScriptLoader loader{ *_runtime };
    loader.LoadScript("app:///Scripts/ammo.js");
    loader.LoadScript("app:///Scripts/recast.js");
    loader.LoadScript("app:///Scripts/babylon.max.js");
    loader.LoadScript("app:///Scripts/babylonjs.loaders.js");
    loader.LoadScript("app:///Scripts/babylonjs.materials.js");
    loader.LoadScript("app:///Scripts/babylon.gui.js");
    loader.LoadScript("app:///Scripts/experience.js");
    self.initialized = YES;
    
    return true;
}

- (void)drawableWillChangeSizeWithWidth:(NSInteger)width height:(NSInteger)height {
    if (_device) {
        _update->Finish();
        _device->FinishRenderingCurrentFrame();

        _device->UpdateSize(static_cast<size_t>(width), static_cast<size_t>(height));

        _device->StartRenderingCurrentFrame();
        _update->Start();
    }
}

- (void)setTouchDown:(int)pointerId x:(int)inX y:(int)inY {
  if (_nativeInput) {
    _nativeInput->TouchDown(static_cast<int>(pointerId), static_cast<int>(inX), static_cast<int>(inY));
  }
}

- (void)setTouchMove:(int)pointerId x:(int)inX y:(int)inY {
  if (_nativeInput) {
    _nativeInput->TouchMove(static_cast<int>(pointerId), static_cast<int>(inX), static_cast<int>(inY));
  }
}

- (void)setTouchUp:(int)pointerId x:(int)inX y:(int)inY {
  if (_nativeInput) {
    _nativeInput->TouchUp(static_cast<int>(pointerId), static_cast<int>(inX), static_cast<int>(inY));
  }
}

- (void)render {
    if (_device && self.initialized) {
        _update->Finish();
        _device->FinishRenderingCurrentFrame();
        _device->StartRenderingCurrentFrame();
        _update->Start();
    }
}

- (void)shutdown {
    if (!self.initialized) {
        return;
    }
    if (_device) {
        _update->Finish();
        _device->FinishRenderingCurrentFrame();
    }

    _nativeInput = nullptr;
    _nativeCanvas.reset();
    _runtime.reset();
    _update.reset();
    _device.reset();
    [_displayLink invalidate];
    _displayLink = NULL;
    self.initialized = NO;
}

- (void)dealloc {
    [self shutdown];
}

#pragma mark - Immersive Mode Support

- (void)initializeImmersiveWithLayerRenderer:(cp_layer_renderer_t)layerRenderer {
    _layerRenderer = layerRenderer;
    self.immersive = YES;
    
    // Get layer configuration
    cp_layer_renderer_configuration_t config = cp_layer_renderer_get_configuration(_layerRenderer);
    cp_layer_renderer_layout layout = cp_layer_renderer_configuration_get_layout(config);
    
    // Initialize viewport dimensions based on layout
    if (layout == cp_layer_renderer_layout_shared) {
        _viewportWidth = 2048;
        _viewportHeight = 2048;
    } else {
        // For stereo/viewpoint layouts
        _viewportWidth = 1024;
        _viewportHeight = 1024;
    }
    
    // Initialize Babylon if not already initialized
    if (!self.initialized) {
        // Initialize without a metal layer for immersive mode
        [self initializeWithWidth:_viewportWidth height:_viewportHeight];
    }
    
    // Stop regular display link if running
    if (_displayLink) {
        [_displayLink invalidate];
        _displayLink = nil;
    }
    
    // Start render loop for immersive mode
    dispatch_async(dispatch_get_main_queue(), ^{
        [self startImmersiveRenderLoop];
    });
}

- (void)startImmersiveRenderLoop {
    if (!self.immersive || !_layerRenderer) {
        return;
    }
    
    @autoreleasepool {
        [self renderImmersive];
    }
    
    // Schedule next frame
    dispatch_async(dispatch_get_main_queue(), ^{
        [self startImmersiveRenderLoop];
    });
}

- (void)renderImmersive {
    if (!_layerRenderer || !self.initialized || !_device) {
        return;
    }
    
    // Wait for new frame timing
    _frame = cp_layer_renderer_query_next_frame(_layerRenderer);
    if (_frame == nil) {
        return;
    }
    
    // Get frame timing
    cp_frame_timing_t timing = cp_frame_get_timing(_frame);
    cp_time_t displayTime = cp_frame_timing_get_presentation_time(timing);
    
    // Wait for optimal render time
    cp_frame_timing_wait_until_optimal_input_time(timing);
    
    // Start frame submission
    cp_frame_start_submission(_frame);
    
    // Get drawable
    _drawable = cp_frame_query_drawable(_frame);
    if (_drawable == nil) {
        cp_frame_end_submission(_frame);
        return;
    }
    
    // For now, render the same content to all views
    // In a full implementation, we would update camera matrices per view
    if (_device && _update) {
        _update->Finish();
        _device->FinishRenderingCurrentFrame();
        _device->StartRenderingCurrentFrame();
        _update->Start();
    }
    
    // Present the drawable
    cp_drawable_encode(_drawable);
    
    // End frame submission
    cp_frame_end_submission(_frame);
}

- (void)processSpatialEvents:(ar_data_providers_t)dataProviders {
    // Process hand tracking or other spatial input events
    // This would integrate with NativeInput for spatial interactions
}

@end
