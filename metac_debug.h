/*
 * metac_debug.h
 *
 *  Created on: Oct 31, 2015
 *      Author: mralex
 */

#ifndef METAC_DEBUG_H_
#define METAC_DEBUG_H_

#include <stdio.h> /* fprintf, stderr */

#ifdef __cplusplus
#define __LOG_FUNCTION__ __PRETTY_FUNCTION__
#else /* ! __cplusplus*/
#ifdef __GNUC__
#define __LOG_FUNCTION__ __func__
#endif
#endif /*  #ifdef __cplusplus */

/*TODO: to use some logging lib */
#define msg_stddbg(_fmt, ...) fprintf(stderr, "DBG:" __FILE__ ":%d:%s: " _fmt, __LINE__, __LOG_FUNCTION__, ##__VA_ARGS__)
//#define msg_stddbg(_fmt, ...) fprintf(stderr, "DBG:" /* __FILE__ ":%d:%s: " _fmt, __LINE__, __LOG_FUNCTION__*/ _fmt, ##__VA_ARGS__)
#define msg_stderr(_fmt, ...) fprintf(stderr, "ERR:" __FILE__ ":%d:%s: " _fmt, __LINE__, __LOG_FUNCTION__, ##__VA_ARGS__)


#endif /* METAC_DEBUG_H_ */
