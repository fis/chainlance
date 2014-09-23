var cells = [];
var progRows = [], progCols = [];
var topHead = [];
var leftHead = [];
var nameHead = [];
var topNameCell, topNameSpan;

function hover(progA, progB, enter)
{
    return function() {
        if (enter)
        {
            leftHead[progA.rank].addClass('hov');
            topHead[progB.rank].addClass('hov');
            nameHead[progA.rank].addClass('hov');
            topNameSpan.text(progB.shortName);
            topNameCell.show();
        }
        else
        {
            leftHead[progA.rank].removeClass('hov');
            topHead[progB.rank].removeClass('hov');
            nameHead[progA.rank].removeClass('hov');
            topNameCell.hide();
        }
    };
}

function build()
{
    var progs = zhill.progs;

    var table = $('<table class="matrix" />');

    var row = $('<tr/>');
    for (var a = -1; a < progs.length; a++)
    {
        var cell = $('<th/>');
        if (a >= 0)
        {
            topHead.push(cell);
            cell.text(a+1);
        }
        row.append(cell);
    }
    topNameSpan = $('<span/>');
    topNameCell = $('<td class="progname hov">← </td>').append(topNameSpan);
    topNameCell.hide();
    row.append(topNameCell);
    table.append(row);

    for (var a = 0; a < progs.length; a++)
    {
        var progA = progs[a];
        var row = $('<tr/>');
        cells.push([]);

        var left = $('<th>'+(a+1)+'</th>');
        leftHead.push(left);
        row.append(left);

        for (var b = 0; b < progs.length; b++)
        {
            var progB = progs[b];
            var cell = $('<td/>');
            cells[a].push({
                'progA': progA,
                'progB': progB,
                'dom': cell
            });

            var results = zhill.results(progA, progB);

            var s = 0;
            for (var i = 0; i < results.length; i++)
                s += results[i];

            if (a == b)
                cell.addClass('me');
            else {
                var link = $('<a/>');
                link.attr('href',
                          '../game/#joust,' + progA['name'] +
                          ',' + progB['name'] +
                          ',' + zhill.commit.substring(0, 7));
                cell.append(link);

                if (s < 0)
                {
                    cell.css('background-color', 'rgb(213,213,'+(213-s)+')');
                    link.css('color', 'rgb(0,0,'+(3*-s)+')');
                    link.append($('<span class="glyphicon glyphicon-minus"></span>'));
                }
                else if (s > 0)
                {
                    cell.css('background-color', 'rgb('+(213+s)+',213,213)');
                    link.css('color', 'rgb('+(3*s)+',0,0)');
                    link.append($('<span class="glyphicon glyphicon-plus"></span>'));
                }
                else
                {
                    cell.addClass('mt');
                    link.append($('<span class="glyphicon glyphicon-remove"></span>'));
                }
            }

            cell.hover(hover(progA, progB, true), hover(progA, progB, false));
            row.append(cell);
        }

        var name = $('<td class="progname"/>').text(progA.shortName);
        nameHead.push(name);
        row.append(name);

        table.append(row);
    }

    $('#matrix').empty().append(table);
}

$(build);
