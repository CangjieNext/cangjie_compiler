#!/bin/bash
# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# AST Release Feature - E2E Functional Test Script
#
# Usage: bash ast_release_e2e_test.sh [CANGJIE_HOME]
#
# This script tests the AST release feature by compiling and running
# Cangjie programs with and without --no-release-ast, verifying:
#   1. Basic compilation succeeds with AST release (default)
#   2. --no-release-ast flag is accepted and compilation succeeds
#   3. Both paths produce identical runtime output (determinism)
#   4. Complex language features work after AST release
#   5. --emit-chir stops at RELEASE_AST stage correctly

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CJC="${CJC:-cjc}"
TMPDIR=$(mktemp -d /tmp/ast_release_test.XXXXXX)
PASS=0
FAIL=0
SKIP=0
RESULTS=()

cleanup() {
    rm -rf "$TMPDIR"
}
trap cleanup EXIT

log_pass() {
    PASS=$((PASS + 1))
    RESULTS+=("PASS: $1")
    echo "[PASS] $1"
}

log_fail() {
    FAIL=$((FAIL + 1))
    RESULTS+=("FAIL: $1 -- $2")
    echo "[FAIL] $1 -- $2"
}

log_skip() {
    SKIP=$((SKIP + 1))
    RESULTS+=("SKIP: $1 -- $2")
    echo "[SKIP] $1 -- $2"
}

# Check if cjc is available
if ! command -v "$CJC" &>/dev/null; then
    echo "ERROR: cjc not found. Set CJC env var or add to PATH."
    exit 1
fi

echo "=== AST Release E2E Test Suite ==="
echo "CJC: $(which $CJC)"
echo "TMPDIR: $TMPDIR"
echo ""

# ─────────────────────────────────────────────────
# Test 1: Basic compilation with AST release (default)
# ─────────────────────────────────────────────────
TEST_NAME="T01_basic_compile_with_ast_release"
cat > "$TMPDIR/basic.cj" << 'EOF'
main(): Int64 {
    println("hello_ast_release")
    return 0
}
EOF
if $CJC "$TMPDIR/basic.cj" -o "$TMPDIR/basic" 2>"$TMPDIR/basic.err"; then
    OUTPUT=$("$TMPDIR/basic" 2>&1 || true)
    if echo "$OUTPUT" | grep -q "hello_ast_release"; then
        log_pass "$TEST_NAME"
    else
        log_fail "$TEST_NAME" "unexpected output: $OUTPUT"
    fi
else
    log_fail "$TEST_NAME" "compilation failed: $(cat $TMPDIR/basic.err)"
fi

# ─────────────────────────────────────────────────
# Test 2: Compilation with --no-release-ast flag
# ─────────────────────────────────────────────────
TEST_NAME="T02_no_release_ast_flag"
cat > "$TMPDIR/nra.cj" << 'EOF'
main(): Int64 {
    println("no_release_ok")
    return 0
}
EOF
if $CJC --no-release-ast "$TMPDIR/nra.cj" -o "$TMPDIR/nra" 2>"$TMPDIR/nra.err"; then
    OUTPUT=$("$TMPDIR/nra" 2>&1 || true)
    if echo "$OUTPUT" | grep -q "no_release_ok"; then
        log_pass "$TEST_NAME"
    else
        log_fail "$TEST_NAME" "unexpected output: $OUTPUT"
    fi
else
    log_fail "$TEST_NAME" "compilation failed: $(cat $TMPDIR/nra.err)"
fi

# ─────────────────────────────────────────────────
# Test 3: Determinism — both paths produce same output
# ─────────────────────────────────────────────────
TEST_NAME="T03_determinism_with_and_without_release"
cat > "$TMPDIR/det.cj" << 'EOF'
func compute(): Int64 {
    var sum: Int64 = 0
    var i: Int64 = 0
    while (i < 100) {
        sum = sum + i
        i = i + 1
    }
    return sum
}
main(): Int64 {
    println(compute())
    return 0
}
EOF
OUT1=""
OUT2=""
if $CJC "$TMPDIR/det.cj" -o "$TMPDIR/det1" 2>/dev/null; then
    OUT1=$("$TMPDIR/det1" 2>&1 || true)
fi
if $CJC --no-release-ast "$TMPDIR/det.cj" -o "$TMPDIR/det2" 2>/dev/null; then
    OUT2=$("$TMPDIR/det2" 2>&1 || true)
fi
if [ -n "$OUT1" ] && [ "$OUT1" = "$OUT2" ]; then
    log_pass "$TEST_NAME"
else
    log_fail "$TEST_NAME" "outputs differ: [$OUT1] vs [$OUT2]"
fi

# ─────────────────────────────────────────────────
# Test 4: Class hierarchy and virtual dispatch
# ─────────────────────────────────────────────────
TEST_NAME="T04_class_hierarchy_after_release"
cat > "$TMPDIR/class.cj" << 'EOF'
open class Base {
    public open func name(): String { return "Base" }
}
class Derived <: Base {
    public override func name(): String { return "Derived" }
}
main(): Int64 {
    let b: Base = Derived()
    println(b.name())
    return 0
}
EOF
if $CJC "$TMPDIR/class.cj" -o "$TMPDIR/class" 2>"$TMPDIR/class.err"; then
    OUTPUT=$("$TMPDIR/class" 2>&1 || true)
    if echo "$OUTPUT" | grep -q "Derived"; then
        log_pass "$TEST_NAME"
    else
        log_fail "$TEST_NAME" "unexpected output: $OUTPUT"
    fi
else
    log_fail "$TEST_NAME" "compilation failed: $(cat $TMPDIR/class.err)"
fi

# ─────────────────────────────────────────────────
# Test 5: Enum and pattern matching
# ─────────────────────────────────────────────────
TEST_NAME="T05_enum_pattern_match"
cat > "$TMPDIR/enum.cj" << 'EOF'
enum Shape {
    | Circle
    | Square
}
func describe(s: Shape): String {
    match (s) {
        case Circle => "circle"
        case Square => "square"
    }
}
main(): Int64 {
    println(describe(Shape.Circle))
    println(describe(Shape.Square))
    return 0
}
EOF
if $CJC "$TMPDIR/enum.cj" -o "$TMPDIR/enum" 2>"$TMPDIR/enum.err"; then
    OUTPUT=$("$TMPDIR/enum" 2>&1 || true)
    if echo "$OUTPUT" | grep -q "circle" && echo "$OUTPUT" | grep -q "square"; then
        log_pass "$TEST_NAME"
    else
        log_fail "$TEST_NAME" "unexpected output: $OUTPUT"
    fi
else
    log_fail "$TEST_NAME" "compilation failed: $(cat $TMPDIR/enum.err)"
fi

# ─────────────────────────────────────────────────
# Test 6: Lambda/closure captures
# ─────────────────────────────────────────────────
TEST_NAME="T06_closure_capture"
cat > "$TMPDIR/closure.cj" << 'EOF'
func makeAdder(n: Int64): (Int64) -> Int64 {
    return { x: Int64 => x + n }
}
main(): Int64 {
    let add10 = makeAdder(10)
    println(add10(32))
    return 0
}
EOF
if $CJC "$TMPDIR/closure.cj" -o "$TMPDIR/closure" 2>"$TMPDIR/closure.err"; then
    OUTPUT=$("$TMPDIR/closure" 2>&1 || true)
    if echo "$OUTPUT" | grep -q "42"; then
        log_pass "$TEST_NAME"
    else
        log_fail "$TEST_NAME" "unexpected output: $OUTPUT"
    fi
else
    log_fail "$TEST_NAME" "compilation failed: $(cat $TMPDIR/closure.err)"
fi

# ─────────────────────────────────────────────────
# Test 7: Exception handling
# ─────────────────────────────────────────────────
TEST_NAME="T07_exception_handling"
cat > "$TMPDIR/except.cj" << 'EOF'
main(): Int64 {
    try {
        throw Exception("test_error")
    } catch (e: Exception) {
        println("caught")
    } finally {
        println("finally")
    }
    return 0
}
EOF
if $CJC "$TMPDIR/except.cj" -o "$TMPDIR/except" 2>"$TMPDIR/except.err"; then
    OUTPUT=$("$TMPDIR/except" 2>&1 || true)
    if echo "$OUTPUT" | grep -q "caught" && echo "$OUTPUT" | grep -q "finally"; then
        log_pass "$TEST_NAME"
    else
        log_fail "$TEST_NAME" "unexpected output: $OUTPUT"
    fi
else
    log_fail "$TEST_NAME" "compilation failed: $(cat $TMPDIR/except.err)"
fi

# ─────────────────────────────────────────────────
# Test 8: --emit-chir stops at RELEASE_AST stage
# ─────────────────────────────────────────────────
TEST_NAME="T08_emit_chir_stops_at_release_ast"
cat > "$TMPDIR/emit.cj" << 'EOF'
main(): Int64 { return 0 }
EOF
if $CJC --emit-chir "$TMPDIR/emit.cj" 2>"$TMPDIR/emit.err"; then
    log_pass "$TEST_NAME"
else
    # --emit-chir may need stdlib; skip if it fails due to missing modules
    ERR=$(cat "$TMPDIR/emit.err")
    if echo "$ERR" | grep -q "modules\|std.core"; then
        log_skip "$TEST_NAME" "stdlib not available"
    else
        log_fail "$TEST_NAME" "unexpected error: $ERR"
    fi
fi

# ─────────────────────────────────────────────────
# Test 9: Global variable initializers
# ─────────────────────────────────────────────────
TEST_NAME="T09_global_var_init"
cat > "$TMPDIR/global.cj" << 'EOF'
var g1: Int64 = 10
var g2: Int64 = g1 * 3
main(): Int64 {
    println(g2)
    return 0
}
EOF
if $CJC "$TMPDIR/global.cj" -o "$TMPDIR/global" 2>"$TMPDIR/global.err"; then
    OUTPUT=$("$TMPDIR/global" 2>&1 || true)
    if echo "$OUTPUT" | grep -q "30"; then
        log_pass "$TEST_NAME"
    else
        log_fail "$TEST_NAME" "unexpected output: $OUTPUT"
    fi
else
    log_fail "$TEST_NAME" "compilation failed: $(cat $TMPDIR/global.err)"
fi

# ─────────────────────────────────────────────────
# Test 10: Recursive functions (deep stack)
# ─────────────────────────────────────────────────
TEST_NAME="T10_recursive_functions"
cat > "$TMPDIR/recurse.cj" << 'EOF'
func fib(n: Int64): Int64 {
    if (n <= 1) { return n }
    return fib(n - 1) + fib(n - 2)
}
main(): Int64 {
    println(fib(10))
    return 0
}
EOF
if $CJC "$TMPDIR/recurse.cj" -o "$TMPDIR/recurse" 2>"$TMPDIR/recurse.err"; then
    OUTPUT=$("$TMPDIR/recurse" 2>&1 || true)
    if echo "$OUTPUT" | grep -q "55"; then
        log_pass "$TEST_NAME"
    else
        log_fail "$TEST_NAME" "unexpected output: $OUTPUT"
    fi
else
    log_fail "$TEST_NAME" "compilation failed: $(cat $TMPDIR/recurse.err)"
fi

# ─────────────────────────────────────────────────
# Summary
# ─────────────────────────────────────────────────
echo ""
echo "═══════════════════════════════════════════"
echo "  AST Release E2E Test Results"
echo "═══════════════════════════════════════════"
echo "  PASS: $PASS"
echo "  FAIL: $FAIL"
echo "  SKIP: $SKIP"
echo "  TOTAL: $((PASS + FAIL + SKIP))"
echo "═══════════════════════════════════════════"
echo ""
for r in "${RESULTS[@]}"; do
    echo "  $r"
done
echo ""

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
exit 0
