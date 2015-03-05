#ifndef __COMPASS_GENERIC_CALIBRATION_H__
#define __COMPASS_GENERIC_CALIBRATION_H__
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif
/* CompassCal_init
 * Initialize calibration algorithm. Must be called at first.
 *
 * If compass has been calibrated before, old calibration data
 * are loaded from calDataFile
 */
void CompassCal_init(FILE *calDataFile);

/* CompassCal_storeResult
 * store the result calibration data into file
 */
void CompassCal_storeResult(FILE *calDataFile);

/* CompassCal_collectData
 * collect compass data to calibrate
 *
 * The input rawdata(rawMagX, rawMagY, rawMaxZ)
 * must be past in unit of micro-Tesla
 * CurrentTimMSec stands for current time in ms.
 *
 * Return 0 if raw data is rejected,
 * return 1 if the data is accepted.
 */
int CompassCal_collectData(float rawMagX, float rawMagY, float rawMagZ,
                           long currentTimeMSec);

/* CompassCal_readyCheck
 * Check if enough raw data has been collected
 * to generate a calibration result.
 *
 * Return 1 if enough raw data has been collected
 * otherwise return 0.
 */
int CompassCal_readyCheck();

/* CompassCal_computeCal
 * Calibrate the raw compass data with current
 * calibration determinants and output the calibrated
 * result.
 *
 * This function should only be called
 * after calling CompassCal_readyCheck and
 * the return value is 1.
 */
void CompassCal_computeCal(float rawX, float rawY, float rawZ, float *resultX,
                          float *resultY, float *resultZ);

#ifdef __cplusplus
}
#endif
#endif /*__COMPASS_CALIBRATION_H__*/
