/* SPDX-License-Identifier: AGPL-3.0-or-later */

#ifndef CHORD_RUN_CHORD_RUN_H
#define CHORD_RUN_CHORD_RUN_H

#include <tempo_utils/status.h>

namespace chord_run {
    tempo_utils::Status chord_run(int argc, const char *argv[]);
}

#endif // CHORD_RUN_CHORD_RUN_H