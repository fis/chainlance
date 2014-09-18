import re
import traceback
from datetime import datetime

import irc.bot

class Bot(irc.bot.SingleServerIRCBot):
    def __init__(self, hill):
        self._hill = hill
        self._cfg = hill.cfg()['irc']
        self._command = self._cfg['command']
        self._log = open('hill.log', 'a')

        cfg = self._cfg
        irc.bot.SingleServerIRCBot.__init__(self, [(cfg['server'], cfg.get('port', 6667))], cfg['nick'], cfg['nick'])

    def on_nicknameinuse(self, c, e):
        c.nick(c.get_nickname() + '_')

    def on_welcome(self, c, e):
        for ch in self._hill.cfg()['irc']['channels'].split(' '):
            c.join(ch)

    def on_pubmsg(self, c, e):
        msg = e.arguments[0].split(' ', 3)

        if len(msg) == 0 or msg[0] != self._command:
            return

        if len(msg) <= 2:
            c.privmsg(e.target, e.source.nick + ': "!bfjoust progname code". See http://zem.fi/bfjoust/ for documentation.')
            return

        orig_prog = msg[1]
        code = msg[2]

        prog = re.sub(r'[^a-zA-Z0-9_-]', '', orig_prog)
        if len(prog) < 0.75*len(orig_prog):
            c.privmsg(e.target, e.source.nick + ': Program name looks like gibberish. Did you forget it?')
            return

        prog = e.source.nick + '~' + prog

        tstamp = datetime.utcnow().strftime('%Y-%m-%d %H:%M:%S')
        self._log.write('{0} {1} {2} {3}\n'.format(tstamp, e.source.nick, prog, code))

        if re.match(r'(?:https?|ftp)://', code):
            # TODO: fetch code over HTTP if required
            c.privmsg(e.target, e.source.nick + ': URL fetch not done yet SORRY')
            return
        else:
            code = code.encode('utf-8')

        try:
            # run challenge

            err, result = self._hill.challenge(prog, code)
            if err is not None:
                c.privmsg(e.target, err)
                return
            c.privmsg(e.target, result)

            # update web components

            report = self._cfg.get('report')
            if report is not None:
                with open(report, 'w') as f:
                    self._hill.write_report(f)

            breakdown = self._cfg.get('breakdown')
            if breakdown is not None:
                with open(breakdown, 'w') as f:
                    self._hill.write_breakdown(prog, f)

        except Exception:
            traceback.print_exc(file=self._log)
            c.privmsg(e.target, 'I broke down! Ask fizzie to help! The details are in the log!')
            self.die()
