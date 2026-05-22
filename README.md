# tsf-devtool

Development tools of a Test Agent, packaged as an external Test
Environment (TE) repository and consumed with the `TE_EXT_REPO` builder
directive.

A test suite needs these when the thing under test has to be built on
the agent rather than shipped to it: a kernel module compiled against
the kernel running there, a helper program built with the agent's own
toolchain, a feature probe that answers "do these headers have it" by
compiling three lines instead of parsing a version string.

Library:

- `tapi_devtool` — engine-side TAPIs, built as a shared library:
  - `tapi_cc` — C and C++ compilers;
  - `tapi_make` — `make`;
  - `tapi_kbuild` — out-of-tree kernel modules (kbuild);
  - `tapi_devtool.h` — the toolchain description and the output type
    they share.

Everything runs through `tapi_job`, so the tools run on the agent host
— local or remote, the test does not change — and no part of this
library is installed on the agent side.

The factory must be an **RPC** factory (`tapi_job_factory_rpc_create()`,
i.e. `ta_rpcprovider` on the agent, declared with `TE_TA_APP` in
`builder.conf`). These TAPIs read what the tool printed, which needs
output channels and filters, and only the RPC factory implements them;
with a CFG factory the job fails to create with `TE_EOPNOTSUPP`.

## Usage

Declare the repository in an external libraries catalog (e.g.
`ext-libs.yml` in the test suite conf directory) and pass it to
`dispatcher.sh --ext-libs=ext-libs.yml`:

```yaml
repositories:
  - name: tsf_devtool
    url: <repo URL>
    ref: <tag>
    libs:
      - tapi_devtool
```

Then bind the library to the engine platform in `builder.conf`:

```
TE_EXT_REPO_USE([tsf_devtool], [], [tapi_devtool])
```

and add it to the `te_libs` list of the suite's `meson.build`:

```meson
te_libs = [
    'tapi',
    'tapi_job',
    'tapi_devtool',
    ...
]
```

Requires TE with `TE_EXT_REPO` support.

## The three TAPIs

All three share one lifecycle, the one TE's own tool TAPIs use:
`_create()`, `_start()`, `_wait()`, `_destroy()`, with `_do()` as a
create-start-wait shortcut and `_get_output()` to look at what the tool
printed. Both output streams are always captured and always logged, and
the diagnostics of a failed build are logged as an error with the
command line that produced them — that text is usually the whole answer
when a build breaks on one agent and not on another.

### tapi_cc — compilers

```c
static const char *sources[] = { "main.c", "helper.c" };
static const char *libs[] = { "m" };
tapi_cc_opt opt = tapi_cc_default_opt;
tapi_cc_app *app = NULL;

opt.sources = sources;
opt.n_sources = TE_ARRAY_LEN(sources);
opt.libs = libs;
opt.n_libs = TE_ARRAY_LEN(libs);
opt.output = "app";
opt.warn_all = true;
opt.warn_error = true;
opt.workdir = src_dir;

CHECK_RC(tapi_cc_do(factory, &opt, TAPI_DEVTOOL_TIMEOUT_MS, &app));
...
CLEANUP_CHECK_RC(tapi_cc_destroy(app));
```

The option set is the one `cc`, `gcc` and `clang` agree on, so any of
them can be named in `tapi_cc_opt::compiler`; `c++` is selected by
`lang`.

`tapi_cc_check_snippet()` is the feature probe: it writes a snippet to
the agent, compiles it, removes both files and tells you whether it
built. A test that branches on what the agent's headers actually have
does not have to guess from a kernel or distribution version.

```c
rc = tapi_cc_check_snippet(factory, NULL, TAPI_DEVTOOL_TIMEOUT_MS,
                           "#include <linux/if_packet.h>\n"
                           "int main(void) { return PACKET_FANOUT; }\n");
if (rc != 0)
    TEST_SKIP("The agent's headers do not know about packet fanout");
```

### tapi_make — make

```c
static const char *targets[] = { "all" };
static const char *variables[] = { "CFLAGS=-O2 -g" };
tapi_make_opt opt = tapi_make_default_opt;
tapi_make_app *app = NULL;

opt.directory = project_dir;
opt.targets = targets;
opt.n_targets = TE_ARRAY_LEN(targets);
opt.variables = variables;
opt.n_variables = TE_ARRAY_LEN(variables);
opt.jobs = 4;

CHECK_RC(tapi_make_do(factory, &opt, TAPI_DEVTOOL_TIMEOUT_MS, &app));
...
CLEANUP_CHECK_RC(tapi_make_destroy(app));
```

### tapi_kbuild — kernel modules

`make -C <kernel build tree> M=<module sources> modules`, with the build
tree of the running kernel resolved for you and the result placed where
TE's kernel module configuration looks for it:

```c
tapi_kbuild_opt opt = tapi_kbuild_default_opt;
tapi_kbuild_app *app = NULL;

opt.module_dir = mod_dir;

CHECK_RC(tapi_kbuild_write_makefile(ta, mod_dir, "tst_mod", NULL, 0));
CHECK_RC(tapi_kbuild_do(factory, &opt, TAPI_DEVTOOL_TIMEOUT_MS, &app));
CHECK_RC(tapi_kbuild_install(ta, mod_dir, "tst_mod"));
CHECK_RC(tapi_cfg_module_add_from_ta_dir(ta, "tst_mod", true));
```

Loading, module parameters and unloading are deliberately not part of
this library: once the module is built it is a configuration object, so
it belongs to TE's own `tapi_cfg_modules.h`, which also unloads it
during configuration rollback.

Building a module needs the kernel build tree of the running kernel on
the agent — a separate package on most distributions (`kernel-devel`,
`linux-headers-$(uname -r)`). A test that cannot rely on it should look
for the tree with `tapi_kbuild_kernel_dir()` and skip itself when it is
not there.

## Cross-compilation

`tapi_devtool_toolchain` describes one toolchain for all three TAPIs:
the compilers, the `make` program, and `ARCH` / `CROSS_COMPILE` for the
Makefiles that expect them. Fill it once — in a prologue, stored in the
suite context — and point every option structure at it; an option set
explicitly on a call always wins over the toolchain.

`ARCH` and `CROSS_COMPILE` are passed to `make` and to kbuild only. A
compiler is invoked directly, so a cross build names the cross compiler
in `tapi_devtool_toolchain::cc` and `::cxx`.

## Note on PATH

A `tapi_job` job does not inherit the environment of the agent, so a
bare `cc` or `make` is resolved against the default `PATH` of the RPC
server. Call `tapi_job_factory_set_path()` on the factory, or give a
full path, when the tools live somewhere unusual.
