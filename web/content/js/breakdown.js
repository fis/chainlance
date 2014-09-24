var progList;
var breakdown;

function build()
{
    var progs = zhill.progNameOrder;

    progList = $('<select class="form-control" />');
    for (var i = 0; i < progs.length; i++)
    {
        var prog = progs[i];
        progList.append(new Option(prog, prog, i == 0));
    }

    breakdown = $('<div/>');
    leftSpan = $('<span class="leftprog"/>');

    $('#breakdown').empty().append(
        $('<p/>').append(progList),
        breakdown);

    var hash = window.location.hash;
    if (hash.length > 1)
    {
        hash = hash.substring(1);
        if (zhill.progMap[hash])
            progList.val(hash);
    }

    update();
    progList.change(update);
}

function update()
{
    breakdown.empty();

    var progs = zhill.progNameOrder;
    var left = progList.val();
    if (!zhill.progMap[left])
        return; // impossible

    window.location.hash = '#'+left;

    for (var i = 0; i < progs.length; i++)
    {
        var right = progs[i];
        if (left == right)
            continue; // not interesting

        var viewLink = $('<a/>')
            .addClass('view')
            .attr('href', '../game/#joust,'+left+','+right+','+zhill.commit.substring(0,7))
            .text('view');

        var resultSpan = $('<span class="result"/>');
        var results = zhill.results(left, right);
        var currentClass = undefined, currentBlock = undefined;
        var rSum = 0;
        for (var ri = 0; ri < results.length; ri++)
        {
            if (ri == Zhill.tapeCount)
                currentBlock.append('&nbsp;'); // must exist at this point, class doesn't matter

            var r = results[ri];
            rSum += r;

            var rSym = r < 0 ? '>' : r > 0 ? '<' : 'X';
            var rClass = r < 0 ? 'loss' : r > 0 ? 'win' : 'tie';

            if (currentClass === rClass)
                currentBlock.text(currentBlock.text() + rSym);
            else
            {
                currentClass = rClass;
                currentBlock = $('<span/>').addClass(rClass).text(rSym);
                resultSpan.append(currentBlock);
            }
        }
        var sumSpan = $('<span/>');
        var sumText = ''+rSum; while (sumText.length < 3) sumText = ' '+sumText;
        sumSpan.css('color', 'rgb('+Math.max(0,rSum*5)+',0,'+Math.max(0,-rSum*6)+')');
        sumSpan.text(' '+sumText);
        resultSpan.append(sumSpan);

        var winner = $('<span/>');
        winner.addClass(rSum < 0 ? 'loss' : rSum > 0 ? 'win' : 'tie').addClass('winner');
        winner.text(rSum < 0 ? right+' wins.' : rSum > 0 ? left+' wins.' : 'Tie.');

        var item = $('<p/>');
        item.append('...vs ', $('<strong/>').text(right), ': ', viewLink, '<br>',
                    resultSpan, '<br>',
                    winner);

        breakdown.append(item);
    }
}

$(build);
