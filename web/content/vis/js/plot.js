'use strict';

if (Math.log10 === undefined) {
    Math.log10 = function (x) { return Math.log(x) / Math.log(10); };
}

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
