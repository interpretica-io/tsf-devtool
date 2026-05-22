/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Development tools TAPI: common definitions
 *
 * @defgroup tapi_devtool Development tools on Test Agents (tapi_devtool)
 * @{
 *
 * Definitions shared by the build tool TAPIs of the external tsf-devtool
 * repository:
 *
 * - @ref tapi_cc - C and C++ compilers;
 * - @ref tapi_make - @c make;
 * - @ref tapi_devtool_run - the primitive they are built on, for TAPIs
 *   outside this library (tsf-kernel uses it).
 *
 * Kernel development - building out-of-tree modules, loading them and
 * looking at what the kernel allows - lives in tsf-kernel, which builds
 * on this library.
 *
 * Every tool of this library is started on a Test Agent through
 * @ref tapi_job, so a test works the same way whether the agent is the
 * local host or a remote one:
 *
 * @code
 * tapi_job_factory_t *factory = NULL;
 *
 * CHECK_RC(tapi_job_factory_rpc_create(pco_iut, &factory));
 * @endcode
 *
 * @note The factory must be an RPC one. These tools read what the tool
 *       printed, which needs output channels and filters, and only the
 *       RPC factory implements them; a CFG factory job fails to create
 *       with @c TE_EOPNOTSUPP. So the agent needs @c ta_rpcprovider,
 *       declared with @c TE_TA_APP in @c builder.conf.
 *
 * The three TAPIs share one lifecycle, borrowed from the tool TAPIs of
 * TE itself: @c _create(), @c _start(), @c _wait(), @c _destroy(), with
 * @c _do() as a create-start-wait shortcut and @c _get_output() to look
 * at what the tool printed. Compiler and linker diagnostics are the
 * point of these tools, so they are always captured and always logged,
 * whether the tool succeeded or not.
 */

#ifndef __TSF_TAPI_DEVTOOL_H__
#define __TSF_TAPI_DEVTOOL_H__

#include "te_defs.h"
#include "te_errno.h"
#include "tapi_job.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Default timeout of a build step, ms.
 *
 * Compilers and @c make are slow and their runtime depends on the agent
 * host, so the default is generous. Pass it to the @c _wait() and
 * @c _do() functions as @c timeout_ms when there is no better estimate.
 */
#define TAPI_DEVTOOL_TIMEOUT_MS         300000

/** Time given to a tool to terminate gracefully, ms. */
#define TAPI_DEVTOOL_TERM_TIMEOUT_MS    1000

/** Source language of a compilation unit. */
typedef enum tapi_devtool_lang {
    TAPI_DEVTOOL_LANG_C = 0,    /**< C, built with @c cc by default */
    TAPI_DEVTOOL_LANG_CXX,      /**< C++, built with @c c++ by default */
} tapi_devtool_lang;

/**
 * Toolchain the tools of this library build with.
 *
 * A test that builds for the agent it runs on needs no toolchain at all:
 * leave the @a toolchain field of the option structures @c NULL and the
 * native @c cc, @c c++ and @c make are used.
 *
 * A single toolchain description can be shared by all three TAPIs, which
 * is what it is for: define it once in a prologue, store it in the test
 * suite context, and pass it to every compiler, @c make and kbuild call.
 *
 * @note @a arch and @a cross_compile are passed to @c make as the @c ARCH
 *       and @c CROSS_COMPILE variables, which is how kbuild and most
 *       Makefiles expect cross-compilation to be requested. They are
 *       @b not applied to @ref tapi_cc: a compiler is invoked directly,
 *       so name the cross compiler in @a cc / @a cxx instead.
 */
typedef struct tapi_devtool_toolchain {
    /** C compiler (@c NULL means @c cc). */
    const char *cc;
    /** C++ compiler (@c NULL means @c c++). */
    const char *cxx;
    /** @c make program (@c NULL means @c make). */
    const char *make;
    /** Target architecture, passed as @c ARCH (@c NULL to omit). */
    const char *arch;
    /** Cross compiler prefix, passed as @c CROSS_COMPILE (@c NULL to omit). */
    const char *cross_compile;
} tapi_devtool_toolchain;

/** Native toolchain of the agent: plain @c cc, @c c++ and @c make. */
extern const tapi_devtool_toolchain tapi_devtool_toolchain_native;

/** What a tool printed and how it exited. */
typedef struct tapi_devtool_output {
    /** Everything the tool wrote to stdout (never @c NULL). */
    const char *out;
    /** Everything the tool wrote to stderr, i.e. diagnostics (never @c NULL). */
    const char *err;
    /** Exit status of the tool. */
    tapi_job_status_t status;
    /** @c true if the tool has been waited for and did complete. */
    bool completed;
} tapi_devtool_output;

/**
 * Get the compiler of @p toolchain for @p lang.
 *
 * @param toolchain     Toolchain (may be @c NULL for the native one).
 * @param lang          Source language.
 *
 * @return Program name or path, never @c NULL.
 */
extern const char *tapi_devtool_compiler(
                                const tapi_devtool_toolchain *toolchain,
                                tapi_devtool_lang lang);

/**
 * Get the @c make program of @p toolchain.
 *
 * @param toolchain     Toolchain (may be @c NULL for the native one).
 *
 * @return Program name or path, never @c NULL.
 */
extern const char *tapi_devtool_make_program(
                                const tapi_devtool_toolchain *toolchain);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TSF_TAPI_DEVTOOL_H__ */

/**@} <!-- END tapi_devtool --> */
