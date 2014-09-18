#! /usr/bin/env python3

import sys

from zhill.hill import Hill
from zhill.bot import Bot

if len(sys.argv) != 2:
    print('usage: {0} hill'.format(sys.argv[0]))
    sys.exit(1)

hill = Hill(sys.argv[1])
bot = Bot(hill)
bot.start()
