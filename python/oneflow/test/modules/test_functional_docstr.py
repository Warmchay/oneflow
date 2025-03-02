"""
Copyright 2020 The OneFlow Authors. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""
import os
import inspect
import unittest
from collections import OrderedDict

from test_util import GenArgList

import oneflow as flow
import oneflow.unittest


def _run_functional_doctest(
    test_case,
    globs=None,
    verbose=None,
    optionflags=0,
    raise_on_error=True,
    module=flow._C,
):
    import doctest

    parser = doctest.DocTestParser()
    if raise_on_error:
        runner = doctest.DebugRunner(verbose=verbose, optionflags=optionflags)
    else:
        runner = doctest.DocTestRunner(verbose=verbose, optionflags=optionflags)
    r = inspect.getmembers(flow._C)
    for (name, fun) in r:
        if fun.__doc__ is not None:
            print("test on docstr of: ", ".".join([module.__name__, name]))
            test = parser.get_doctest(fun.__doc__, {}, __name__, __file__, 0)
            runner.run(test)


@flow.unittest.skip_unless_1n1d()
@unittest.skipIf(os.getenv("ONEFLOW_TEST_CPU_ONLY"), "only test cpu cases")
class TestFunctionalDocstrModule(flow.unittest.TestCase):
    def test_functional_docstr(test_case):
        arg_dict = OrderedDict()
        arg_dict["module"] = [flow._C]
        for arg in GenArgList(arg_dict):
            _run_functional_doctest(
                test_case, raise_on_error=True, verbose=True, module=arg[0]
            )


if __name__ == "__main__":
    unittest.main()
