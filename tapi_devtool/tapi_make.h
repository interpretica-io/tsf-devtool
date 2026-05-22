/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief make TAPI
 *
 * @defgroup tapi_make make (tapi_make)
 * @ingroup tapi_devtool
 * @{
 *
 * Run @c make on a Test Agent.
 *
 * Building a project that ships its own Makefile:
 *
 * @code
 * static const char *targets[] = { "all" };
 * static const char *variables[] = { "CFLAGS=-O2 -g" };
 * tapi_make_opt opt = tapi_make_default_opt;
 * tapi_make_app *app = NULL;
 *
 * opt.directory = project_dir;
 * opt.targets = targets;
 * opt.n_targets = TE_ARRAY_LEN(targets);
 * opt.variables = variables;
 * opt.n_variables = TE_ARRAY_LEN(variables);
 * opt.jobs = 4;
 *
 * CHECK_RC(tapi_make_do(factory, &opt, TAPI_DEVTOOL_TIMEOUT_MS, &app));
 * ...
 * CLEANUP_CHECK_RC(tapi_make_destroy(app));
 * @endcode
 *
 * @note A variable passed in @a variables overrides the one set inside
 *       the Makefile, which is what a test normally wants; a variable
 *       that must only provide a default belongs in the environment
 *       instead.
 */

#ifndef __TSF_TAPI_MAKE_H__
#define __TSF_TAPI_MAKE_H__

#include "te_defs.h"
#include "te_errno.h"
#include "tapi_job.h"
#include "tapi_job_opt.h"

#include "tapi_devtool.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @c make invocation options. */
typedef struct tapi_make_opt {
    /** Toolchain to take @c make, @c ARCH and @c CROSS_COMPILE from. */
    const tapi_devtool_toolchain *toolchain;
    /** @c make program; overrides the toolchain when not @c NULL. */
    const char *make;

    /** Directory to change into first, passed as @c -C (@c NULL to omit). */
    const char *directory;
    /** Makefile to read, passed as @c -f (@c NULL for the default one). */
    const char *makefile;
    /**
     * Number of parallel jobs, passed as @c -j. Use
     * @c TAPI_JOB_OPT_OMIT_UINT to let @c make decide, which is what
     * @ref tapi_make_default_opt does.
     */
    unsigned int jobs;
    /** Keep building what is still buildable after an error (@c -k). */
    bool keep_going;
    /** Rebuild everything unconditionally (@c -B). */
    bool always_make;
    /** Do not echo the commands (@c -s). */
    bool silent;
    /** Print the directory before and after building (@c -w). */
    bool print_directory;
    /** Ignore errors of the individual recipes (@c -i). */
    bool ignore_errors;
    /** Print the recipes without running them (@c -n). */
    bool dry_run;

    /**
     * Target architecture, passed as @c ARCH. Taken from the toolchain
     * when @c NULL.
     */
    const char *arch;
    /**
     * Cross compiler prefix, passed as @c CROSS_COMPILE. Taken from the
     * toolchain when @c NULL.
     */
    const char *cross_compile;

    /** Number of variable assignments. */
    size_t n_variables;
    /** Variable assignments, each of them a @c "VAR=VALUE" string. */
    const char **variables;
    /** Number of targets. */
    size_t n_targets;
    /** Targets to build (none means the default target of the Makefile). */
    const char **targets;

    /** Working directory on the agent (@c NULL to keep the default one). */
    const char *workdir;
} tapi_make_opt;

/** Default options: native @c make, default target, no parallelism. */
extern const tapi_make_opt tapi_make_default_opt;

/** @c make invocation handle. */
typedef struct tapi_make_app tapi_make_app;

/**
 * Create a @c make invocation.
 *
 * @param[in]  factory      Job factory.
 * @param[in]  opt          @c make options.
 * @param[out] app          @c make handle.
 *
 * @return Status code.
 */
extern te_errno tapi_make_create(tapi_job_factory_t *factory,
                                 const tapi_make_opt *opt,
                                 tapi_make_app **app);

/**
 * Start @c make. The output of a previous run, if any, is dropped.
 *
 * @param app           @c make handle.
 *
 * @return Status code.
 */
extern te_errno tapi_make_start(tapi_make_app *app);

/**
 * Wait for @c make and capture its output.
 *
 * @param app           @c make handle.
 * @param timeout_ms    Timeout, ms (negative for the tapi_job default).
 *
 * @return Status code.
 * @retval TE_EINPROGRESS   @c make is still running.
 * @retval TE_ESHCMD        @c make failed.
 */
extern te_errno tapi_make_wait(tapi_make_app *app, int timeout_ms);

/**
 * Send a signal to @c make.
 *
 * @param app           @c make handle.
 * @param signum        Signal number.
 *
 * @return Status code.
 */
extern te_errno tapi_make_kill(tapi_make_app *app, int signum);

/**
 * Stop @c make; it can be started over with tapi_make_start().
 *
 * @param app           @c make handle.
 *
 * @return Status code.
 */
extern te_errno tapi_make_stop(tapi_make_app *app);

/**
 * Get what @c make printed and how it exited.
 *
 * @param[in]  app      @c make handle.
 * @param[out] output   Output description; the strings belong to @p app
 *                      and stay valid until the next tapi_make_start()
 *                      or tapi_make_destroy().
 */
extern void tapi_make_get_output(const tapi_make_app *app,
                                 tapi_devtool_output *output);

/**
 * Destroy a @c make handle. It must not be used afterwards.
 *
 * @param app           @c make handle (may be @c NULL).
 *
 * @return Status code.
 */
extern te_errno tapi_make_destroy(tapi_make_app *app);

/**
 * Create, start and wait for a @c make invocation.
 *
 * The handle is returned even when the build fails, so that the test can
 * report the output; destroy it with tapi_make_destroy().
 *
 * @param[in]  factory      Job factory.
 * @param[in]  opt          @c make options.
 * @param[in]  timeout_ms   Timeout, ms.
 * @param[out] app          @c make handle.
 *
 * @return Status code.
 * @retval TE_ESHCMD        @c make failed.
 */
extern te_errno tapi_make_do(tapi_job_factory_t *factory,
                             const tapi_make_opt *opt,
                             int timeout_ms,
                             tapi_make_app **app);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TSF_TAPI_MAKE_H__ */

/**@} <!-- END tapi_make --> */
