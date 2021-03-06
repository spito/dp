#!/usr/bin/env python3
import sys
from collections import OrderedDict
from structures import *

def hList(f=1, n=22):
    return [ 'pheme{:02d}'.format( i ) for i in range(f, f+n) ]

commonValues = {
    'hosts': range( 1, 17 ),
    'threads': [ 1, 2, 3, 4 ],
    'runs': 10
}

kindDemo = Kind('./demo', '-h', OrderedDict([
    ( 'algorithms',[] ),
    ( 'hosts', commonValues[ 'hosts' ] ),
    ( 'threads', commonValues[ 'threads' ] ),
    ( 'workLoad', [] ),
    ( 'selection', [] ),
    ( 'runs', commonValues[ 'runs' ] )
] ) )
kindMpi = Kind( 'mpiexec', '-hostfile', OrderedDict( [
    ( 'hosts', commonValues[ 'hosts' ] ),
    ( 'program', './mpi' ),
    ( 'algorithms', [] ),
    ( 'threads', commonValues[ 'threads' ] ),
    ( 'workLoad', [] ),
    ( 'selection', [] ),
    ( 'runs', commonValues[ 'runs' ] )
] ) )

options = [
    Option( 'loadShortDemo', kindDemo, hList(), {
        'algorithms': [ 'load dedicated', 'load shared' ],
        'workLoad': [ 3000 ],
        'selection': [ 1 ]
    } ),
    Option( 'loadSLDemo', kindDemo, hList(), {
        'hosts': range( 16, 23 ),
        'algorithms': [ 'load dedicated', 'load shared' ],
        'workLoad': [ 10000 ],
        'selection': [ 1 ]
    } ),
    Option( 'loadLongDemo', kindDemo, hList(), {
        'algorithms': [ 'load dedicated long', 'load shared long' ],
        'workLoad': [ 1000 ],
        'selection': [ 1 ]
    } ),
    Option( 'pingDemo', kindDemo, hList(), {
        'hosts': range( 2, 13 ),
        'algorithms': [ 'ping dedicated', 'ping shared' ],
        'workLoad': [ 10000 ],
        'selection': [ 3 ]
    } ),
    Option( 'loadShortMpi', kindMpi, hList(), {
        'algorithms': [ 'load' ],
        'workLoad': [ 3000 ],
        'selection': [ 1 ]
    } ),
    Option( 'loadSLMpi', kindMpi, hList(), {
        'hosts': range( 16, 23 ),
        'algorithms': [ 'load' ],
        'workLoad': [ 10000 ],
        'selection': [ 1 ]
    } ),
    Option( 'loadLongMpi', kindMpi, hList(), {
        'algorithms': [ 'load long' ],
        'workLoad': [ 1000 ],
        'selection': [ 1 ]
    } ),
    Option( 'pingMpi', kindMpi, hList(), {
        'hosts': range( 2, 13 ),
        'algorithms': [ 'ping' ],
        'workLoad': [ 10000 ],
        'selection': [ 3 ]
    } )
]

class Generator(object):
    def __init__(self, program, hostList, hostSwitch):
        self.program = program
        self.hostList = hostList
        self.hostSwitch = hostSwitch
        self.numberOfHosts = 1
        self.runs = 1


    def selector( self, selected, keys, toSelect ):
        if keys and keys[ 0 ] == 'runs':
            self.runs = toSelect[ keys[ 0 ] ]
            keys = keys[1:]

        result = []

        if keys:
            if keys[ 0 ] == 'program':
                return self.selector(
                    selected + [ toSelect[ keys[ 0 ] ] ],
                    keys[1:],
                    toSelect
                )

            if keys[ 0 ] == 'hosts':
                selected = selected + [ self.hostSwitch, 'hosts' ]
                for value in toSelect[ keys[ 0 ] ]:
                    self.numberOfHosts = value
                    result += self.selector(
                        selected,
                        keys[1:],
                        toSelect
                    )
                return result

            if keys[ 0 ] == 'threads':
                selected = selected + [ '-n' ]
            if keys[ 0 ] == 'workLoad':
                selected = selected + [ '-w' ]
            if keys[ 0 ] == 'selection':
                selected = selected + [ '-s' ]

            for value in toSelect[ keys[ 0 ] ]:
                result += self.selector(
                    selected + [ str( value ) ],
                    keys[1:],
                    toSelect
                )
            return result;
        # else
        for r in range(self.runs):
            result += [ Run( self.numberOfHosts, self.program, *selected ) ]
        return result

    def generate( self, file, settings ):
        result = self.selector( [], list(settings.keys()), settings )

        with open( file, 'w' ) as f:
            print( ' '.join( self.hostList ), file=f )
            for r in result:
                print(str(r), file=f)

def help( args ):
    print( 'usage: {} testCase [testCase [...]]'.format( args[0] ) )
    print( 'usage: {} all'.format( args[0] ) )
    if len( args ) > 1:
        print( 'wrong testCase value: {}'.format( ' '.join( args[1:] ) ) )
    print( 'testCase could be one of the following:' )
    for opt in options:
        print( '\t{}'.format( opt.name() ) )


if __name__ != '__main__':
    print('must be executed as an independent program')
    sys.exit( 1 )

args = sys.argv

if len( args ) == 1:
    help( args )
    sys.exit( 2 )
if len( args ) == 2 and args[1] == 'all':
    del args[1]
    for opt in options:
        args += [opt.name()]

for testCase in args[1:]:
    match = None

    for opt in options:
        if testCase == opt.name():
            match = opt

    if match is None:
        help( args )
        sys.exit( 3 )

    g = Generator( match.program(), match.hostList(), match.switch() )
    g.generate( match.name(), match.options() )

