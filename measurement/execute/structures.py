
class Run(object):
    def __init__(self, hosts, program, *args):
        self.hosts = hosts
        self.program = program
        self.args = list(args)

    def __str__(self):
        return "{} {} {}".format(
            self.hosts,
            self.program,
            ' '.join(self.args)
        )

class Kind(object):
    def __init__(self, program, switch, opts):
        self.program = program
        self.switch = switch
        self.opts = opts

class Option(object):
    def __init__(self, name, kind, hostList, opts):
        self._name = name
        self._kind = kind
        self._hostList = hostList
        self._opts = opts

    def name(self):
        return self._name

    def program(self):
        return self._kind.program

    def switch(self):
        return self._kind.switch

    def hostList(self):
        return self._hostList

    def options(self):
        opts = self._kind.opts.copy()
        opts.update( self._opts )
        return opts

