/*
 * Copyright (c) 2013 Heikki Kallasjoki
 *
 * This file is a modified copy of the EgoJSout software, altered to
 * work from within a Node.js program, for regression testing the
 * chainlance/cranklance/gearlance/torquelance family of BFJoust
 * engines.  It is licensed under the same license as the original
 * EgoJSout; see below.
 */

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

var EgoJSout = new (function() {
    var ejOut = null;

    // constants
    var BFJ = {
        LEFT: 0,
        RIGHT: 1,
        MINUS: 2,
        PLUS: 3,
        NOOP: 4,
        LSTART: 5,
        LEND: 6,

        PSTART: 10,
        PEND: 11,
        CBSTART: 12,
        CBEND: 13,

        JSDEBUG: 20
    };
    this.BFJ = BFJ;

    // parse a BFJoust program
    function parse(bfj, err) {
        var ret = [];
        var loops = [];
        var loopses = [];
        var parens = [];
        var braces = [];
        var error = "";

        ret.src = bfj;

        function unmatched(t, at) {
            if (at === undefined) {
                error += "Unmatched " + t + ".\n";
            } else {
                error += "Unmatched " + t + " at " + at + ".\n";
            }
        }
        function pushLoops() {
            loopses.push(loops);
            loops = [];
        }
        function popLoops() {
            if (loops.length > 0) {
                unmatched("loop");
            }
            if (loopses.length > 0) {
                loops = loopses.pop();
            } else {
                loops = [];
            }
        }

        for (var i = 0; i < bfj.length; i++) {
            var cur = null;
            var obj = {};
            var match = null;
            var type = null;
            var count = null;

            switch (bfj[i]) {
                case '<':
                    cur = BFJ.LEFT;
                    break;

                case '>':
                    cur = BFJ.RIGHT;
                    break;

                case '-':
                    cur = BFJ.MINUS;
                    break;

                case '+':
                    cur = BFJ.PLUS;
                    break;

                case '.':
                    cur = BFJ.NOOP;
                    break;

                case '[':
                    loops.push(ret.length);
                    cur = BFJ.LSTART;
                    obj.jmp = null;
                    break;

                case ']':
                    if (loops.length == 0) {
                        // unmatched loop!
                        cur = BFJ.NOOP;
                        unmatched("loop", i);
                    } else {
                        cur = BFJ.LEND;
                        obj.jmp = loops.pop();
                        ret[obj.jmp].jmp = ret.length;
                    }
                    break;

                case '(':
                    pushLoops();

                    parens.push(ret.length);
                    cur = BFJ.PSTART;
                    obj.ct = null;
                    obj.cbstart = null;
                    obj.cbend = null;
                    obj.end = null;
                    break;

                case ')':
                    popLoops();

                    if (parens.length == 0) {
                        // unmatched parens!
                        cur = BFJ.NOOP;
                        unmatched("paren");
                    } else {
                        // must be followed by a '*' or '%', then a number
                        i++;
                        if (i < bfj.length) {
                            type = bfj[i];
                        }
                        count = parseInt(bfj.substring(i+1), 10);
                        if (count < 0 || count > 100000) count = 100000;

                        // find the matching paren
                        cur = BFJ.PEND;
                        obj.start = parens.pop();
                        ret[obj.start].end = ret.length;

                        // actually, we don't care what type is :P
                        match = ret[obj.start];
                        match.ct = count;
                    }
                    break;

                case '{':
                    pushLoops();

                    // look for a paren with no braces
                    for (var pi = parens.length - 1; pi >= 0; pi--) {
                        if (ret[parens[pi]].cbstart === null) {
                            // found one
                            braces.push(ret.length);
                            cur = BFJ.CBSTART;
                            obj.start = parens[pi];
                            ret[obj.start].cbstart = ret.length;
                            break;
                        }
                    }
                    if (pi < 0) {
                        // didn't find one!
                        cur = BFJ.NOOP;
                        unmatched("brace");
                    }
                    break;

                case '}':
                    popLoops();

                    if (braces.length == 0) {
                        // unmatched braces!
                        cur = BFJ.NOOP;
                        unmatched("brace");
                    } else {
                        cur = BFJ.CBEND;
                        obj.start = ret[braces.pop()].start;
                        ret[obj.start].cbend = ret.length;
                    }
                    break;

                case '`':
                    // possibly debugging
                    if (bfj.length > i + 3 && bfj[i+1] == "`" && bfj[i+2] == "(") {
                        cur = BFJ.JSDEBUG;
                        i += 3;
                        var depth = 1;
                        var js = "";

                        // find the matching paren
                        for (; i < bfj.length && depth > 0; i++) {
                            if (bfj[i] == "(") {
                                depth++;
                            } else if (bfj[i] == ")") {
                                depth--;
                            }
                            if (depth > 0) {
                                js += bfj[i];
                            }
                        }

                        obj.js = js;
                        break;
                    }

                default:
                    if (ret.length > 0) {
                        ret[ret.length-1].txt += bfj[i];
                    }
            }

            // now push the newly-created object onto the return
            if (cur !== null) {
                obj.type = cur;
                obj.txt = "";
                ret.push(obj);
            }
        }

        // verify that all loops and parens are matched
        for (var i = 0; i < ret.length; i++) {
            var cmd = ret[i];
            switch (cmd.type) {
                case BFJ.LSTART:
                    if (cmd.jmp === null) {
                        unmatched("loop");
                    }
                    break;

                case BFJ.PSTART:
                    if (cmd.end === null || cmd.ct === null) {
                        unmatched("paren");
                    }
                    break;

                case BFJ.CBSTART:
                    if (ret[cmd.start].cbend === null) {
                        unmatched("brace");
                    }
                    break;
            }
        }

        if (error !== null && err !== undefined) {
            err.msg = error;
        }

        return ret;
    }

    // run a single command for this BFJoust program
    function bfjStep(prog, tape, pctp, dir, pol, allpcs) {
        var meta = true;
        var ch = 0;
        var start = null, match = null;
        var cmd;

        while (meta) {
            meta = false;

            if (pctp.pc < 0 || pctp.pc >= prog.length) break;

            // run this command
            cmd = prog[pctp.pc];
            allpcs.push(pctp.pc);
            switch (cmd.type) {
                case BFJ.LEFT:
                    pctp.tp -= dir;
                    break;

                case BFJ.RIGHT:
                    pctp.tp += dir;
                    break;

                case BFJ.MINUS:
                    ch = -pol;
                    break;

                case BFJ.PLUS:
                    ch = pol;
                    break;

                case BFJ.LSTART:
                    if (tape[pctp.tp] == 0) {
                        pctp.pc = cmd.jmp;
                    }
                    break;

                case BFJ.LEND:
                    if (tape[pctp.tp] != 0) {
                        pctp.pc = cmd.jmp;
                    }
                    break;

                case BFJ.PSTART:
                    meta = true;
                    cmd.iter = 0;
                    if (cmd.ct == 0) {
                        pctp.pc = cmd.end;
                    }
                    break;

                case BFJ.PEND:
                    meta = true;
                    start = prog[cmd.start];
                    if (start.cbend !== null) {
                        match = start.cbend;
                        start.iter--;
                        if (start.iter >= 0)
                            pctp.pc = match;

                    } else {
                        match = cmd.start;
                        start.iter++;
                        if (start.iter < start.ct)
                            pctp.pc = match;

                    }
                    break;

                case BFJ.CBSTART:
                    meta = true;
                    start = prog[cmd.start];
                    start.iter++;
                    if (start.iter < start.ct) {
                        pctp.pc = cmd.start;
                    }
                    break;

                case BFJ.CBEND:
                    meta = true;
                    start = prog[cmd.start];
                    start.iter = start.ct - 1;
                    break;

                case BFJ.JSDEBUG:
                    meta = true;
                    eval(cmd.js);
                    break;
            }

            pctp.pc++;
        }

        return ch;
    }

    var debugFunctions = [];
    this.debugFunctions = debugFunctions;

    // run a BFJoust competition
    function joust(left, right, lpol, rpol, tlen, outp, cb) {
        // set up the tape
        var tape = new Array(tlen);
        for (var i = 1; i < tlen - 1; i++) {
            tape[i] = 0;
        }
        tape[0] = tape[tlen-1] = 128;

        // program counters and tape pointers
        var lpctp = {pc: 0, tp: 0};
        var rpctp = {pc: 0, tp: tlen-1};
        var lpc = 0, rpc = 0;

        // tape pointers
        var ltp = 0, rtp = tlen - 1;

        // flag counters
        var lflag = 0, rflag = 0;

        // changes
        var lchange = 0, rchange = 0;

        // and run
        var step = 0;
        function runSteps() {
            bg = null;

            // start time
            var stTime = new Date().getTime();
            var wait = null;
            var swait;

            while (true) {
                var lpcs = [];
                var rpcs = [];
                var lotp = lpctp.tp;
                var rotp = lpctp.tp;

                // run a step
                lchange = bfjStep(left, tape, lpctp, 1, lpol, lpcs);
                rchange = bfjStep(right, tape, rpctp, -1, rpol, rpcs);

                // apply changes
                tape[lpctp.tp] += lchange;
                if (tape[lpctp.tp] < 0) tape[lpctp.tp] += 256;
                if (tape[lpctp.tp] >= 256) tape[lpctp.tp] -= 256;
                tape[rpctp.tp] += rchange;
                if (tape[rpctp.tp] < 0) tape[rpctp.tp] += 256;
                if (tape[rpctp.tp] >= 256) tape[rpctp.tp] -= 256;

                // check flags
                if (tape[0] == 0) {
                    lflag++;
                } else {
                    lflag = 0;
                }
                if (tape[tlen-1] == 0) {
                    rflag++;
                } else {
                    rflag = 0;
                }

                // check positions
                if (lpctp.tp < 0 || lpctp.tp >= tlen) {
                    lflag = 3;
                }
                if (rpctp.tp < 0 || rpctp.tp >= tlen) {
                    rflag = 3;
                }

                // output
                if (outp !== null) {
                    for (var i = 0; i < outp.length; i++) {
                        swait = outp[i](ejOut, {
                            step: step,
                            left: left, right: right,
                            lpctp: lpctp, rpctp: rpctp,
                            lpcs: lpcs, rpcs: rpcs,
                            lotp: lotp, rotp: rotp,
                            lflag: lflag, rflag: rflag,
                            tape: tape
                        });
                        if (swait !== undefined && (wait === null || swait > wait)) {
                            wait = swait;
                        }
                    }       
                }

                // next step
                step++;
                if (step >= 100000 || lflag >= 2 || rflag >= 2) {
                    // termination
                    if (outp !== null) {
                        for (var i = 0; i < outp.length; i++) {
                            outp[i](ejOut, "end");
                        }
                    }

                    // who's the winner?
                    if (lflag >= 2 && rflag < 2) {
                        cb(1);
                    } else if (rflag >= 2 && lflag < 2) {
                        cb(-1);
                    } else {
                        cb(0);
                    }
                    break;

                } else if (new Date().getTime() > stTime + 100) {
                    // don't stall
                    bg = setTimeout(runSteps, 100);
                    break;

                } else if (wait !== null) {
                    bg = setTimeout(runSteps, wait);
                    break;

                }
            }
        }
        runSteps();
    }

    // set up our runner
    this.go = function(ltxt, rtxt) {
        var lerr = {};
        var rerr = {};
        var left = parse(ltxt, lerr);
        var right = parse(rtxt, rerr);
        var span;

        // report errors
        if (lerr.msg) {
            console.error('Left:');
            console.error(lerr.msg);
            process.exit(1);
        }
        if (rerr.msg) {
            console.error('Right:');
            console.error(right.msg);
            process.exit(1);
        }

        var scores = [new Array(31), new Array(31)];

        var pol = 1;
        var tlen = 10;
        var lwins = 0, rwins = 0;
        function run() {
            bg = null;

            joust(left, right, 1, pol, tlen, null, function(res) {
                scores[pol > 0 ? 0 : 1][tlen] = -res;
    
                // advance
                tlen++;
                if (tlen > 30) {
                    tlen = 10;
                    pol -= 2;
                }
                if (pol >= -1)
                    run();
            });
        }
        run();

        var summary = '';
        var score = 0;
        for (pol = 0; pol < 2; pol++)
        {
            for (tlen = 10; tlen <= 30; tlen++)
            {
                if (scores[pol][tlen] > 0) summary += '<';
                else if (scores[pol][tlen] < 0) summary += '>';
                else summary += 'X';
                score += scores[pol][tlen];
            }
            summary += ' ';
        }
        summary += score;

        console.log(summary);
        process.exit(0);
    };
})();

if (process.argv.length != 4)
{
    console.error('usage: nodejs egojsout.js left.bfjoust right.bfjoust');
    process.exit(1);
}

fs = require('fs');

var left = fs.readFileSync(process.argv[2], { encoding: 'utf8' });
var right = fs.readFileSync(process.argv[3], { encoding: 'utf8' });

EgoJSout.go(left, right);
