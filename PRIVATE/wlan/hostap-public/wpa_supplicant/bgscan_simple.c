/*
 * WPA Supplicant - background scan and roaming module: simple
 * Copyright (c) 2009-2010, Jouni Malinen <j@w1.fi>
 * Copyright(c) 2013 - 2014 Intel Mobile Communications GmbH.
 * Copyright(c) 2011 - 2014 Intel Corporation. All rights reserved.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"
#include "eloop.h"
#include "drivers/driver.h"
#include "config_ssid.h"
#include "wpa_supplicant_i.h"
#include "driver_i.h"
#include "scan.h"
#include "bgscan.h"

#define SIGNAL_TRACKING_MODE_THRESHOLD -82 /* used in signal tracking mode */

struct bgscan_simple_data {
	struct wpa_supplicant *wpa_s;
	const struct wpa_ssid *ssid;
	int scan_interval;
	int signal_threshold;
	int short_scan_count; /* counter for scans using short scan interval */
	int max_short_scans; /* maximum times we short-scan before back-off */
	int short_interval; /* use if signal < threshold */
	int long_interval; /* use if signal > threshold */
	struct os_reltime last_bgscan;

	/*
	 * In signal tracking mode, scan is not done periodically, but
	 * only when signal drops below threshold or further.
	 */
	unsigned int signal_tracking_mode;
};


static void bgscan_simple_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct bgscan_simple_data *data = eloop_ctx;
	struct wpa_supplicant *wpa_s = data->wpa_s;
	struct wpa_driver_scan_params params;

	os_memset(&params, 0, sizeof(params));
	params.num_ssids = 1;
	params.ssids[0].ssid = data->ssid->ssid;
	params.ssids[0].ssid_len = data->ssid->ssid_len;
	params.freqs = data->ssid->scan_freq;

	/*
	 * A more advanced bgscan module would learn about most like channels
	 * over time and request scans only for some channels (probing others
	 * every now and then) to reduce effect on the data connection.
	 */

	wpa_printf(MSG_DEBUG, "bgscan simple: Request a background scan");
	if (wpa_supplicant_trigger_scan(wpa_s, &params)) {
		wpa_printf(MSG_DEBUG, "bgscan simple: Failed to trigger scan");
		eloop_register_timeout(data->scan_interval, 0,
				       bgscan_simple_timeout, data, NULL);
	} else {
		if (data->scan_interval == data->short_interval) {
			data->short_scan_count++;
			/*
			 * Spend at most the duration of a long scan interval
			 * scanning at the short scan interval. After that,
			 * revert to the long scan interval.
			 */
			if (data->short_scan_count > data->max_short_scans) {
				data->scan_interval = data->long_interval;
				wpa_printf(MSG_DEBUG, "bgscan simple: Backing "
					   "off to long scan interval");
			}
		} else if (data->short_scan_count > 0) {
			/*
			 * If we lasted a long scan interval without any
			 * CQM triggers, decrease the short-scan count,
			 * which allows 1 more short-scan interval to
			 * occur in the future when CQM triggers.
			 */
			data->short_scan_count--;
		}
		os_get_reltime(&data->last_bgscan);
	}
}


static int bgscan_simple_get_params(struct bgscan_simple_data *data,
				    const char *params)
{
	const char *pos;

	if (params == NULL)
		return 0;

	data->short_interval = atoi(params);

	pos = os_strchr(params, ':');
	if (pos == NULL)
		return 0;
	pos++;
	data->signal_threshold = atoi(pos);
	pos = os_strchr(pos, ':');
	if (pos == NULL) {
		wpa_printf(MSG_ERROR, "bgscan simple: Missing scan interval "
			   "for high signal");
		return -1;
	}
	pos++;
	data->long_interval = atoi(pos);

	return 0;
}


static int bgscan_simple_set_signal_monitor(struct bgscan_simple_data *data,
					    int signal_threshold)
{
	struct wpa_signal_info siginfo;

	/* Poll for signal info to set initial scan interval */
	if (wpa_drv_signal_poll(data->wpa_s, &siginfo) == 0 &&
	    siginfo.current_signal >= signal_threshold)
		data->scan_interval = data->long_interval;
	else
		data->scan_interval = data->short_interval;

	if (wpa_drv_signal_monitor(data->wpa_s, signal_threshold, 4) < 0)
		return -1;

	return 0;
}


static void * bgscan_simple_init(struct wpa_supplicant *wpa_s,
				 const char *params,
				 const struct wpa_ssid *ssid)
{
	struct bgscan_simple_data *data;

	data = os_zalloc(sizeof(*data));
	if (data == NULL)
		return NULL;
	data->wpa_s = wpa_s;
	data->ssid = ssid;
	if (bgscan_simple_get_params(data, params) < 0) {
		os_free(data);
		return NULL;
	}
	if (data->short_interval <= 0)
		data->short_interval = 30;
	if (data->long_interval <= 0)
		data->long_interval = 30;

	wpa_printf(MSG_DEBUG, "bgscan simple: Signal strength threshold %d  "
		   "Short bgscan interval %d  Long bgscan interval %d",
		   data->signal_threshold, data->short_interval,
		   data->long_interval);

	data->max_short_scans = data->long_interval / data->short_interval + 1;

	if (data->signal_threshold &&
	    bgscan_simple_set_signal_monitor(data, data->signal_threshold) < 0)
		wpa_printf(MSG_ERROR, "bgscan simple: Failed to enable "
			   "signal strength monitoring");

	wpa_printf(MSG_DEBUG, "bgscan simple: Init scan interval: %d",
		   data->scan_interval);
	eloop_register_timeout(data->scan_interval, 0, bgscan_simple_timeout,
			       data, NULL);

	/*
	 * This function is called immediately after an association, so it is
	 * reasonable to assume that a scan was completed recently. This makes
	 * us skip an immediate new scan in cases where the current signal
	 * level is below the bgscan threshold.
	 */
	os_get_reltime(&data->last_bgscan);

	return data;
}


static void bgscan_simple_deinit(void *priv)
{
	struct bgscan_simple_data *data = priv;
	eloop_cancel_timeout(bgscan_simple_timeout, data, NULL);
	if (data->signal_threshold || data->signal_tracking_mode)
		wpa_drv_signal_monitor(data->wpa_s, 0, 0);
	os_free(data);
}


static int bgscan_simple_notify_scan(void *priv,
				     struct wpa_scan_results *scan_res)
{
	struct bgscan_simple_data *data = priv;

	wpa_printf(MSG_DEBUG, "bgscan simple: scan result notification");

	eloop_cancel_timeout(bgscan_simple_timeout, data, NULL);
	if (!data->signal_tracking_mode)
		eloop_register_timeout(data->scan_interval, 0,
				       bgscan_simple_timeout, data, NULL);

	/*
	 * A more advanced bgscan could process scan results internally, select
	 * the BSS and request roam if needed. This sample uses the existing
	 * BSS/ESS selection routine. Change this to return 1 if selection is
	 * done inside the bgscan module.
	 */

	return 0;
}


static void bgscan_simple_notify_beacon_loss(void *priv)
{
	wpa_printf(MSG_DEBUG, "bgscan simple: beacon loss");
	/* TODO: speed up background scanning */
}


static void bgscan_simple_notify_signal_change(void *priv, int above,
					       int current_signal,
					       int current_noise,
					       int current_txrate)
{
	struct bgscan_simple_data *data = priv;
	int scan = 0;
	struct os_reltime now;

	if (data->signal_tracking_mode) {
		/*
		 * If the signal dropped below threshold or further 4 dB, always
		 * scan once immediately because if we don't scan now we won't
		 * have a chance to roam. Otherwise there is nothing to do
		 * because we don't do background scanning in signal tracking
		 * mode anyway.
		 */
		if (!above) {
			wpa_printf(MSG_DEBUG,
				   "bgscan simple: Trigger immediate scan");
			eloop_register_timeout(0, 0, bgscan_simple_timeout,
					       data, NULL);
		}
		return;
	}

	if (data->short_interval == data->long_interval ||
	    data->signal_threshold == 0)
		return;

	wpa_printf(MSG_DEBUG, "bgscan simple: signal level changed "
		   "(above=%d current_signal=%d current_noise=%d "
		   "current_txrate=%d))", above, current_signal,
		   current_noise, current_txrate);
	if (data->scan_interval == data->long_interval && !above) {
		wpa_printf(MSG_DEBUG, "bgscan simple: Start using short "
			   "bgscan interval");
		data->scan_interval = data->short_interval;
		os_get_reltime(&now);
		if (now.sec > data->last_bgscan.sec + 1 &&
		    data->short_scan_count <= data->max_short_scans)
			/*
			 * If we haven't just previously (<1 second ago)
			 * performed a scan, and we haven't depleted our
			 * budget for short-scans, perform a scan
			 * immediately.
			 */
			scan = 1;
		else if (data->last_bgscan.sec + data->long_interval >
			 now.sec + data->scan_interval) {
			/*
			 * Restart scan interval timer if currently scheduled
			 * scan is too far in the future.
			 */
			eloop_cancel_timeout(bgscan_simple_timeout, data,
					     NULL);
			eloop_register_timeout(data->scan_interval, 0,
					       bgscan_simple_timeout, data,
					       NULL);
		}
	} else if (data->scan_interval == data->short_interval && above) {
		wpa_printf(MSG_DEBUG, "bgscan simple: Start using long bgscan "
			   "interval");
		data->scan_interval = data->long_interval;
		eloop_cancel_timeout(bgscan_simple_timeout, data, NULL);
		eloop_register_timeout(data->scan_interval, 0,
				       bgscan_simple_timeout, data, NULL);
	} else if (!above) {
		/*
		 * Signal dropped further 4 dB. Request a new scan if we have
		 * not yet scanned in a while.
		 */
		os_get_reltime(&now);
		if (now.sec > data->last_bgscan.sec + 10)
			scan = 1;
	}

	if (scan) {
		wpa_printf(MSG_DEBUG, "bgscan simple: Trigger immediate scan");
		eloop_cancel_timeout(bgscan_simple_timeout, data, NULL);
		eloop_register_timeout(0, 0, bgscan_simple_timeout, data,
				       NULL);
	}
}

static void bgscan_simple_signal_tracking_mode(struct bgscan_simple_data *data)
{
	if (wpa_drv_signal_monitor(data->wpa_s,
				   SIGNAL_TRACKING_MODE_THRESHOLD, 4) < 0) {
		wpa_printf(MSG_ERROR,
			   "bgscan simple: Failed to change signal threshold to %d",
			   SIGNAL_TRACKING_MODE_THRESHOLD);
		return;
	}

	eloop_cancel_timeout(bgscan_simple_timeout, data, NULL);
	data->signal_tracking_mode = 1;
	wpa_printf(MSG_DEBUG, "bgscan_simple: Start signal tracking mode");
}

static void bgscan_simple_normal_mode(struct bgscan_simple_data *data)
{
	struct os_reltime now;
	int scan_interval;

	data->signal_tracking_mode = 0;

	/*
	 * If signal monitor was originally set, restore signal threshold.
	 * Othrewise cancel the signal monitor we set for signal tracking mode.
	 */
	if (!data->signal_threshold)
		wpa_drv_signal_monitor(data->wpa_s, 0, 0);
	else if (bgscan_simple_set_signal_monitor(data,
						  data->signal_threshold) < 0)
		wpa_printf(MSG_ERROR,
			   "bgscan simple: Failed to restore signal threshold to %d",
			   data->signal_threshold);

	os_get_reltime(&now);

	/*
	 * Re-activate background scanning: If we haven't scanned for as long as
	 * the scan interval, trigger scan immediately. Otherwise wait until the
	 * scan interval is over.
	 */
	if (data->last_bgscan.sec + data->scan_interval > now.sec)
		scan_interval = data->scan_interval -
			(now.sec - data->last_bgscan.sec);
	else
		scan_interval = 0;
	eloop_register_timeout(scan_interval, 0, bgscan_simple_timeout, data,
			       NULL);
	wpa_printf(MSG_DEBUG, "bgscan_simple: Start periodic mode");
}

static void bgscan_simple_notify_tcm_changed(void *priv,
					     enum traffic_load traffic_load,
					     int vi_vo_present)
{
	struct bgscan_simple_data *data = priv;
	unsigned int need_signal_tracking = traffic_load == TRAFFIC_LOAD_HIGH ||
		vi_vo_present;

	/*
	 * If notified change doesn't affect bgscan operating mode,
	 * ignore this notification.
	 */
	if (data->signal_tracking_mode == need_signal_tracking)
		return;

	if (need_signal_tracking)
		bgscan_simple_signal_tracking_mode(data);
	else
		bgscan_simple_normal_mode(data);
}

const struct bgscan_ops bgscan_simple_ops = {
	.name = "simple",
	.init = bgscan_simple_init,
	.deinit = bgscan_simple_deinit,
	.notify_scan = bgscan_simple_notify_scan,
	.notify_beacon_loss = bgscan_simple_notify_beacon_loss,
	.notify_signal_change = bgscan_simple_notify_signal_change,
	.notify_tcm_changed = bgscan_simple_notify_tcm_changed,
};
