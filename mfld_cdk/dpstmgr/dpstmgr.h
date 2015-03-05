#ifndef H_DPSTMGR_HEADER_H
#define H_DPSTMGR_HEADER_H

struct dpstmgr_cmd_hdr {
        unsigned int   	module;		/* module to receive the command */
        unsigned int   	cmd;		/* command from userspace */
        unsigned int   	data_size;	/* data size of command_data by bytes */
        void* 		data;		/* command data */
};

enum dispmgr_dpst_event_enum {
	DISPMGR_DPST_UNKNOWN,
	DISPMGR_DPST_INIT_COMM,
	DISPMGR_DPST_UPDATE_GUARD,
	DISPMGR_DPST_HIST_ENABLE,
	DISPMGR_DPST_HIST_DATA,
	DISPMGR_DPST_BL_SET,
	DISPMGR_DPST_GAMMA_SET,
	DISPMGR_DPST_DIET_ENABLE,
	DISPMGR_DPST_DIET_DISABLE,
	DISPMGR_DPST_GET_MODE,
};

enum dispmgr_module_enum {
	DISPMGR_MOD_UNKNOWN,
	DISPMGR_MOD_NETLINK,
	DISPMGR_MOD_DPST,
};

int dpst_netlink_init(void);
void dpst_recv_from_kernel(void* cmd_hdr);

#endif
