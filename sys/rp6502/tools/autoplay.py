#!/usr/bin/env python3
"""Play NetHack automatically on the host build, for call-graph profiles and
smoke tests.

    autoplay.py [--turns N] [--seed S] [--wizard] NETHACK PLAYDIR

Answers prompts, wanders, fights, searches, uses items and takes the stairs
down, then saves (so atexit handlers such as cgprof.c run).
"""
import argparse
import os
import random
import re
import sys

import pexpect

ANSI = re.compile(rb'\x1b\[[0-9;?]*[A-Za-z]|\x1b[()][0-9A-Za-z]|[\x0e\x0f]')

MOVES = 'hjklyubn'
ACTIONS = (
    [lambda r: r.choice(MOVES)] * 30 +
    [lambda r: r.choice(MOVES.upper())] * 8 +
    [lambda r: 's', lambda r: '10s', lambda r: ',', lambda r: '>'] * 2 +
    [lambda r: '_>.', lambda r: '_<.'] * 3 +
    [lambda r: 'i', lambda r: 'e', lambda r: 'q', lambda r: 'r', lambda r: 'z',
     lambda r: 'a', lambda r: 'w', lambda r: 'W', lambda r: 'P', lambda r: 'd',
     lambda r: 't', lambda r: 'o', lambda r: 'c', lambda r: '#pray\r',
     lambda r: ':', lambda r: '\x18', lambda r: '^', lambda r: 'E-',
     lambda r: 'F' + r.choice(MOVES), lambda r: '\x10', lambda r: '#enhance\r',
     lambda r: 'D', lambda r: 'A', lambda r: '#loot\r', lambda r: '#chat\r']
)
WIZARD = [lambda r: '\x16%d\r' % r.randint(1, 40), lambda r: '\x06',
          lambda r: '\x17', lambda r: '\x05']

PROMPTS = [
    (rb'--More--', '\r'),
    (rb'Shall I pick', 'y'),
    (rb'Is this ok\? \[ynq\]', 'y'),
    (rb'Do you want your possessions identified', 'q'),
    (rb'Do you want to see', 'n'),
    (rb'Die\? \[yn\]', 'n'),
    (rb'Really (?:attack|quit|save)', 'n'),
    (rb'There is .* here; eat it\?', 'n'),
    (rb'In what direction', None),
    (rb'Where do you want to', '.'),
    (rb'(?:Call|Name|What do you want to (?:name|call|write|wish))', '\x1b'),
    (rb'\[yn\]|\[ynq\]|\[ynaq\]', None),
    (rb'What do you want to|\[[- a-zA-Z$*?]+ or \?\*\]', '\x1b'),
    (rb'\(end\)|\([0-9]+ of [0-9]+\)', '\x1b'),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--turns', type=int, default=2000)
    ap.add_argument('--seed', type=int, default=1)
    ap.add_argument('--wizard', action='store_true')
    ap.add_argument('--name', default='auto')
    ap.add_argument('--log')
    ap.add_argument('nethack')
    ap.add_argument('playdir')
    a = ap.parse_args()

    rnd = random.Random(a.seed)
    env = dict(os.environ, NETHACKDIR=a.playdir, TERM='xterm')
    args = ['-u', '%s%d' % (a.name, a.seed), '-@']
    if a.wizard:
        args.insert(0, '-D')
    child = pexpect.spawn(a.nethack, args, env=env, dimensions=(24, 80),
                          timeout=10)
    if a.log:
        child.logfile_read = open(a.log, 'wb')
    actions = ACTIONS + (WIZARD * 3 if a.wizard else [])
    tail = b''
    sent = 0
    while True:
        try:
            data = child.read_nonblocking(65536, timeout=0.3)
            tail = (tail + ANSI.sub(b'', data))[-600:]
            continue
        except pexpect.TIMEOUT:
            pass
        except pexpect.EOF:
            break
        reply = None
        for pat, ans in PROMPTS:
            m = None
            for m in re.finditer(pat, tail):
                pass
            if m and not tail[m.end():].strip(b' \r\n'):
                if ans is None:
                    ans = rnd.choice(MOVES + ('y' if b'[y' in pat else ''))
                    if b'[yn' in m.group(0):
                        ans = rnd.choice('yn')
                reply = ans
                break
        if reply is None:
            if sent >= a.turns:
                reply = 'Sy'
                sent = -10**9
            elif sent < 0:
                reply = '\x1b'
            else:
                reply = rnd.choice(actions)(rnd)
                sent += 1
        tail = b''
        child.send(reply)
    status = child.wait()
    if status is None:
        print("killed by signal", child.signalstatus)
    print('actions sent: %d, exit status %s' % (max(sent, 0) or a.turns,
                                                status))
    return status


if __name__ == '__main__':
    sys.exit(main())
