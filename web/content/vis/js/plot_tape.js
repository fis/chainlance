var Plot = {};

Plot.setup = function (opts) {
    var svg = d3.select('#plot-canvas')
        .append('svg')
        .attr('width', opts.width + opts.left + opts.right)
        .attr('height', opts.height + opts.top + opts.bottom);

    Plot.svgDefs = svg.append('defs');

    var transform = 'translate(' + opts.left + ',' + opts.top + ')';

    Plot.svg = svg.append('g').attr('transform', transform);
    if (opts.topLayer)
        Plot.topLayer = svg.append('g').attr('transform', transform);

    Vis.nameMap = {};
    for (var n = 0; n < Vis.N; n++)
        Vis.nameMap[Vis.names[n]] = n;
};

Plot.arg = function (idx) {
    var args = window.location.hash.substring(1).split(',');
    return args[idx];
};

(function (){
    function equalDomain(n) {
        return function (min, max, log) {
            var domain = Array(n);
            if (log)
                min = Math.log10(min), max = Math.log10(max);
            for (var i = 0; i < n; i++)
                domain[i] = log ? Math.pow(10, min + i/(n-1)*(max-min)) : min + i/(n-1)*(max-min);
            return domain;
        }
    }

    Plot.colors = {
        heat: {
            range: ['rgb(215,48,39)','rgb(252,141,89)','rgb(254,224,144)','rgb(255,255,191)','rgb(224,243,248)','rgb(145,191,219)','rgb(69,117,180)'].reverse(),
            domain: equalDomain(7)
        }
    };
})();

var ProgPlot = {
    prog: 0
};

ProgPlot.setup = function (opts) {
    Plot.setup(opts);
    ProgPlot.progData = opts.progData;
    ProgPlot.onProg = opts.onProg;

    var hashProg = Plot.arg(0);
    if (hashProg in Vis.nameMap)
        ProgPlot.prog = Vis.nameMap[hashProg];

    ProgPlot.form = d3.select('#plot-controls').append('form')
        .attr('role', 'form');

    var progGroup = ProgPlot.form.append('div').attr('class', 'form-group');
    progGroup.append('label').attr('for', 'prog').text('Program:');

    ProgPlot.progSelect = progGroup.append('select')
        .attr('id', 'prog')
        .attr('class', 'form-control');
    ProgPlot.progSelect.selectAll('option').data(Vis.names)
        .enter().append('option')
        .attr('value', function (d,i) { return i; })
        .text(function (d) { return d; });

    ProgPlot.progSelect.node().value = ProgPlot.prog;
    ProgPlot.progSelect.on('change', ProgPlot.onProgChange);
    ProgPlot.onProgChange();
};

ProgPlot.onProgChange = function () {
    var sel = ProgPlot.progSelect.node();
    var prog = +sel.value;
    sel.disabled = true;

    d3.json('../data/' + ProgPlot.progData + '.p' + prog + '.json',
        function (error, json) { if (json) ProgPlot.onProgLoad(prog, json); });
};

ProgPlot.onProgLoad = function (prog, json) {
    var sel = ProgPlot.progSelect.node();
    sel.value = prog;
    sel.disabled = false;

    ProgPlot.prog = prog;
    ProgPlot.onProg(prog, json);

    if (!window.location.hash.startsWith('#' + Vis.names[prog]))
        window.location.hash = Vis.names[prog];
};

var TapePlot = {
    opponent: 0,
    splitPol: false,
};

(function (){
    var cw = 15, ch = 15, cgap = 1, lgap = 8;

    TapePlot.setup = function (opts) {
        TapePlot.logScale = opts.logScale;
        ProgPlot.setup({
            width: Vis.maxtape * (cw + cgap) + cgap,
            height: Math.max(2*Vis.T, Vis.N+1) * (ch + cgap) + cgap,
            left: 30, right: 120, top: 30, bottom: 80,
            topLayer: true,
            progData: opts.progData,
            onProg: TapePlot.onProg
        });

        d3.select(window).on('popstate', parseHash);
        parseHash();

        var oppGroup = ProgPlot.form.append('div').attr('class', 'form-group');
        oppGroup.append('label').attr('for', 'opp').text('Opponent:');

        var oppData = ['{average}'].concat(Vis.names);

        TapePlot.oppSelect = oppGroup.append('select')
            .attr('id', 'opp')
            .attr('class', 'form-control');
        TapePlot.oppSelect.selectAll('option').data(oppData)
            .enter().append('option')
            .attr('value', function (d,i) { return i; })
            .text(function (d) { return d; });
        TapePlot.oppSelect.node().disabled = true;

        TapePlot.oppSelect.on('change', onOppChange);

        var polLabel = ProgPlot.form
            .append('div').attr('class', 'checkbox')
            .append('label');
        TapePlot.polCheck = polLabel.append('input').attr('type', 'checkbox');
        polLabel.node().appendChild(document.createTextNode(' Show sieve/kettle polarity separately'));
        TapePlot.polCheck.node().disabled = true;

        polLabel.on('change', onPolChange);

        TapePlot.scale_grad = Plot.svgDefs.append('linearGradient').attr('id', 'scale_grad');
    };

    TapePlot.onProg = function (prog, json) {
        TapePlot.json = json;

        var sel = TapePlot.oppSelect.node();
        sel.value = TapePlot.opponent;
        sel.disabled = false;

        var check = TapePlot.polCheck.node();
        check.checked = TapePlot.splitPol;
        check.disabled = false;

        refresh();
    };

    function onOppChange() {
        var sel = TapePlot.oppSelect.node();
        TapePlot.opponent = +sel.value;
        refresh();
    }

    function onPolChange() {
        var check = TapePlot.polCheck.node();
        TapePlot.splitPol = check.checked;
        refresh();
    }

    function onRowClick() {
        var id = d3.event.target.id;
        if (!id.startsWith('row') && !id.startsWith('lab'))
            return;
        var row = +id.substring(3);
        var rowData = TapePlot.rows[row];

        if (rowData.opp === undefined) {
            // switch from all-tapes to one-tape view
            TapePlot.oneTape = { idx: rowData.len - Vis.mintape, pol: rowData.pol };
            TapePlot.oppSelect.node().disabled = true;
            TapePlot.polCheck.node().disabled = true;
        } else {
            // switch from one-tape to all-tapes view
            delete TapePlot.oneTape;
            TapePlot.opponent = rowData.opp;
            TapePlot.oppSelect.node().value = rowData.opp;
            TapePlot.oppSelect.node().disabled = false;
            TapePlot.polCheck.node().disabled = false;
        }

        refresh();
    }

    function onTitlePolClick() {
        var newPol = d3.event.target.textContent;
        if (newPol == 'averaged') TapePlot.oneTape.pol = undefined;
        else if (newPol == 'sieve') TapePlot.oneTape.pol = false;
        else if (newPol == 'kettle') TapePlot.oneTape.pol = true;

        if (TapePlot.oneTape.pol === undefined) {
            TapePlot.splitPol = false;
            TapePlot.polCheck.node().checked = false;
        } else {
            TapePlot.splitPol = true;
            TapePlot.polCheck.node().checked = true;
        }

        refresh();
    }

    function updateHash() {
        var needsOpp = TapePlot.opponent;
        var needsPol = TapePlot.splitPol;
        var needsTape = TapePlot.oneTape;

        var hash = Vis.names[ProgPlot.prog];
        if (needsOpp || needsPol || needsTape)
            hash += ',' + (needsOpp ? TapePlot.opponent : '');
        if (needsPol || needsTape)
            hash += ',' + (needsPol ? 'p' : '');
        if (needsTape) {
            hash += ',' + TapePlot.oneTape.idx;
            if (TapePlot.oneTape.pol !== undefined)
                hash += ',' + (TapePlot.oneTape.pol ? 1 : 0);
        }

        window.location.hash = hash;
    }

    function parseHash() {
        var opp, splitPol, oneTape, tapeIdx, tapePol;

        var hashOpp = Plot.arg(1);
        if (hashOpp !== undefined)
            opp = +hashOpp;
        else opp = 0;

        var hashPol = Plot.arg(2);
        if (hashPol !== undefined)
            splitPol = !!hashPol;
        else splitPol = false;

        var hashTape = Plot.arg(3), hashTapePol = Plot.arg(4);
        if (hashTape !== undefined) {
            oneTape = true;
            tapeIdx = +hashTape;
            tapePol = hashTapePol;
            if (tapePol !== undefined) tapePol = !!+tapePol;
        }
        else oneTape = false;

        if (opp != TapePlot.opponent || splitPol != TapePlot.splitPol
            || (!TapePlot.oneTape && oneTape)
            || (TapePlot.oneTape && !oneTape)
            || (TapePlot.oneTape && oneTape
                && (TapePlot.oneTape.idx != tapeIdx
                    || TapePlot.oneTape.pol !== tapePol))) {
            TapePlot.opponent = opp;
            TapePlot.splitPol = splitPol;
            if (!oneTape)
                delete TapePlot.oneTape;
            else
                TapePlot.oneTape = { idx: tapeIdx, pol: tapePol };
            if (TapePlot.json) // only refresh if data already loaded
                refresh();
        }
    }

    function refresh() {
        if (!TapePlot.oneTape)
            makeData();
        else
            makeDataForTape();
        makeLabelsX();
        updatePlot();
        updateHash();
    }

    function makeData() {
        var opponent = TapePlot.opponent;
        var src = !opponent ? TapePlot.json.avg : TapePlot.json.all[opponent - 1];
        var factor = !opponent ? (1/Vis.N) : 1;

        var data = [];
        var min = Vis.maxcycles, max = 0;

        for (var t = 0; t < Vis.T; t++) {
            var tape = src[t], ptape = src[Vis.T + t];
            for (var pos = 0; pos < tape.length; pos++) {
                if (!TapePlot.splitPol) {
                    var v = factor * (tape[pos] + ptape[pos]) / 2;
                    data.push({ y: t, x: pos, v: v });
                    min = Math.min(min, v);
                    max = Math.max(max, v);
                } else {
                    var v = factor * tape[pos], pv = factor * ptape[pos];
                    data.push({ y: 2*t, x: pos, v: v });
                    data.push({ y: 2*t+1, x: pos, v: pv });
                    min = Math.min(min, v, pv);
                    max = Math.max(max, v, pv);
                }
            }
        }

        TapePlot.data = data;
        makeDataScale(min, max);

        TapePlot.grid_x = [];
        for (var x = 0; x <= Vis.maxtape; x++) {
            if (!TapePlot.splitPol)
                TapePlot.grid_x.push(Math.max(0, x - Vis.mintape));
            else
                TapePlot.grid_x.push(Math.max(0, 2 * (x - Vis.mintape)));
        }

        TapePlot.grid_y = [];
        if (!TapePlot.splitPol)
            for (var y = 0; y <= Vis.T; y++)
                TapePlot.grid_y.push(Math.min(Vis.maxtape, Vis.mintape + y));
        else
            for (var y = 0; y <= 2*Vis.T; y++)
                TapePlot.grid_y.push(Math.min(Vis.maxtape, Vis.mintape + Math.floor(y / 2)));

        TapePlot.labels_y = [];
        for (var t = 0; t < Vis.T; t++) {
            var y = (TapePlot.splitPol ? 2 : 1) * t * (ch + cgap) + cgap + ch / 2;
            TapePlot.labels_y.push({ x: -lgap, y: y, l: (Vis.mintape + t) + (TapePlot.splitPol ? 's' : '') });
            if (TapePlot.splitPol)
                TapePlot.labels_y.push({ x: -lgap, y: y+ch+cgap, l: (Vis.mintape + t) + 'k' });
        }

        TapePlot.rows = [];
        for (var t = Vis.mintape; t <= Vis.maxtape; t++) {
            if (!TapePlot.splitPol)
                TapePlot.rows.push({ len: t });
            else {
                TapePlot.rows.push({ len: t, pol: false });
                TapePlot.rows.push({ len: t, pol: true });
            }
        }
    }

    function makeDataForTape() {
        var tapeIdx = TapePlot.oneTape.idx;
        var tapePol = TapePlot.oneTape.pol;
        var tapeLen = Vis.mintape + tapeIdx;

        var data = [];
        var min = Vis.maxcycles, max = 0;

        for (var n = 0; n <= Vis.N; n++) {
            var opp = n == 0 ? TapePlot.json.avg : TapePlot.json.all[n - 1];
            var factor = n == 0 ? (1/Vis.N) : 1;
            var tape = opp[tapeIdx], ptape = opp[Vis.T + tapeIdx];
            for (var pos = 0; pos < tapeLen; pos++) {
                var v = factor * (tapePol === undefined ? (tape[pos] + ptape[pos]) / 2 :
                                  tapePol ? ptape[pos] : tape[pos]);
                data.push({ y: n, x: pos, v: v });
                min = Math.min(min, v);
                max = Math.max(max, v);
            }
        }

        TapePlot.data = data;
        makeDataScale(min, max);

        TapePlot.grid_x = [];
        for (var x = 0; x <= Vis.mintape + tapeIdx; x++)
            TapePlot.grid_x.push(0);

        TapePlot.grid_y = [];
        for (var y = 0; y <= Vis.N + 1; y++)
            TapePlot.grid_y.push(Vis.mintape + tapeIdx);

        TapePlot.labels_y = [];
        for (var n = 0; n <= Vis.N + 1; n++) {
            TapePlot.labels_y.push({
                x: tapeLen * (cw + cgap) + cgap + lgap,
                y: n * (ch + cgap) + cgap + ch / 2,
                l: n == 0 ? '{average}' : Vis.names[n - 1]
            });
        }

        TapePlot.rows = [];
        for (var n = 0; n <= Vis.N; n++)
            TapePlot.rows.push({ len: tapeLen, opp: n });
    }

    function makeDataScale(min, max) {
        if (min < 0.1) min = 0.1;
        if (max <= min + 1) max = min + 1;

        var domain = Plot.colors.heat.domain(min, max, TapePlot.logScale);
        var range = Plot.colors.heat.range;

        var scale = TapePlot.logScale ? d3.scale.log() : d3.scale.linear();
        scale.domain(domain).range(range);
        TapePlot.scale = scale;

        // TODO: maybe make scale fixed-width
        var ncols = TapePlot.oneTape ? Vis.mintape + TapePlot.oneTape.idx : Vis.maxtape;
        // 30 -> 7  // 20 -> 5  // 10 -> 3
        var nlabels = 3 + Math.floor((ncols - 10) / 5); // 3 @10, 5 @20, 7 @30

        if (TapePlot.logScale)
            min = Math.log10(min), max = Math.log10(max);

        TapePlot.labels_scale = [];
        for (var i = 0; i < nlabels; i++) {
            var stop = min + i/(nlabels-1) * (max - min);
            if (TapePlot.logScale)
                stop = Math.pow(10, stop);
            TapePlot.labels_scale.push(Math.round(stop));
        }
    }

    function makeLabelsX() {
        var w = TapePlot.oneTape ? Vis.mintape + TapePlot.oneTape.idx : Vis.maxtape;

        TapePlot.labels_x = [
            { x: 1, l: 'home' },
            { x: 10, l: '10' }
        ];
        if (w >= 20) TapePlot.labels_x.push({ x: 20, l: '20' });
        if (w >= 30) TapePlot.labels_x.push({ x: 30, l: '30' });
        if (w % 10 >= 2 && w % 10 <= 8)
            TapePlot.labels_x.push({ x: w, l: ''+w });
    }

    function updatePlot() {
        var textTitle;
        if (!TapePlot.oneTape)
            textTitle = 'All tape lengths, ' +
                (TapePlot.opponent == 0 ?
                 '<tspan class="b">averaged' :
                 'vs. <tspan class="b">' + Vis.names[TapePlot.opponent - 1]) +
                '</tspan>';
        else {
            textTitle = 'Tape <tspan class="b">' +
                (TapePlot.oneTape.idx + Vis.mintape) +
                '</tspan>, polarity <tspan class="b">' +
                (TapePlot.oneTape.pol === undefined ? 'averaged' :
                 TapePlot.oneTape.pol ? 'kettle' : 'sieve') +
                '</tspan> (show ';
            var links = [];
            if (TapePlot.oneTape.pol !== undefined)
                links.push('<tspan class="gotoPol">averaged</tspan>');
            if (TapePlot.oneTape.pol !== false)
                links.push('<tspan class="gotoPol">sieve</tspan>');
            if (TapePlot.oneTape.pol !== true)
                links.push('<tspan class="gotoPol">kettle</tspan>');
            textTitle += links.join(', ') + ')';
        }

        var scale = TapePlot.scale;
        var range = scale.range();

        var cells = Plot.svg.selectAll('.cell').data(TapePlot.data);
        cells.enter().append('rect').attr('class', 'cell')
            .attr('width', cw).attr('height', ch);
        cells
            .attr('x', function (d) { return cgap + d.x * (cw + cgap); })
            .attr('y', function (d) { return cgap + d.y * (ch + cgap); })
            .style('fill', function (d) { return scale(d.v); });
        cells.exit().remove();

        var nrows = TapePlot.oneTape ? Vis.N + 1 : (TapePlot.splitPol ? 2 : 1) * Vis.T;
        var bottom = cgap + nrows * (ch + cgap);
        var ncols = TapePlot.oneTape ? Vis.mintape + TapePlot.oneTape.idx : Vis.maxtape;
        var width = cgap + ncols * (cw + cgap);

        Plot.svg.selectAll('#title').remove();
        var title = Plot.svg.append('text').attr('id', 'title');
        title
            .attr('x', 0).attr('y', -ch)
            .html(textTitle);
        Plot.svg.selectAll('.gotoPol').on('click', onTitlePolClick);

        var gridlines_x = Plot.svg.selectAll('.grid_x').data(TapePlot.grid_x);
        gridlines_x.enter().append('line').attr('class', 'grid_x')
            .attr('x1', function (d,i) { return cgap/2 + i * (cw + cgap); })
            .attr('x2', function (d,i) { return cgap/2 + i * (cw + cgap); });
        gridlines_x
            .attr('y1', function (d) { return d * (ch + cgap); })
            .attr('y2', bottom);
        gridlines_x.exit().remove();

        var gridlines_y = Plot.svg.selectAll('.grid_y').data(TapePlot.grid_y);
        gridlines_y.enter().append('line').attr('class', 'grid_y')
            .attr('y1', function (d,i) { return cgap/2 + i * (ch + cgap); })
            .attr('y2', function (d,i) { return cgap/2 + i * (ch + cgap); })
            .attr('x1', '0');
        gridlines_y
            .attr('x2', function (d) { return d * (cw + cgap); });
        gridlines_y.exit().remove();

        var lab_y = Plot.svg.selectAll('.lab_y').data(TapePlot.labels_y);
        lab_y.enter().append('text').attr('class', 'lab_y')
            .attr('id', function (d,i) { return 'lab'+i; })
            .on('click', onRowClick);
        lab_y
            .attr('text-anchor', TapePlot.oneTape ? 'start' : 'end')
            .text(function (d) { return d.l; })
            .attr('x', function (d) { return d.x; })
            .attr('y', function (d) { return d.y; });
        lab_y.exit().remove();

        var lab_x = Plot.svg.selectAll('.lab_x').data(TapePlot.labels_x);
        lab_x.enter().append('text').attr('class', 'lab_x')
            .attr('text-anchor', 'middle');
        lab_x
            .text(function (d) { return d.l; })
            .attr('x', function (d) { return (d.x-1) * (cw + cgap) + cgap + cw/2; })
            .attr('y', bottom + cgap + ch);
        lab_x.exit().remove();

        var nstops = range.length;
        var stops = TapePlot.scale_grad.selectAll('stop').data(range);
        stops.enter().append('stop');
        stops
            .attr('offset', function (d,i) { return i / (nstops-1); })
            .attr('stop-color', function (d) { return d; });

        Plot.svg.selectAll('#scale_g').remove();
        var g = Plot.svg.append('g').attr('id', 'scale_g');
        g.append('rect')
            .attr('x', 0.5).attr('y', bottom + 30.5)
            .attr('width', width - 1).attr('height', 29)
            .style('fill', 'url(#scale_grad)')
            .style('stroke-width', 1)
            .style('stroke', 'black');

        var nlabels = TapePlot.labels_scale.length;

        var lab_s = Plot.svg.selectAll('.lab_s').data(TapePlot.labels_scale);
        lab_s.enter().append('text').attr('class', 'lab_s')
            .attr('text-anchor', 'middle');
        lab_s
            .text(function (d) { return d; })
            .attr('x', function (d,i) { return 0.5 + i/(nlabels-1) * (width - 1); })
            .attr('y', bottom + 60 + ch);
        lab_s.exit().remove();

        var rows = Plot.topLayer.selectAll('.row').data(TapePlot.rows);
        rows.enter().append('rect').attr('class', 'row')
            .attr('id', function (d,i) { return 'row'+i; })
            .attr('x', 0)
            .attr('y', function (d,i) { return i * (ch + cgap); })
            .attr('height', ch + cgap)
            .attr('pointer-events', 'visible')
            .on('click', onRowClick);
        rows
            .attr('width', function (d) { return d.len * (cw + cgap) + cgap; });
        rows.exit().remove();
    }
})();
