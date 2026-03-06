# Run Cangjie Tests

Use this skill when you need to run test cases in `cangjie_test` repository.

## Prerequisites

- Python >= 3.5.2
- `CANGJIE_HOME` environment variable set
- Python packages: `pexpect`, `fasteners`

## Test Framework Entry

```bash
python3 cangjie_test_framework/main.py [options] [test_paths]
```

## Common Command Patterns

### Run Single HLT Test Case

```bash
python3 cangjie_test_framework/main.py \
  --test_cfg=cangjie_test/testsuites/HLT/configs/cjnative/cangjie2cjnative_linux_x86_test.cfg \
  --verbose \
  cangjie_test/testsuites/HLT/compiler/cjnative/Chir/ForIn/for_in_01.cj
```

### Run All HLT Tests

```bash
python3 cangjie_test_framework/main.py \
  --test_cfg=cangjie_test/testsuites/HLT/configs/cjnative/cangjie2cjnative_linux_x86_test.cfg \
  --test_list=cangjie_test/testsuites/HLT/testlist \
  -pFAIL -j20 --timeout=180 \
  cangjie_test/testsuites/HLT/
```

### Run All LLT Tests

```bash
python3 cangjie_test_framework/main.py \
  --test_cfg=cangjie_test/testsuites/LLT/configs/cjnative/cjnative_test.cfg \
  --test_list=cangjie_test/testsuites/LLT/cjnative_testlist,cangjie-ci/scripts/cangjie/ci/test/llt/linux/x64/exclude_cjnative_testlist \
  --fail_exit -pFAIL --retry=3 --debug \
  --fail-verbose -j20 --timeout=180 \
  cangjie_test/testsuites/LLT/
```

### Run LLT cjcov Tests

```bash
python3 cangjie_test_framework/main.py \
  --test_cfg=cangjie_test/testsuites/LLT/Tools/cjcov/configs/test.cfg \
  --test_list=cangjie_test/testsuites/LLT/Tools/cjcov/configs/testlist \
  --fail_exit --retry=3 --debug \
  --fail-verbose -pFAIL -j20 --timeout=180 \
  cangjie_test/testsuites/LLT/Tools/cjcov
```

### Run Both HLT and LLT

First prepare config files:
```bash
cp cangjie_test/testsuites/HLT/configs/cjnative/cangjie2cjnative_linux_x86_test.cfg cangjie_test/testsuites/HLT/test.cfg
cp cangjie_test/testsuites/LLT/configs/cjnative/cjnative_test.cfg cangjie_test/testsuites/LLT/test.cfg
```

Update `[root] path = ./` in both config files, then:
```bash
python3 cangjie_test_framework/main.py -pFAIL -j20 --timeout=180
```

## Key Parameters

- `--test_cfg <file>`: Test suite config file (required for single case or with --test_list)
- `--test_list <file1>,<file2>`: Comma-separated test list files (inclusion and exclusion lists)
- `-j <num>`: Run tests in parallel (e.g., `-j20`)
- `--timeout <seconds>`: Timeout per test case (e.g., `--timeout=180`)
- `-pFAIL`: Print only failed tests
- `-pPASS`: Print only passed tests
- `--verbose`: Enable verbose output for all tests
- `--fail-verbose`: Enable verbose output only for failed tests
- `--retry <num>`: Retry failed tests (e.g., `--retry=3`)
- `--fail_exit`: Exit with non-zero code if any test fails
- `--debug`: Keep only failed test temp files
- `--keep_temp`: Keep all test temp files

## Config File Selection (Linux x86_64)

HLT configs:
- Default: `cangjie_test/testsuites/HLT/configs/cjnative/cangjie2cjnative_linux_x86_test.cfg`
- O2: `cangjie_test/testsuites/HLT/configs/cjnative/cangjie2cjnative_linux_x86_test_O2.cfg`
- Debug: `cangjie_test/testsuites/HLT/configs/cjnative/cangjie2cjnative_linux_x86_test_g.cfg`

LLT configs:
- Default: `cangjie_test/testsuites/LLT/configs/cjnative/cjnative_test.cfg`
- cjcov: `cangjie_test/testsuites/LLT/Tools/cjcov/configs/test.cfg`

Other platforms: Replace `linux_x86` with `linux_arm`, `mac_x86`, `mac_arm`, or `win`.

## Test Case Structure

Test cases are `.cj` files with execution directives in comments:

```cj
// EXEC: %compiler %cmp_opt %f -o %output
// RUN-EXEC: %run %run_opt %output %run_args

main() {
    return 0
}
```

Built-in variables:
- `%f`: Current file name
- `%n`: File name without extension
- `%compiler`: Compiler path (from config)
- `%output`: Output file name

Built-in commands:
- `EXEC`: Execute command (expects exit code 0)
- `EXEC-NUM`: Execute command (expects exit code NUM)
- `RUN-EXEC`: Run with configured script
- `ERRCHECK`: Equivalent to `EXEC-PIPE-1: <cmd> 2>&1 | compare %f`
- `DEPENDENCE`: Declare dependency files
- `ASSERT`: Result verification

## Troubleshooting

- Use `--verbose` or `--fail-verbose` for detailed output
- Use `--debug` or `--keep_temp` to preserve temp files
- Check `CANGJIE_HOME` is set correctly
- Verify all dependencies are installed
- Check logs in `../test_temp/log/`

## LSP Tests Special Setup

Before running LSP tests:
```bash
cp -r ${CANGJIE_HOME}/modules ${CANGJIE_HOME}/tools/bin
```

Then update `lsp_server` path in `cangjie_test/testsuites/HLT/Tools/cjlsp/lsp_config.txt`.
