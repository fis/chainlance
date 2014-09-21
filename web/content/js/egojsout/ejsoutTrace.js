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

EgoJSout.debugFunctions.push(function(outp, state) {
    function txt(txt) {
        return document.createTextNode(txt);
    }


    // convert a program to a debugger box
    function programToBox(into, prog, pcs) {
        var html = "";
        var div = document.createElement("div");
        div.style.border = "1px solid black";
        div.style.margin = "1em";
        var pc;

        // turn pcs into an associative array
        var pcsa = {};
        for (var i = 0; i < pcs.length; i++) {
            pcsa[pcs[i]] = true;
        }
        pcs = pcsa;

        // get a list of things to embolden
        var bold = [];
        var BFJ = EgoJSout.BFJ;
        for (pc in pcsa) {
            if (pc < prog.length) {
                var cmd = prog[pc];
                var cmdi = pc;
                if (cmd.type == BFJ.PEND || cmd.type == BFJ.CBSTART || cmd.type == BFJ.CBEND) {
                    cmdi = cmd.start;
                    cmd = prog[cmdi];
                }
                if (cmd.type == BFJ.PSTART) {
                    bold[cmdi] = true;
                    if (cmd.cbstart !== null) bold[cmd.cbstart] = true;
                    if (cmd.cbend !== null) bold[cmd.cbend] = true;
                    bold[cmd.end] = true;
                }
            }
        }

        // flesh it out
        for (var i = 0; i < prog.length; i++) {
            var cmd = prog[i];
            if (i in pcs) {
                html += "<span style=\"color: red; font-size: 2em\">";
            } else if (i in bold) {
                html += "<span style=\"font-size: 2em\">";
            }
            switch (cmd.type) {
                case BFJ.LEFT:      html += "&lt;"; break;
                case BFJ.RIGHT:     html += "&gt;"; break;
                case BFJ.MINUS:     html += "-"; break;
                case BFJ.PLUS:      html += "+"; break;
                case BFJ.NOOP:      html += "."; break;
                case BFJ.LSTART:    html += "["; break;
                case BFJ.LEND:      html += "]"; break;
                case BFJ.PSTART:    html += "("; break;
                case BFJ.PEND:      html += ")"; break;
                case BFJ.CBSTART:   html += "{"; break;
                case BFJ.CBEND:     html += "}"; break;
                case BFJ.JSDEBUG:   html += "`"; break;
            }
            if (i in pcs || i in bold) {
                html += "</span>";
            }
            if ("ct" in cmd) {
                html += "<span style=\"color: green\">" + cmd.ct + "</span>";
            }

            if (cmd.txt != "") {
                html += "<span style=\"color: gray\">" + cmd.txt.replace(/</g, "&lt;").replace(/>/g, "&gt;") + "</span>";
            }
        }

        div.innerHTML = html;
        into.appendChild(div);
    }

    if (typeof(state) === "string") {
        return;
    }

    var tlen = state.tape.length;

    // convert a value 0-255 to hex
    function toHex(val) {
        var hex = val.toString(16).toUpperCase();
        while (hex.length < 2) hex = "0" + hex;
        hex = hex.substring(hex.length - 2);
        return hex;
    }

    // display a fancy flag
    function displayFlag(flag) {
        var flagstr = "\u2691\u00a0";
        if (flag > 2) {
            flagstr = "\u2716\u00a0";
        } else if (flag > 0) {
            flagstr = "\u2690" + flag;
        }
        tstate.appendChild(txt(flagstr));
    }

    // set up this line
    var tstate = document.createElement("div");
    tstate.style.fontFamily = "monospace";

    // step
    var stepstr = state.step + "";
    while (stepstr.length < 5) stepstr = "\u00a0" + stepstr;
    var tcell = document.createElement("span");
    tcell.appendChild(txt(stepstr + ":\u00a0\u00a0"));
    tstate.appendChild(tcell);

    // left flag
    displayFlag(state.lflag);
    tstate.appendChild(txt("\u00a0"));

    for (var ti = 0; ti < tlen; ti++) {
        var tcell = document.createElement("span");
        var tcstr = "\u00a0";

        // mark flags
        if (ti == 0 || ti == tlen - 1) {
            tcell.style.color = "green";
        }

        // figure out the cell value and info
        if (state.lpctp.tp == ti) {
            if (state.rpctp.tp == ti) {
                tcstr += "X";
            } else {
                tcstr += ">";
            }
            tcell.style.color = "red";
        } else if (state.rpctp.tp == ti) {
            tcstr += "<";
            tcell.style.color = "red";
        } else {
            tcstr += "\u00a0";
        }

        // and the value
        tcstr += toHex(state.tape[ti]);

        tcell.appendChild(txt(tcstr));
        tstate.appendChild(tcell);
    }

    // right flag
    tstate.appendChild(txt("\u00a0\u00a0"));
    displayFlag(state.rflag);

    outp.appendChild(tstate);

    // make a debugger box for it
    tstate.onclick = function() {
        EgoJSout.ejDbg.innerHTML = "";

        var close = document.createElement("a");
        close.innerHTML = "X";
        close.href = "javascript:;";
        EgoJSout.ejDbg.appendChild(close);
        close.onclick = function(ev) {
            tstate.removeChild(EgoJSout.ejDbg);
            EgoJSout.ejDbg.innerHTML = "";
            ev.stopPropagation();
            return false;
        };

        programToBox(EgoJSout.ejDbg, state.left, state.lpcs);
        programToBox(EgoJSout.ejDbg, state.right, state.rpcs);
        tstate.appendChild(EgoJSout.ejDbg);
    };
});
