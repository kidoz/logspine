# Tooling

## Formatting

The repository ships an LLVM-based `.clang-format` file. When `clang-format` is installed, Meson exposes:

```bash
meson compile -C build format
meson compile -C build format-check
```

## Static Analysis

The repository ships `.clang-tidy` with broad bug-prone, analyzer, modernization, performance, portability, and readability coverage. When `clang-tidy` is installed, Meson exposes:

```bash
meson compile -C build clang-tidy
```

## Developer Mode

`-Ddeveloper_mode=true` enables the strict warning policy configured in `meson.build`. Warnings-as-errors can be disabled separately through Meson's built-in `-Dwerror=false`.
