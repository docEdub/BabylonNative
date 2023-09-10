mocha.setup({ ui: "bdd", reporter: "spec", retries: 5 });

const expect = chai.expect;

xdescribe("AudioContext", function () {
    this.timeout(0);

    it("is constructable", function () {
        const audioContext = new AudioContext();
        expect(audioContext).to.not.be.undefined;
    });

    it("create a gain node", function () {
        const audioContext = new AudioContext();
        // const gainNode = audioContext.createGain();
        // expect(gainNode).to.not.be.undefined;
    });
});

describe("GainNode", function () {
    this.timeout(0);

    // it("is constructable", function () {
    //     const audioContext = new AudioContext();
    //     const gainNode = new GainNode(audioContext);
    //     expect(gainNode).to.not.be.undefined;
    // });

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
