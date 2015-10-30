/*
 * metac_debug.h
 *
 *  Created on: Oct 31, 2015
 *      Author: mralex
 */

#ifndef METAC_DEBUG_H_
#define METAC_DEBUG_H_

#include <stdio.h>

/*TODO: to use some logging lib */
#define msg_stddbg(...) fprintf(stderr, ##__VA_ARGS__)
#define msg_stderr(...) fprintf(stderr, ##__VA_ARGS__)


#endif /* METAC_DEBUG_H_ */
