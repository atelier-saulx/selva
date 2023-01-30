/*
 * TODO This should be moved to util.
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

struct selva_thread {
    int core_id; /*!< CPU core selected for this thread. */
    pthread_t pthread; /*!< Thread descriptor. */
};

int selva_thread_self_set_core(int core_id);
