#!/usr/bin/env python3
import subprocess
import time
import sys
import os
import random
import string
from collections import OrderedDict

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


def generateHostFile(hostList, n):
    hostFile = ''.join(random.choice(string.ascii_uppercase + string.digits) for _ in range(10))
    with open( hostFile, 'w' ) as f:
        for host in random.sample( hostList, int( n ) ):
            print( host, file = f )
    return hostFile

def prettyName(name, hosts):
    ignoreNext = False
    result = []
    name = name.replace('hosts', str( hosts ) )

    for i in name.split()[1:]:
        if i[0] == '-':
            continue
        if '/' in i:
            i = os.path.basename( i )
        result += [i]
    return '-'.join(result)

timeout = 12*60*60

def run(batch):
    path =  '../{}/'.format( batch )
    if not os.path.exists( path ):
        os.makedirs( path )

    with open( batch, 'r' ) as f:
        hostList = f.readline().split()
        for case in f:
            case = case.rstrip()
            if not case:
                continue

            hosts, case = case.split(maxsplit=1)

            hostFile = generateHostFile( hostList, hosts )
            print( case )
            t = Timer()
            out = b''
            try:
                t.start()
                out = subprocess.check_output( case.replace('hosts', hostFile).split(), timeout=timeout )
            except (subprocess.TimeoutExpired, subprocess.CalledProcessError):
                pass
            t.stop()
            os.remove(hostFile)
            name = path + prettyName( case, hosts )

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
