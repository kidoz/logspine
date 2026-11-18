# Run Formatting and Static Analysis

The repository ships `.clang-format` and `.clang-tidy`. Meson exposes the
formatting and analysis targets only when the corresponding tool is installed.

## Format sources

```bash
meson compile -C build format
```

This rewrites the formatting targets in place.

## Check formatting without writing

```bash
meson compile -C build format-check
```

Use this in review and CI. It fails when a file would be reformatted.

## Run clang-tidy

```bash
meson compile -C build clang-tidy
```

clang-tidy runs against the library sources using the build directory's compile
database.

## Enable strict warnings

```bash
meson setup build -Ddeveloper_mode=true
meson compile -C build
```

Developer mode adds the project's strict `-Werror` flag. Warnings are errors by
default because the project sets `werror=true`; configure with `-Dwerror=false`
to keep them non-fatal.
