#include <kern/e1000.h>
#include <kern/pmap.h>
#include <inc/string.h>
#include <inc/error.h>

static struct E1000 *base;

#define TX_PKT_SIZE 1518
#define N_TXDESC (PGSIZE / sizeof(struct tx_desc))
struct tx_desc *tx_descs;
char tx_bufs[N_TXDESC][TX_PKT_SIZE];

int
e1000_tx_init()
{
	// Allocate one page for descriptors
	struct PageInfo* page = page_alloc(ALLOC_ZERO);
	tx_descs = (struct tx_desc *)page2kva(page);

	memset(tx_bufs, 0, sizeof(tx_bufs));

	// Initialize all descriptors
	for (int i = 0; i < N_TXDESC; ++i) {
		tx_descs[i].addr = PADDR(tx_bufs[i]);
		tx_descs[i].status |= E1000_TX_STATUS_DD;
	}
	// Set hardward registers
	// Look kern/e1000.h to find useful definations
	base->TDBAL = PADDR(tx_descs);
	base->TDBAH = 0;
	base->TDLEN = PGSIZE;
	base->TDH = 0;
	base->TDT = 0;
	base->TCTL |= E1000_TCTL_EN | E1000_TCTL_PSP | E1000_TCTL_CT_ETHER | E1000_TCTL_COLD_FULL_DUPLEX;
	base->TIPG = E1000_TIPG_DEFAULT;

	return 0;
}

#define N_RXDESC (PGSIZE / sizeof(struct rx_desc))
struct rx_desc *rx_descs;
char rx_bufs[N_RXDESC][2048];

int
e1000_rx_init()
{
	// Allocate one page for descriptors
	struct PageInfo* page = page_alloc(ALLOC_ZERO);
	rx_descs = (struct rx_desc *)page2kva(page);
	memset(rx_bufs, 0, sizeof(rx_bufs));

	// Initialize all descriptors
	// You should allocate some pages as receive buffer ---- of?
	for (int i = 0; i < N_RXDESC; ++i) {
		rx_descs[i].addr = PADDR(rx_bufs[i]);
	}
	// Set hardward registers
	// Look kern/e1000.h to find useful definations
	base->RAL = 0x12005452;	//mac addr 52:54:00:12:34:56 
	base->RAH = 0x56340000;
	base->RDBAL = PADDR(rx_descs);
	base->RDBAH = 0;
	base->RDLEN = PGSIZE;
	base->RDT = N_RXDESC - 1;
	base->RDH = 0;
	base->RCTL |= E1000_RCTL_EN | E1000_RCTL_BSIZE_2048 | E1000_RCTL_SECRC;
	return 0;
}

int
pci_e1000_attach(struct pci_func *pcif)
{
	// Enable PCI function
	pci_func_enable(pcif);
	// Map MMIO region and save the address in 'base;
	base = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);
	assert(base->STATUS == 0x80080783);
	e1000_tx_init();	
	e1000_rx_init();
	
	return 0;
}

int
e1000_tx(const void *buf, uint32_t len)
{
	// Send 'len' bytes in 'buf' to ethernet
	// Hint: buf is a kernel virtual address

	//copying the packet data into the next packet buffer
	uint32_t tdt = base->TDT;
	if (!(tx_descs[tdt].status & E1000_TX_STATUS_DD))	
		return -E_FULL_TX;		//full, return & repeat

	memset(tx_bufs[tdt], 0, TX_PKT_SIZE);	
	memmove(tx_bufs[tdt], buf, len);
	tx_descs[tdt].length = len;
	tx_descs[tdt].cmd |= E1000_TX_CMD_EOP | E1000_TX_CMD_RS;
	tx_descs[tdt].status &= ~E1000_TX_STATUS_DD;

	base->TDT = (tdt + 1) % N_TXDESC;		//update the TDT

	return 0;
}

int
e1000_rx(void *buf, uint32_t len)
{
	// Copy one received buffer to buf
	// You could return -E_AGAIN if there is no packet
	// Check whether the buf is large enough to hold
	// the packet
	// Do not forget to reset the decscriptor and
	// give it back to hardware by modifying RDT
	uint32_t rdt = (base->RDT+1) % N_RXDESC;

	if (!(rx_descs[rdt].status & E1000_RX_STATUS_DD))	//no packet received, try again
		return -E_AGAIN;

	while(rdt == base->RDH);			//spin until des is free
	len = rx_descs[rdt].length;			//length can change
	memmove(buf, rx_bufs[rdt], len);	//copy one received buffer to buf
	
	rx_descs[rdt].status &= ~E1000_RX_STATUS_DD;

	base->RDT = rdt;		//reset, give back RDT
	return len;
}
