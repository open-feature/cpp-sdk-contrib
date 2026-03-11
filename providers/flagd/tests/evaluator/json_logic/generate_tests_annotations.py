#!/usr/bin/env python3
"""
JsonLogic Test Annotator and Filter.

This script processes the official JsonLogic 'tests.json' file to:
1. Filter out tests that use unsupported or "non-strict" operators (e.g., '==', 'log').
2. Apply strict type checking rules (e.g., numeric operators must have numeric arguments).
3. Group the remaining valid tests by category and operator.
4. Output the results to 'tests_annotated.json'.

Usage:
    python3 generate_tests_annotations.py

The script automatically locates 'tests.json' relative to its own location.
"""
import json
import os

# Operators configuration
STRICT_OPS = {
    "===": "Equality",
    "!==": "Equality",
    "==": "Equality",
    "!=": "Equality",
    ">": "Numeric",
    ">=": "Numeric",
    "<": "Numeric",
    "<=": "Numeric",
    "+": "Numeric",
    "-": "Numeric",
    "*": "Numeric",
    "/": "Numeric",
    "%": "Numeric",
    "min": "Numeric",
    "max": "Numeric",
    "!": "Logic",
    "!!": "Logic",
    "or": "Logic",
    "and": "Logic",
    "?:": "Logic",
    "if": "Logic",
    "cat": "String",
    "substr": "String",
    "in": "String/Array",
    "merge": "Array",
    "var": "Data",
    "missing": "Data",
    "missing_some": "Data",
}

IGNORED_OPS = {
    "map",
    "reduce",
    "filter",
    "all",
    "some",
    "none",
    "log",
}


def is_literal(x):
    return isinstance(x, (int, float, str, bool, type(None)))


def get_operator(logic):
    if isinstance(logic, dict) and len(logic) > 0:
        return list(logic.keys())[0]
    return None


def get_args(logic, op):
    args = logic[op]
    if not isinstance(args, list):
        return [args]
    return args


def resolve_var(data, var_name):
    if var_name == "" or var_name is None:
        return data
    if not isinstance(data, (dict, list)):
        return None

    parts = str(var_name).split(".")
    curr = data
    for p in parts:
        if isinstance(curr, dict) and p in curr:
            curr = curr[p]
        elif isinstance(curr, list) and p.isdigit():
            idx = int(p)
            if 0 <= idx < len(curr):
                curr = curr[idx]
            else:
                return None
        else:
            return None
    return curr


def recursive_scan(node, data):
    """
    Scans the logic tree recursively.
    Returns exclusion_reason (str) or None.
    """
    if not isinstance(node, dict) or not node:
        return None

    op = get_operator(node)
    if not op:
        return None

    if op in IGNORED_OPS:
        return f"Uses ignored operator '{op}'"

    if op not in STRICT_OPS:
        return f"Uses unknown operator '{op}'"

    args = get_args(node, op)

    # Resolve simple vars for type checking
    resolved_args = []
    for arg in args:
        if isinstance(arg, dict) and get_operator(arg) == "var":
            v_args = get_args(arg, "var")
            if isinstance(v_args, list) and len(v_args) > 0:
                v_name = v_args[0]
            else:
                v_name = v_args

            if isinstance(v_name, str):
                val = resolve_var(data, v_name)
                if val is None and isinstance(v_args, list) and len(v_args) > 1:
                    val = v_args[1]
                resolved_args.append(val)
            else:
                resolved_args.append(arg)  # Can't resolve or invalid key type
        else:
            resolved_args.append(arg)

    # --- Strict Check for Current Operator ---

    # Numeric Strictness
    if op in ["+", "-", "*", "/", "%", "min", "max", "<", "<=", ">", ">="]:
        for arg in resolved_args:
            if (
                is_literal(arg)
                and not isinstance(arg, (int, float))
                and arg is not None
            ):
                return f"Operator '{op}' has non-numeric literal argument: {arg} (type: {type(arg).__name__})"
            if arg is None:
                return f"Operator '{op}' has null argument"

    # String Strictness (cat)
    if op == "cat":
        for arg in resolved_args:
            if is_literal(arg) and not isinstance(arg, str):
                return f"Operator 'cat' has non-string literal argument: {arg}"

    # Substr strictness
    if op == "substr":
        if len(resolved_args) > 0:
            if is_literal(resolved_args[0]) and not isinstance(resolved_args[0], str):
                return f"Operator 'substr' first argument must be string, got {resolved_args[0]}"
        if len(resolved_args) > 1:
            if is_literal(resolved_args[1]) and not isinstance(resolved_args[1], int):
                return f"Operator 'substr' second argument must be integer, got {resolved_args[1]}"
        if len(resolved_args) > 2:
            if is_literal(resolved_args[2]) and not isinstance(resolved_args[2], int):
                return f"Operator 'substr' third argument must be integer, got {resolved_args[2]}"

    # Logic Strictness (if/?:/and/or/!/!!)
    if op in ["if", "?:", "and", "or", "!", "!!"]:
        pass

    # Equality strictness
    if op in ["==", "!="]:
        if len(resolved_args) == 2:
            left, right = resolved_args[0], resolved_args[1]
            if is_literal(left) and is_literal(right):
                if type(left) is not type(right):
                    # Allow int/float cross-comparison as both are numeric
                    if not (
                        isinstance(left, (int, float))
                        and isinstance(right, (int, float))
                    ):
                        return f"Operator '{op}' has mismatched types: {type(left).__name__} and {type(right).__name__}"

    # In strictness
    if op == "in":
        if len(resolved_args) == 2:
            sub = resolved_args[0]
            container = resolved_args[1]
            if is_literal(container) and isinstance(container, str):
                if is_literal(sub) and not isinstance(sub, str):
                    return f"Operator 'in' (string) has non-string member: {sub}"

    # Var strictness
    if op == "var":
        v_args = get_args(node, "var")
        v_name = v_args[0] if isinstance(v_args, list) and len(v_args) > 0 else v_args
        if (
            is_literal(v_name)
            and not isinstance(v_name, str)
            and v_name != ""
            and not v_name == None
        ):
            return f"Operator 'var' has non-string key: {v_name} (type: {type(v_name).__name__})"

    # --- Recursive Step ---
    for arg in args:
        if isinstance(arg, dict):
            reason = recursive_scan(arg, data)
            if reason:
                return reason

    return None


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    path = os.path.join(script_dir, "tests.json")
    if not os.path.exists(path):
        print(f"File not found: {path}")
        return

    with open(path, "r") as f:
        tests = json.load(f)

    valid_tests = [t for t in tests if isinstance(t, list) and len(t) >= 3]

    grouped_tests = {}
    skipped_details = {}

    for t in valid_tests:
        logic = t[0]
        data = t[1]
        expected = t[2]

        main_op = get_operator(logic)

        reason = recursive_scan(logic, data)
        if not reason:
            # Check expected value for boolean operators
            if main_op in [
                "!",
                "!!",
                "<",
                "<=",
                ">",
                ">=",
                "===",
                "!==",
                "==",
                "!=",
            ]:
                if not isinstance(expected, bool):
                    reason = f"Operator '{main_op}' expected to return non-boolean: {expected}"

        if reason:
            if reason not in skipped_details:
                skipped_details[reason] = []
            skipped_details[reason].append(t)
            continue

        if not main_op:
            main_op = "primitive"

        category = STRICT_OPS.get(main_op, "Unknown")
        group_key = f"{category}/{main_op}"
        if group_key not in grouped_tests:
            grouped_tests[group_key] = []
        grouped_tests[group_key].append(t)

    # Generate config
    output_path = os.path.join(script_dir, "tests_annotated.json")
    
    # We add skipped details to the JSON root but as an object (not array)
    # so the C++ test loader (which expects arrays for groups) will skip it.
    final_output = grouped_tests.copy()
    final_output["__skipped__"] = skipped_details

    with open(output_path, "w") as f:
        json.dump(final_output, f, indent=2)

    print(f"Generated {output_path}.")
    print(f"Included {sum(len(v) for v in grouped_tests.values())} tests.")
    print(
        f"Skipped {len(valid_tests) - sum(len(v) for v in grouped_tests.values())} tests."
    )

    print("\nSkipped Reasons Summary:")
    for r, tests_list in sorted(skipped_details.items(), key=lambda x: -len(x[1])):
        print(f"\n- {len(tests_list)} tests: {r}")
        for st in tests_list:
            print(f"    Logic: {json.dumps(st[0])}  Expected: {json.dumps(st[2])}")


if __name__ == "__main__":
    main()
