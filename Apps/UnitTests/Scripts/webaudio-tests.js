﻿describe("WebAudio", function () {
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
            expect(gainNode).to.not.be.null;
            expect(gainNode).to.not.be.undefined;
            expect(gainNode instanceof GainNode).to.be.true;
        });

        it("sets gain to 1.0 by default", function () {
            const audioContext = new AudioContext();
            const gainNode = new GainNode(audioContext);
            expect(gainNode.gain.value).to.equal(1.0);
        });

        it("sets gain to given value", function () {
            const audioContext = new AudioContext();
            const gainNode = new GainNode(audioContext);
            gainNode.gain.value = 0.5;
            expect(gainNode.gain.value).to.equal(0.5);
        });

        it("is instanceof AudioNode", function () {
            const audioContext = new AudioContext();
            const gainNode = new GainNode(audioContext)
            expect(gainNode instanceof AudioNode).to.be.true;
        });

        // This fails when using JavaScriptCore engine.
        // TODO: Get this test to pass when using JavaScriptCore engine.
        xit("is not instanceof AudioContext", function () {
            const audioContext = new AudioContext();
            const gainNode = new GainNode(audioContext)
            expect(gainNode instanceof AudioContext).to.be.false;
        });

        it("connects to audio context destination", function () {
            const audioContext = new AudioContext();
            const gainNode = new GainNode(audioContext);
            const connectedNode = gainNode.connect(audioContext.destination);
            expect(connectedNode).to.equal(audioContext.destination);
        });
    });

    describe("OscillatorNode", function () {
        this.timeout(0);

        it("is constructable", function () {
            const audioContext = new AudioContext();
            const oscillatorNode = new OscillatorNode(audioContext);
            expect(oscillatorNode).to.not.be.null;
            expect(oscillatorNode).to.not.be.undefined;
            expect(oscillatorNode instanceof OscillatorNode).to.be.true;
        });

        it("is instanceof AudioNode", function () {
            const audioContext = new AudioContext();
            const oscillatorNode = new OscillatorNode(audioContext);
            expect(oscillatorNode instanceof AudioNode).to.be.true;
        });

        it("is instanceof AudioScheduledSourceNode", function () {
            const audioContext = new AudioContext();
            const oscillatorNode = new OscillatorNode(audioContext);
            expect(oscillatorNode instanceof AudioScheduledSourceNode).to.be.true;
        });

        it("connects to audio context destination", function () {
            const audioContext = new AudioContext();
            const oscillatorNode = new OscillatorNode(audioContext);
            const connectedNode = oscillatorNode.connect(audioContext.destination);
            expect(connectedNode).to.equal(audioContext.destination);
        });

        it("connects to gain node", function () {
            const audioContext = new AudioContext();
            const oscillatorNode = new OscillatorNode(audioContext);
            const gainNode = new GainNode(audioContext);
            const connectedNode = oscillatorNode.connect(gainNode);
            expect(connectedNode).to.equal(gainNode);
        });

        it("starts without error", function () {
            function start() {
                const audioContext = new AudioContext();
                const oscillatorNode = new OscillatorNode(audioContext);
                const gainNode = new GainNode(audioContext);
                gainNode.gain.value = 0;
                oscillatorNode.connect(gainNode);
                gainNode.connect(audioContext.destination);
                oscillatorNode.start();
            }
            expect(start).to.not.throw();
        });

        it("stops without error", function () {
            function stop() {
                const audioContext = new AudioContext();
                const oscillatorNode = new OscillatorNode(audioContext);
                const gainNode = new GainNode(audioContext);
                gainNode.gain.value = 0;
                oscillatorNode.connect(gainNode);
                gainNode.connect(audioContext.destination);
                oscillatorNode.start();
                oscillatorNode.stop();
            }
            expect(stop).to.not.throw();
        });
    });
});