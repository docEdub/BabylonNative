mocha.setup({ ui: "bdd", reporter: "spec", retries: 5 });

const expect = chai.expect;

describe("AudioContext", function () {
    this.timeout(0);

    it("is constructable", function () {
        const audioContext = new AudioContext();
        expect(audioContext).to.not.be.undefined;
    });

    it("create a gain node", function () {
        const audioContext = new AudioContext();
        const gainNode = audioContext.createGain();
        expect(gainNode).to.not.be.undefined;
    });
});

describe("GainNode", function () {
    it("is constructable", function () {
        const audioContext = new AudioContext();
        const gainNode = new GainNode(audioContext);
        expect(gainNode).to.not.be.undefined;
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
