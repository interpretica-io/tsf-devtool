/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief C/C++ compiler TAPI
 *
 * Implementation of the C and C++ compiler TAPI.
 */

#define TE_LGR_USER "TAPI CC"

#include "te_config.h"

#include <stdarg.h>
#include <stdlib.h>

#include "logger_api.h"
#include "tapi_cfg_base.h"
#include "tapi_file.h"
#include "te_alloc.h"
#include "te_string.h"

#include "tapi_cc.h"
#include "tapi_devtool_run.h"

struct tapi_cc_app {
    /** Compiler job. */
    tapi_devtool_run run;
};

const tapi_cc_opt tapi_cc_default_opt = {
    .toolchain = NULL,
    .compiler  = NULL,
    .lang      = TAPI_DEVTOOL_LANG_C,
};

static const tapi_job_opt_bind cc_binds[] = TAPI_JOB_OPT_SET(
    TAPI_JOB_OPT_STRING("-std=", true, tapi_cc_opt, std),
    TAPI_JOB_OPT_STRING("-O", true, tapi_cc_opt, opt_level),
    TAPI_JOB_OPT_BOOL("-g", tapi_cc_opt, debug),
    TAPI_JOB_OPT_BOOL("-Wall", tapi_cc_opt, warn_all),
    TAPI_JOB_OPT_BOOL("-Wextra", tapi_cc_opt, warn_extra),
    TAPI_JOB_OPT_BOOL("-Werror", tapi_cc_opt, warn_error),
    TAPI_JOB_OPT_BOOL("-pedantic", tapi_cc_opt, pedantic),
    TAPI_JOB_OPT_BOOL("-fPIC", tapi_cc_opt, pic),
    TAPI_JOB_OPT_BOOL("-shared", tapi_cc_opt, shared),
    TAPI_JOB_OPT_BOOL("-c", tapi_cc_opt, compile_only),
    TAPI_JOB_OPT_BOOL("-E", tapi_cc_opt, preprocess_only),
    TAPI_JOB_OPT_BOOL("-fsyntax-only", tapi_cc_opt, syntax_only),
    TAPI_JOB_OPT_STRING("-o", false, tapi_cc_opt, output),
    TAPI_JOB_OPT_ARRAY_PTR(tapi_cc_opt, n_include_dirs, include_dirs,
        TAPI_JOB_OPT_CONTENT(TAPI_JOB_OPT_STRING, "-I", true)),
    TAPI_JOB_OPT_ARRAY_PTR(tapi_cc_opt, n_defines, defines,
        TAPI_JOB_OPT_CONTENT(TAPI_JOB_OPT_STRING, "-D", true)),
    TAPI_JOB_OPT_ARRAY_PTR(tapi_cc_opt, n_undefines, undefines,
        TAPI_JOB_OPT_CONTENT(TAPI_JOB_OPT_STRING, "-U", true)),
    TAPI_JOB_OPT_ARRAY_PTR(tapi_cc_opt, n_cflags, cflags,
        TAPI_JOB_OPT_CONTENT(TAPI_JOB_OPT_STRING, NULL, false)),
    TAPI_JOB_OPT_ARRAY_PTR(tapi_cc_opt, n_sources, sources,
        TAPI_JOB_OPT_CONTENT(TAPI_JOB_OPT_STRING, NULL, false)),
    /*
     * Library search paths and libraries go after the sources: with
     * static libraries the linker resolves symbols in command line
     * order, so the other way round links nothing.
     */
    TAPI_JOB_OPT_ARRAY_PTR(tapi_cc_opt, n_lib_dirs, lib_dirs,
        TAPI_JOB_OPT_CONTENT(TAPI_JOB_OPT_STRING, "-L", true)),
    TAPI_JOB_OPT_ARRAY_PTR(tapi_cc_opt, n_libs, libs,
        TAPI_JOB_OPT_CONTENT(TAPI_JOB_OPT_STRING, "-l", true)),
    TAPI_JOB_OPT_ARRAY_PTR(tapi_cc_opt, n_ldflags, ldflags,
        TAPI_JOB_OPT_CONTENT(TAPI_JOB_OPT_STRING, NULL, false))
);

/** Compiler to run for @p opt. */
static const char *
cc_program(const tapi_cc_opt *opt)
{
    if (opt->compiler != NULL)
        return opt->compiler;

    return tapi_devtool_compiler(opt->toolchain, opt->lang);
}

/* See description in tapi_cc.h */
te_errno
tapi_cc_create(tapi_job_factory_t *factory, const tapi_cc_opt *opt,
               tapi_cc_app **app)
{
    const char *program = cc_program(opt);
    tapi_cc_app *result;
    te_errno rc;

    result = TE_ALLOC(sizeof(*result));
    result->run = (tapi_devtool_run)TAPI_DEVTOOL_RUN_INIT;

    rc = tapi_devtool_run_init(&result->run, factory, program, program,
                               cc_binds, opt, opt->workdir);
    if (rc != 0)
    {
        free(result);
        return rc;
    }

    *app = result;

    return 0;
}

/* See description in tapi_cc.h */
te_errno
tapi_cc_start(tapi_cc_app *app)
{
    return tapi_devtool_run_start(&app->run);
}

/* See description in tapi_cc.h */
te_errno
tapi_cc_wait(tapi_cc_app *app, int timeout_ms)
{
    te_errno rc;

    rc = tapi_devtool_run_wait(&app->run, timeout_ms);
    if (rc != 0)
        return rc;

    return tapi_devtool_run_check(&app->run);
}

/* See description in tapi_cc.h */
te_errno
tapi_cc_kill(tapi_cc_app *app, int signum)
{
    return tapi_devtool_run_kill(&app->run, signum);
}

/* See description in tapi_cc.h */
te_errno
tapi_cc_stop(tapi_cc_app *app)
{
    return tapi_devtool_run_stop(&app->run);
}

/* See description in tapi_cc.h */
void
tapi_cc_get_output(const tapi_cc_app *app, tapi_devtool_output *output)
{
    tapi_devtool_run_get_output(&app->run, output);
}

/* See description in tapi_cc.h */
te_errno
tapi_cc_destroy(tapi_cc_app *app)
{
    te_errno rc;

    if (app == NULL)
        return 0;

    rc = tapi_devtool_run_fini(&app->run);
    free(app);

    return rc;
}

/* See description in tapi_cc.h */
te_errno
tapi_cc_do(tapi_job_factory_t *factory, const tapi_cc_opt *opt,
           int timeout_ms, tapi_cc_app **app)
{
    te_errno rc;

    rc = tapi_cc_create(factory, opt, app);
    if (rc != 0)
        return rc;

    rc = tapi_cc_start(*app);
    if (rc != 0)
        return rc;

    return tapi_cc_wait(*app, timeout_ms);
}

/* See description in tapi_cc.h */
te_errno
tapi_cc_check_snippet(tapi_job_factory_t *factory, const tapi_cc_opt *opt,
                      int timeout_ms, const char *snippet_fmt, ...)
{
    tapi_cc_opt probe_opt = (opt != NULL) ? *opt : tapi_cc_default_opt;
    const char *ta = tapi_job_factory_ta(factory);
    te_string source = TE_STRING_INIT;
    te_string object = TE_STRING_INIT;
    te_string content = TE_STRING_INIT;
    tapi_devtool_output output;
    bool source_created = false;
    bool object_created = false;
    const char *sources[1];
    char *tmp_dir = NULL;
    tapi_cc_app *app = NULL;
    va_list ap;
    te_errno rc;

    if (ta == NULL)
    {
        ERROR("Cannot determine the agent behind the job factory");
        return TE_RC(TE_TAPI, TE_EINVAL);
    }

    tmp_dir = tapi_cfg_base_get_ta_dir(ta, TAPI_CFG_BASE_TA_DIR_TMP);
    if (tmp_dir == NULL)
    {
        ERROR("Failed to get the temporary directory of TA %s", ta);
        return TE_RC(TE_TAPI, TE_EFAIL);
    }

    tapi_file_make_custom_pathname(&source, tmp_dir,
            probe_opt.lang == TAPI_DEVTOOL_LANG_CXX ? ".cc" : ".c");
    tapi_file_make_custom_pathname(&object, tmp_dir, ".o");

    va_start(ap, snippet_fmt);
    te_string_append_va(&content, snippet_fmt, ap);
    va_end(ap);

    rc = tapi_file_create_ta(ta, source.ptr, "%s", te_string_value(&content));
    if (rc != 0)
    {
        ERROR("Failed to put the snippet on TA %s: %r", ta, rc);
        goto out;
    }
    source_created = true;

    sources[0] = source.ptr;
    probe_opt.sources = sources;
    probe_opt.n_sources = 1;
    probe_opt.output = object.ptr;
    probe_opt.compile_only = true;
    probe_opt.preprocess_only = false;
    probe_opt.syntax_only = false;

    rc = tapi_cc_create(factory, &probe_opt, &app);
    if (rc != 0)
        goto out;

    rc = tapi_cc_start(app);
    if (rc != 0)
        goto out;

    /*
     * A snippet that does not build is the expected answer of a probe,
     * not a failure of the test, so the status is examined here instead
     * of going through tapi_cc_wait(), which would log it as an error.
     */
    rc = tapi_devtool_run_wait(&app->run, timeout_ms);
    if (rc != 0)
        goto out;

    tapi_cc_get_output(app, &output);
    if (output.status.type == TAPI_JOB_STATUS_EXITED &&
        output.status.value == 0)
    {
        /* The object file only exists if the compilation succeeded. */
        object_created = true;
        rc = 0;
    }
    else
    {
        RING("The snippet does not build on TA %s:\n%s", ta, output.err);
        rc = TE_RC(TE_TAPI, TE_ESHCMD);
    }

out:
    tapi_cc_destroy(app);
    if (object_created)
        tapi_file_ta_unlink_fmt(ta, "%s", object.ptr);
    if (source_created)
        tapi_file_ta_unlink_fmt(ta, "%s", source.ptr);
    free(tmp_dir);
    te_string_free(&source);
    te_string_free(&object);
    te_string_free(&content);

    return rc;
}
