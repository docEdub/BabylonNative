/// <reference path="../../node_modules/babylonjs/babylon.module.d.ts" />
/// <reference path="../../node_modules/babylonjs-loaders/babylonjs.loaders.module.d.ts" />
/// <reference path="../../node_modules/babylonjs-materials/babylonjs.materials.module.d.ts" />
/// <reference path="../../node_modules/babylonjs-gui/babylon.gui.module.d.ts" />

var logfps = true;

var engine = new BABYLON.NativeEngine();

var createScene = function () {
    var scene = new BABYLON.Scene(engine);
    window.scene = scene;
    // This creates and positions a free camera (non-mesh)
    var camera = new BABYLON.FreeCamera("camera1", new BABYLON.Vector3(0, 6, -20), scene);

    // This targets the camera to scene origin
    camera.setTarget(BABYLON.Vector3.Zero());

    // This attaches the camera to the canvas
    camera.attachControl(undefined, true);

    // This creates a light, aiming 0,1,0 - to the sky (non-mesh)
    var light = new BABYLON.HemisphericLight("light", new BABYLON.Vector3(1, 1, 0), scene);

    // Default intensity is 1. Let's dim the light a small amount
    light.intensity = 0.7;

    // our render target texture
    var renderTarget = new BABYLON.RenderTargetTexture("depth", 800, scene, true);
    renderTarget.clearColor = new BABYLON.Color4(1, 1, 0, 1);
    scene.customRenderTargets.push(renderTarget);

    // add a series of spheres in a circle for the scene data with a purple material
    var materialBlue = new BABYLON.StandardMaterial("blue", scene);
    materialBlue.diffuseColor = BABYLON.Color3.Blue();
    var spheresCount = 20;
    var alpha = 0;
    for (var index = 0; index < spheresCount; index++) {
        var sphere = BABYLON.Mesh.CreateSphere("Sphere" + index, 32, 3, scene);
        sphere.position.x = 10 * Math.cos(alpha);
        sphere.position.z = 10 * Math.sin(alpha);
        sphere.material = materialBlue;
        alpha += (2 * Math.PI) / spheresCount;
        // add only every two spheres the RTT
        if (index % 2) {
            renderTarget.renderList.push(sphere);
        }
    }

    // this is the plane that will show the RTT.
    var plane = BABYLON.Mesh.CreatePlane("map", 10, scene);
    plane.billboardMode = BABYLON.AbstractMesh.BILLBOARDMODE_ALL;
    plane.scaling.y = 1.0 / engine.getAspectRatio(scene.activeCamera);

    // create a material for the RTT and apply it to the plane
    var rttMaterial = new BABYLON.StandardMaterial("RTT material", scene);
    rttMaterial.emissiveTexture = renderTarget;
    rttMaterial.disableLighting = true;
    plane.material = rttMaterial;

    // renderTarget.onBeforeRenderObservable.add(() => {
    //     engine.enableScissor(50, 600, 700, 150);
    //     engine.clear(new BABYLON.Color4(0, 1, 1, 1), true, true, true);
    //     engine.enableScissor(100, 100, 600, 600);
    // });
    // scene.onAfterRenderObservable.add(() => {
    //     engine.disableScissor();
    // });

    // scene.onBeforeRenderTargetsRenderObservable.add(() => {
    //     engine.enableScissor(100, 100, 600, 600);
    // });
    // scene.onAfterRenderTargetsRenderObservable.add(() => {
    //     engine.disableScissor();
    // });

    // scene.onBeforeDrawPhaseObservable.add(() => {
    //     const w = engine.getRenderWidth(), h = engine.getRenderHeight();
    //     engine.enableScissor(w * 0.1, h * 0.3, w / 1.25, h / 1.5);
    // });

    // scene.onAfterDrawPhaseObservable.add(() => {
    //     engine.disableScissor();
    // });

    return scene;
};

if (logfps) {
    var logFpsLoop = function () {
        BABYLON.Tools.Log("FPS: " + Math.round(engine.getFps()));
        window.setTimeout(logFpsLoop, 1000);
    };

    window.setTimeout(logFpsLoop, 3000);
}

var scene = createScene();

engine.runRenderLoop(function () {
    scene.render();
});
