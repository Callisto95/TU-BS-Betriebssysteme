#!/usr/bin/env python3

import argparse
from collections import defaultdict
from dataclasses import dataclass, field, is_dataclass, asdict
import glob
from itertools import chain
from pathlib import Path
import inspect
import json
import logging
import os
import random
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import threading
import traceback
import yaml

logging.addLevelName(
    logging.WARNING, "\033[1;33m%s\033[1;0m" % logging.getLevelName(logging.WARNING)
)
logging.addLevelName(
    logging.ERROR, "\033[1;41m%s\033[1;0m" % logging.getLevelName(logging.ERROR)
)
logging.addLevelName(
    logging.INFO, "\033[1;34m%s\033[1;0m" % logging.getLevelName(logging.INFO)
)

PRESENT_OWN_THRESHOLD = 0.75
PRESENT_ML_THRESHOLD = 0.50

SCRIPT_ROOT = os.path.dirname(__file__)
GDB_SCRIPT_TRACE = os.path.join(SCRIPT_ROOT, "gdb_trace.py")
GDB = os.environ.get("USE_GDB")
KEEP_FILES = os.environ.get("KEEP_FILES")


def wrap_in_list(o):
    if o is None:
        return []
    if isinstance(o, list):
        return o
    return [o]


class Disabler:
    """Objects of this class raise on every access a MissingRequirementError.
    This is intended to be used as an initial value for variables
    used in later tests that should be disabled
    if there is no assignment of a new value"""

    def __init__(self, msg):
        self.msg = msg

    def __call__(self, *args, **kwargs):
        raise MissingRequirementError(self.msg)

    def __setitem__(self, *args, **kwargs):
        self()

    def __getattr__(self, *args, **kwargs):
        self()


@dataclass
class TestcaseResult:
    """Class for storing the results of a single (sub)test"""

    ty: str
    name: str
    failed: bool = True
    skipped: bool = False
    timeout: bool = False
    points: int = 0
    max_points: int = 0
    subtests: list = field(default_factory=list)
    requirements: dict = field(default_factory=dict)
    warnings: list = field(default_factory=list)
    errors: list = field(default_factory=list)
    body: str = ""
    action: str = ""
    expect: str = ""
    stdout: str = ""
    stderr: str = ""
    variables: dict = field(default_factory=dict)
    # code_snippets: dict = field(default_factory=lambda: defaultdict(dict))
    code_snippets: list = field(default_factory=list)
    compiler_output: dict = field(default_factory=dict)
    compilation_failed: bool = False
    executions: list = field(default_factory=list)
    # Workaround until python on srabmit is new enough to support defaultdicts in data classes
    failed_source_lines: dict = field(default_factory=dict)
    failed_testcase_lines: list = field(default_factory=list)

    def apply(self, **kwargs):
        for k, v in kwargs.items():
            setattr(self, k, v)
        return self


class SourceCodeList(list):
    def __add__(self, other):
        if isinstance(other, list):
            return SourceCodeList(self + other)
        if isinstance(other, SourceCode):
            return SourceCodeList(list(self) + [other])


@dataclass
class SourceCode:
    name: str
    body: str
    snippet: bool = False

    def __add__(self, other):
        if isinstance(other, list):
            return SourceCodeList([self] + other)
        if isinstance(other, SourceCode):
            return SourceCodeList([self, other])
        raise NotImplementedError(f"adding {type(self)} and {type(other)}")

    def __iter__(self):
        return iter([self])


@dataclass
class ExecutionResult:
    retcode: int
    cmd: list[str]
    stdin: bytes
    stdout: bytes
    stderr: bytes
    stdout_expect: bytes = None
    stderr_expect: bytes = None
    failed: bool = False
    line: int = 0

    def __iter__(self):
        return iter((self.stdout, self.stderr))

    def log_io(self, msg, expect=None):
        logging.error(
            "%s (exitcode=%s):\n%s\n%sSTDOUT:\n%s\nSTDERR:\n%s",
            red(msg),
            self.retcode,
            " ".join(self.cmd),
            f"Expected:\n{expect}\n" if expect else "",
            self.stdout,
            red(self.stderr),
        )
        if self.stdin:
            logging.error("STDIN:\n%s\n", self.stdin)

    def check_stderr_contains(self, candidates, **kwargs):
        self._check_contains(candidates, "stderr", **kwargs)

    def check_stdout_contains(self, candidates, **kwargs):
        self._check_contains(candidates, "stdout", **kwargs)

    def _check_contains(
        self, candidates, stream, case_sensitive=False, msg=None, expect=None
    ):
        for candidate in wrap_in_list(candidates):
            if case_sensitive:
                candidate_l = candidate
                searchspace_l = getattr(self, stream)
            else:
                candidate_l = candidate.lower()
                searchspace_l = getattr(self, stream).lower()
            if candidate_l not in searchspace_l:
                msg = msg or f"Candidate '{candidate}' not found in {stream}"
                self.log_io(msg, expect=expect)
                self.failed = True
                setattr(self, f"{stream}_expect", expect or candidate)
                raise RuntimeError(msg)


class MissingRequirementError(RuntimeError):
    pass


class LocalError(RuntimeError):
    def __init__(self, *args, malus=0):
        super().__init__(*args)
        self.malus = malus


class FatalError(RuntimeError):
    pass


class Testcase:
    def __init__(self, testcase):
        self.testcase = testcase
        self.variables = {
            "Compilation": Compilation,
            "os": os,
            "logging": logging,
            "LocalError": LocalError,
            "FatalError": FatalError,
            "MissingRequirementError": MissingRequirementError,
            "Disabler": Disabler,
            "tokens": {},
        }
        Compilation.globals = self.variables
        self.failed = False
        self.skipped_tests = 0
        self.skipped = False
        self.skip_following_tests = False
        self.points = 0
        self.max_points = 0
        self.subtests = []

        with open(testcase) as fd:
            content = fd.read()
            self.load(content, execute=True)

    def load(self, content, execute=False):
        block_types = {
            "!yaml": self.block_yaml,
            "!source": self.block_source,
            "!python": self.block_nop,
            "!inherit": self.block_inherit,
            "!python_helper": self.block_python_helper,
        }
        if execute:
            block_types["!python"] = self.block_python

        blocks = re.split("\n---", "\n" + content)
        blocks = [x.lstrip() for x in blocks if x]
        # For every block, we call the block_ function with the given arguments
        for block in blocks:
            head, body = (block + "\n").split("\n", 1)
            block_ty, *block_args = head.split()
            if block_ty not in block_types:
                raise FatalError(f"Block type {block_ty} is unknown")

            ret = block_types[block_ty](
                body, *block_args, skip=self.skip_following_tests
            )
            if ret:
                # logging.warning("RET: %s", ret)
                self.subtests.append(ret)
                if ret.failed:
                    self.failed = True
                if len(ret.requirements) > 0 and not all(ret.requirements.values()):
                    self.skipped = True
                    ret.warnings.append(
                        "Unment requirements: "
                        + ", ".join(k for k, v in ret.requirements.items() if not v)
                    )

                if ret.skipped:
                    self.skipped_tests += 1

    def block_nop(self, *args, **kwargs):
        pass

    def block_yaml(self, block, *args, **kwargs):
        ret = TestcaseResult(
            "yaml",
            " ".join(args),
            failed=False,
            # skipped=kwargs["skip"],
        )
        new_vars = yaml.safe_load(block)
        if not new_vars:
            return ret.apply(failed=True)
        if "tokens" in new_vars:
            for k, v in new_vars["tokens"].items():
                new_vars["tokens"][k] = eval(v)
        ret.variables = new_vars
        self.variables.update(new_vars)
        if "requirements" in new_vars:
            # try:
            #     Compilation.check_requirements(new_vars["requirements"])
            # except MissingRequirementError as e:
            # self.skipped = True
            # logging.warning(e)
            # return False, True
            ret.requirements = Compilation.check_requirements(new_vars["requirements"],
                                                              raise_on_failure=False)
            ret.skipped = len(ret.requirements) > 0 and not all(
                ret.requirements.values()
            )
            self.skip_following_tests = ret.skipped
        return ret

    def block_source(self, block, variable, *args, **kwargs):
        self.variables[variable.strip()] = SourceCode(variable, block, True)
        return TestcaseResult("src", variable, failed=False, body=block)

    def block_inherit(self, block, other_testcase, *args, **kwargs):
        basedir = os.path.dirname(self.testcase)
        with open(os.path.join(basedir, other_testcase)) as fd:
            content = fd.read()
            self.load(content, execute=False)
        return TestcaseResult("inherit", "other_testcase", failed=False)

    def block_python_helper(self, block, *args, **kwargs):
        code = compile(block, f"{self.testcase}: {' '.join(args)}", "exec")
        exec(code, self.variables)

    def block_python(self, block, *args, **kwargs):
        block = block.strip()
        ret = TestcaseResult("python", " ".join(args), body=block)
        Compilation.testcase_result = ret
        if kwargs["skip"]:
            logging.warning('... Subtest: "%s" skipped', " ".join(args))
            return ret.apply(skipped=True)
        if args:
            logging.info('... Subtest: "%s"', " ".join(args))
        try:
            code = compile(block, f"{self.testcase}: {' '.join(args)}", "exec")
            exec(code, self.variables)
        except TypeError as e:
            ret.failed_testcase_lines.append(sys.exc_info()[2].tb_next.tb_lineno)
            raise e
        except MissingRequirementError as e:
            logging.warning('...    Skipped: Missing requirement: "%s"', str(e))
            ret.requirements[str(e)] = False
            ret.failed_testcase_lines.append(sys.exc_info()[2].tb_next.tb_lineno)
            return ret.apply(skipped=True)
        except RuntimeWarning as e:
            logging.warning(e)
            ret.warnings.append(e)
            return ret.apply(skipped=True)
        except subprocess.TimeoutExpired as e:
            logging.error("testcase failed by timeout: %s, %s", self.testcase, e)
            out = e.stdout or ""
            err = e.stderr or ""
            logging.error("STDOUT:\n%s\nSTDERR:\n%s", out, red(err))
            ret.failed_testcase_lines.append(sys.exc_info()[2].tb_next.tb_lineno)
            return ret.apply(
                failed=True, timeout=True, stdout=e.stdout, stderr=e.stderr
            )
        except Exception as e:
            m = self.variables.get("malus", 0)
            if isinstance(e, LocalError):
                m += e.malus
            ret.points = -m
            self.points -= m
            pts = " (-%s points)" % m if m else ""
            if "action_desc" in self.variables:
                ret.action = self.variables["action_desc"]
                logging.info("...    Description: %s", self.variables["action_desc"])
            if "expect_desc" in self.variables:
                ret.expect = self.variables["expect_desc"]
                logging.info("...    Expectation: %s", self.variables["expect_desc"])
            logging.error("...    FAILED%s: %s", pts, e)
            # print(traceback.format_exc(), file=sys.stderr)
            ret.errors.append(e)
            ret.failed_testcase_lines.append(sys.exc_info()[2].tb_next.tb_lineno)
            return ret.apply(failed=True)
        else:
            b = self.variables.get("bonus", 0)
            if not b and not self.variables.get("malus"):
                sys.exit(F"Invalid Testcase '{' '.join(args)}' in {os.path.basename(self.testcase)}: No points defined")
            ret.points = b
            self.points += b
            logging.info(f"...    OK ({b} points)")
        finally:
            self.max_points += self.variables.get("bonus", 0)
            self.variables.pop("bonus", None)
            self.variables.pop("malus", None)
            self.variables.pop("expect_desc", None)
            self.variables.pop("action_desc", None)
        return ret.apply(failed=False)

    def to_json(self):
        return {
            "name": self.testcase,
            "failed": self.failed,
            "skipped": self.skipped,
            "skipped_tests": self.skipped_tests,
            "points": self.points,
            "subtests": self.subtests,
        }


gdb_wait_snippet = """
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
void handler(int sig){}
void __attribute__ ((constructor)) wait_for_gdb() {
  signal(SIGPROF, &handler);
  if (getenv("WAIT_FOR_GDB")) {
    // We are stoping this process untill gdb sends "continue"
    pause();
  }
}
"""


def red(msg):
    return "\x1b[31;4m%s\x1b[0m" % msg


def replace_all(text, pairs):
    for old, new in pairs:
        text = text.replace(str(old), str(new))
    return text


class Compilation:
    """Compilation is executed in the context of a testcase"""

    globals = None

    instances = []

    no_strace = False

    testcase_result = None

    def __init__(self, after_main=None, before_main=None, source_files=None):
        if source_files is None:
            if "sources" in Compilation.globals:
                source_files = Compilation.globals["sources"]
            else:
                sys.exit("Invalid Testcase: No sources given")
        assert type(source_files) is dict, "source_files must be a dict"
        self.source_files = source_files
        for fn in source_files:
            if os.path.exists(fn) or "content" in source_files[fn]:
                continue
            prec_name = os.path.splitext(fn)[0] + ".o"
            if source_files[fn].get("precompiled"):
                if os.path.exists(prec_name):
                    continue
                raise RuntimeError(f"Neither {fn} nor {prec_name} exist.")
            raise RuntimeError(f"File {fn} does not exist.")
        self.after_main = wrap_in_list(after_main)
        self.after_main_cat = ""
        self.before_main = wrap_in_list(before_main)
        self.tmpdir = tempfile.mkdtemp(prefix="gbs_test_")

        # We use a random number as a flag to replace {{{FINISHED}}}
        # in the output.
        self.flag = str(random.randint(0, 10000000))
        Compilation.instances.append(self)

    @classmethod
    def check_requirements(cls, requirements, raise_on_failure=True, log=True):
        if os.environ.get("IGNORE_REQUIREMENTS", False):
            return {"IGNORE_REQUIREMENTS": True}
        ret = {req: cls.is_implemented(req) for req in requirements}
        msg = 'Requirement "%s" is missing. Remove marker to activate testcase.'
        for req, impl in ret.items():
            if not impl:
                if log:
                    logging.warning(msg, req)
                if raise_on_failure:
                    raise MissingRequirementError(msg % req)
        return ret


    @classmethod
    def is_implemented(cls, marker):
        sources = cls.globals.get("sources", None)
        if not sources:
            return False
        for fn in sources:
            if "content" in sources[fn]:
                content = sources[fn]["content"]
            elif "precompiled" in sources[fn]:
                continue
            else:
                with open(fn) as fd:
                    content = fd.read()
            if f"{marker}_NOT_IMPLEMENTED_MARKER" in content:
                return False
        return True

    def fail_marker(self, marker, msg):
        if not self.after_main_cat:
            raise RuntimeError(msg)
        lines = self.after_main_cat.split("\n")
        for idx, line in enumerate(lines):
            if "Marker " + str(marker) in line:
                lines[idx] = "// --> " + red("Error is around here")
            elif "Marker" in line:
                lines[idx] = None
        logging.error(red(msg) + "\n" + "\n".join([x for x in lines if x is not None]))

        raise RuntimeError(msg)

    def compile(self, flags=None, remap=None, fail_silent=False):
        try:
            flags = flags or []
            # There are default flags
            flags += Compilation.globals.get("cflags", [])
            # self.testcase_result.code_snippets = self.source_files
            return self.__compile(flags, remap)
        except subprocess.CalledProcessError as e:
            if fail_silent:
                return Disabler("Compilation failed: " + " ".join(self.source_files))
            logging.error(
                "Compilation failed: %s\n%s", " ".join(e.args[1]),
                e.stderr.decode(errors='replace')
            )
            self.__mark_failure_context(e.stderr.decode(errors='replace'))
            Compilation.testcase_result.compilation_failed = True
            raise RuntimeError("Compilation Failed") from e

    def __compile(self, flags=[], remap=None):
        if remap is None:
            remap = {}

        if self.after_main:
            remap["main"] = "studentMain"

        line = "#line 1 "
        if GDB:
            self.source_files["wait_for_gdb.c"] = {"content": gdb_wait_snippet}
            line = "//# line 1 "

        found_main = False
        logging.debug(self.source_files)
        for fn in self.source_files:
            src, dst = fn, os.path.join(self.tmpdir, os.path.basename(fn))

            if "content" in self.source_files[fn]:
                content = self.source_files[fn]["content"]
            elif not os.path.exists(src) and self.source_files[fn].get("precompiled"):
                prec_name = os.path.splitext(src)[0] + ".o"
                shutil.copyfile(prec_name, dst+".o")
                logging.debug("copied precompiled object %s into %s", prec_name, dst+".o")
                continue
            else:
                with open(src) as fd:
                    content = fd.read()
            if not content.endswith("\n") and len(content) > 0:
                logging.warning("Missing newline at End of File")
                content += "\n"

            Compilation.testcase_result.code_snippets.append(
                SourceCode(name=fn, body=content)
            )

            if self.source_files[src].get("main"):
                found_main = True
                before = ""
                after = ""
                for oldname, newname in remap.items():
                    before += "#define %s %s\n" % (oldname, newname)
                    after += "#undef %s\n" % (oldname)

                Compilation.testcase_result.code_snippets += SourceCode(
                    "<<remap_before>>", before, snippet=True
                )
                Compilation.testcase_result.code_snippets += SourceCode(
                    "<<remap_after>>", after, snippet=True
                )

                if self.before_main:
                    for s in self.before_main:
                        Compilation.testcase_result.code_snippets += s

                if self.after_main:
                    for s in self.after_main:
                        Compilation.testcase_result.code_snippets += s

                out = ""
                if before:
                    if not GDB:
                        out += f"""#line 1 "<<__remap_before>>"\n"""
                    out += before
                for i in self.before_main:
                    if not GDB:
                        out += f"""#line 1 "{i.name}"\n"""
                    out += i.body
                if not GDB:
                    out += f"""#line 1 "{src}"\n"""
                out += content
                all_tokens = [
                    ("{{{" + x[0] + "}}}", str(x[1]))
                    for x in chain(
                        self.globals["tokens"].items(), (("FINISHED", self.flag),)
                    )
                ]

                if after:
                    if not GDB:
                        out += f"""#line 1 "<<__remap_after>>"\n"""
                        self.after_main_cat += f"""#line 1 "<<__remap_after>>"\n"""
                    out += replace_all(after, all_tokens)
                    self.after_main_cat += after
                for i in self.after_main:
                    if not GDB:
                        out += f"""#line 1 "{i.name}"\n"""
                        self.after_main_cat += f"""#line 1 "{i.name}"\n"""
                    out += replace_all(i.body, all_tokens)
                    self.after_main_cat += i.body
                content = out
                Compilation.testcase_result.code_snippets += SourceCode(
                    f'"{fn}" combined with snippets', content, False
                )

            with open(dst, "w+") as fd:
                fd.write(content)
                logging.debug("copied injected source %s into %s", src, dst)

        assert found_main, "One source file must be attributed with 'main: true'"

        compiler = Compilation.globals.get("CC", "gcc")

        object_files = []
        logging.debug("Compile sources: %s", self.source_files)
        for fn in self.source_files:
            if not fn.endswith(".c"):
                continue
            obj = os.path.join(self.tmpdir, os.path.basename(fn) + ".o")
            if self.source_files[fn].get("precompiled") and not os.path.exists(os.path.join(self.tmpdir, os.path.basename(fn))):
                object_files += [obj]
                continue
            result = subprocess.run(
                [compiler, "-fdiagnostics-color=always"]
                + flags
                + ["-c", "-o", obj, os.path.join(self.tmpdir, fn)],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
            Compilation.testcase_result.compiler_output[fn] = result
            result.check_returncode()
            object_files += [obj]
        main = os.path.join(self.tmpdir, "main")

        logging.debug("Link objects: %s", object_files)

        result = subprocess.run(
            [compiler, "-fdiagnostics-color=always"] + flags + object_files + ["-o", main],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        Compilation.testcase_result.compiler_output["main"] = result
        result.check_returncode()
        self.main = main

        return self

    def __mark_failure_context(self, stderr):
        ret = ""
        for snippet in self.after_main:
            m = re.search(f"{snippet.name}:([0-9]+)", stderr)
            if m:
                line = int(m.group(1))
                # Workaround until python on srabmit is new enough to
                # support defaultdicts in data classes
                if not snippet.name in self.testcase_result.failed_source_lines:
                    self.testcase_result.failed_source_lines[snippet.name] = []
                Compilation.testcase_result.failed_source_lines[snippet.name] += [line]
                lines = snippet.body.split("\n")
                if line < len(lines):
                    lines[line - 1] += "   " + red("<- ERROR")
                    ret = "\n".join(lines)
                    logging.info("Code containing the error:\n%s", ret)
        return ret

    def check_allowed_global_symbols(self, symbols, segment):
        allowed = set(symbols)
        defaults = {
            "T": ["_fini", "_init", "_start"],
            "B": ["__bss_start", "_end", "stderr"],
            "D": ["__data_start", "__dso_handle", "_edata", "__TMC_END__"],
        }
        allowed.update(defaults.get(segment, []))
        try:
            nm = subprocess.run(
                ["nm", "-P", self.main], stdout=subprocess.PIPE, check=True
            )
            illegal_symbols = []
            for line in nm.stdout.decode().split("\n"):
                m = re.match(r"(?P<name>[^\s]*) *(?P<segment>[^\s]*)", line)
                if m and m.group("segment") == segment:
                    name = m.group("name")
                    if not any(name.startswith(a) for a in allowed):
                        illegal_symbols.append(name)
            if illegal_symbols:
                raise LocalError(
                    "Found illegal symbols in {} segment: {}".format(
                        {"t": "Text", "b": "BSS", "d": "data"}[segment.lower()],
                        ", ".join(illegal_symbols),
                    ),
                    malus=1,
                )
        except FileNotFoundError:
            raise MissingRequirementError("Tool 'nm' not found")

    def run(
        self,
        args=[],
        cmd_prefix=[],
        must_fail=False,
        retcode_expected=lambda retcode: retcode != 0,
        input=None,
        **kwargs,
    ):
        input_ = input.encode() if type(input) == str else input
        if input_:
            for k, v in (self.globals["tokens"] | {"FINISHED": self.flag}).items():
                input_ = input_.replace(("{{{" + k + "}}}").encode(), str(v).encode())
        result = self.__run(args, cmd_prefix, input=input_, **kwargs)
        if result.retcode < 0:
            result.stderr += "<<Killed by Signal '%s'>>" % (
                signal.Signals(result.retcode * -1)
            )

        after_main_txt = ""

        logging.debug("Run: %s, [retcode=%d]", cmd_prefix + args, result.retcode)
        if result.retcode != 0 and not must_fail:
            self.__mark_failure_context(result.stderr)
            msg = "Program Run failed unexpected"
            result.log_io(msg)
            if GDB:
                self.__run_with_gdb(
                    args, cmd_prefix, input_data=input_, gdb=GDB, **kwargs
                )
            result.failed = True
            raise RuntimeError(msg)
        elif must_fail and not retcode_expected(result.retcode):
            msg = "Program Run did not fail, while it should"
            result.log_io(red(msg))
            if input_:
                logging.error("STDIN:\n%s\n", input_.decode())
            if after_main_txt:
                logging.info("\n%s", after_main_txt)
            elif self.after_main_cat:
                logging.info("\n%s", self.after_main_cat)
            result.failed = True
            raise RuntimeError(msg)
        else:
            # If the testcase was successful, we look for our flag in
            # the stdout output. If this was not present, the user
            # probably cheated on us.
            if (self.after_main_cat and "{{{FINISHED}}}" in self.after_main_cat) or (
                input_ and b"{{{FINISHED}}}" in input_
            ):
                if self.flag not in result.stdout:
                    msg = "Testcase was not run until the end. Flag was not found."
                    result.log_io(msg)
                    result.failed = True
                    raise RuntimeError("Testcase was not run until the end")

        return result

    def __run(self, args=[], cmd_prefix=[], input=None, **kwargs):
        assert self.main, "Compilation failed"
        cmd = cmd_prefix + [self.main] + args
        stdin = subprocess.PIPE if input else subprocess.DEVNULL
        env = os.environ.copy()
        env["LANGUAGE"] = "C"
        p = subprocess.Popen(
            cmd,
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            stdin=stdin,
            **kwargs,
        )
        stdout, stderr = p.communicate(input=input, timeout=10)
        result = ExecutionResult(
            stdout=stdout.decode(errors="replace"),
            stderr=stderr.decode(errors="replace"),
            retcode=p.returncode,
            cmd=cmd,
            stdin=input or "",
            failed=False,
            line=get_pp_src_line_no(),
        )
        Compilation.testcase_result.executions.append(result)
        return result

    def __run_with_gdb(
        self, args=[], cmd_prefix=[], input_data=None, gdb=None, **kwargs
    ):
        assert self.main, "Compilation failed"
        assert gdb, "run with gdb without gdb is nonsense"
        while True:
            ans = input(
                "Testcase failed. Restart testcase with {}? [Yes/no]".format(gdb)
            )
            if ans.lower() in ["y", "j", "yes", "ja", ""]:
                break
            if ans.lower() in ["no", "nein", "n"]:
                return
        cmd = cmd_prefix + [self.main] + args
        stdin = subprocess.PIPE if input_data else subprocess.DEVNULL
        env = os.environ.copy()
        env["WAIT_FOR_GDB"] = "true"
        p = subprocess.Popen(cmd, env=env, stdin=stdin, **kwargs)
        gdb_p = subprocess.Popen(
            [gdb, "-ex", "break main", "-ex", "signal SIGPROF", self.main, str(p.pid)]
        )
        threading.Thread(target=p.communicate, args=(input_data,), daemon=True).start()
        gdb_p.communicate()
        while True:
            ans = input("Continue with next testcase? [Yes/no]")
            if ans.lower() in ["y", "j", "yes", "ja", ""]:
                break
            if ans.lower() in ["no", "nein", "n"]:
                exit()

    def spawn(self, args=[], cmd_prefix=[], input=None, **kwargs):
        """Spawn a child process and return handle immediately"""
        assert self.main, "Compilation failed"
        cmd = cmd_prefix + [self.main] + args
        stdin = subprocess.PIPE if input else subprocess.DEVNULL
        return subprocess.Popen(
            cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, stdin=stdin, **kwargs
        )

    def trace(self, functions, **kwargs):
        log_file = tempfile.NamedTemporaryFile()
        logging.debug(log_file.name)
        os.environ["TRACE_FILE"] = log_file.name
        os.environ["TRACE_FUNCTIONS"] = ",".join(functions)
        if not os.path.exists(GDB_SCRIPT_TRACE):
            m = "GDB_SCRIPT_TRACE not found: " + GDB_SCRIPT_TRACE
            raise RuntimeError(m)
        cmd_prefix = ["gdb", "-nx", "-x", GDB_SCRIPT_TRACE, "--args"]
        stdout, stderr = self.run(cmd_prefix=cmd_prefix, **kwargs)
        ret = []
        for line in log_file.readlines():
            ret.append(eval(line))

        return Trace(ret)

    def signal_trace(self, args=[], functions=[], **kwargs):
        log_file = tempfile.NamedTemporaryFile()
        logging.debug(log_file.name)
        os.environ["TRACE_FILE"] = log_file.name
        os.environ["TRACE_FUNCTIONS"] = ",".join(functions)
        os.environ["TRACE_SIGNALS"] = "True"
        if not os.path.exists(GDB_SCRIPT_TRACE):
            m = "GDB_SCRIPT_TRACE not found: " + GDB_SCRIPT_TRACE
            raise RuntimeError(m)
        cmd_prefix = ["gdb", "-nx", "-x", GDB_SCRIPT_TRACE, "--args"]
        return (log_file.name, self.spawn(args=args, cmd_prefix=cmd_prefix, **kwargs))

    def strace(self, syscalls=None, **kwargs):
        if Compilation.no_strace:
            raise MissingRequirementError("strace is not installed. Test skipped.")
        log_file = tempfile.NamedTemporaryFile()
        try:
            cmd_prefix = ["strace", "-qqf", "-o", log_file.name]
            if syscalls:
                cmd_prefix += ["-e", "trace=" + syscalls]
            run_result = self.run(cmd_prefix=cmd_prefix, **kwargs)
            lines = log_file.readlines()
            lines = [l.decode() for l in lines]
            return run_result, lines
        except FileNotFoundError as e:
            if e.filename == cmd_prefix[0]:
                Compilation.no_strace = True
                raise MissingRequirementError(
                    "strace is not installed. "
                    "To run this test install strace."
                    "\n  apt-get install strace\n"
                )
            raise RuntimeError(dir(e))

    def cleanup(self):
        if KEEP_FILES or self.tmpdir is None:
            return
        shutil.rmtree(self.tmpdir)
        self.tmpdir = None


class Trace:
    def __init__(self, records):
        invocations = {}
        for x in records:
            if x[0] == "call":
                _, parent, child, name, asm_name, args, thread_id = x
                invocations[child] = {
                    "id": child,
                    "name": name,
                    "asm_name": asm_name,
                    "args": args,
                    "parent": None,
                    "return": None,
                    "children": [],
                    "thread_id": thread_id,
                }
                if parent is not None:
                    invocations[child]["parent"] = invocations[parent]
                    invocations[parent]["children"].append(invocations[x[1]])
            elif x[0] == "return":
                _, id, _, ret = x
                invocations[id]["return"] = ret

        self.records = invocations

    def function_called(self, name):
        for record in sorted(self.records.values(), key=lambda x: x["id"]):
            if type(name) is str and record["name"] == name:
                yield record
            if type(name) in [list, tuple] and record["name"] in name:
                yield record


def output_results(max_points, earned_points, results, result_file):
    if any(x.failed for x in results):
        print()
        print(
            "Failed Testcases: \n - "
            + "\n - ".join([x.testcase for x in results if x.failed]),
            file=sys.stderr,
        )
    did_skip = False
    if any(x.skipped_tests for x in results):
        did_skip = True
        print(
            f"{sum(x.skipped_tests for x in results)} test(s) were skipped.",
            file=sys.stderr,
        )

    result = "failed"
    if did_skip:
        print(f"WARNING: Some tests where skipped, cannot estimate earned points.\n  ↳ Re-run with IGNORE_REQUIREMENTS=1 to determine earned points.", file=sys.stderr)
    else:
        sep = "-"*72
        print(f"\n{sep}\nYou earned {earned_points}/{max_points} points for this task.")
        relative_points = earned_points / max_points
        if relative_points >= PRESENT_OWN_THRESHOLD:
            result = "own"
        elif relative_points >= PRESENT_ML_THRESHOLD:
            result = "ml"

    if result_file:
        with open(result_file, "w") as f:
            f.write(result + "\n")

def exit_with_status(results):
    exit_code = 0
    if any(x.failed for x in results):
        exit_code += 1
    if any(x.skipped_tests for x in results):
        exit_code += 2
    sys.exit(exit_code)


class DataclassJsonEncoder(json.JSONEncoder):
    def default(self, o):
        if is_dataclass(o):
            return asdict(o)
        if isinstance(o, Testcase):
            return o.to_json()
        if isinstance(o, Exception):
            return str(o)
        if isinstance(o, bytes):
            return o.decode(errors='replace')
        if isinstance(o, subprocess.CompletedProcess):
            return {
                x: getattr(o, x) for x in ["args", "returncode", "stdout", "stderr"]
            }
        return super().default(o)


def walk(x, fn):
    if isinstance(x, list):
        for i in x:
            yield fn(i)
    elif isinstance(x, Testcase):
        yield (fn(x))
        for i in x.subtests:
            walk(i, fn)
    elif isinstance(x, TestcaseResult):
        yield (fn(x))
        for i in x.subtests:
            walk(i, fn)


def print_json(max_points, earned_points, results, target):
    total = {
        "tests": results,
        "success": all(walk(results, lambda t: not t.failed and not t.skipped)),
        "max_points": max_points,
        "points": earned_points,
    }
    with open(target, "w") as json_file:
        json.dump(total, json_file, cls=DataclassJsonEncoder)


def get_pp_src_line_no():
    parent_parent = inspect.currentframe().f_back.f_back.f_back
    return parent_parent.f_lineno


def main():
    parser = argparse.ArgumentParser(description="GBS Testcase Tool.")
    parser.add_argument(
        "-t",
        "--testcases",
        dest="testcases",
        default="tests",
        help="Testcase(s) - directory or file",
    )
    parser.add_argument(
        "-v",
        "--verbose",
        dest="verbose",
        action="store_true",
        default=False,
        help="verbose output",
    )
    parser.add_argument("--json", help="output in json format")
    parser.add_argument("--result", help="write result file")
    parser.add_argument(
        "--no-error-on-failure",
        action="store_false",
        dest="error_on_failure",
        help="Don't set exit status to non-zero in case of a failing testcase",
    )

    args = parser.parse_args()
    if args.verbose:
        logging.basicConfig(level=logging.DEBUG)
    else:
        logging.basicConfig(level=logging.INFO)

    if os.path.isdir(args.testcases):
        testcases = glob.glob(os.path.join(args.testcases, "*.test"))
    else:
        testcases = [args.testcases]

    results = []
    for test in sorted(testcases):
        # Execute testcase
        logging.info("Testcase: %s", test)
        t = Testcase(test)
        results.append(t)

    for c in Compilation.instances:
        c.cleanup()
        pass

    max_points = 0
    earned_points = 0
    did_skip = any(x.skipped_tests for x in results)
    if not did_skip:
        for t in results:
            max_points += t.max_points
            earned_points += t.points

    # Ensure that earned_points is never negative.
    earned_points = max(earned_points, 0)

    if args.json:
        print_json(max_points, earned_points, results, args.json)
    output_results(max_points, earned_points, results, args.result)
    if args.error_on_failure:
        exit_with_status(results)


if __name__ == "__main__":
    main()
