mocha.setup({ ui: "bdd", reporter: "spec", retries: 5 });

const expect = chai.expect;

describe("AudioContext", function () {
    this.timeout(0);

    it("is constructable", function () {
        const audioContext = new AudioContext();
        expect(audioContext).to.not.be.null;
        expect(audioContext).to.not.be.undefined;
        expect(audioContext instanceof AudioContext).to.be.true;
    });

    it("create a gain node", function () {
        const audioContext = new AudioContext();
        const gainNode = audioContext.createGain();
        expect(gainNode).to.not.be.null;
        expect(gainNode).to.not.be.undefined;
        expect(gainNode instanceof GainNode).to.be.true;
    });
});

describe("GainNode", function () {
    this.timeout(0);

     it("is constructable", function () {
         const audioContext = new AudioContext();
         const gainNode = new GainNode(audioContext);
         expect(gainNode).to.not.be.undefined;
     });

    it("is instanceof AudioNode", function () {
        const audioContext = new AudioContext();
        const gainNode = new GainNode(audioContext)
        expect(gainNode instanceof AudioNode).to.be.true;
    });

    it("connects to audio context destination", function () {
        const audioContext = new AudioContext();
        const gainNode = new GainNode(audioContext);
        const connectedNode = gainNode.connect(audioContext.destination);
        expect(connectedNode).to.equal(audioContext.destination);
    });
});

mocha.run(failures => {
    // Test program will wait for code to be set before exiting
    if (failures > 0) {
        // Failure
        SetExitCode(1);
    } else {
        // Success
        SetExitCode(0);
    }
});
