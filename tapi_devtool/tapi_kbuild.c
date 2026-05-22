/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Kernel module build TAPI
 *
 * Implementation of the out-of-tree kernel module build TAPI.
 */

#define TE_LGR_USER "TAPI KBUILD"

#include "te_config.h"

#include <stdlib.h>
#include <sys/utsname.h>

#include "logger_api.h"
#include "tapi_cfg_base.h"
#include "tapi_file.h"
#include "te_alloc.h"
#include "te_string.h"

#include "tapi_devtool_run.h"
#include "tapi_kbuild.h"

struct tapi_kbuild_app {
    /** Build job. */
    tapi_devtool_run run;
};

const tapi_kbuild_opt tapi_kbuild_default_opt = {
    .toolchain = NULL,
    .make      = NULL,
    .target    = NULL,
    .jobs      = TAPI_JOB_OPT_OMIT_UINT,
};

static const tapi_job_opt_bind kbuild_binds[] = TAPI_JOB_OPT_SET(
    TAPI_JOB_OPT_STRING("-C", false, tapi_kbuild_opt, kernel_dir),
    TAPI_JOB_OPT_UINT_OMITTABLE("-j", false, NULL, tapi_kbuild_opt, jobs),
    TAPI_JOB_OPT_STRING("M=", true, tapi_kbuild_opt, module_dir),
    TAPI_JOB_OPT_STRING("ARCH=", true, tapi_kbuild_opt, arch),
    TAPI_JOB_OPT_STRING("CROSS_COMPILE=", true, tapi_kbuild_opt,
                        cross_compile),
    TAPI_JOB_OPT_STRING("KCFLAGS=", true, tapi_kbuild_opt, kcflags),
    TAPI_JOB_OPT_ARRAY_PTR(tapi_kbuild_opt, n_variables, variables,
        TAPI_JOB_OPT_CONTENT(TAPI_JOB_OPT_STRING, NULL, false)),
    TAPI_JOB_OPT_STRING(NULL, false, tapi_kbuild_opt, target)
);

/* See description in tapi_kbuild.h */
te_errno
tapi_kbuild_kernel_dir(const char *ta, const char *release, te_string *dest)
{
    struct utsname uts;
    te_errno rc;

    if (release == NULL)
    {
        rc = tapi_cfg_base_get_ta_uname(ta, &uts);
        if (rc != 0)
        {
            ERROR("Failed to get uname of TA %s: %r", ta, rc);
            return rc;
        }
        release = uts.release;
    }

    te_string_append(dest, "/lib/modules/%s/build", release);

    return 0;
}

/* See description in tapi_kbuild.h */
te_errno
tapi_kbuild_write_makefile(const char *ta, const char *module_dir,
                           const char *module_name, const char **objects,
                           size_t n_objects)
{
    te_string content = TE_STRING_INIT;
    te_string path = TE_STRING_INIT;
    size_t i;
    te_errno rc;

    te_string_append(&content, "obj-m += %s.o\n", module_name);

    if (n_objects != 0)
    {
        te_string_append(&content, "%s-y :=", module_name);
        for (i = 0; i < n_objects; i++)
            te_string_append(&content, " %s", objects[i]);
        te_string_append(&content, "\n");
    }

    te_string_append(&path, "%s/Makefile", module_dir);

    rc = tapi_file_create_ta(ta, path.ptr, "%s", content.ptr);
    if (rc != 0)
        ERROR("Failed to write %s on TA %s: %r", path.ptr, ta, rc);

    te_string_free(&content);
    te_string_free(&path);

    return rc;
}

/* See description in tapi_kbuild.h */
te_errno
tapi_kbuild_install(const char *ta, const char *module_dir,
                    const char *module_name)
{
    te_string src = TE_STRING_INIT;
    te_string dst = TE_STRING_INIT;
    char *kmod_dir;
    te_errno rc;

    kmod_dir = tapi_cfg_base_get_ta_dir(ta, TAPI_CFG_BASE_TA_DIR_KMOD);
    if (kmod_dir == NULL)
    {
        ERROR("Failed to get the kernel module directory of TA %s", ta);
        return TE_RC(TE_TAPI, TE_EFAIL);
    }

    te_string_append(&src, "%s/%s.ko", module_dir, module_name);
    te_string_append(&dst, "%s/%s.ko", kmod_dir, module_name);

    rc = tapi_file_copy_ta(ta, src.ptr, ta, dst.ptr);
    if (rc != 0)
        ERROR("Failed to copy %s to %s on TA %s: %r", src.ptr, dst.ptr, ta, rc);
    else
        RING("Module %s is installed as %s on TA %s", module_name, dst.ptr, ta);

    free(kmod_dir);
    te_string_free(&src);
    te_string_free(&dst);

    return rc;
}

/* See description in tapi_kbuild.h */
te_errno
tapi_kbuild_create(tapi_job_factory_t *factory, const tapi_kbuild_opt *opt,
                   tapi_kbuild_app **app)
{
    tapi_kbuild_opt effective = *opt;
    te_string kernel_dir = TE_STRING_INIT;
    const char *program;
    tapi_kbuild_app *result;
    te_errno rc;

    if (opt->module_dir == NULL)
    {
        ERROR("Directory with the kernel module sources is not set");
        return TE_RC(TE_TAPI, TE_EINVAL);
    }

    program = (opt->make != NULL) ? opt->make
                                  : tapi_devtool_make_program(opt->toolchain);

    if (effective.target == NULL)
        effective.target = TAPI_KBUILD_TARGET_MODULES;

    /* Explicit options win over the toolchain defaults. */
    if (effective.arch == NULL && opt->toolchain != NULL)
        effective.arch = opt->toolchain->arch;
    if (effective.cross_compile == NULL && opt->toolchain != NULL)
        effective.cross_compile = opt->toolchain->cross_compile;

    if (effective.kernel_dir == NULL)
    {
        const char *ta = tapi_job_factory_ta(factory);

        if (ta == NULL)
        {
            ERROR("Cannot determine the agent behind the job factory");
            return TE_RC(TE_TAPI, TE_EINVAL);
        }

        rc = tapi_kbuild_kernel_dir(ta, opt->kernel_release, &kernel_dir);
        if (rc != 0)
            return rc;

        effective.kernel_dir = kernel_dir.ptr;
    }

    result = TE_ALLOC(sizeof(*result));
    result->run = (tapi_devtool_run)TAPI_DEVTOOL_RUN_INIT;

    rc = tapi_devtool_run_init(&result->run, factory, "kbuild", program,
                               kbuild_binds, &effective, NULL);

    /* The arguments are built by now, so the resolved path is not needed. */
    te_string_free(&kernel_dir);

    if (rc != 0)
    {
        free(result);
        return rc;
    }

    *app = result;

    return 0;
}

/* See description in tapi_kbuild.h */
te_errno
tapi_kbuild_start(tapi_kbuild_app *app)
{
    return tapi_devtool_run_start(&app->run);
}

/* See description in tapi_kbuild.h */
te_errno
tapi_kbuild_wait(tapi_kbuild_app *app, int timeout_ms)
{
    te_errno rc;

    rc = tapi_devtool_run_wait(&app->run, timeout_ms);
    if (rc != 0)
        return rc;

    return tapi_devtool_run_check(&app->run);
}

/* See description in tapi_kbuild.h */
te_errno
tapi_kbuild_kill(tapi_kbuild_app *app, int signum)
{
    return tapi_devtool_run_kill(&app->run, signum);
}

/* See description in tapi_kbuild.h */
te_errno
tapi_kbuild_stop(tapi_kbuild_app *app)
{
    return tapi_devtool_run_stop(&app->run);
}

/* See description in tapi_kbuild.h */
void
tapi_kbuild_get_output(const tapi_kbuild_app *app, tapi_devtool_output *output)
{
    tapi_devtool_run_get_output(&app->run, output);
}

/* See description in tapi_kbuild.h */
te_errno
tapi_kbuild_destroy(tapi_kbuild_app *app)
{
    te_errno rc;

    if (app == NULL)
        return 0;

    rc = tapi_devtool_run_fini(&app->run);
    free(app);

    return rc;
}

/* See description in tapi_kbuild.h */
te_errno
tapi_kbuild_do(tapi_job_factory_t *factory, const tapi_kbuild_opt *opt,
               int timeout_ms, tapi_kbuild_app **app)
{
    te_errno rc;

    rc = tapi_kbuild_create(factory, opt, app);
    if (rc != 0)
        return rc;

    rc = tapi_kbuild_start(*app);
    if (rc != 0)
        return rc;

    return tapi_kbuild_wait(*app, timeout_ms);
}
