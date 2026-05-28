/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Running a development tool on a Test Agent
 *
 * @defgroup tapi_devtool_run Running a tool and capturing its output
 * @ingroup tapi_devtool
 * @{
 *
 * The primitive every tool TAPI of this library is built on: take an
 * option structure and its bindings, build an argument vector, run the
 * program on a Test Agent in a working directory, capture both output
 * streams and remember how it exited.
 *
 * A TAPI for a tool this library does not cover - and the kernel TAPIs
 * of tsf-kernel are exactly that - gets the whole lifecycle by holding
 * one #tapi_devtool_run and forwarding to the functions below. Tests
 * normally use the tool TAPIs instead of calling this directly.
 *
 * @note Reading what a tool printed needs output channels and filters,
 *       which only the RPC job factory implements; see @ref tapi_devtool.
 */

#ifndef __TSF_TAPI_DEVTOOL_RUN_H__
#define __TSF_TAPI_DEVTOOL_RUN_H__

#include "te_defs.h"
#include "te_errno.h"
#include "te_string.h"
#include "tapi_job.h"
#include "tapi_job_opt.h"

#include "tapi_devtool.h"

#ifdef __cplusplus
extern "C" {
#endif

/** A running (or finished) development tool. */
typedef struct tapi_devtool_run {
    /** Job handle. */
    tapi_job_t *job;
    /** Primary output channels: stdout and stderr. */
    tapi_job_channel_t *out_chs[2];
    /** Readable and logged filter over stdout. */
    tapi_job_channel_t *out_filter;
    /** Readable and logged filter over stderr. */
    tapi_job_channel_t *err_filter;
    /** Accumulated stdout of the last run. */
    te_string out;
    /** Accumulated stderr of the last run. */
    te_string err;
    /** Exit status of the last run, valid if @a completed. */
    tapi_job_status_t status;
    /** @c true once the tool has been waited for and did complete. */
    bool completed;
    /** Command line, kept for log messages and verdicts. */
    te_string cmd;
    /** Tool name used in log messages, e.g. @c "cc". */
    const char *name;
} tapi_devtool_run;

/** Static initializer for #tapi_devtool_run. */
#define TAPI_DEVTOOL_RUN_INIT \
    { .out = TE_STRING_INIT, .err = TE_STRING_INIT, .cmd = TE_STRING_INIT }

/**
 * Create a job for @p program with arguments built from @p opt.
 *
 * Both output streams get a filter that is readable by the test and
 * logged: stdout with @c TE_LL_RING, stderr with @c TE_LL_WARNING,
 * since a tool writing to stderr is not by itself a failure.
 *
 * @param[out] run      Run handle, must be zeroed or
 *                      @ref TAPI_DEVTOOL_RUN_INIT initialized.
 * @param[in]  factory  Job factory.
 * @param[in]  name     Tool name for log messages.
 * @param[in]  program  Program name or path.
 * @param[in]  binds    Option bindings.
 * @param[in]  opt      Option structure matching @p binds.
 * @param[in]  workdir  Working directory on the agent (may be @c NULL).
 *
 * @return Status code.
 */
extern te_errno tapi_devtool_run_init(tapi_devtool_run *run,
                                      tapi_job_factory_t *factory,
                                      const char *name,
                                      const char *program,
                                      const tapi_job_opt_bind *binds,
                                      const void *opt,
                                      const char *workdir);

/**
 * Create a job as tapi_devtool_run_init() does, with an environment.
 *
 * A job does not inherit the agent's environment, so @p env is the
 * whole of what the program gets: what is not in it is not there. That
 * is why this is a second function rather than a parameter on the
 * first — a tool that only wants to be run should not have to say
 * anything about an environment it does not care about.
 *
 * The caller that does care is tsf-time, which runs a program under a
 * clock that is not the machine's by preloading a shim into it. An
 * environment is the only way that reaches a process.
 *
 * @param[out] run      Run handle, initialized as above.
 * @param[in]  factory  Job factory.
 * @param[in]  name     Tool name for log messages.
 * @param[in]  program  Program name or path.
 * @param[in]  binds    Option bindings.
 * @param[in]  opt      Option structure matching @p binds.
 * @param[in]  workdir  Working directory on the agent (may be @c NULL).
 * @param[in]  env      @c NULL terminated environment, or @c NULL to
 *                      give the program none.
 *
 * @return Status code.
 */
extern te_errno tapi_devtool_run_init_env(tapi_devtool_run *run,
                                          tapi_job_factory_t *factory,
                                          const char *name,
                                          const char *program,
                                          const tapi_job_opt_bind *binds,
                                          const void *opt,
                                          const char *workdir,
                                          const char **env);

/**
 * Start the tool, discarding the output of a previous run.
 *
 * @param run           Run handle.
 *
 * @return Status code.
 */
extern te_errno tapi_devtool_run_start(tapi_devtool_run *run);

/**
 * Wait until the tool has printed @p needle on its standard output.
 *
 * For a tool that has to be *ready* before anything else happens, and
 * that says so: a server that prints @c ACCEPT once it is listening, a
 * daemon that logs a version line once it has read its configuration.
 * Starting one and carrying on is a race, and it is the kind that
 * passes on a quiet machine and fails in CI - measured on
 * @c s_server, whose client got @c ECONNREFUSED five milliseconds
 * before the server printed @c ACCEPT.
 *
 * What is read here is appended to the run's output rather than
 * consumed, so tapi_devtool_run_wait() still sees the whole of it
 * afterwards.
 *
 * @param run           Run handle of a started tool.
 * @param needle        Text to wait for.
 * @param timeout_ms    How long to wait, ms.
 *
 * @return Status code.
 * @retval TE_ETIMEDOUT The text did not appear, or the stream ended
 *                      first.
 */
extern te_errno tapi_devtool_run_expect(tapi_devtool_run *run,
                                        const char *needle,
                                        int timeout_ms);

/**
 * Wait for the tool and capture everything it printed.
 *
 * The output is captured and the exit status is stored even when the
 * tool fails, which is the point: the diagnostics of a failed build are
 * what the test needs to report.
 *
 * @param run           Run handle.
 * @param timeout_ms    Timeout, ms (negative for the tapi_job default).
 *
 * @return Status code.
 * @retval TE_EINPROGRESS   The tool is still running.
 */
extern te_errno tapi_devtool_run_wait(tapi_devtool_run *run, int timeout_ms);

/**
 * Check the exit status of a completed run and log the diagnostics of
 * a failed one.
 *
 * @param run           Run handle.
 *
 * @return Status code.
 * @retval TE_ESHCMD    The tool was never waited for, or it exited with
 *                      a non-zero status, or it was killed by a signal.
 */
extern te_errno tapi_devtool_run_check(const tapi_devtool_run *run);

/**
 * Send a signal to the tool.
 *
 * @param run           Run handle.
 * @param signum        Signal number.
 *
 * @return Status code.
 */
extern te_errno tapi_devtool_run_kill(tapi_devtool_run *run, int signum);

/**
 * Stop the tool; it can be started over with tapi_devtool_run_start().
 *
 * @param run           Run handle.
 *
 * @return Status code.
 */
extern te_errno tapi_devtool_run_stop(tapi_devtool_run *run);

/**
 * Fill @p output from @p run.
 *
 * @param[in]  run      Run handle.
 * @param[out] output   Output description.
 */
extern void tapi_devtool_run_get_output(const tapi_devtool_run *run,
                                        tapi_devtool_output *output);

/**
 * Destroy the job and release everything @p run holds.
 *
 * @param run           Run handle.
 *
 * @return Status code.
 */
extern te_errno tapi_devtool_run_fini(tapi_devtool_run *run);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TSF_TAPI_DEVTOOL_RUN_H__ */

/**@} <!-- END tapi_devtool_run --> */
