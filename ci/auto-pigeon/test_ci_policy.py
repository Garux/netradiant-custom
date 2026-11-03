"""Negative controls of smoke and full qualification: what must never read as a pass. selftest.py runs these."""

import copy
import json
import pathlib
import subprocess
import sys
import tempfile
import unittest

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import publish  # noqa: E402
import selftest  # noqa: E402
import source_archive  # noqa: E402
from common import (ACCEPTANCE_STEPS, SMOKE_CASES, SMOKE_NEGATIVE, SMOKE_POSITIVE, SMOKE_TOOL_SECONDS, Failure, regress_summary,  # noqa: E402
                    run, sha256_file, validated_regress, write_json)


def full_summary():
    cases = {"A": {"status": "pass", "leak": False, "stages": {"bsp": 0}},
             "B": {"status": "known_failure", "leak": False, "stages": {"bsp": 0}}}
    return {"mode": "full", "exit": 0, "seconds": 1.0, "counts": {"pass": 1, "known_failure": 1}, "expected": ["A", "B"], "cases": cases,
            "required": {"cases": 1, "passed": 1, "failed": 0}, "q3map2_sha256": "0" * 64,
            "known_failures": {"cases": 1, "still_failing": 1, "unexpected_pass": 0}}


class Qualification(unittest.TestCase):
    def refused(self, summary):
        with self.assertRaises(Failure):
            validated_regress(summary)

    def test_a_declared_run_qualifies_in_its_own_mode(self):
        self.assertEqual(validated_regress(selftest.smoke_summary()), "smoke")
        self.assertEqual(validated_regress(full_summary()), "full")

    def test_a_missing_or_empty_report_is_not_a_pass(self):
        self.refused(None)
        self.refused({})
        s = selftest.smoke_summary()
        s.update(cases={}, expected=[])
        self.refused(s)

    def test_a_partial_or_unknown_mode_is_not_a_pass(self):
        for mode in ("partial", "unknown", None):
            s = selftest.smoke_summary()
            s["mode"] = mode
            self.refused(s)

    def test_a_nonzero_exit_is_not_a_pass(self):
        s = selftest.smoke_summary()
        s["exit"] = 1
        self.refused(s)

    def test_a_case_that_did_not_run_is_not_a_pass(self):
        s = selftest.smoke_summary()
        del s["cases"][SMOKE_NEGATIVE]
        self.refused(s)
        s = selftest.smoke_summary()
        s["expected"] = [SMOKE_POSITIVE]
        del s["cases"][SMOKE_NEGATIVE]
        self.refused(s)

    def test_a_skipped_stage_is_not_a_pass(self):
        s = selftest.smoke_summary()
        del s["cases"][SMOKE_POSITIVE]["stages"]["light"]
        self.refused(s)

    def test_a_timed_out_stage_is_not_a_pass(self):
        s = selftest.smoke_summary()
        s["cases"][SMOKE_POSITIVE]["stages"]["vis"] = "timeout"
        self.refused(s)
        s = selftest.smoke_summary()
        s["cases"][SMOKE_NEGATIVE]["status"] = "fail"
        self.refused(s)

    def test_the_negative_must_actually_leak_and_the_positive_seal(self):
        s = selftest.smoke_summary()
        s["cases"][SMOKE_NEGATIVE]["leak"] = False
        self.refused(s)
        s = selftest.smoke_summary()
        s["cases"][SMOKE_POSITIVE]["leak"] = True
        self.refused(s)

    def test_smoke_cases_cannot_be_swapped_for_easier_ones(self):
        s = selftest.smoke_summary()
        s["cases"]["R01.easier"] = s["cases"].pop(SMOKE_NEGATIVE)
        s["expected"] = list(s["cases"])
        self.refused(s)

    def test_a_full_run_refuses_a_failure_and_an_unexpected_pass(self):
        s = full_summary()
        s["cases"]["A"]["status"] = "fail"
        self.refused(s)
        s = full_summary()
        s["cases"]["B"]["status"] = "unexpected_pass"
        self.refused(s)
        s = full_summary()
        s["required"]["passed"] = 0
        self.refused(s)

    def test_the_summary_keeps_what_the_contract_reads(self):
        report = {"mode": "smoke", "counts": {"pass": 1}, "required": {}, "known_failures": {}, "q3map2_sha256": "0", "expected": ["X"],
                  "cases": [{"id": "X", "status": "pass", "leak": False, "stages": {"bsp": {"exit": 0, "seconds": 1.0}}}]}
        s = regress_summary(report, 0, 1.0)
        self.assertEqual(s["cases"]["X"]["stages"], {"bsp": 0})
        self.assertEqual(s["mode"], "smoke")

    def test_a_tool_that_outlives_its_deadline_is_a_failure_even_unchecked(self):
        with self.assertRaises(Failure):
            run([sys.executable, "-c", "import time; time.sleep(5)"], check=False, capture=True, timeout=0.1)

    def test_a_smoke_tool_bound_is_at_most_a_minute(self):
        self.assertLessEqual(SMOKE_TOOL_SECONDS, 60)


class Publication(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = pathlib.Path(self.tmp.name)
        self.plan = {"commit": "c" * 40, "banner": "b", "assets": {"linux-amd64": "t.zip"}}
        archive = self.root / "t.zip"
        archive.write_bytes(b"archive")
        self.digest = sha256_file(archive)
        self.package = {"commit": "c" * 40, "banner": "b", "sha256": self.digest, "program_sha256": "0" * 64,
                        "test_mode": "smoke", "regress": selftest.smoke_summary()}
        self.report = {"verdict": "pass", "commit": "c" * 40, "banner": "b", "archive_sha256": self.digest, "platform": "linux-amd64",
                       "acceptance_mode": "smoke", "steps": [{"status": "pass"}] * ACCEPTANCE_STEPS, "regress": selftest.smoke_summary()}

    def tearDown(self):
        self.tmp.cleanup()

    def evaluate(self):
        write_json(self.root / "t.package.json", self.package)
        write_json(self.root / "acceptance-linux-amd64.json", self.report)
        return publish.evaluate("linux-amd64", self.plan, self.root)

    def test_a_complete_smoke_acceptance_is_admitted_as_smoke(self):
        record, reason = self.evaluate()
        self.assertIsNone(reason)
        self.assertEqual(record["acceptance"]["mode"], "smoke")
        self.assertEqual(sorted(record["regressions"]["cases"]), sorted(SMOKE_CASES))

    def test_a_missing_report_admits_nothing(self):
        write_json(self.root / "t.package.json", self.package)
        record, reason = publish.evaluate("linux-amd64", self.plan, self.root)
        self.assertIsNone(record)
        self.assertIn("no report", reason)

    def test_partial_acceptance_is_refused(self):
        self.report["steps"] = self.report["steps"][:-1]
        with self.assertRaises(Failure):
            self.evaluate()

    def test_an_unlabelled_acceptance_is_refused(self):
        del self.report["acceptance_mode"]
        with self.assertRaises(Failure):
            self.evaluate()

    def test_a_report_without_regressions_is_refused(self):
        del self.report["regress"]
        with self.assertRaises(Failure):
            self.evaluate()

    def test_a_smoke_report_cannot_be_labelled_full(self):
        self.report["acceptance_mode"] = "full"
        with self.assertRaises(Failure):
            self.evaluate()
        self.report["acceptance_mode"] = "smoke"
        self.package["test_mode"] = "full"
        with self.assertRaises(Failure):
            self.evaluate()

    def test_a_timed_out_case_in_the_report_is_refused(self):
        self.report["regress"]["cases"][SMOKE_POSITIVE]["stages"]["light"] = "timeout"
        with self.assertRaises(Failure):
            self.evaluate()

    def test_notes_claim_only_what_ran(self):
        record, _ = self.evaluate()
        record["sha256"] = self.digest
        plan = {**self.plan, "tag": "t", "version": "v", "upstream": {"repository": "u", "base_commit": "b", "base_tag": "bt"}, "fork_repository": "f",
                "patch_series": []}
        manifest, notes = publish.manifest_and_notes(plan, [record], [], {"name": "s.tar.gz", "path": "x", "rebuilt_and_tested_from_the_archive": False}, "run", {})
        self.assertNotIn("rebuilt and its regressions re-run", notes)
        self.assertNotIn("all required regression cases", notes)
        self.assertIn("did not rebuild it", notes)
        self.assertIn("sanitizers", notes)
        self.assertIn("smoke", manifest["qualification"])
        self.assertNotIn("every declared regression case", manifest["qualification"])

    def test_a_smoke_build_cannot_mark_the_source_rebuilt(self):
        plan = {"source_asset": "s.tar.gz", "commit": "c" * 40}
        write_json(self.root / "s.source.json", {"commit": "c" * 40})
        report = {"commit": "c" * 40, "regress": selftest.smoke_summary(), "platform": "linux-amd64", "program_sha256": "0", "toolchain": {}}
        write_json(self.root / "report.json", report)
        with self.assertRaises(Failure):
            source_archive.mark_rebuilt(plan, self.root / "report.json", self.root)
        report["regress"] = full_summary()
        write_json(self.root / "report.json", report)
        source_archive.mark_rebuilt(plan, self.root / "report.json", self.root)
        self.assertTrue(json.loads((self.root / "s.source.json").read_text())["rebuild_verified"])


class Workflow(unittest.TestCase):
    def test_a_heavy_step_put_back_is_caught(self):
        text = selftest.WORKFLOW.read_text(encoding="utf-8")
        selftest.light_ci(text)
        for label, broken in (("sanitizers", text + "\n  sanitizers:\n    runs-on: x\n"),
                              ("--full", text.replace("--smoke", "--full", 1)),
                              ("mode dropped", text.replace(" --test-mode smoke", "", 1))):
            with self.subTest(label), self.assertRaises(SystemExit):
                selftest.light_ci(broken)


class Runner(unittest.TestCase):
    def test_regress_refuses_a_smoke_run_that_names_its_own_cases(self):
        p = subprocess.run([sys.executable, str(HERE / "regress.py"), "--q3map2", "x", "--work", "y", "--smoke", "--case", "R01.room"],
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        self.assertNotEqual(p.returncode, 0)

    def test_the_declared_smoke_cases_exist_with_their_stages(self):
        cases = {c["id"]: c for c in json.loads((HERE.parent.parent / "regression_tests" / "auto-pigeon" / "cases.json").read_text())["cases"]}
        self.assertTrue(cases[SMOKE_POSITIVE]["expect"]["leak"] is False and tuple(cases[SMOKE_POSITIVE]["stages"]) == SMOKE_CASES[SMOKE_POSITIVE])
        self.assertTrue(cases[SMOKE_NEGATIVE]["expect"]["leak"] is True)
        self.assertTrue(cases[SMOKE_POSITIVE]["probes"], "a smoke positive must assert solid and air, not only a verdict")


if __name__ == "__main__":
    unittest.main()
