'use strict';

var Cycles = {
    cfg: 0,
};

(function (){
    var cw = 15, ch = 15, cgap = 1;
    var subh = 130, substub = 10;
    var subc = ['#af8dc3', '#7f7f7f', '#7fbf7b'];

    function setup() {
        Plot.setup({
            width: Math.max(Vis.N, 2*Vis.T + 1) * (cw + cgap) + cgap,
            height: Vis.N * (ch + cgap) + cgap,
            left: 80, right: 180, top: 0, bottom: 200,
            topLayer: true,
        });

        d3.json('../data/cycles.json',
            function (error, json) { if (json) onData(json); });

        Cycles.scale = d3.scale.log()
            .domain(Plot.colors.heat.domain(1, 4200000, true)) // placeholder
            .range(Plot.colors.heat.range)
            .clamp(true);
        Cycles.vscale = d3.scale.log()
            .domain([1, 4200000])
            .range([Vis.N * (ch + cgap) + cgap, 3])
            .clamp(true);

        var scaleGrad = Plot.svgDefs.append('linearGradient').attr('id', 'scale_grad')
            .attr('x1', 0).attr('y1', 1).attr('x2', 0).attr('y2', 0);

        var range = Cycles.scale.range();
        var nstops = range.length;
        var stops = scaleGrad.selectAll('stop').data(range);
        stops.enter().append('stop')
            .attr('offset', function (d,i) { return i / (nstops-1); })
            .attr('stop-color', function (d) { return d; });

        var scaleRect = Plot.svg.append('rect').attr('class', 'scale')
            .attr('x', -39.5)
            .attr('y', 0.5)
            .attr('width', 19)
            .attr('height', Vis.N * (ch + cgap) + cgap - 1)
            .style('stroke', 'black')
            .style('stroke-width', 1)
            .style('fill', 'url(#scale_grad)');

        var dimFilter = Plot.svgDefs.append('filter').attr('id', 'dimmer');
        var dimComp = dimFilter.append('feComponentTransfer');
        dimComp.append('feFuncR').attr('type', 'linear').attr('slope', 0.25);
        dimComp.append('feFuncG').attr('type', 'linear').attr('slope', 0.25);
        dimComp.append('feFuncB').attr('type', 'linear').attr('slope', 0.25);

        Cycles.cellGroup = Plot.svg.append('g');

        Cycles.bottomLabel = Plot.svg.append('text')
            .attr('class', 'lb')
            .attr('x', 0)
            .attr('y', (Vis.N + 1) * (ch + cgap))
            .style('font-size', 13)
            .style('visibility', 'hidden')
            .text('{placeholder}');

        Cycles.valueLabel = Plot.svg.append('text')
            .attr('class', 'lb')
            .attr('x', -45)
            .attr('y', 0)
            .attr('text-anchor', 'end')
            .style('dominant-baseline', 'middle')
            .style('font-size', 13)
            .style('visibility', 'hidden')
            .text('{placeholder}');

        var subgBox = Plot.svg.append('g')
            .attr('transform', 'translate(1,' + (Vis.N * (ch + cgap) + 30 + subh + substub) + ')');
        Cycles.subgBox = subgBox;
        Cycles.subg = subgBox.append('g')
            .attr('transform', 'scale(1 -1)');

        Cycles.subg.append('line').attr('class', 'grid')
            .attr('x1', -0.5).attr('y1', -1)
            .attr('x2', -0.5).attr('y2', subh + substub);
        Cycles.subg.append('line').attr('class', 'grid')
            .attr('x1', -1).attr('y1', -0.5)
            .attr('x2', Vis.T * (cw + cgap) - 1).attr('y2', -0.5);
        Cycles.subg.append('line').attr('class', 'grid')
            .attr('x1', (Vis.T + 1) * (cw + cgap)).attr('y1', -0.5)
            .attr('x2', (2*Vis.T + 1) * (cw + cgap)).attr('y2', -0.5);
        Cycles.subg.append('line').attr('class', 'grid')
            .attr('x1', (2*Vis.T + 1) * (cw + cgap) - 0.5).attr('y1', -1)
            .attr('x2', (2*Vis.T + 1) * (cw + cgap) - 0.5).attr('y1', subh + substub);

        Cycles.subscale = d3.scale.log()
            .domain([1, 100000]) // placeholder
            .range([0, subh])
            .clamp(true);

        Cycles.subg.selectAll('rect.bar').data(new Array(2*Vis.T))
            .enter().append('rect').attr('class', 'bar')
            .attr('x', function (d,i) {
                return (i < Vis.T ? Vis.T + 1 + i : Vis.T - 1 - (i - Vis.T)) * (cw + cgap) + 1;
            })
            .attr('y', 0)
            .attr('width', cw - 2)
            .attr('height', substub)
            .style('fill', subc[1]);

        var tlabels = [];
        for (var t = Vis.mintape; t <= Vis.maxtape; t += 10)
            tlabels.push({ x: Vis.T - 1 - (t - Vis.mintape), l: t },
                         { x: Vis.T + 1 + (t - Vis.mintape), l: t });
        subgBox.selectAll('text.t').data(tlabels)
            .enter().append('text').attr('class', 't')
            .attr('x', function (d) { return (d.x + 0.5) * (cw + cgap); })
            .attr('y', 16)
            .attr('text-anchor', 'middle')
            .style('font-size', 13)
            .text(function (d) { return d.l; });

        Cycles.subtmin = subgBox.append('text').attr('class', 'lb')
            .attr('x', (2 * Vis.T + 1.5) * (cw + cgap))
            .attr('y', -2-substub)
            .style('dominant-baseline', 'middle')
            .style('font-size', 13)
            .style('visibility', 'hidden')
            .text('{placeholder}');
        Cycles.subtmax = subgBox.append('text').attr('class', 'lb')
            .attr('x', (2 * Vis.T + 1.5) * (cw + cgap))
            .attr('y', -subh-substub)
            .style('dominant-baseline', 'middle')
            .style('font-size', 13)
            .style('visibility', 'hidden')
            .text('{placeholder}');
        subgBox.selectAll('text.winl').data(['{placeholder}','tie','{placeholder}'])
            .enter().append('text').attr('class', 'winl lb')
            .attr('x', (2 * Vis.T + 1.5) * (cw + cgap))
            .attr('y', function (d,i) { return -subh/2-substub + (i-1)*16; })
            .style('dominant-baseline', 'middle')
            .style('font-size', 13)
            .style('visibility', 'hidden')
            .style('fill', function (d,i) { return subc[i]; })
            .text(function (d) { return d; });

        var form = d3.select('#plot-controls').append('form')
            .attr('role', 'form').attr('name', 'ctrl');

        var cfgGroup = form.append('div').attr('class', 'form-group');
        cfgGroup.append('label').attr('for', 'cfg').text('Tape/polarity configuration:');

        Cycles.cfgSelect = cfgGroup.append('select').attr('id', 'cfg').attr('class', 'form-control');
        Cycles.cfgSelect.selectAll('option').data(new Array(2 * Vis.T + 1))
            .enter().append('option')
            .attr('value', function (d,i) { return i; })
            .text(function (d,i) {
                return i == 0
                    ? 'Total cycles'
                    : i <= Vis.T
                    ? 'Sieve, length ' + (Vis.mintape + i - 1)
                    : 'Kettle, length ' + (Vis.mintape + i - Vis.T - 1);
            });
        Cycles.cfgSelect.on('change', onCfgChange);

        var orderGroup = form.append('div').attr('class', 'form-group');
        orderGroup.append('label').text('Sort by:');
        orderGroup.append('br');

        var orders = ['total cycles', 'current cycles', 'alpha'];
        for (var o = 0; o < orders.length; o++) {
            var orderLabel = orderGroup.append('label').attr('class', 'radio-inline');
            orderLabel.append('input').attr('type', 'radio')
                .attr('name', 'ord').attr('id', 'ord'+o)
                .attr('value', o)
                .on('change', onOrdChange);
            orderLabel.node().appendChild(document.createTextNode(' ' + orders[o]));
        }
        document.forms.ctrl.ord.value = 0;
    }

    function onData(json) {
        // TODO: error messages in general
        if (json.data.length != Vis.N * (Vis.N + 1) / 2)
            return;

        Cycles.min = json.min;

        var data = new Array(Vis.N * Vis.N);

        var totals = new Array(2 * Vis.T + 1);
        for (var t = 0; t < 2 * Vis.T + 1; t++) {
            totals[t] = new Array(Vis.N);
            for (var n = 0; n < Vis.N; n++)
                totals[t][n] = 0;
        }

        var at = 0;
        for (var left = 0; left < Vis.N; left++) {
            for (var right = left; right < Vis.N; right++) {
                var cycles = json.data[at];
                var pwins = json.wins[at];
                at++;

                data[left * Vis.N + right] = { left: left, right: right, cycles: cycles, wins: pwins, winFlip: 1 };
                if (left != right)
                    data[right * Vis.N + left] = { left: right, right: left, cycles: cycles, wins: pwins, winFlip: -1 };

                for (var t = 0; t < 2 * Vis.T + 1; t++) {
                    totals[t][left] += cycles[t];
                    if (left != right)
                        totals[t][right] += cycles[t];
                }
            }
        }

        Cycles.data = data;
        Cycles.totals = totals;

        recomputeOrder();
        refresh();
    }

    function recomputeOrder() {
        // TODO: refactor utilities into Plot

        var func, ord = +document.forms.ctrl.ord.value;
        if (ord == 0)
            func = function (a, b) {
                var ta = Cycles.totals[0][a], tb = Cycles.totals[0][b];
                if (ta > tb) return -1; else if (ta < tb) return 1;
                return Vis.names[a] < Vis.names[b] ? -1 : 1;
            };
        else if (ord == 1)
            func = function (a, b) {
                var ta = Cycles.totals[Cycles.cfg][a], tb = Cycles.totals[Cycles.cfg][b];
                if (ta > tb) return -1; else if (ta < tb) return 1;
                return Vis.names[a] < Vis.names[b] ? -1 : 1;
            };

        var iorder = new Array(Vis.N);
        for (var n = 0; n < Vis.N; n++)
            iorder[n] = n;
        if (func) {
            iorder.sort(func);
            Cycles.order = new Array(Vis.N);
            for (var n = 0; n < Vis.N; n++)
                Cycles.order[iorder[n]] = n;
        } else
            Cycles.order = iorder;
    }

    function onCfgChange() {
        if (Cycles.chosen) { onCellClick(); clearHover(); } // clear freeze

        Cycles.cfg = +Cycles.cfgSelect.property('value');

        if (+document.forms.ctrl.ord.value == 1)
            recomputeOrder();
        refresh();
    }

    function onOrdChange() {
        if (Cycles.chosen) { onCellClick(); clearHover(); } // clear freeze

        recomputeOrder();
        refresh();
    }

    function onCellClick() {
        if (Cycles.chosen) {
            d3.select('text#rl' + Cycles.chosen.right).classed('lb', false);
            Cycles.chosen.marker.remove();
            Cycles.cellGroup.attr('filter', null);
            delete Cycles.chosen;
            onCellHover(true /* enter */); // refresh hover
            return;
        }

        var d = d3.event.target.__data__;
        if (!(d && 'left' in d && 'right' in d))
            return;

        var marker = makeCells(Plot.topLayer, [d], false /* hover */);
        Cycles.cellGroup.attr('filter', 'url(#dimmer)');

        Cycles.chosen = {
            left: d.left, right: d.right,
            marker: marker,
        };
    }

    function clearHover() {
        Plot.svg.select('text.rl').classed('lb', false);
        Cycles.bottomLabel.style('visibility', 'hidden');
        Cycles.valueLabel.style('visibility', 'hidden');
        refreshSubgraph(null);
    }

    function onCellHover(enter) {
        if (Cycles.chosen)
            return; // hover behavior frozen
        var d = d3.event.target.__data__;

        if (!d || !('cycles' in d))
            return; // not the right sort of click

        Plot.svg.select('text#rl' + d.right).classed('lb', enter);

        if (!enter) {
            Cycles.bottomLabel.style('visibility', 'hidden');
            Cycles.valueLabel.style('visibility', 'hidden');
            refreshSubgraph(null);
        } else {
            var c = d.cycles[Cycles.cfg];
            Cycles.bottomLabel
                .style('visibility', 'visible')
                .attr('x', Cycles.order[d.left] * (cw + cgap) + cgap)
                .text(Vis.names[d.left]);
            Cycles.valueLabel
                .style('visibility', 'visible')
                .text(c)
                .transition().duration(250)
                .attr('y', Cycles.vscale(c));
            refreshSubgraph(d.cycles.slice(1), d.wins, d.winFlip, Vis.names[d.left], Vis.names[d.right]);
        }
    }

    function refresh() {
        var smin = Cycles.min[Cycles.cfg];
        var smax = Cycles.cfg > 0 ? 100000 : 4200000;
        Cycles.scale.domain(Plot.colors.heat.domain(smin, smax, true));
        Cycles.vscale.domain([smin, smax]);

        makeCells(Cycles.cellGroup, Cycles.data, true /* hover */);

        var rightNames = Plot.svg.selectAll('text.rl').data(Vis.names);
        rightNames.enter().append('text').attr('class', 'rl')
            .attr('id', function (d,i) { return 'rl' + i; })
            .style('font-size', 13)
            .style('dominant-baseline', 'middle')
            .attr('x', Vis.N * (cw + cgap) + 10)
            .text(function (d) { return d; });
        rightNames
            .attr('y', function (d,i) { return (Cycles.order[i] + 0.5) * (ch + cgap); });
        Cycles.rightNames = rightNames;
    }

    function makeCells(sel, data, hover) {
        var cells = sel.selectAll('rect.c').data(data);

        var enter = cells.enter().append('rect').attr('class', 'c');
        enter
            .attr('width', cw + cgap)
            .attr('height', ch + cgap)
            .style('stroke', 'black')
            .style('stroke-width', cgap)
            .on('click', onCellClick);

        if (hover)
            enter
                .on('mouseover', function () { onCellHover(true); })
                .on('mouseout', function () { onCellHover(false); });

        cells
            .attr('x', function (d) { return Cycles.order[d.left] * (cw + cgap) + cgap/2; })
            .attr('y', function (d) { return Cycles.order[d.right] * (ch + cgap) + cgap/2; })
            .style('fill', function (d) {
                return Cycles.scale(Math.max(1, d.cycles[Cycles.cfg]));
            });
        return cells;
    }

    function refreshSubgraph(data, wins, flip, left, right) {
        if (data) {
            var min = data[0], max = data[0];
            for (var i = 1; i < data.length; i++) {
                if (data[i] < min) min = data[i];
                if (data[i] > max) max = data[i];
            }
            if (min < 1) min = 1;
            if (max < min) max = min;
            Cycles.subscale.domain(max == min ? [max-1, max+1] : [min, max]);
            Cycles.subtmin.style('visibility', 'visible').text(min);
            Cycles.subtmax.style('visibility', 'visible').text(max);
            Cycles.subgBox.selectAll('text.winl').data([left, 'tie', right])
                .style('visibility', 'visible')
                .text(function (d) { return d; });
        }
        else {
            data = new Array(2 * Vis.T);
            wins = new Array(2 * Vis.T);
            Cycles.subtmin.style('visibility', 'hidden');
            Cycles.subtmax.style('visibility', 'hidden');
            Cycles.subgBox.selectAll('text.winl').style('visibility', 'hidden');
        }

        Cycles.subg.selectAll('rect.bar').data(data)
            .transition().duration(250)
            .attr('height', function (d) {
                return d ? substub + Cycles.subscale(d) : substub;
            })
            .style('fill', function (d,i) {
                return subc[d ? 1 + flip*wins[i] : 1];
            });
    }

    setup();
})();
