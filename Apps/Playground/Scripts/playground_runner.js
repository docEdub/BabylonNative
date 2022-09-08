if (typeof createScene === "function") {

    console.log("Creating engine ...")
    engine = new BABYLON.NativeEngine({adaptToDeviceRatio: true});
    console.log("Creating engine - done")

    console.log("Creating scene ...")
    var scene = createScene();
    console.log("Creating scene - done")

    engine.runRenderLoop(function () {
        scene.render();
    });

    setTimeout(() => {
        engine._engine.dispose()
        engine._engine.letTextureLoadingProceed()
    }, 1000)
}