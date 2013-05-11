#!/usr/bin/python
# -*- coding: utf-8 -*-
# vim: ts=4:et:sw=4:

# ----------------------------------------------------------------------
# Copyleft (K), Jose M. Rodriguez-Rosa (a.k.a. Boriel)
#
# This program is Free Software and is released under the terms of
#                    the GNU General License
# ----------------------------------------------------------------------

from symbol import Symbol


class SymbolSTRING(Symbol):
    ''' Defines a string constant.
    '''
    def __init__(self, value, lineno):
        Symbol.__init__(self)
        self.value = value
        self.type_ = 'string'
        self.lineno = lineno

    def __str__(self):
        return self.value

    def __repr__(self):
        return '"%s"' % str(self)