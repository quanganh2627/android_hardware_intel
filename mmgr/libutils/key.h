#ifndef __MMGR_UTILS_KEYS__
#define __MMGR_UTILS_KEYS__

typedef void *key_hdle_t;

/**
 * Initializes the module
 *
 * @param [in] inst_id MMGR instance id
 *
 * @return valid handler. Must be freed by key_dispose
 */
key_hdle_t *key_init(size_t inst_id);

/**
 * Disposes the module
 *
 * @param [in] hdle module handler
 *
 */
void key_dispose(key_hdle_t *hdle);

/**
 * Gets the platform key
 *
 * @param [in] unused unused parameter
 *
 * @return the key. Must not be freed by caller
 */
inline const char *key_get_platform(const key_hdle_t *unused)
{
    (void)unused;
    return "ro.board.platform";
}

/**
 * Gets the build type key
 *
 * @param [in] unused unused parameter
 *
 * @return the key. Must not be freed by caller
 */
inline const char *key_get_build_type(const key_hdle_t *unused)
{
    (void)unused;
    return "ro.build.type";
}

/**
 * Gets the telephony state key
 *
 * @param [in] unused unused parameter
 *
 * @return the key. Must not be freed by caller
 */
inline const char *key_get_telephony_state(const key_hdle_t *unused)
{
    (void)unused;
    return "persist.sys.telephony.off";
}

/**
 * Gets AMTL configuration key
 *
 * @param [in] hdle key handler
 *
 * @return the key. Must not be freed by caller
 */
const char *key_get_amtl(const key_hdle_t *hdle);

/**
 * Gets the hash configuration key
 *
 * @param [in] hdle key handler
 *
 * @return the key. Must not be freed by caller
 */
const char *key_get_cfg(const key_hdle_t *hdle);

/**
 * Gets the hash blob key
 *
 * @param [in] hdle key handler
 *
 * @return the key. Must not be freed by caller
 */
const char *key_get_blob(const key_hdle_t *hdle);

/**
 * Gets the modem reboot key
 *
 * @param [in] hdle key handler
 *
 * @return the key. Must not be freed by caller
 */
const char *key_get_reboot_counter(const key_hdle_t *hdle);

/**
 * Gets the modem version key
 *
 * @param [in] hdle key handler
 *
 * @return the key. Must not be freed by caller
 */
const char *key_get_mdm_version(const key_hdle_t *hdle);

#endif
