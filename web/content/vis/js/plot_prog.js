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

    ProgPlot.form = d3.select('#plot-controls').append('form')
        .attr('role', 'form').attr('name', 'ctrl');

    var progGroup = ProgPlot.form.append('div').attr('class', 'form-group');
    progGroup.append('label').attr('for', 'prog').text('Program:');

    ProgPlot.progSelect = progGroup.append('select')
        .attr('id', 'prog')
        .attr('class', 'form-control');
    ProgPlot.progSelect.selectAll('option').data(Vis.names)
        .enter().append('option')
        .attr('value', function (d,i) { return i; })
        .text(function (d) { return d; });

    ProgPlot.parseHash(false);

    ProgPlot.progSelect.node().value = ProgPlot.prog;
    ProgPlot.progSelect.on('change', ProgPlot.onProgChange);
    ProgPlot.onProgChange();

    if (opts.handlePopState)
        d3.select(window).on('popstate', function () { ProgPlot.parseHash(true); });
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

    var hash = window.location.hash;
    if (!hash.startsWith('#' + Vis.names[prog]) && (prog > 0 || hash.length > 1))
        window.location.hash = Vis.names[prog];
};

ProgPlot.parseHash = function (trigger) {
    var hashProg = Plot.arg(0);
    if (!hashProg)
        hashProg = Vis.names[0];
    if (hashProg in Vis.nameMap) {
        var prog = Vis.nameMap[hashProg];
        if (prog != ProgPlot.prog) {
            ProgPlot.prog = prog;
            ProgPlot.progSelect.node().value = prog;
            if (trigger)
                ProgPlot.onProgChange();
        }
    }
};