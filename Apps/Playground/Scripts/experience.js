/// <reference path="../../node_modules/babylonjs/babylon.module.d.ts" />

// See https://playground.babylonjs.com/#VZ99Q8#11.

var logfps = false;

var engine = new BABYLON.NativeEngine();
var scene = new BABYLON.Scene(engine);

var camera = new BABYLON.ArcRotateCamera("camera", -Math.PI / 2, Math.PI / 2, 20, BABYLON.Vector3.Zero(), scene);
var light = new BABYLON.HemisphericLight("light", new BABYLON.Vector3(0, 1, -2), scene);
light.intensity = 0.7;

const frameSize = 3;
const frameWidth = 0.1;
const axisIndicatorSize = 3 * frameWidth;
const audioParamChangeTimeInSeconds = 0.1;
const maxVolume = 0.25;
const minPitch = 110;
const maxPitch = 1760;

var frameLeft = BABYLON.MeshBuilder.CreateCylinder("frameLeft");
frameLeft.scaling.x = frameWidth;
frameLeft.scaling.y = frameSize;
frameLeft.scaling.z = frameWidth;
frameLeft.position.x = -frameSize;

var frameBottom = BABYLON.MeshBuilder.CreateCylinder("frameRight");
frameBottom.rotation.z = Math.PI / 2;
frameBottom.scaling.x = frameWidth;
frameBottom.scaling.y = frameSize;
frameBottom.scaling.z = frameWidth;
frameBottom.position.y = -frameSize;

var frameCorner = BABYLON.MeshBuilder.CreateSphere("frameSphere");
frameCorner.scaling.setAll(frameLeft.scaling.x);
frameCorner.position.x = frameCorner.position.y = -frameLeft.scaling.y;

var frame = BABYLON.Mesh.MergeMeshes([
    frameLeft, frameBottom, frameCorner
]);
frame.name = "frame";
frame.material = new BABYLON.StandardMaterial("frame.material");
frame.material.alpha = 0.25;
frame.material.backFaceCulling = false;

var volumeAxisIndicator = BABYLON.MeshBuilder.CreateSphere("volumeAxisIndicator");
volumeAxisIndicator.material = new BABYLON.StandardMaterial("volumeAxisIndicator.material");
volumeAxisIndicator.material.diffuseColor.set(1, 0, 0);
volumeAxisIndicator.scaling.setAll(axisIndicatorSize * 0.75);
volumeAxisIndicator.position.x = -frameSize;

var pitchAxisIndicator = BABYLON.MeshBuilder.CreateSphere("pitchAxisIndicator");
pitchAxisIndicator.material = new BABYLON.StandardMaterial("pitchAxisIndicator.material");
pitchAxisIndicator.material.diffuseColor.set(0, 0, 1);
pitchAxisIndicator.scaling.setAll(axisIndicatorSize * 0.75);
pitchAxisIndicator.position.y = -frameSize;

var xyPositionIndicator = BABYLON.MeshBuilder.CreateSphere("xyPositionIndicator");
xyPositionIndicator.material = new BABYLON.StandardMaterial("xyPositionIndicator.material");
xyPositionIndicator.material.diffuseColor.set(1, 0, 1);
xyPositionIndicator.scaling.setAll(axisIndicatorSize);

var hitPlane = BABYLON.MeshBuilder.CreatePlane("hitPlane", { size: 2 * frameSize });
hitPlane.material = frame.material;

var audioContext = new AudioContext();
var gainNode = new GainNode(audioContext);
var oscillatorNode = new OscillatorNode(audioContext);

if (!(engine instanceof BABYLON.NativeEngine)) {
    console.log(`Adding click event listener to unlock audio context on user input`);
    document.body.addEventListener('click', () => {
        audioContext.resume();
    });
}

oscillatorNode.connect(gainNode);
gainNode.connect(audioContext.destination);

gainNode.gain.value = 0;
oscillatorNode.start();

scene.onPointerObservable.add((pointerInfo) => {
    let pickInfo = scene.pick(scene.pointerX, scene.pointerY, function (mesh) { return mesh == hitPlane; });
    if (pickInfo.hit) {
        var targetVolume = maxVolume * (pickInfo.pickedPoint.y + frameSize) / (2 * frameSize);
        gainNode.gain.setTargetAtTime(targetVolume, 0, audioParamChangeTimeInSeconds);

        var targetPitch = minPitch + (maxPitch - minPitch) * ((pickInfo.pickedPoint.x + frameSize) / (2 * frameSize));
        oscillatorNode.frequency.setTargetAtTime(targetPitch, 0, audioParamChangeTimeInSeconds);

        pitchAxisIndicator.position.x = pickInfo.pickedPoint.x;
        volumeAxisIndicator.position.y = pickInfo.pickedPoint.y;
        xyPositionIndicator.position.set(pickInfo.pickedPoint.x, pickInfo.pickedPoint.y, 0);
        frame.material.diffuseColor.set(0.75, 1, 0.75);
    }
    else {
        gainNode.gain.setTargetAtTime(0, 0, audioParamChangeTimeInSeconds);
        frame.material.diffuseColor.set(1, 1, 1);
    }
});

if (logfps) {
    var logFpsLoop = () => {
        BABYLON.Tools.Log("FPS: " + Math.round(engine.getFps()));
        window.setTimeout(logFpsLoop, 1000);
    };
    window.setTimeout(logFpsLoop, 3000);
}

engine.runRenderLoop(() => {
    scene.render();
});
