var tableBody, tableHead;
var tableRows = [];
var columns = [];
var columnAddButtons = {};
var extraBlock;

var sorts = {
    'rank': { 'key': function (p) { return p.rank; }, 'dom': [] },
    'name': { 'key': function (p) { return p.name; }, 'dom': [] },
    'points': { 'key': function (p) { return p.points; }, 'dom': [] },
};
function scoresort(sm) { return function (p) { return p.score[sm]; } }
for (var sm in Zhill.scoringMethods)
    sorts[sm] = { 'key': scoresort(sm), 'dom': [] };

var currentSort = { 'kind': 'rank', 'descending': true };

function sort(kind, descending)
{
    currentSort.kind = kind;
    currentSort.descending = descending;

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
    return function () { sort(kind, descending); };
}

function addColumnClick(sm)
{
    return function () {
        var c = {
            'sort': sm,
            'head': makeHead(sm, Zhill.scoringMethods[sm], true)
        };
        columns.push(c);

        tableHead.append(c.head);

        for (var i = 0; i < tableRows.length; i++)
        {
            var r = tableRows[i];
            var cell = $('<td/>').text(r.prog.score[sm].toFixed(2));
            r.domScores.push(cell);
            r.domRow.append(cell);
        }

        columnAddButtons[sm].detach();
    };
}

function delColumnClick(sm)
{
    return function () {
        if (currentSort.kind == sm)
            sort('rank', true);

        var idx;
        for (idx = 0; idx < columns.length; idx++)
            if (columns[idx].sort === sm)
                break;
        if (idx == columns.length)
            return; /* impossible */
        var c = columns[idx];
        columns.splice(idx, 1);

        tableHead.children().slice(3+idx, 3+idx+1).remove();

        for (var i = 0; i < tableRows.length; i++)
        {
            var r = tableRows[i];
            var cell = r.domScores[idx];
            r.domScores.splice(idx, 1);
            cell.remove();
        }

        extraBlock.append(columnAddButtons[sm]);
    };
}

function makeHead(sort, label, remove)
{
    var th = $('<th/>');

    var sortAsc = $('<span class="sorticon glyphicon glyphicon-chevron-down"></span>');
    var sortDesc = $('<span class="sorticon glyphicon glyphicon-chevron-up"></span>');
    sortAsc.click(sortClick(sort, false));
    sortDesc.click(sortClick(sort, true));
    sorts[sort].dom = [sortAsc, sortDesc];
    th.append(sortAsc, label, sortDesc);

    if (remove)
    {
        var del = $('<span class="sorticon glyphicon glyphicon-remove"></span>');
        del.click(delColumnClick(sort));
        th.append(del);
    }

    return th;
}

function build()
{
    var progs = zhill.progs;
    var sm = zhill.scoringMethod;

    var table = $('<table class="table table-condensed table-hover" />');

    tableHead = $('<tr/>');
    var heads = [{ 'sort': 'rank', 'label': 'Rank', 'colspan': '2' },
                 { 'sort': 'name', 'label': 'Prog' },
                 { 'sort': 'points', 'label': 'Points' }]
    for (var i = 0; i < heads.length; i++)
    {
        var h = heads[i];
        var th = makeHead(h.sort, h.label, false);
        if (h.colspan) th.attr('colspan', h.colspan);
        tableHead.append(th);
    }
    table.append($('<thead/>').append(tableHead));
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
        var row = $('<tr/>').append(rankCell, changeCell, progCell, pointsCell);

        tableBody.append(row);
        tableRows.push({
            'prog': prog,
            'domRank': rankCell,
            'domProg': progCell,
            'domPoints': pointsCell,
            'domScores': [],
            'domRow': row
        });
    }
    table.append(tableBody);

    extraBlock = $('<blockquote>More columns:<br></blockquote>');
    for (var i = 0; i < Zhill.scoringMethodOrder.length; i++)
    {
        var sid = Zhill.scoringMethodOrder[i];
        var label = Zhill.scoringMethods[sid];
        var btn = $('<button type="button" class="btn btn-default btn-sm"><span class="glyphicon glyphicon-plus-sign"></span> '+label+'</button>');
        btn.click(addColumnClick(sid));
        extraBlock.append(btn);
        columnAddButtons[sid] = btn;
    }

    addColumnClick(sm)();

    $('#scoretable')
        .empty()
        .append($('<div class="extracolumns"/>').append(extraBlock),
                $('<div class="table-responsive"/>').append(table));
}

$(build);
