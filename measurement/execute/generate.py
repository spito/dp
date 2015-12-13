#!/usr/bin/env python3
import sys
from collections import OrderedDict

class Run(object):
    def __init__(self, program, *args):
        self.program = program
        self.args = list(args)

    def __str__(self):
        return "{} {}".format(
            self.program,
            ' '.join(self.args)
        )

def selector( program, selected, keys, toSelect ):
    rep = 1
    if keys and keys[ 0 ] == 'runs':
       rep = toSelect[ keys[ 0 ] ]
       keys = keys[1:]

    result = []

    if keys:
        if keys[ 0 ] == 'threads':
            selected = selected + [ '-n' ]
        if keys[ 0 ] == 'workLoad':
            selected = selected + [ '-w' ]
        if keys[ 0 ] == 'selection':
            selected = selected + [ '-s' ]
        if keys[ 0 ] == 'hosts':
            selected = selected + [ '-h' ]
        if keys[ 0 ] == 'hostfile':
            selected = selected + [ '-hostfile' ]

        for r in range(rep):
            if isinstance( toSelect[ keys[ 0 ] ], str ):
                result += selector(
                    program,
                    selected + [ toSelect[ keys[ 0 ] ] ],
                    keys[1:],
                    toSelect
                )
            else:
                for value in toSelect[ keys[ 0 ] ]:
                    result += selector(
                        program,
                        selected + [ str( value ) ],
                        keys[1:],
                        toSelect
                    )
    else:
        for r in range(rep):
            result += [ Run( program, *selected ) ]
    return result



def generator(file, program, settings):
    result = selector( program, [], list(settings.keys()), settings )

    with open( file, 'w' ) as f:
        for r in result:
            print(str(r), file=f)


if __name__ != '__main__':
    print('must be executed as an independent program')
    sys.exit( 1 )

args = sys.argv

if len( args ) != 3:
    print( 'usage: {} {{mpi|demo}} batchFile'.format( args[0] ) )
    sys.exit( 2 )

if args[ 1 ] == 'demo':
    program = './demo'
    settings = OrderedDict([
        ('algorithms', [
            'load shared',
            'load dedicated'
        ]),
        ('threads', [1, 2, 3, 4]),
        ('workLoad', [1000] ),
        ('selection', [1, 20, 25]),
        ('hosts', [ 'hosts' ]),
        ('runs', 10)
    ])
elif args[ 1 ] == 'mpi':
    program = 'mpiexec'
    settings = OrderedDict([
        ('hostfile', [ 'hosts{:02d}'.format(i) for i in range(1,17) ]),
        ('program', './mpi'),
        ('threads', [1, 2, 3, 4]),
        ('workLoad', [1000]),
        ('selection', [1, 25]),
        ('runs', 6)
    ])

generator(
    args[ 2 ],
    program,
    settings
)

