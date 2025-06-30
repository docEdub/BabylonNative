import SwiftUI
import CompositorServices
import simd

struct BabylonLayerConfiguration: CompositorLayerConfiguration {
  func makeConfiguration(capabilities: LayerRenderer.Capabilities, configuration: inout LayerRenderer.Configuration) {
    configuration.depthFormat = .depth32Float
    configuration.colorFormat = .bgra8Unorm_srgb
    
    let options = LayerRenderer.Capabilities.SupportedLayoutsOptions(
      maxCapacity: 5000,
      configuration: .viewpoints
    )
    
    configuration.layout = capabilities.supportedLayouts(options: options).first ?? .shared
  }
}

class MetalView: UIView {
  override init(frame: CGRect) {
    super.init(frame: frame)
    self.backgroundColor = .clear
  }
  
  required init?(coder: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }
  
  func setupMetalLayer() {
    guard let bridge = LibNativeBridge.sharedInstance() else { return }
    
    if bridge.metalLayer != nil {
      return
    }
    
    self.addGestureRecognizer(
      UIBabylonGestureRecognizer(
        target: self,
        onTouchDown: bridge.setTouchDown,
        onTouchMove: bridge.setTouchMove,
        onTouchUp: bridge.setTouchUp
      )
    )
    metalLayer.pixelFormat = .bgra8Unorm
    metalLayer.framebufferOnly = true
    
    bridge.metalLayer = self.metalLayer
    
    let scale = UITraitCollection.current.displayScale
    bridge.initialize(withWidth: Int(self.bounds.width * scale), height: Int(self.bounds.height * scale))
  }
  
  var metalLayer: CAMetalLayer {
    return layer as! CAMetalLayer
  }
  
  override class var layerClass: AnyClass {
    return CAMetalLayer.self
  }
  
  override func layoutSubviews() {
    super.layoutSubviews()
    setupMetalLayer()
    updateDrawableSize()
  }
  
  private func updateDrawableSize() {
    let scale = UITraitCollection.current.displayScale
    LibNativeBridge.sharedInstance().drawableWillChangeSize(withWidth: Int(bounds.width * scale), height: Int(bounds.height * scale))
    metalLayer.drawableSize = CGSize(width: bounds.width * scale, height: bounds.height * scale)
  }
}

struct MetalViewRepresentable: UIViewRepresentable {
  typealias UIViewType = MetalView
  
  func makeUIView(context: Context) -> MetalView {
    MetalView(frame: .zero)
  }
  
  func updateUIView(_ uiView: MetalView, context: Context) {}
}

struct ContentView: View {
  @Environment(\.openImmersiveSpace) var openImmersiveSpace
  @Environment(\.dismissImmersiveSpace) var dismissImmersiveSpace
  @State private var isShowingImmersive = false
  
  var body: some View {
    VStack {
      MetalViewRepresentable()
        .frame(maxWidth: .infinity, maxHeight: .infinity)
      
      HStack {
        Button(isShowingImmersive ? "Exit Immersive" : "Enter Immersive") {
          Task {
            if isShowingImmersive {
              await dismissImmersiveSpace()
              isShowingImmersive = false
            } else {
              await openImmersiveSpace(id: "ImmersiveSpace")
              isShowingImmersive = true
            }
          }
        }
        .padding()
      }
    }
  }
}


@main
struct ExampleApp: App {
  @State private var currentStyle: ImmersionStyle = .full
  
  var body: some Scene {
    WindowGroup {
      ContentView()
    }
    
    ImmersiveSpace(id: "ImmersiveSpace") {
      CompositorLayer(configuration: BabylonLayerConfiguration()) { layerRenderer in
        let bridge = LibNativeBridge.sharedInstance()
        bridge?.initializeImmersive(with: layerRenderer)
        
        layerRenderer.onSpatialEvent = { eventCollection in
          bridge?.processSpatialEvents(eventCollection)
        }
      }
    }
    .immersionStyle(selection: $currentStyle, in: .full)
  }
}
