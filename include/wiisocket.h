#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
 * wiisocket_init
 * wiisocket library initialisation
 *
 * returns:
 *  1 -> the library is already initialized
 *  0 -> library successfully initialized
 * -1 -> library initialisation already in progress
 * -2 -> library initialisation failed
 * -3 -> network initialisation failed
 * -4 -> devoptab mount failed
 */
int
wiisocket_init(void);

/*
 * wiisocket_async_init
 * asynchronous wiisocket library initialisation
 *
 * returns:
 *  see wiisocket_init
 */
int
wiisocket_async_init(void (*usrcb)(int res, void *usrdata),
                     void *usrdata);

/*
 * wiisocket_deinit
 * wiisocket library deinitialisation
 */
void
wiisocket_deinit(void);

/*
 * wiisocket_get_status
 * get wiisocket library status
 * 
 * returns:
 *  1 -> library successfully initialized
 *  0 -> library not initialized
 * -1 -> library initialization in progress
 */
int
wiisocket_get_status(void);

#ifdef __cplusplus
}
#endif
