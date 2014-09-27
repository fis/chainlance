function build()
{
    var joustResult = $('#joustResult');

    // result presentation

    var resultMode;
    var modeClass = { 'ok': 'bg-success', 'info': 'bg-info', 'error': 'bg-danger' };
    function switchMode(newMode)
    {
        if (resultMode === newMode)
            return;
        joustResult.removeClass(modeClass[resultMode]);
        joustResult.addClass(modeClass[newMode]);
        resultMode = newMode;
    }

    function result(mode, message)
    {
        switchMode(mode);
        joustResult.text(message);
    }

    // form submission

    function go(submit)
    {
        result('info', 'Submitting program...');

        $.ajax({
            url: 'cgi/submit.cgi',
            data: {
                prog: $('#prog').val(),
                code: $('#code').val(),
                submit: submit ? 'yes' : 'no'
            },
            type: 'POST',
            dataType: 'json',
            timeout: 10000,
            success: done,
            error: function (xhr, status, error) {
                done({ result: 'error', message: error });
            }
        });
    }

    function done(data)
    {
        if (data['message'] === undefined)
            result('error', 'Something went badly wrong, sorry.');
        else if (data['result'] != 'ok')
            result('error', data['message']);
        else
            result('ok', data['message']);
    }

    $('#submitTest').click(function () { go(false); });
    $('#submitReal').click(function () { go(true); });
}

$(build);
