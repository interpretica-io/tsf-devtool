/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Development tools TAPI: common definitions
 *
 * Toolchain resolution shared by the build tool TAPIs.
 */

#define TE_LGR_USER "TAPI DEVTOOL"

#include "te_config.h"

#include "tapi_devtool.h"

const tapi_devtool_toolchain tapi_devtool_toolchain_native = {
    .cc            = NULL,
    .cxx           = NULL,
    .make          = NULL,
    .arch          = NULL,
    .cross_compile = NULL,
};

/* See description in tapi_devtool.h */
const char *
tapi_devtool_compiler(const tapi_devtool_toolchain *toolchain,
                      tapi_devtool_lang lang)
{
    const char *program = NULL;

    if (toolchain != NULL)
        program = (lang == TAPI_DEVTOOL_LANG_CXX) ? toolchain->cxx
                                                  : toolchain->cc;

    if (program != NULL)
        return program;

    return (lang == TAPI_DEVTOOL_LANG_CXX) ? "c++" : "cc";
}

/* See description in tapi_devtool.h */
const char *
tapi_devtool_make_program(const tapi_devtool_toolchain *toolchain)
{
    if (toolchain != NULL && toolchain->make != NULL)
        return toolchain->make;

    return "make";
}
