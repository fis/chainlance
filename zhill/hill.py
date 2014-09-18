import os
import subprocess as sp
from configparser import ConfigParser
from glob import glob
from os.path import join

from zhill.gear import Gear
from zhill.score import scorings

# Hill combat logic:
#
# Hill consists of N programs.  When a new challenger is submitted,
# the currently worst-ranking program is replaced by the challenger.
# If the program name exactly matches an existing program, that one is
# replaced instead.  Then the duel points and rankings are recomputed.

# self._dir: hill repository directory
# self._progs: dict name -> progobj
# self._hill: list name -- in order of progobj['id']
# self._results: dict name -> dict name -> list int -- only for (A, B) pairs where A['id'] < B['id']
# self._ranking: list name -- in reverse order of score
# self._gear: gearlanced instance running the hill
# self._max_score: maximum theoretical score under current scoring method
# self._tapecount: total number of tape configurations on this hill
# self._scoring: current scoring mechanism
#
# progobj:
#   'id': integer index in _hill and in gear
#   'name': program canonical name
#   'points': sum of duel points for scoring
#   'score': current score under the scoring mechanic
#   'rank': current index in _ranking
#   'prevrank': previous index (if any) before the latest challenge
#   'file': stored source file, or None if a pseudo-program

class Hill:
    """General logic for maintaining a (version-controlled) BF Joust hill."""

    def __init__(self, hilldir):
        self._dir = hilldir
        os.chdir(hilldir)

        # verify that hill directory is a Git repo

        with open('/dev/null', 'wb') as devnull:
            sp.check_call(('git', 'rev-parse', 'HEAD'), stdout=devnull, stderr=devnull)

        # load configuration

        cfg = ConfigParser()
        cfg.read('hill.cfg')
        if 'hill' not in cfg:
            raise Exception('no hill.cfg or [hill] section found')
        hillcfg = cfg['hill']

        nprogs = int(hillcfg['size'])
        self._scoring = scorings[hillcfg['scoring']]
        self._gear = Gear(nprogs, hillcfg['gearlanced'])
        self._tapecount = int(hillcfg['tapecount'])
        self._tiebreak = hillcfg['tiebreak']

        self._cfg = cfg # for supplementary configuration

        # construct the initial hill with initial results

        self._progs = {}
        self._hill = []
        self._results = {}

        progfiles = sorted(glob('*.bfjoust'))

        for idp in range(nprogs):
            # prepare program

            p = { 'id': idp }
            if idp < len(progfiles):
                p['name'] = progfiles[idp][:-8]
                p['file'] = progfiles[idp]
                with open(p['file'], 'rb') as f:
                    code = f.read()
            else:
                p['name'] = 'dummy-{0}'.format(idp)
                code = b'<'

            prog = p['name']

            # insert into data structures

            self._progs[prog] = p
            self._hill.append(prog)
            self._results[prog] = {}

            # test against previous programs to update results

            if idp > 0:
                err, result = self._gear.test(code)
                if err is not None: raise Exception(err)
                for idA in range(idp):
                    progA = self._hill[idA]
                    self._results[progA][prog] = [-p for p in result[idA]]

            # put into gearlanced

            err = self._gear.set(idp, code)
            if err is not None: raise Exception(err)

        # compute initial scores

        self._max_score = self._scoring['max'](self)
        self._rescore()

        for p in self._progs.values():
            p['prevrank'] = p['rank']

        pass

    def dir(self):
        return self._dir

    def cfg(self):
        return self._cfg

    def tapecount(self):
        return self._tapecount

    # implement a set-like abstraction for the program names

    def __len__(self):
        return len(self._hill)

    def __iter__(self):
        return iter(self._hill)

    def __getitem__(self, key):
        return self._hill[key]

    def __contains__(self, prog):
        return prog in self._progs

    def progs(self):
        return self._hill

    def pairs(self):
        n = len(self._hill)
        for idA in range(n-1):
            for idB in range(idA+1, n):
                yield self._hill[idA], self._hill[idB]

    def idpairs(self):
        n = len(self._hill)
        for idA in range(n-1):
            for idB in range(idA+1, n):
                yield idA, self._hill[idA], idB, self._hill[idB]

    def id(self, prog):
        return self._progs[prog]['id']

    # accessors for raw results and duel points, as well as current ranking

    def results(self, progA, progB):
        progA = self._progs[progA]
        progB = self._progs[progB]

        if progA['id'] == progB['id']:
            return [0] * (2*self._tapecount)

        scale = 1
        if progA['id'] > progB['id']:
            scale, progA, progB = -1, progB, progA

        return [scale*p for p in self._results[progA['name']][progB['name']]]

    def points(self, prog):
        return self._progs[prog]['points']

    def ranking(self):
        for prog in self._ranking:
            p = self._progs[prog]
            yield prog, p['points'], p['score']

    def max_score(self):
        return self._max_score

    # hill program file management

    def _save(self, replaced, newprog, code):
        newfile = newprog + '.bfjoust'

        if replaced['file'] is not None and replaced['file'] != newfile:
            git('rm', '-q', '--', replaced['file'])
            replaced['file'] = None

        with open(newfile, 'wb') as sf:
            sf.write(code)
        git('add', '--', newfile)

        if replaced['name'] == newprog:
            commitmsg = 'Updating {0}'.format(newprog)
        else:
            commitmsg = 'Replacing {0} by {1}'.format(replaced['name'], newprog)

        git('commit', '-q', '-m', commitmsg)

        return newfile

    # a new challenger appears (or an old one is changed)

    def challenge(self, newprog, code):
        # select a program to replace

        replaced = self._progs.get(newprog, self._progs[self._ranking[-1]])
        newid = replaced['id']

        # test for parse errors, compute scores, replace old program

        err, result = self._gear.test(code)
        if err is not None:
            return '{0} failed: {1}'.format(newprog, err), None

        err = self._gear.set(newid, code)
        if err is not None:
            return '{0} impossibly failed: {1}'.format(newprog, err), None

        # replace old program with new in hill data structures

        newp = { 'id': newid,
                 'name': newprog }
        if replaced['name'] == newprog:
            newp['rank'] = replaced['rank'] # for change in reports

        del self._progs[replaced['name']]
        self._progs[newprog] = newp

        self._hill[newid] = newprog

        # remove old results

        del self._results[replaced['name']]
        for progA in self._hill[:newid]:
            del self._results[progA][replaced['name']]

        # insert in new results

        self._results[newprog] = {}

        for oppid, oppres in enumerate(result):
            opp = self._hill[oppid]
            if oppid < newid:
                self._results[opp][newprog] = [-p for p in oppres]
            elif newid < oppid:
                self._results[newprog][opp] = oppres

        # recompute points and ranking

        self._rescore()

        # update source repository

        newp['file'] = self._save(replaced, newprog, code)

        # return summary information about the new program

        rankchg = ''
        if 'prevrank' in newp:
            delta = newp['prevrank'] - newp['rank']
            if delta == 0:
                rankchg = '--'
            else:
                rankchg = '{0:+d}'.format(delta)
            rankchg = ' (change: ' + rankchg + ')'

        return None, '{0}: points {1:.2f}, score {2:.2f}/{3}, rank {4:d}/{5:d}{6}'.format(
            newprog,
            newp['points'],
            newp['score'], self._max_score,
            1+newp['rank'], len(self._progs), rankchg)

    # point and score computations

    def _rescore(self):
        n = len(self._progs)

        for p in self._progs.values():
            if 'rank' in p: p['prevrank'] = p['rank']

        # recompute points for all programs

        points = [0] * n

        for idA, progA, idB, progB in self.idpairs():
            s = sum(self._results[progA][progB])
            points[idA] += s
            points[idB] -= s

        for i, p in enumerate(points):
            self._progs[self._hill[i]]['points'] = p / (2*self._tapecount)

        # recompute scores

        scores = self._scoring['score'](self)
        for prog, p in self._progs.items():
            p['score'] = scores[prog]

        # recompute ranking

        self._ranking = sorted(self._progs.keys(),
                               key=lambda prog: self._progs[prog]['score'],
                               reverse=True)
        for r, prog in enumerate(self._ranking):
            self._progs[prog]['rank'] = r

    # reports

    def write_report(self, out):
        n = len(self._progs)

        def sym(r):
            if r < 0: return '-'
            if r > 0: return '+'
            return '0'
        def chg(p):
            if 'prevrank' not in p:
                return 'new'
            delta = p['prevrank'] - p['rank']
            if delta == 0:
                return ' --'
            return '{0:+3d}'.format(delta)

        out.write('Rank/chg   Score  Points  Program\n')
        for rank, (prog, points, score) in enumerate(self.ranking()):
            out.write('{0:4d} {1} {2:7.2f} {3:7.2f}  {4}\n'.format(rank+1, chg(self._progs[prog]), score, points, prog))
        out.write('\n')

        out.write('   | ' +
                  ' '.join(str(i//10) if i//10 > 0 else ' ' for i in range(1,n+1)) +
                  ' |\n')
        out.write('   | ' +
                  ' '.join(str(i%10) for i in range(1,n+1)) +
                  ' | score | pts   | program\n')
        for rank, (prog, points, score) in enumerate(self.ranking()):
            out.write('{0:2d} | '.format(rank+1) +
                      ' '.join(sym(sum(self.results(prog, opp))) if opp != prog else ' '
                               for opp, _, _ in self.ranking()) +
                      ' |{0:6.1f} |{1:6.1f} | {2}\n'.format(score, points, prog))

    def write_breakdown(self, prog, out):
        sym = { -1: '>', 0: 'X', +1: '<' }
        t = self._tapecount

        for opp in sorted(self._hill):
            if opp == prog:
                continue

            res = self.results(prog, opp)
            tot = sum(res)

            out.write('{0} vs {1}\n'.format(prog, opp))
            out.write(''.join(sym[r] for r in res[:t]) +
                      ' ' +
                      ''.join(sym[r] for r in res[t:]) +
                      ' {0:d}\n'.format(tot))
            if tot == 0:
                out.write('Tie.\n')
            else:
                out.write('{0} wins.\n'.format(prog if tot > 0 else opp))
            out.write('\n')

# git helper

def git(*args):
    sp.check_call(('git',) + args)
