/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

/**
 * Abbreviation describing the signal `signum`.
 */
const char *sigstr_abbrev(int signum);

/**
 * Description of the signal `signum`.
 */
const char *sigstr_descr(int signum);
