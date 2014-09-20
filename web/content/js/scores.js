var tableBody;
var tableRows = [];

var sorts = {
    'rank': { 'key': function (p) { return p.rank; }, 'dom': [] },
    'name': { 'key': function (p) { return p.name; }, 'dom': [] },
    'points': { 'key': function (p) { return p.points; }, 'dom': [] },
};
function scoresort(sm) { return function (p) { return p.score[sm]; } }
for (var sm in Zhill.scoringMethods)
    sorts[sm] = { 'key': scoresort(sm), 'dom': [] };

function sort(kind, descending)
{
    var keyf = sorts[kind].key;
    var cmpf = function (a, b) {
        var x = keyf((descending ? a : b).prog);
        var y = keyf((descending ? b : a).prog);
        if (x < y) return -1;
        if (x > y) return 1;
        return 0;
    };
    tableRows.sort(cmpf);

    tableBody.empty();
    for (var i = 0; i < tableRows.length; i++)
        tableBody.append(tableRows[i].domRow);

    sortUpdateIcons(kind, descending);
}

function sortUpdateIcons(kind, descending)
{
    for (var k in sorts)
    {
        var s = sorts[k];
        if (s.dom.length != 2)
            continue;
        for (var i = 0; i < 2; i++)
        {
            if (k == kind && i == (descending ? 1 : 0))
                s.dom[i].addClass('sortactive');
            else
                s.dom[i].removeClass('sortactive');
        }
    }
}

function sortClick(kind, descending)
{
    return function () { sort(kind, descending); }
}

function build()
{
    var progs = zhill.progs;
    var sm = zhill.scoringMethod;

    var table = $('<table class="table table-condensed table-hover" />');

    var row = $('<tr/>');
    var heads = [{ 'sort': 'rank', 'label': 'Rank', 'colspan': '2' },
                 { 'sort': 'name', 'label': 'Prog' },
                 { 'sort': 'points', 'label': 'Points' },
                 { 'sort': sm, 'label': Zhill.scoringMethods[sm] }];
    for (var i = 0; i < heads.length; i++)
    {
        var h = heads[i];
        var th = $('<th/>');
        if (h.colspan) th.attr('colspan', h.colspan);
        var sortAsc = $('<span class="sorticon glyphicon glyphicon-chevron-down"></span>');
        var sortDesc = $('<span class="sorticon glyphicon glyphicon-chevron-up"></span>');
        sortAsc.click(sortClick(h.sort, false));
        sortDesc.click(sortClick(h.sort, true));
        sorts[h.sort].dom = [sortAsc, sortDesc];
        th.append(sortAsc, h.label, sortDesc);
        row.append(th);
    }
    table.append($('<thead/>').append(row));
    sortUpdateIcons('rank', true);

    tableBody = $('<tbody/>');
    for (var i = 0; i < progs.length; i++)
    {
        var prog = progs[i];

        var link = $('<a/>');
        link.attr('href', 'http://zem.fi/git/?p=hill;a=blob;f=' + prog.name + '.bfjoust;hb=' + zhill.commit);
        link.text(prog.name);

        var rankCell = $('<td/>').text(prog.rank + 1);
        var changeCell = $('<td/>').text(zhill.delta(prog));
        var progCell = $('<td/>').append(link);
        var pointsCell = $('<td/>').text(prog.points.toFixed(2));
        var scoreCells = [$('<td/>').text(prog.score[sm].toFixed(2))];
        var row = $('<tr/>').append(rankCell, changeCell, progCell, pointsCell, scoreCells);

        tableBody.append(row);
        tableRows.push({
            'prog': prog,
            'domRank': rankCell,
            'domProg': progCell,
            'domPoints': pointsCell,
            'domScores': scoreCells,
            'domRow': row
        });
    }
    table.append(tableBody);

    $('#scoretable').empty().append($('<div class="table-responsive"/>').append(table));
}

$(build);
