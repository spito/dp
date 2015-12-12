#!/usr/bin/env python3
import subprocess
import time
import sys
import os
from collections import OrderedDict

class Hosts:
    def __init__(self, name='hosts'):
        self._hosts = []
        self._name = name

    def add(self, host):
        self._hosts.append( host )

    def save(self):
        with open(self._name, 'w') as f:
            for h in self._hosts:
                f.write( h + '\n' )

class Timer:
    def __init__(self):
        self._start = 0
        self._stop = 0

    def start(self):
        self._stop = 0
        self._start = time.time()

    def stop(self):
        self._stop = time.time()

    def elapsed(self):
        return self._stop - self._start


def generateHostFile(name, hList):
    hosts = Hosts(name)
    for h in hList:
        hosts.add(h)

    hosts.save()

def prettyName(name):
    ignoreNext = False
    result = []
    for i in name.split()[1:]:
        if i[0] == '-':
            continue
        if '/' in i:
            i = os.path.basename( i )
        result += [i]
    return '-'.join(result)

timeout = 20*60

def run(batch):
    path =  '../{}/'.format( batch )
    if not os.path.exists( path ):
        os.makedirs( path )

    with open( batch, 'r' ) as f:
        for case in f:
            case = case.rstrip()
            if not case:
                continue

            print( case )
            t = Timer()
            out = b''
            try:
                t.start()
                out = subprocess.check_output( case.split(), timeout=timeout )
            except (subprocess.TimeoutExpired, subprocess.CalledProcessError):
                pass
            t.stop()

            name = path + prettyName( case )

            with open( name, 'a' ) as f:
                print( '{:.2f}'.format( t.elapsed() ), file=f)
            with open( 'log.txt', 'a' ) as f:
                print( case, file=f )
                if out:
                    print( '\t{}'.format( out.decode('ascii') ), file=f )


if __name__ != '__main__':
    print('must be executed as an independent program')
    sys.exit( 1 )

args = sys.argv

if len( args ) != 2:
    print( 'usage: {} batchFile'.format( args[0] ) )
    sys.exit( 2 )

run( args[ 1 ] )
