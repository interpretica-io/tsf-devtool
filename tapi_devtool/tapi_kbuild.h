/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Kernel module build TAPI
 *
 * @defgroup tapi_kbuild Out-of-tree kernel modules (tapi_kbuild)
 * @ingroup tapi_devtool
 * @{
 *
 * Build an out-of-tree Linux kernel module on a Test Agent.
 *
 * This is kbuild as the kernel documents it,
 * @c "make -C <kernel build tree> M=<module sources> modules", with the
 * kernel build tree of the running kernel found for you and the result
 * placed where TE's kernel module configuration expects it.
 *
 * The whole build-and-insert flow:
 *
 * @code
 * tapi_kbuild_opt opt = tapi_kbuild_default_opt;
 * tapi_kbuild_app *app = NULL;
 *
 * opt.module_dir = mod_dir;
 *
 * CHECK_RC(tapi_kbuild_write_makefile(ta, mod_dir, "tst_mod", NULL, 0));
 * CHECK_RC(tapi_kbuild_do(factory, &opt, TAPI_DEVTOOL_TIMEOUT_MS, &app));
 * CHECK_RC(tapi_kbuild_install(ta, mod_dir, "tst_mod"));
 * CHECK_RC(tapi_cfg_module_add_from_ta_dir(ta, "tst_mod", true));
 * @endcode
 *
 * Loading, parameters and unloading are not repeated here: the module is
 * a configuration object once it is built, so it belongs to
 * @ref tapi_conf_modules (@c tapi_cfg_modules.h), which also unloads it
 * during configuration rollback.
 *
 * @note Building a module needs the kernel build tree of the running
 *       kernel on the agent, which is a separate package on most
 *       distributions (@c kernel-devel, @c linux-headers-$(uname -r)).
 *       A test that cannot rely on it should check the tree out with
 *       tapi_kbuild_kernel_dir() and skip itself when it is missing.
 */

#ifndef __TSF_TAPI_KBUILD_H__
#define __TSF_TAPI_KBUILD_H__

#include "te_defs.h"
#include "te_errno.h"
#include "te_string.h"
#include "tapi_job.h"
#include "tapi_job_opt.h"

#include "tapi_devtool.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Build the modules of the module directory. */
#define TAPI_KBUILD_TARGET_MODULES  "modules"
/** Remove what a previous build produced. */
#define TAPI_KBUILD_TARGET_CLEAN    "clean"

/** Kernel module build options. */
typedef struct tapi_kbuild_opt {
    /** Toolchain to take @c make, @c ARCH and @c CROSS_COMPILE from. */
    const tapi_devtool_toolchain *toolchain;
    /** @c make program; overrides the toolchain when not @c NULL. */
    const char *make;

    /**
     * Kernel build tree, passed as @c -C. When @c NULL it is derived
     * from @a kernel_release by tapi_kbuild_kernel_dir().
     */
    const char *kernel_dir;
    /**
     * Kernel release the module is built for, used only when
     * @a kernel_dir is @c NULL. @c NULL means the kernel running on the
     * agent.
     */
    const char *kernel_release;
    /**
     * Directory with the module sources and their Makefile, passed as
     * @c M. It is a path on the agent and it must be absolute, because
     * kbuild resolves it from inside the kernel tree. Mandatory.
     */
    const char *module_dir;
    /** Target to build; @c NULL means @ref TAPI_KBUILD_TARGET_MODULES. */
    const char *target;
    /** Extra compiler flags, passed as @c KCFLAGS (@c NULL to omit). */
    const char *kcflags;

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
    /**
     * Number of parallel jobs, passed as @c -j. Use
     * @c TAPI_JOB_OPT_OMIT_UINT to let @c make decide.
     */
    unsigned int jobs;

    /** Number of extra variable assignments. */
    size_t n_variables;
    /** Extra variable assignments, each a @c "VAR=VALUE" string. */
    const char **variables;
} tapi_kbuild_opt;

/** Default options: native toolchain, running kernel, @c modules target. */
extern const tapi_kbuild_opt tapi_kbuild_default_opt;

/** Kernel module build handle. */
typedef struct tapi_kbuild_app tapi_kbuild_app;

/**
 * Get the kernel build tree of a kernel on an agent.
 *
 * The path is the conventional @c /lib/modules/<release>/build; its
 * existence is not checked.
 *
 * @param[in]  ta       Agent name.
 * @param[in]  release  Kernel release, or @c NULL for the kernel running
 *                      on the agent.
 * @param[out] dest     String to append the path to.
 *
 * @return Status code.
 */
extern te_errno tapi_kbuild_kernel_dir(const char *ta, const char *release,
                                       te_string *dest);

/**
 * Write the kbuild Makefile of a single module.
 *
 * The generated Makefile is the one the kernel documentation prescribes:
 *
 * @code
 * obj-m += <module_name>.o
 * <module_name>-y := <objects>
 * @endcode
 *
 * The second line is written only when @p objects are given. Omit them
 * for a module built from a single @c <module_name>.c, which is the case
 * kbuild handles without any object list.
 *
 * @param ta            Agent name.
 * @param module_dir    Directory with the module sources on the agent.
 * @param module_name   Module name, without the @c .ko suffix.
 * @param objects       Object files the module consists of, each with a
 *                      @c .o suffix (may be @c NULL).
 * @param n_objects     Number of @p objects.
 *
 * @return Status code.
 */
extern te_errno tapi_kbuild_write_makefile(const char *ta,
                                           const char *module_dir,
                                           const char *module_name,
                                           const char **objects,
                                           size_t n_objects);

/**
 * Put a built module where TE looks for the modules of an agent.
 *
 * Copies @c <module_dir>/<module_name>.ko into the kernel module
 * directory of the agent, which is what makes
 * tapi_cfg_module_add_from_ta_dir() find it.
 *
 * @note Both paths are on the same agent, but the copy goes through
 *       the engine, because that is the only file transfer RCF offers:
 *       the module travels the network twice.
 *
 * @param ta            Agent name.
 * @param module_dir    Directory the module was built in.
 * @param module_name   Module name, without the @c .ko suffix.
 *
 * @return Status code.
 */
extern te_errno tapi_kbuild_install(const char *ta, const char *module_dir,
                                    const char *module_name);

/**
 * Create a kernel module build.
 *
 * @param[in]  factory      Job factory.
 * @param[in]  opt          Build options.
 * @param[out] app          Build handle.
 *
 * @return Status code.
 * @retval TE_EINVAL        tapi_kbuild_opt::module_dir is not set.
 */
extern te_errno tapi_kbuild_create(tapi_job_factory_t *factory,
                                   const tapi_kbuild_opt *opt,
                                   tapi_kbuild_app **app);

/**
 * Start the build. The output of a previous run, if any, is dropped.
 *
 * @param app           Build handle.
 *
 * @return Status code.
 */
extern te_errno tapi_kbuild_start(tapi_kbuild_app *app);

/**
 * Wait for the build and capture its output.
 *
 * @param app           Build handle.
 * @param timeout_ms    Timeout, ms (negative for the tapi_job default).
 *
 * @return Status code.
 * @retval TE_EINPROGRESS   The build is still running.
 * @retval TE_ESHCMD        The build failed.
 */
extern te_errno tapi_kbuild_wait(tapi_kbuild_app *app, int timeout_ms);

/**
 * Send a signal to the build.
 *
 * @param app           Build handle.
 * @param signum        Signal number.
 *
 * @return Status code.
 */
extern te_errno tapi_kbuild_kill(tapi_kbuild_app *app, int signum);

/**
 * Stop the build; it can be started over with tapi_kbuild_start().
 *
 * @param app           Build handle.
 *
 * @return Status code.
 */
extern te_errno tapi_kbuild_stop(tapi_kbuild_app *app);

/**
 * Get what the build printed and how it exited.
 *
 * @param[in]  app      Build handle.
 * @param[out] output   Output description; the strings belong to @p app
 *                      and stay valid until the next tapi_kbuild_start()
 *                      or tapi_kbuild_destroy().
 */
extern void tapi_kbuild_get_output(const tapi_kbuild_app *app,
                                   tapi_devtool_output *output);

/**
 * Destroy a build handle. It must not be used afterwards.
 *
 * @param app           Build handle (may be @c NULL).
 *
 * @return Status code.
 */
extern te_errno tapi_kbuild_destroy(tapi_kbuild_app *app);

/**
 * Create, start and wait for a kernel module build.
 *
 * The handle is returned even when the build fails, so that the test can
 * report the output; destroy it with tapi_kbuild_destroy().
 *
 * @param[in]  factory      Job factory.
 * @param[in]  opt          Build options.
 * @param[in]  timeout_ms   Timeout, ms.
 * @param[out] app          Build handle.
 *
 * @return Status code.
 * @retval TE_ESHCMD        The build failed.
 */
extern te_errno tapi_kbuild_do(tapi_job_factory_t *factory,
                               const tapi_kbuild_opt *opt,
                               int timeout_ms,
                               tapi_kbuild_app **app);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TSF_TAPI_KBUILD_H__ */

/**@} <!-- END tapi_kbuild --> */
