/*
 * Copyright (c) 2011 Gregor Richards
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

(function() {
    var animdiv = document.createElement("div");

    // canvas and context
    var canvas = document.createElement("canvas");
    canvas.width = 320;
    canvas.height = 285;
    animdiv.appendChild(canvas);
    animdiv.appendChild(document.createElement("br"));
    var ctx;

    // play/pause button
    var play = "&gt;";
    var pause = "||";
    var playB = document.createElement("button");
    playB.innerHTML = play;
    playB.style.width = "40px";
    animdiv.appendChild(playB);

    // play timer
    var playing = false;
    var playT = null;

    // and seeker
    var seeker = document.createElement("input");
    seeker.type = "range";
    seeker.style.width = "270px";
    animdiv.appendChild(seeker);

    // frames
    var frames;

    EgoJSout.debugFunctions.push(function(outp, state) {
        if (state === "start") {
            frames = [];

            outp.appendChild(animdiv);
            outp.appendChild(document.createElement("br"));
            outp.appendChild(document.createElement("br"));
            seeker.min = seeker.max = seeker.value = 0;

            ctx = canvas.getContext("2d");
            ctx.fillStyle = "rgb(0, 0, 0)";
            ctx.fillRect(0, 0, 320, 285);
            return;

        } else if (state === "end") {
            // autoplay
            if ("query" in window && window["query"].ap && playT === null) {
                playB.innerHTML = pause;
                playT = setTimeout(animate, 0);
            }
            return;

        } else if (typeof(state) === "string") {
            return;
        }

        frames.push({
            tape: state.tape.slice(0),
            lotp: state.lotp, rotp: state.rotp,
            ltp: state.lpctp.tp, rtp: state.rpctp.tp
        });
        seeker.max = frames.length - 1;
    });

    function drawFrame(frame) {
        var tlen = frame.tape.length;

        var x = 0;
        function box(r, b, fr, fb, h) {
            // wow this function has become stupid
            ctx.fillStyle = "rgb(0,0,0)";
            ctx.fillRect(x + 10, 0, 10, 285);

            var tp = 138, bm = 148;

            if (h > 128) h -= 256;
            if (h > 0) tp -= h;
            else       bm -= h;

            ctx.fillStyle = fr?"rgb(255,64,64)":
                            fb?"rgb(64,64,255)":
                            "rgb(255,255,255)";
            ctx.fillRect(x + 10, tp, 10, bm - tp);

            ctx.fillStyle = "rgb(" + r + ",0," + b + ")";
            ctx.fillRect(x + 10, 138, 10, 10);

            x += 10;
        }

        for (var ti = 0; ti < tlen; ti++) {
            var r = 0; 
            var b = 0;
            var fr = false;
            var fb = false

            // mark flags
            if (ti == 0) {
                fr = true
            }
            if (ti == tlen - 1) {
                fb = true;
            }

            // figure out the cell value and info
            if (frame.ltp == ti) {
                r = 255;
            }
            if (frame.rtp == ti) {
                b = 255;
            }

            box(r, b, fr, fb, frame.tape[ti]);
        }
    }

    function animate() {
        var fi = Math.floor(seeker.value);
        playT = null;

        // animate this frame
        var frame = frames[fi];
        if (frame !== undefined) drawFrame(frames[fi]);

        seeker.value = ++fi;
        if (fi >= frames.length) {
            playB.innerHTML = play;
            playT = null;
            return;
        }

        // how long do we have to wait to animate?
        var w = 0;
        if (frame.lotp != frame.ltp ||
            frame.rotp != frame.rtp) {
            w = 30;
        }

        playT = setTimeout(animate, w);
    }

    seeker.onchange = function() {
        var fi = Math.floor(seeker.value);
        playT = null;

        // animate this frame
        var frame = frames[fi];
        if (frame !== undefined) drawFrame(frames[fi]);
    };

    // the play button action
    playB.onclick = function() {
        if (playT === null) {
            // play
            if (Math.floor(seeker.value) >= frames.length - 1) seeker.value = 0;
            playB.innerHTML = pause;
            playT = setTimeout(animate, 0);

        } else {
            // pause
            playB.innerHTML = play;
            clearTimeout(playT);
            playT = null;
        }
    };
})();
