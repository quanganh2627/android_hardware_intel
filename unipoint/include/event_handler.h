/*! \file event_handler.h
 *  \brief The main event loop for the Unipoint driver
 *  \date June 16, 2010
 *  \author praem@sensorstar.com
 */ 
#ifndef EVENT_HANDLER_H_
#define EVENT_HANDLER_H_

#ifdef __cplusplus
extern "C" {
#endif




#define CONFIG_USE_DAVI_ALGORITHM



/*! Print events prior to output.
 */
#define OUTPUT_DEBUG 0

/*! \brief The main event loop of the executable
 *  \return Exit status -- 0 for normal or an error code otherwise.
 */
	int event_loop(char *op, char *name);



#ifdef __cplusplus
}
#endif

#endif/*EVENT_HANDLER_H_*/
