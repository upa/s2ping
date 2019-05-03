/* s2ping.h*/

#ifndef _S2PING_H_
#define _S2PING_H_

#include <net/ethernet.h>


#define ETH_P_S2PING	0x8950	/* not used, next of NSH */
#define S2PING_VERSION	0x0A

#define S2PING_TYPE_ECHO	0x01
#define S2PING_TYPE_REPLY	0x02

struct s2ping_frame {
	struct ether_header eth;
	uint8_t		ver;
	uint8_t		type;
	uint16_t	rsv;
	uint8_t		src[ETH_ALEN];	/* original addr of src */
	uint64_t	ts;
} __attribute__((packed));


#endif /* _S2PING_H_ */
