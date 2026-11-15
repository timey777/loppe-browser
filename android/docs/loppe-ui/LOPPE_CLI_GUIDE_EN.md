# Building and Running a LOPPE Project with loppe-cli

> 📖 [中文版](LOPPE_CLI_GUIDE.md) | English

---

## 1. What This Document Covers

This document covers how to build a LOPPE project, how to validate the build script without compiling, where the output goes, and where every argument comes from. The command is `loppe-cli`.

**Audience**: people — and AI assistants — working in an existing LOPPE project (created by the IDE or hand-written `build.lpj`).

**Prerequisites**

| Item | Requirement |
|---|---|
| `loppe-cli.exe` | Ships with the IDE installer, in the same directory as `loppe.exe` |
| Project file | The project root has `build.lpj` (the interface file may live anywhere the project decides) |
| Toolchain | Desktop projects use the `toolchain/` inside the IDE directory; Android / ESP32 projects declare toolchain paths at the top of `build.lpj` |

**Contents**

| What you want to do | Section |
|---|---|
| Just build and run | §2 |
| Know what each argument does | §3 |
| Validate `build.lpj` with the tool | §4 |
| Find the output, rebuild | §5 |
| Tell which approaches do not work | §6 |
| Read build errors | §7 |

---

## 2. Building and Running

In a terminal at the project root:

```bash
loppe-cli build .
```

- Omitting the project path means the current directory; omitting the configuration means `Debug`.
- Output goes to `<project root>/build/Debug/`; for desktop projects the executable name is the `exe <name>` declared in `build.lpj`.
- Desktop: run the produced `.exe` directly. Android: the output is `app/build/apk/app-debug.apk`; install and read logs with `adb`. ESP32: the target-level `build` in `build.lpj` runs `idf.py build`, and output stays in the project's own `build/`.

A Release build:

```bash
loppe-cli build . Release
```

In Release, desktop projects use `subsystem = windows` (no console window); Debug keeps the console for logs. That difference lives in the project's `build.lpj`.

---

## 3. Arguments

```bash
loppe-cli build [<project dir|build.lpj path>] [Debug|Release] [--targets=a,b,c] [--out=<dir>] [--toolchain=<name>]
loppe-cli config [<project dir|build.lpj path>] [Debug|Release] [--targets=a,b,c] [--out=<dir>] [--toolchain=<name>]
loppe-cli help
```

| Argument | Meaning |
|---|---|
| Project path | First positional argument: a project directory or the path of `build.lpj`. Omitted = current directory |
| Configuration | `Debug` or `Release`, at any position. Omitted = `Debug` |
| `--targets=a,b,c` | Build only these targets (names from `exe <name>` / `lib <name>` in `build.lpj`). Omitted = all targets |
| `--out=<dir>` | Root of build output; actual artifacts land in `<out>/<config>/`. Omitted = `<project root>/build` |
| `--toolchain=<name>` | Name of a built-in toolchain of the builder (currently only `windows-x64`); other platforms declare `toolchain <root>` in `build.lpj` |
| `--help` / `-h` | Print help |

Exit code `0` means success; non-`0` means failure (script did not parse, compile error, or link error).

```bash
loppe-cli build . Debug --targets=myapp
loppe-cli build D:/work/myproj Release --out=D:/work/out
loppe-cli build template/desktop Release        # build a project template directory directly
```

---

## 4. Validate Without Compiling

`config` follows the same pipeline as `build` and stops once the build manifest has been generated: it parses `build.lpj`, expands paths and globs, checks target names, prints the manifest, and does not invoke a compiler. Use it after editing `build.lpj`:

```bash
loppe-cli config .
loppe-cli config . Debug --targets=myapp
```

Missing `build.lpj` at the project root, undefined variables, malformed arrays — all reported here, with line numbers.

---

## 5. Output and Rebuilding

| Configuration | Output location (default `--out`) |
|---|---|
| `Debug` | `<project root>/build/Debug/` |
| `Release` | `<project root>/build/Release/` |

- Builds are incremental: only changed sources and affected edges are rebuilt.
- Deleting `build/` is allowed; the next build is just a full one.
- Desktop projects can keep `ui/main.lui` next to the executable (or up to two levels above it), so text and style edits do not need a rebuild. The lookup order is decided by the project's own loading code.

---

## 6. Approaches That Do Not Work

| Approach | Why not |
|---|---|
| Building with `cmake`, `make`, `msbuild`, or `cl.exe` directly | Projects have no `CMakeLists.txt` and no `.sln`; the only build description is `build.lpj` |
| Typing a `clang++` command by hand | Toolchain paths, includes, libraries and `-D` flags live in `build.lpj` and the IDE's platform configuration; a hand-written line always misses some |
| Adding a `CMakeLists.txt` as a second build path | The builder ignores it, and two manifests drift apart — editing one leaves the other stale |
| Passing `build` and friends to `loppe.exe` | `loppe.exe` is the GUI and does not parse command lines; it opens its window regardless. The command-line entry point is `loppe-cli.exe` only |
| Running `loppe-cli build` inside a sub-project directory of a repository | Sub-projects share variables defined by the root `build.lpj`; building there fails with "undefined variable 'pkg'". In a repository, build by target instead (see the project's own notes) |

---

## 7. Reading Build Output

Build output is normalized to one line per diagnostic, in VS format:

```
src/main.cpp(31,5): error: use of undeclared identifier 'foo'
```

- Paths inside the project are shown relative to the **project root**.
- A failed edge is reported as `[loppe] FAILED: src/main.cpp (target myapp)`; read the following `error:` line for the cause.
- Source snippets, `^` markers, and `N errors generated.` are preserved as-is.

---

## Related Documents

| Document | Content |
|---|---|
| `LOPPE_GETTING_STARTED.md` | Wiring the engine into C++, the first interface |
| `LOPPE_DSL_USER_GUIDE.md` | `.lui` syntax, selectors, style and layout properties |
| `LOPPE_BUILTIN_CONTROLS.md` | Built-in controls: properties, default styles, events, runtime API |
| `ai/LOPPE_DSL_AI_REFERENCE.md` | Full property table and key differences from standard CSS (for AI) |
