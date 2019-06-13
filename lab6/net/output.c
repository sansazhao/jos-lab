#include "ns.h"

extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";
	int perm_store; 
	envid_t from_env_store;
	// LAB 6: Your code here:
	// 	- read a packet request (using ipc_recv)
	//	- send the packet to the device driver (using sys_net_send)
	//	do the above things in a loop
	int r;
	while (1) {
		if ((r = ipc_recv(&from_env_store, &nsipcbuf, &perm_store)) < 0)
			panic("ipc_recv: %e", r);
		while ((r = sys_net_send(nsipcbuf.pkt.jp_data, nsipcbuf.pkt.jp_len)) < 0) {
			if (r != -E_FULL_TX)			//if full, repeat sending 
				panic("sys_net_send: %e", r);
		}
	}
}
