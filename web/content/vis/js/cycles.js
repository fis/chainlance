var Cycles = {
    cfg: 0,
};

(function (){
    var cw = 15, ch = 15, cgap = 1;

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

        var subh = 120;

        var subgBox = Plot.svg.append('g')
            .attr('transform', 'translate(1,' + (Vis.N * (ch + cgap) + 30 + subh) + ')');
        Cycles.subg = subgBox.append('g')
            .attr('transform', 'scale(1 -1)');

        Cycles.subg.append('line').attr('class', 'grid')
            .attr('x1', -0.5).attr('y1', -1)
            .attr('x2', -0.5).attr('y2', subh);
        Cycles.subg.append('line').attr('class', 'grid')
            .attr('x1', -1).attr('y1', -0.5)
            .attr('x2', Vis.T * (cw + cgap) - 1).attr('y2', -0.5);
        Cycles.subg.append('line').attr('class', 'grid')
            .attr('x1', (Vis.T + 1) * (cw + cgap)).attr('y1', -0.5)
            .attr('x2', (2*Vis.T + 1) * (cw + cgap)).attr('y2', -0.5);
        Cycles.subg.append('line').attr('class', 'grid')
            .attr('x1', (2*Vis.T + 1) * (cw + cgap) - 0.5).attr('y1', -1)
            .attr('x2', (2*Vis.T + 1) * (cw + cgap) - 0.5).attr('y1', subh);

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
            .attr('height', 0)
            .style('fill', '#8c6bb1');

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
            .attr('y', -2)
            .style('dominant-baseline', 'middle')
            .style('font-size', 13)
            .style('visibility', 'hidden')
            .text('{placeholder}');
        Cycles.subtmax = subgBox.append('text').attr('class', 'lb')
            .attr('x', (2 * Vis.T + 1.5) * (cw + cgap))
            .attr('y', -subh)
            .style('dominant-baseline', 'middle')
            .style('font-size', 13)
            .style('visibility', 'hidden')
            .text('{placeholder}');
    }

    function onData(json) {
        // TODO: error messages in general
        if (json.data.length != Vis.N * (Vis.N + 1) / 2)
            return;

        Cycles.min = json.min;

        var data = new Array(Vis.N * Vis.N);
        var totals = new Array(Vis.N);
        for (var n = 0; n < Vis.N; n++)
            totals[n] = 0;

        var at = 0;
        for (var left = 0; left < Vis.N; left++) {
            for (var right = left; right < Vis.N; right++) {
                var cycles = json.data[at++];

                data[left * Vis.N + right] = { left: left, right: right, cycles: cycles };
                if (left != right)
                    data[right * Vis.N + left] = { left: right, right: left, cycles: cycles };

                totals[left] += cycles[0];
                if (left != right)
                    totals[right] += cycles[0];
            }
        }

        Cycles.data = data;

        var iorder = new Array(Vis.N);
        for (var n = 0; n < Vis.N; n++)
            iorder[n] = n;
        iorder.sort(function (a, b) {
            return totals[a] > totals[b] ? -1 : 1;
        });
        Cycles.order = new Array(Vis.N);
        for (var n = 0; n < Vis.N; n++)
            Cycles.order[iorder[n]] = n;

        refresh();
    }

    function onCellClick() {
        if (Cycles.chosen) {
            d3.select('text#rl' + Cycles.chosen.right).classed('lb', false);
            Cycles.chosen.marker.remove();
            Cycles.cellGroup.attr('filter', null);
            delete Cycles.chosen;
            onCellHover(true /* enter */);
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

    function onCellHover(enter) {
        if (Cycles.chosen)
            return; // hover behavior frozen
        var d = d3.event.target.__data__;
        if (!(d && 'left' in d && 'right' in d))
            return;

        d3.select('text#rl' + d.right).classed('lb', enter);
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
            refreshSubgraph(d.cycles.slice(1));
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

    function refreshSubgraph(data) {
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
        }
        else {
            data = new Array(2 * Vis.T);
            Cycles.subtmin.style('visibility', 'hidden');
            Cycles.subtmax.style('visibility', 'hidden');
        }

        Cycles.subg.selectAll('rect.bar').data(data)
            .transition().duration(250)
            .attr('height', function (d) {
                return d ? Cycles.subscale(d) : 0;
            });

    }

    setup();
})();