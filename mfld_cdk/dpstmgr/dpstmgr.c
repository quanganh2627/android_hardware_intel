#include <cutils/log.h>
#include <stdio.h>
#include <stdlib.h>
#include "dpstmgr.h"

int main(int argc, char** argv)
{
	int ret = 0;
	struct dpstmgr_cmd_hdr cmd_hdr;

	ret = dpst_netlink_init();
	if (ret) {
		LOGE("Init netlink socket FAILD!\n");
		return -EFAULT;
	}

	while (1) {
		dpst_recv_from_kernel(&cmd_hdr);
	}

	return 0;
}
