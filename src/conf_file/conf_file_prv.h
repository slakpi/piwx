/**
 * @file conf_file_prv.h
 */
#if !defined CONF_FILE_PRV_H
#define CONF_FILE_PRV_H

#include "conf_file.h"
#include "geo.h"
#include "log.h"
#include <stdbool.h>
#include <stdio.h>

/**
 * @brief   Parse configuration settings from a file stream.
 * @param[in,out] cfg     The configuration struct to receive the settings.
 * @param[in]     cfgFile The file stream to parse.
 * @returns True if the parse succeeds, otherwise false.
 */
bool conf_parseStream(PiwxConfig *cfg, FILE *cfgFile);

#endif /* CONF_FILE_PRV_H */
