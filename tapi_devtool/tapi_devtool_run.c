/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Development tools TAPI: internal job wrapper
 *
 * Implementation of the job wrapper shared by the build tool TAPIs.
 */

#define TE_LGR_USER "TAPI DEVTOOL"

#include "te_config.h"

#include <signal.h>

#include "logger_api.h"
#include "te_string.h"
#include "te_vector.h"

#include "tapi_devtool_run.h"

/** Time to wait for the next chunk of an already finished tool, ms. */
#define TAPI_DEVTOOL_RECEIVE_TIMEOUT_MS 1000

/**
 * Read everything a filter has to offer into @p dest.
 *
 * Called after the tool has exited, so the data are already there and
 * the timeout only bounds the final, empty read.
 */
static te_errno
drain_filter(tapi_job_channel_t *filter, te_string *dest)
{
    tapi_job_buffer_t buf = TAPI_JOB_BUFFER_INIT;
    te_errno rc = 0;

    while (true)
    {
        rc = tapi_job_receive(TAPI_JOB_CHANNEL_SET(filter),
                              TAPI_DEVTOOL_RECEIVE_TIMEOUT_MS, &buf);
        if (rc != 0)
        {
            /* Nothing left to read: what the tool printed is in the buffer. */
            if (TE_RC_GET_ERROR(rc) == TE_ETIMEDOUT)
                rc = 0;
            break;
        }

        if (buf.eos)
            break;
    }

    if (rc == 0 && buf.data.ptr != NULL)
        te_string_append(dest, "%s", buf.data.ptr);

    te_string_free(&buf.data);

    return rc;
}

/* See description in tapi_devtool_run.h */
te_errno
tapi_devtool_run_init(tapi_devtool_run *run, tapi_job_factory_t *factory,
                      const char *name, const char *program,
                      const tapi_job_opt_bind *binds, const void *opt,
                      const char *workdir)
{
    te_vec args = TE_VEC_INIT(char *);
    te_string out_name = TE_STRING_INIT;
    te_string err_name = TE_STRING_INIT;
    te_errno rc;

    run->name = name;

    rc = tapi_job_opt_build_args(program, binds, opt, &args);
    if (rc != 0)
    {
        ERROR("Failed to build %s arguments: %r", name, rc);
        goto out;
    }

    te_string_join_vec(&run->cmd, &args, " ");
    te_string_append(&out_name, "%s stdout", name);
    te_string_append(&err_name, "%s stderr", name);

    rc = tapi_job_simple_create(factory,
                        &(tapi_job_simple_desc_t){
                            .name       = name,
                            .program    = program,
                            .argv       = (const char **)args.data.ptr,
                            .job_loc    = &run->job,
                            .stdout_loc = &run->out_chs[0],
                            .stderr_loc = &run->out_chs[1],
                            .filters    = TAPI_JOB_SIMPLE_FILTERS(
                                {
                                    .use_stdout  = true,
                                    .readable    = true,
                                    .log_level   = TE_LL_RING,
                                    .filter_name = out_name.ptr,
                                    .filter_var  = &run->out_filter,
                                },
                                {
                                    .use_stderr  = true,
                                    .readable    = true,
                                    /*
                                     * A tool writing to stderr is not a
                                     * failure by itself: warnings live
                                     * there too. The exit status decides.
                                     */
                                    .log_level   = TE_LL_WARN,
                                    .filter_name = err_name.ptr,
                                    .filter_var  = &run->err_filter,
                                }
                            )
                        });
    if (rc != 0)
    {
        ERROR("Failed to create a job for %s: %r", name, rc);
        goto out;
    }

    if (workdir != NULL)
    {
        rc = tapi_job_set_workdir(run->job, workdir);
        if (rc != 0)
        {
            ERROR("Failed to set working directory '%s' for %s: %r",
                  workdir, name, rc);
            tapi_job_destroy(run->job, TAPI_DEVTOOL_TERM_TIMEOUT_MS);
            run->job = NULL;
            goto out;
        }
    }

out:
    te_vec_deep_free(&args);
    te_string_free(&out_name);
    te_string_free(&err_name);

    if (rc != 0)
        te_string_free(&run->cmd);

    return rc;
}

/* See description in tapi_devtool_run.h */
te_errno
tapi_devtool_run_start(tapi_devtool_run *run)
{
    te_string_reset(&run->out);
    te_string_reset(&run->err);
    run->completed = false;

    RING("Running %s: %s", run->name, te_string_value(&run->cmd));

    return tapi_job_start(run->job);
}

/* See description in tapi_devtool_run.h */
te_errno
tapi_devtool_run_wait(tapi_devtool_run *run, int timeout_ms)
{
    te_errno rc;

    rc = tapi_job_wait(run->job, timeout_ms, &run->status);
    if (rc != 0)
        return rc;

    run->completed = true;

    rc = drain_filter(run->out_filter, &run->out);
    if (rc != 0)
    {
        ERROR("Failed to read stdout of %s: %r", run->name, rc);
        return rc;
    }

    rc = drain_filter(run->err_filter, &run->err);
    if (rc != 0)
    {
        ERROR("Failed to read stderr of %s: %r", run->name, rc);
        return rc;
    }

    return 0;
}

/* See description in tapi_devtool_run.h */
te_errno
tapi_devtool_run_check(const tapi_devtool_run *run)
{
    if (!run->completed)
    {
        ERROR("%s has not been waited for", run->name);
        return TE_RC(TE_TAPI, TE_ESHCMD);
    }

    if (run->status.type == TAPI_JOB_STATUS_EXITED && run->status.value == 0)
        return 0;

    if (run->status.type == TAPI_JOB_STATUS_SIGNALED)
        ERROR("%s was killed by signal %d", run->name, run->status.value);
    else if (run->status.type == TAPI_JOB_STATUS_EXITED)
        ERROR("%s exited with status %d", run->name, run->status.value);
    else
        ERROR("%s terminated for an unknown reason", run->name);

    ERROR("Command line was: %s", te_string_value(&run->cmd));
    if (run->err.len != 0)
        ERROR("%s diagnostics:\n%s", run->name, te_string_value(&run->err));

    return TE_RC(TE_TAPI, TE_ESHCMD);
}

/* See description in tapi_devtool_run.h */
te_errno
tapi_devtool_run_kill(tapi_devtool_run *run, int signum)
{
    return tapi_job_kill(run->job, signum);
}

/* See description in tapi_devtool_run.h */
te_errno
tapi_devtool_run_stop(tapi_devtool_run *run)
{
    return tapi_job_stop(run->job, SIGTERM, TAPI_DEVTOOL_TERM_TIMEOUT_MS);
}

/* See description in tapi_devtool_run.h */
void
tapi_devtool_run_get_output(const tapi_devtool_run *run,
                            tapi_devtool_output *output)
{
    output->out = te_string_value(&run->out);
    output->err = te_string_value(&run->err);
    output->status = run->status;
    output->completed = run->completed;
}

/* See description in tapi_devtool_run.h */
te_errno
tapi_devtool_run_fini(tapi_devtool_run *run)
{
    te_errno rc = 0;

    if (run->job != NULL)
    {
        rc = tapi_job_destroy(run->job, TAPI_DEVTOOL_TERM_TIMEOUT_MS);
        if (rc != 0)
            ERROR("Failed to destroy the %s job: %r", run->name, rc);

        run->job = NULL;
    }

    te_string_free(&run->out);
    te_string_free(&run->err);
    te_string_free(&run->cmd);

    return rc;
}
