#import <Foundation/Foundation.h>
#import <CompositorServices/CompositorServices.h>

@class CAMetalLayer;

@interface LibNativeBridge : NSObject

@property (nonatomic, assign, getter=isInitialized) BOOL initialized;
@property (nonatomic, strong) CAMetalLayer *metalLayer;
@property (nonatomic, assign, getter=isImmersive) BOOL immersive;

+ (instancetype)sharedInstance;

- (void)setTouchDown:(int)pointerId x:(int)inX y:(int)inY;
- (void)setTouchMove:(int)pointerId x:(int)inX y:(int)inY;
- (void)setTouchUp:(int)pointerId x:(int)inX y:(int)inY;

- (void)drawableWillChangeSizeWithWidth:(NSInteger)width height:(NSInteger)height;

- (bool)initializeWithWidth:(NSInteger)width height:(NSInteger)height;
- (void)shutdown;
- (void)render;

// Immersive mode support
- (void)initializeImmersiveWithLayerRenderer:(cp_layer_renderer_t)layerRenderer;
- (void)renderImmersive;
- (void)processSpatialEvents:(ar_data_providers_t)dataProviders;

@end

