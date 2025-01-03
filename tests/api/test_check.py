import unittest

from src.api import check
from src.api.constants import SCOPE
from src.symbols import sym


class TestCheck(unittest.TestCase):
    """Tests api.check"""

    def test_is_temporary_value_const_string(self):
        node = sym.STRING("Hello world", 1)
        self.assertFalse(check.is_temporary_value(node))

    def test_is_temporary_value_var(self):
        node = sym.ID("a", 1).to_var()
        self.assertFalse(check.is_temporary_value(node))

    def test_is_temporary_value_param(self):
        node = sym.ID("a", 1).to_var()
        node.scope = SCOPE.parameter
        self.assertFalse(check.is_temporary_value(node))

    def test_is_temporary_value_expr(self):
        child = sym.ID("a", 1).to_var()
        node = sym.BINARY("PLUS", child, child, 1)
        self.assertTrue(check.is_temporary_value(node))
