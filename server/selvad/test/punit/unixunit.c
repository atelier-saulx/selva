/**
 * @file unixunit.c
 * @brief Unix Unit, a Unix unit test toolchain for PUnit.
 * Copyright (c) 2012, Ninjaware Oy, Olli Vanhoja <olli.vanhoja@ninjaware.fi>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/** @addtogroup PUnit
  * @{
  */

/** @addtogroup Unix_Unit
  * @{
  */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "unixunit.h"

/** @addtogroup Stdin_writer
  * @{
  */

/* Stdin writer variables */
static FILE * uu_stdin_writer = NULL; /*!< Pointer used for writing to stdin */
static int uu_pipe[2]; /* Pipe inodes. */

/**
 * Open pipe.
 * This should be called in test setup.
 */
void uu_open_pipe()
{
    if (pipe(uu_pipe) == -1) {
        fprintf(stderr, "FAILED: Pipe failed.");
        exit(EXIT_FAILURE);
    }
}

/**
 * Opens a file stream for writing to stdin.
 * This should be called before using uu_write_stdin(char * str).
 * @pre uu_open_pipe() should be called before this function.
 */
void uu_open_stdin_writer()
{
    dup2(uu_pipe[0], STDIN_FILENO);
    uu_stdin_writer = fdopen(uu_pipe[1], "w");
    if (uu_stdin_writer == NULL) {
        fprintf(stderr, "FAILED: Cannot open stdin for write access.");
        exit(EXIT_FAILURE);
    }
}

/**
 * Writes a null-terminated string to stdin.
 * @pre uu_open_pipe() should be called before this function.
 * @pre uu_open_stdin_writer() should ve called before this function.
 * @param str a null-terminated string.
 */
void uu_write_stdin(char * str)
{
    if (uu_stdin_writer == NULL) {
        fprintf(stderr, "FAILED: Stdin writer not open.");
        exit(EXIT_FAILURE);
    }
    fwrite(str, 1, strlen(str), uu_stdin_writer);
}

/**
 * Closes the stdin writer.
 * This should be called before executing any code that uses stdin.
 */
void uu_close_stdin_writer()
{
    fclose(uu_stdin_writer);
    uu_stdin_writer = NULL;
}

/**
 * Close pipe.
 * This should be called in test teardown.
 */
void uu_close_pipe()
{
    close(uu_pipe[0]);
    close(uu_pipe[1]);
}

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */
