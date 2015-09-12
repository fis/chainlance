var ProgWins = {};

(function (){
    var cw = 15, ch = 15, cgap = 1;
    var json = undefined;

    function setup() {
        ProgPlot.setup({
            width: (Vis.T * 2 + 1) * (cw + cgap) + cgap,
            height: Vis.N * (ch + cgap) + cgap,
            left: 0, right: 180, top: 0, bottom: 20,
            progData: 'prog_wins',
            onProg: onProg,
            handlePopState: true,
        });

        var orderGroup = ProgPlot.form.append('div').attr('class', 'form-group');
        orderGroup.append('label').text('Sort by:');
        orderGroup.append('br');

        var orders = ['alpha', 'points', 'tournament rank'];
        for (var o = 0; o < orders.length; o++) {
            var orderLabel = orderGroup.append('label').attr('class', 'radio-inline');
            orderLabel.append('input').attr('type', 'radio')
                .attr('name', 'ord').attr('id', 'ord'+o)
                .attr('value', o)
                .on('change', onOrdChange);
            orderLabel.node().appendChild(document.createTextNode(' ' + orders[o]));
        }

        ProgWins.curOrder = 0;
        document.forms.ctrl.ord.value = ProgWins.curOrder;

        ProgWins.scale = d3.scale.linear()
            .domain(Plot.colors.heat.domain(-1, 1))
            .range(Plot.colors.heat.range)
            .clamp(true);

        for (var t = Vis.mintape; t <= Vis.maxtape; t += 10) {
            var x = [Vis.T + 1 + t - Vis.mintape, Vis.T - 1 - (t - Vis.mintape)];
            Plot.svg.selectAll('text.tl'+t).data(x)
                .enter().append('text').attr('class', 'tl'+t)
                .attr('x', function (d) { return (d + 0.5) * (cw + cgap); })
                .attr('y', (Vis.N + 1) * (ch + cgap))
                .attr('text-anchor', 'middle')
                .style('font-size', 13)
                .text(t);
        }
    }

    function onProg(prog, json) {
        ProgWins.opps = json.opps;
        ProgWins.data = json.data;
        refresh();
    }

    function onOrdChange() {
        var newOrder = +document.forms.ctrl.ord.value;
        if (newOrder == ProgWins.curOrder)
            return;
        ProgWins.curOrder = newOrder;
        refresh();
    }

    function refresh() {
        makeData();
        updatePlot();
    }

    function makeData() {
        var rowData = new Array(Vis.N);
        for (var i = 0; i < Vis.N; i++) {
            var d = ProgWins.data[i];
            var p = 0;
            for (var j = 0; j < d.length; j++)
                p += -d[j];
            var opp = ProgWins.opps[i];
            rowData[i] = {
                opp: opp,
                name: opp >= 0 ? Vis.names[opp] : '{average}',
                data: d,
                points: '' + Math.round(10*p) / 10,
                order: i,
            };
        }

        var orderFunc = undefined;
        if (ProgWins.curOrder == 1) {
            orderFunc = function (a,b) {
                var pa = +rowData[a].points, pb = +rowData[b].points;
                if (pa > pb) return -1; else if (pa < pb) return 1;
                return rowData[a].name < rowData[b].name ? -1 : 1;
            };
        } else if (ProgWins.curOrder == 2) {
            orderFunc = function (a,b) {
                var oa = rowData[a].opp, ob = rowData[b].opp;
                if (oa < 0) return -1; else if (ob < 0) return 1;
                var sa = Vis.scores[oa], sb = Vis.scores[ob];
                if (sa > sb) return -1; else if (sa < sb) return 1;
                return rowData[a].name < rowData[b].name ? -1 : 1;
            };
        }
        if (orderFunc !== undefined) {
            var order = new Array(Vis.N);
            for (var i = 0; i < Vis.N; i++)
                order[i] = i;
            order.sort(orderFunc);
            for (var i = 0; i < Vis.N; i++)
                rowData[order[i]].order = i;
        }

        ProgWins.rowData = rowData;
    }

    function updatePlot() {
        var rowCoord = function (d) {
            return 'translate(0,' + d.order * (ch + cgap) + ')';
        };

        var rows = Plot.svg.selectAll('g.row').data(ProgWins.rowData, function (d) { return d.opp; });
        rows.transition().duration(1000)
            .attr('transform', rowCoord);
        rows.enter()
            .append('g').attr('class', 'row')
            .attr('transform', rowCoord);
        rows.attr('XXX', function (d) { return 'opp' + d.opp; });
        rows.exit().remove();

        var cells = rows.selectAll('rect').data(function (d) { return d.data; });
        cells.enter().append('rect')
            .attr('width', cw + cgap)
            .attr('height', ch + cgap)
            .attr('x', function (d,i) {
                var x = (i < Vis.T ? Vis.T + 1 + i : Vis.T - 1 - (i - Vis.T));
                return cgap/2 + x * (cw + cgap);
            })
            .attr('y', cgap/2)
            .style('stroke', 'black')
            .style('stroke-width', cgap)
            .style('fill', function (d) { return ProgWins.scale(-d); });
        cells.transition()
            .style('fill', function (d) { return ProgWins.scale(-d); });

        var points = rows.selectAll('text.p').data(function (d) { return [d.points]; });
        points.enter().append('text').attr('class', 'p')
            .attr('x', (2 * Vis.T + 1) * (cw + cgap) + 35)
            .attr('y', (ch + cgap) / 2)
            .attr('text-anchor', 'end')
            .style('dominant-baseline', 'middle')
            .style('font-size', 13);
        points
            .text(function (d) { return d; });

        var names = rows.selectAll('text.l').data(function (d) { return [d.opp]; });
        names.enter().append('text').attr('class', 'l')
            .attr('x', (2 * Vis.T + 1) * (cw + cgap) + 50)
            .attr('y', (ch + cgap) / 2)
            .attr('text-anchor', 'start')
            .style('dominant-baseline', 'middle')
            .style('font-size', 13);
        names
            .text(function (d) {
                return d >= 0 ? Vis.names[d] : '{average}';
            });
    }

    setup();
})();