# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 Interpretica Unipessoal Lda
#
# External TE library (see TE_EXT_REPO): engine-side TAPIs for the
# development tools of a Test Agent - C/C++ compilers, make and the
# kernel module build. Built as a shared library on the engine platform
# and as a static library when agents or agent apps are built.

if get_option('engine')
    build_lib_shared = true
endif
build_lib_static = not get_option('engine') \
    or get_option('agents').split().length() != 0 \
    or get_option('agent-apps').split().length() != 0
install_lib = install_dev

headers += files(
    'tapi_cc.h',
    'tapi_devtool.h',
    'tapi_kbuild.h',
    'tapi_make.h',
)

# tapi_devtool_run.h is internal to this library and is not installed.
sources += files(
    'tapi_cc.c',
    'tapi_devtool.c',
    'tapi_devtool_run.c',
    'tapi_kbuild.c',
    'tapi_make.c',
)

te_libs += [
    'tapi',
    'tapi_job',
]
