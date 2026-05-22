/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief C/C++ compiler TAPI
 *
 * @defgroup tapi_cc C and C++ compilers (tapi_cc)
 * @ingroup tapi_devtool
 * @{
 *
 * Compile and link C and C++ sources on a Test Agent.
 *
 * The TAPI speaks the option language shared by @c cc, @c gcc, @c clang
 * and their C++ counterparts, so it works with any of them; name the one
 * to use in tapi_cc_opt::compiler or in the toolchain, or leave both
 * unset for the agent's native @c cc / @c c++.
 *
 * Compiling a program out of two sources and linking it against @c libm:
 *
 * @code
 * static const char *sources[] = { "main.c", "helper.c" };
 * static const char *libs[] = { "m" };
 * tapi_cc_opt opt = tapi_cc_default_opt;
 * tapi_cc_app *app = NULL;
 *
 * opt.sources = sources;
 * opt.n_sources = TE_ARRAY_LEN(sources);
 * opt.libs = libs;
 * opt.n_libs = TE_ARRAY_LEN(libs);
 * opt.output = "app";
 * opt.warn_all = true;
 * opt.warn_error = true;
 * opt.workdir = src_dir;
 *
 * CHECK_RC(tapi_cc_do(factory, &opt, TAPI_DEVTOOL_TIMEOUT_MS, &app));
 * ...
 * CLEANUP_CHECK_RC(tapi_cc_destroy(app));
 * @endcode
 *
 * @note The job the compiler runs in does not inherit the environment of
 *       the agent, so a bare @c cc is resolved against the default
 *       @c PATH of the RPC server. Call tapi_job_factory_set_path() on
 *       the factory, or give a full path, when the compiler lives in an
 *       unusual place.
 */

#ifndef __TSF_TAPI_CC_H__
#define __TSF_TAPI_CC_H__

#include "te_compiler.h"
#include "te_defs.h"
#include "te_errno.h"
#include "tapi_job.h"

#include "tapi_devtool.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Compiler invocation options. */
typedef struct tapi_cc_opt {
    /** Toolchain to take the compiler from (@c NULL for the native one). */
    const tapi_devtool_toolchain *toolchain;
    /** Compiler to run; overrides the toolchain when not @c NULL. */
    const char *compiler;
    /** Source language; selects the default compiler and is not passed on. */
    tapi_devtool_lang lang;

    /** Language standard, passed as @c -std= (@c NULL to omit). */
    const char *std;
    /** Optimization level, passed as @c -O, e.g. @c "2" (@c NULL to omit). */
    const char *opt_level;
    /** Generate debugging information (@c -g). */
    bool debug;
    /** Enable the common warnings (@c -Wall). */
    bool warn_all;
    /** Enable the extra warnings (@c -Wextra). */
    bool warn_extra;
    /** Turn warnings into errors (@c -Werror). */
    bool warn_error;
    /** Reject anything the standard does not allow (@c -pedantic). */
    bool pedantic;
    /** Generate position independent code (@c -fPIC). */
    bool pic;
    /** Produce a shared object (@c -shared). */
    bool shared;
    /** Compile only, do not link (@c -c). */
    bool compile_only;
    /** Preprocess only (@c -E). */
    bool preprocess_only;
    /** Check the syntax only, produce nothing (@c -fsyntax-only). */
    bool syntax_only;
    /** Output file, passed as @c -o (@c NULL to omit). */
    const char *output;

    /** Number of include directories. */
    size_t n_include_dirs;
    /** Include directories, passed as @c -I. */
    const char **include_dirs;
    /** Number of macro definitions. */
    size_t n_defines;
    /** Macro definitions, passed as @c -D, e.g. @c "NDEBUG" or @c "N=4". */
    const char **defines;
    /** Number of macro cancellations. */
    size_t n_undefines;
    /** Macros to cancel, passed as @c -U. */
    const char **undefines;
    /** Number of extra compiler flags. */
    size_t n_cflags;
    /** Extra compiler flags, passed verbatim before the sources. */
    const char **cflags;
    /** Number of sources. */
    size_t n_sources;
    /**
     * Sources and object files, passed verbatim. The compiler decides
     * what each of them is by its suffix, so a C++ source with a @c .c
     * suffix has to be forced with a @c -x entry in @a cflags.
     */
    const char **sources;
    /** Number of library directories. */
    size_t n_lib_dirs;
    /** Library directories, passed as @c -L after the sources. */
    const char **lib_dirs;
    /** Number of libraries. */
    size_t n_libs;
    /** Libraries, passed as @c -l after the sources, e.g. @c "m". */
    const char **libs;
    /** Number of extra linker flags. */
    size_t n_ldflags;
    /** Extra linker flags, passed verbatim last. */
    const char **ldflags;

    /** Working directory on the agent (@c NULL to keep the default one). */
    const char *workdir;
} tapi_cc_opt;

/** Default options: native C compiler, no flags, no sources. */
extern const tapi_cc_opt tapi_cc_default_opt;

/** Compiler invocation handle. */
typedef struct tapi_cc_app tapi_cc_app;

/**
 * Create a compiler invocation.
 *
 * @param[in]  factory      Job factory.
 * @param[in]  opt          Compiler options.
 * @param[out] app          Compiler handle.
 *
 * @return Status code.
 */
extern te_errno tapi_cc_create(tapi_job_factory_t *factory,
                               const tapi_cc_opt *opt,
                               tapi_cc_app **app);

/**
 * Start the compiler. The output of a previous run, if any, is dropped.
 *
 * @param app           Compiler handle.
 *
 * @return Status code.
 */
extern te_errno tapi_cc_start(tapi_cc_app *app);

/**
 * Wait for the compiler and capture its diagnostics.
 *
 * Whatever the compiler printed is available through
 * tapi_cc_get_output() afterwards, and the diagnostics of a failed
 * compilation are logged.
 *
 * @param app           Compiler handle.
 * @param timeout_ms    Timeout, ms (negative for the tapi_job default).
 *
 * @return Status code.
 * @retval TE_EINPROGRESS   The compiler is still running.
 * @retval TE_ESHCMD        The compiler failed.
 */
extern te_errno tapi_cc_wait(tapi_cc_app *app, int timeout_ms);

/**
 * Send a signal to the compiler.
 *
 * @param app           Compiler handle.
 * @param signum        Signal number.
 *
 * @return Status code.
 */
extern te_errno tapi_cc_kill(tapi_cc_app *app, int signum);

/**
 * Stop the compiler; it can be started over with tapi_cc_start().
 *
 * @param app           Compiler handle.
 *
 * @return Status code.
 */
extern te_errno tapi_cc_stop(tapi_cc_app *app);

/**
 * Get what the compiler printed and how it exited.
 *
 * @param[in]  app      Compiler handle.
 * @param[out] output   Output description; the strings belong to @p app
 *                      and stay valid until the next tapi_cc_start() or
 *                      tapi_cc_destroy().
 */
extern void tapi_cc_get_output(const tapi_cc_app *app,
                               tapi_devtool_output *output);

/**
 * Destroy a compiler handle. It must not be used afterwards.
 *
 * @param app           Compiler handle (may be @c NULL).
 *
 * @return Status code.
 */
extern te_errno tapi_cc_destroy(tapi_cc_app *app);

/**
 * Create, start and wait for a compiler invocation.
 *
 * The handle is returned even when the compilation fails, so that the
 * test can report the diagnostics; destroy it with tapi_cc_destroy().
 *
 * @param[in]  factory      Job factory.
 * @param[in]  opt          Compiler options.
 * @param[in]  timeout_ms   Timeout, ms.
 * @param[out] app          Compiler handle.
 *
 * @return Status code.
 * @retval TE_ESHCMD        The compiler failed.
 */
extern te_errno tapi_cc_do(tapi_job_factory_t *factory,
                           const tapi_cc_opt *opt,
                           int timeout_ms,
                           tapi_cc_app **app);

/**
 * Check whether a snippet of code builds on the agent.
 *
 * This is the classic feature probe: write a few lines that only
 * compile if the agent's headers, compiler or kernel have what the test
 * needs, and branch on the result instead of guessing from a version
 * number. The snippet is compiled, not linked, and neither it nor the
 * object file outlives the call.
 *
 * @code
 * rc = tapi_cc_check_snippet(factory, NULL, TAPI_DEVTOOL_TIMEOUT_MS,
 *                            "#include <linux/if_packet.h>\n"
 *                            "int main(void) { return PACKET_FANOUT; }\n");
 * if (rc == 0)
 *     ... the agent's headers know about fanout ...
 * @endcode
 *
 * @param factory       Job factory.
 * @param opt           Compiler options; @a sources, @a output,
 *                      @a compile_only and @a preprocess_only are
 *                      overridden. May be @c NULL for the defaults.
 * @param timeout_ms    Timeout, ms.
 * @param snippet_fmt   Format string of the snippet.
 * @param ...           Format arguments.
 *
 * @return Status code.
 * @retval 0            The snippet compiles.
 * @retval TE_ESHCMD    The snippet does not compile.
 */
extern te_errno tapi_cc_check_snippet(tapi_job_factory_t *factory,
                                      const tapi_cc_opt *opt,
                                      int timeout_ms,
                                      const char *snippet_fmt, ...)
    TE_LIKE_PRINTF(4, 5);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TSF_TAPI_CC_H__ */

/**@} <!-- END tapi_cc --> */
