# Test coverage gaps

Notes from a 2026-07-31 audit pass, after fixing the memory-safety/correctness issues found in
`src/*.c` (handle-struct leak, `snprintf`/`vsnprintf` stack over-reads, `rawStream()` fd
double-ownership, `Perfidious\open()` not bailing out on a type error). These are follow-up ideas
for test coverage that weren't specific bug fixes, roughly in priority order.

1. **Get an actual coverage report instead of guessing further.** The flake already builds
   `php81-gcc-coverage` / `php81-gcc-debug-coverage`, and `codecov.yml` exists. Building one of
   those and reading the HTML report (`genhtml`) would show real uncovered lines/branches instead
   of continuing to speculate from reading the source.

2. **`Perfidious\open([])`** - opening with zero event names (just the dummy counter) isn't
   tested. Worth checking `read()`/`readArray()` behave sanely (empty `values` array) rather than
   crashing.

3. **Readonly / no-dynamic-properties enforcement** on `PmuInfo`, `PmuEventInfo`, `ReadResult`.
   They're declared `ZEND_ACC_READONLY` / `ZEND_ACC_NO_DYNAMIC_PROPERTIES` in the C registration
   code, but nothing asserts `$info->name = "x"` or `$info->foo = 1` actually throws. Cheap
   insurance against someone accidentally dropping the flag later.

4. **`rawStream(-1)`** - only the over-large (`PHP_INT_MAX`) out-of-bounds index is tested
   (`tests/handle/raw-stream-invalid-idx.phpt`); a negative index takes a different path through
   the `(size_t)` cast in `PerfidousHandle::rawStream()` and isn't separately verified.

5. **`Handle::debugCloseFd()` exercised directly** against `read()`/`readArray()`/`enable()`, not
   just indirectly through `phpinfo()` in `tests/info-global-closed.phpt` /
   `tests/info-request-closed.phpt`. Would pin down the resulting `IOException` messages more
   precisely.
