/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief make TAPI
 *
 * Implementation of the @c make TAPI.
 */

#define TE_LGR_USER "TAPI MAKE"

#include "te_config.h"

#include <stdlib.h>

#include "logger_api.h"
#include "te_alloc.h"

#include "tapi_devtool_run.h"
#include "tapi_make.h"

struct tapi_make_app {
    /** @c make job. */
    tapi_devtool_run run;
};

const tapi_make_opt tapi_make_default_opt = {
    .toolchain = NULL,
    .make      = NULL,
    .jobs      = TAPI_JOB_OPT_OMIT_UINT,
};

static const tapi_job_opt_bind make_binds[] = TAPI_JOB_OPT_SET(
    TAPI_JOB_OPT_STRING("-C", false, tapi_make_opt, directory),
    TAPI_JOB_OPT_STRING("-f", false, tapi_make_opt, makefile),
    TAPI_JOB_OPT_UINT_OMITTABLE("-j", false, NULL, tapi_make_opt, jobs),
    TAPI_JOB_OPT_BOOL("-k", tapi_make_opt, keep_going),
    TAPI_JOB_OPT_BOOL("-B", tapi_make_opt, always_make),
    TAPI_JOB_OPT_BOOL("-s", tapi_make_opt, silent),
    TAPI_JOB_OPT_BOOL("-w", tapi_make_opt, print_directory),
    TAPI_JOB_OPT_BOOL("-i", tapi_make_opt, ignore_errors),
    TAPI_JOB_OPT_BOOL("-n", tapi_make_opt, dry_run),
    TAPI_JOB_OPT_STRING("ARCH=", true, tapi_make_opt, arch),
    TAPI_JOB_OPT_STRING("CROSS_COMPILE=", true, tapi_make_opt, cross_compile),
    TAPI_JOB_OPT_ARRAY_PTR(tapi_make_opt, n_variables, variables,
        TAPI_JOB_OPT_CONTENT(TAPI_JOB_OPT_STRING, NULL, false)),
    TAPI_JOB_OPT_ARRAY_PTR(tapi_make_opt, n_targets, targets,
        TAPI_JOB_OPT_CONTENT(TAPI_JOB_OPT_STRING, NULL, false))
);

/* See description in tapi_make.h */
te_errno
tapi_make_create(tapi_job_factory_t *factory, const tapi_make_opt *opt,
                 tapi_make_app **app)
{
    tapi_make_opt effective = *opt;
    const char *program;
    tapi_make_app *result;
    te_errno rc;

    program = (opt->make != NULL) ? opt->make
                                  : tapi_devtool_make_program(opt->toolchain);

    /* Explicit options win over the toolchain defaults. */
    if (effective.arch == NULL && opt->toolchain != NULL)
        effective.arch = opt->toolchain->arch;
    if (effective.cross_compile == NULL && opt->toolchain != NULL)
        effective.cross_compile = opt->toolchain->cross_compile;

    result = TE_ALLOC(sizeof(*result));
    result->run = (tapi_devtool_run)TAPI_DEVTOOL_RUN_INIT;

    rc = tapi_devtool_run_init(&result->run, factory, "make", program,
                               make_binds, &effective, opt->workdir);
    if (rc != 0)
    {
        free(result);
        return rc;
    }

    *app = result;

    return 0;
}

/* See description in tapi_make.h */
te_errno
tapi_make_start(tapi_make_app *app)
{
    return tapi_devtool_run_start(&app->run);
}

/* See description in tapi_make.h */
te_errno
tapi_make_wait(tapi_make_app *app, int timeout_ms)
{
    te_errno rc;

    rc = tapi_devtool_run_wait(&app->run, timeout_ms);
    if (rc != 0)
        return rc;

    return tapi_devtool_run_check(&app->run);
}

/* See description in tapi_make.h */
te_errno
tapi_make_kill(tapi_make_app *app, int signum)
{
    return tapi_devtool_run_kill(&app->run, signum);
}

/* See description in tapi_make.h */
te_errno
tapi_make_stop(tapi_make_app *app)
{
    return tapi_devtool_run_stop(&app->run);
}

/* See description in tapi_make.h */
void
tapi_make_get_output(const tapi_make_app *app, tapi_devtool_output *output)
{
    tapi_devtool_run_get_output(&app->run, output);
}

/* See description in tapi_make.h */
te_errno
tapi_make_destroy(tapi_make_app *app)
{
    te_errno rc;

    if (app == NULL)
        return 0;

    rc = tapi_devtool_run_fini(&app->run);
    free(app);

    return rc;
}

/* See description in tapi_make.h */
te_errno
tapi_make_do(tapi_job_factory_t *factory, const tapi_make_opt *opt,
             int timeout_ms, tapi_make_app **app)
{
    te_errno rc;

    rc = tapi_make_create(factory, opt, app);
    if (rc != 0)
        return rc;

    rc = tapi_make_start(*app);
    if (rc != 0)
        return rc;

    return tapi_make_wait(*app, timeout_ms);
}
