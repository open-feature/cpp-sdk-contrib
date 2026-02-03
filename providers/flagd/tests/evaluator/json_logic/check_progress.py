#!/usr/bin/env python3
import subprocess
import re
import sys

def run_tests():
    # Run the bazel test command and capture output
    cmd = [
        "gbazelisk", "test", 
        "//providers/flagd/tests/evaluator/json_logic:json_logic_official_test", 
        "--test_output=all", 
        "--cache_test_results=no",
        "--keep_going"
    ]
    
    # Run command, capturing stdout and stderr
    result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)

    # Return code: 0 - all tests passed, 3 - execution passed but not all testcases
    if result.returncode != 0 and result.returncode != 3:
        raise Exception("gbazelisk command failed to run:\n" + result.stdout)
    
    return result.stdout

def parse_output(output):
    # Look for [  PASSED  ] X tests
    # Look for [  FAILED  ] Y tests

    passed_pattern = r"(?:\[\s+PASSED\s+\])\s+(\d+)\s+tests"
    failed_pattern = r"(?:\[\s+FAILED\s+\])\s+(\d+)\s+tests"
    
    passed_match = re.search(passed_pattern, output)
    failed_match = re.search(failed_pattern, output)
    
    passed = int(passed_match.group(1)) if passed_match else 0
    failed = int(failed_match.group(1)) if failed_match else 0
    
    return passed, failed

def main():
    print("Running tests...")
    output = run_tests()
    
    passed, failed = parse_output(output)
    total = passed + failed
    
    print("\n" + "="*40)
    print("Test Progress Report")
    print("="*40)
    print(f"Total Tests: {total}")
    print(f"Passing:     {passed}")
    print(f"Failing:     {failed}")
    
    if total > 0:
        percent = (passed / total) * 100
        print(f"Success Rate: {percent:.1f}%")
    else:
        print("No tests found or execution failed completely.")
        
    print("="*40)

if __name__ == "__main__":
    main()
