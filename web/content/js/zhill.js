function Zhill(data)
{
    this.init(data);
}

(function () {
    Zhill.scoringMethodOrder = [
        'markov', 'trad', 'tradtwist', 'iter', 'itertwist'
    ];
    Zhill.scoringMethods = {
        'markov': 'Markov',
        'trad': 'Trad.',
        'tradtwist': 'Tweaked',
        'iter': 'Iter.',
        'itertwist': 'Tweaked iter.'
    };
    Zhill.tapeCount = 21;

    Zhill.prototype.init = function(data)
    {
        this.progNames = data.progs;
        this.commit = data.commit;
        this.scoringMethod = data.scoringMethod;

        var n = data.progs.length;

        // build the result map and recompute points

        var points = new Array(n);
        for (var i = 0; i < n; i++)
            points[i] = 0;

        this.rawResults = {};
        for (var a = 0; a < n-1; a++)
        {
            var res = {};
            var rr = data.results[a];

            for (var b = a+1; b < n; b++)
            {
                var dp = rr[b - (a+1)];
                res[this.progNames[b]] = dp;

                var s = 0;
                for (var i = 0; i < dp.length; i++)
                    s += dp[i];
                points[a] += s / (2*Zhill.tapeCount);
                points[b] -= s / (2*Zhill.tapeCount);
            }

            this.rawResults[this.progNames[a]] = res;
        }

        // construct objectified list of programs

        this.progNames = data.progs;
        this.progMap = {};
        this.progs = [];
        for (var i = 0; i < n; i++)
        {
            var score = {};
            for (var s in Zhill.scoringMethods)
                score[s] = data.scores[s].score[i];

            var name = this.progNames[i];
            var shortName = name;
            if (shortName.length > 16)
                shortName = shortName.substring(0, 16) + '…';

            var p = {
                'name': name,
                'shortName': shortName,
                'points': points[i],
                'score': score,
                'rank': i,
                'prevRank': data.prevrank[i]
            };
            this.progs.push(p);
            this.progMap[this.progNames[i]] = p;
        }

        // helpful occasionally

        this.progNameOrder = this.progNames.slice(0);
        this.progNameOrder.sort();
    };

    Zhill.prototype.delta = function(prog)
    {
        if (typeof prog === 'string') prog = this.progMap[prog];

        if (prog.prevRank === null)
            return 'new';
        var delta = prog.prevRank - prog.rank;
        if (delta == 0)
            return '--';
        return (delta > 0 ? '+' : '') + delta;
    };

    Zhill.prototype.results = function(progA, progB)
    {
        if (typeof progA === 'string') progA = this.progMap[progA];
        if (typeof progB === 'string') progB = this.progMap[progB];

        if (progA.rank == progB.rank)
        {
            var res = new Array(2 * Zhill.tapeCount);
            for (var i = 0; i < res.length; i++)
                res[i] = 0;
            return res;
        }

        if (progA.rank < progB.rank)
            return this.rawResults[progA.name][progB.name];

        var raw = this.rawResults[progB.name][progA.name];
        var res = new Array(2 * Zhill.tapeCount);
        for (var i = 0; i < res.length; i++)
            res[i] = -raw[i];
        return res;
    };
})();

var zhill = new Zhill(zhillData);
zhillData = undefined;
