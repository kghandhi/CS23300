/**********************************************************************
 * file:  sr_router.c
 * date:  Mon Feb 18 12:50:42 PST 2002
 * Contact: casado@stanford.edu
 *
 * Description:
 *
 * This file contains all the functions that interact directly
 * with the routing table, as well as the main entry method
 * for routing.
 *
 **********************************************************************/

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "sr_if.h"
#include "sr_rt.h"
#include "sr_router.h"
#include "sr_protocol.h"
#include "sr_arpcache.h"
#include "sr_utils.h"

/*---------------------------------------------------------------------
 * Method: sr_init(void)
 * Scope:  Global
 *
 * Initialize the routing subsystem
 *
 *---------------------------------------------------------------------*/

void sr_init(struct sr_instance* sr)
{
    /* REQUIRES */
    assert(sr);

    /* Initialize cache and cache cleanup thread */
    sr_arpcache_init(&(sr->cache));

    pthread_attr_init(&(sr->attr));
    pthread_attr_setdetachstate(&(sr->attr), PTHREAD_CREATE_JOINABLE);
    pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
    pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
    pthread_t thread;

    pthread_create(&thread, &(sr->attr), sr_arpcache_timeout, sr);
    
    /* Add initialization code here! */

} /* -- sr_init -- */

/*---------------------------------------------------------------------
 * Method: sr_handlepacket(uint8_t* p,char* interface)
 * Scope:  Global
 *
 * This method is called each time the router receives a packet on the
 * interface.  The packet buffer, the packet length and the receiving
 * interface are passed in as parameters. The packet is complete with
 * ethernet headers.
 *
 * Note: Both the packet buffer and the character's memory are handled
 * by sr_vns_comm.c that means do NOT delete either.  Make a copy of the
 * packet instead if you intend to keep it around beyond the scope of
 * the method call.
 *
 *---------------------------------------------------------------------*/

/*
 * NOTE: for these two functions we might want to not send a sum and instead
 * calculate the checksum from the other information we put in (type and code)
 * but it's also possible we want to send in the sum; right now I have it as
 * the former
 */
/*sr_icmp_hdr *make_icmp_hdr(uint8_t type, uint8_t code) {
  sr_icmp_hdr *tmp = (sr_icmp_hdr*)malloc(sizeof(sr_icmp_hdr));
  tmp->icmp_type = type;
  tmp->icmp_code = code;
  tmp->icmp_sum = 0;

  tmp->icmp_sum = cksum(tmp, length (sizeof icmp_hdr?));

  return tmp;
}

sr_icmp_t3_hdr *make_icmp_t3_hdr(uint8_t type, uint8_t code, uint8_t data[ICMP_DATA_SIZE]) {
  sr_icmp_t3_hdr *tmp = (sr_icmp_t3_hdr*)malloc(sizeof(sr_icmp_t3_hdr));
  tmp->icmp_type = type;
  tmp->icmp_code = code;
  tmp->icmp_sum = 0;
  tmp->unused = 0;
  tmp->next_mtu = 0;
  tmp->data = data;

  tmp->icmp_sum = cksum(tmp, length (sizeof icmp_hdr?));

  return tmp;
}*/
/*
 * END OF TWO FUNCTIONS I MENTIONED IN COMMENT ABOVE
 */

/*sr_ip_hdr *make_sr_ip_hdr(uint8_t tos, uint16_t len, uint16_t id, 
			  uint16_t off, uint8_t ttl, uint8_t p, 
			  uint16_t sum, uint32_t src, uint32_t dst) {*/
  

void icmp_reply(struct sr_instance* sr, uint8_t* packet, 
		struct sr_if *place2send, int icmptype, int icmpcode) {
  int offset3, offset4;

  sr_ethernet_hdr_t *ethhdr = (sr_ethernet_hdr_t*)(packet);
  int offset1 = sizeof(sr_ethernet_hdr_t);
  sr_ip_hdr_t *iphdr = (sr_ip_hdr_t*)(packet + offset1);
  int offset2 = offset1 + sizeof(sr_ip_hdr_t);
  sr_icmp_hdr_t *icmphdr = (sr_icmp_hdr_t*)(packet + offset2);

  switch(icmptype) {
  case 0 : ; /* echo reply */
    offset3 = sizeof(sr_icmp_hdr_t);
    offset4 = offset2 + offset3;
    uint8_t* pack2send = (uint8_t*)malloc(offset4);
    sr_ethernet_hdr_t* ethpart = (sr_ethernet_hdr_t*)(pack2send);
    sr_ip_hdr_t* ippart = (sr_ip_hdr_t*)(pack2send + offset1);
    sr_icmp_hdr_t* icmppart = (sr_icmp_hdr_t*)(pack2send + offset2);
    

    icmppart->icmp_type = 0;
    icmppart->icmp_code = 0;
    icmppart->icmp_sum = 0;
    uint16_t csum1 = htons(cksum(pack2send + offset2, offset3));
    icmppart->icmp_sum = csum1;
    ippart->ip_hl = 5;
    ippart->ip_v = 4;
    ippart->ip_tos = 0;
    ippart->ip_len = htons(4*ippart->ip_hl + offset3)/* ?????? */;
    ippart->ip_id = 0;
    ippart->ip_off = htons(IP_DF);
    ippart->ip_ttl = htons(INIT_TTL);
    ippart->ip_p = htons(0x001); /* ??? */
    ippart->ip_sum = 0;
    ippart->ip_src = place2send->ip;
    ippart->ip_dst = iphdr->ip_src;

    uint16_t csum = cksum(pack2send + offset1, 4*ippart->ip_hl);
    ippart->ip_sum = htons(csum);

    memcpy(ethpart->ether_dhost, ethhdr->ether_shost, ETHER_ADDR_LEN);
    memcpy(ethpart->ether_shost, place2send->addr, ETHER_ADDR_LEN);

   
    ethpart->ether_type = htons(ethertype_ip);

    sr_send_packet(sr, pack2send, offset4, place2send->name);
    break;
  case 3 :

    switch(icmpcode) {
    case 0 : /* destination net unreachable */
    case 1 : /* destination host unreachable */
    case 3 : ;/* port unreachable */
      offset3 = sizeof(sr_icmp_t3_hdr_t);
      offset4 = offset2 + offset3;
      uint8_t* pack2send = (uint8_t*)malloc(offset4);
      sr_ethernet_hdr_t* ethpart = (sr_ethernet_hdr_t*)(pack2send);
      sr_ip_hdr_t* ippart = (sr_ip_hdr_t*)(pack2send + offset1);
      sr_icmp_t3_hdr_t* icmppart = (sr_icmp_t3_hdr_t*)(pack2send + offset2);
      int i;

      icmppart->icmp_type = 3;
      icmppart->icmp_code = htons(icmphdr->icmp_code);
      icmppart->icmp_sum = 0;
      icmppart->unused = 0;
      icmppart->next_mtu = 0;

      ippart->ip_hl = 5;
      ippart->ip_v = 4;
      ippart->ip_tos = 0;
      ippart->ip_len = htons(4*ippart->ip_hl + offset3)/* ?????? */;
      ippart->ip_id = 0;
      ippart->ip_off = htons(IP_DF);
      ippart->ip_ttl = htons(INIT_TTL);
      ippart->ip_p = htons(0x001); 
      ippart->ip_sum = 0;
      ippart->ip_src = htonl(place2send->ip);
      ippart->ip_dst = htonl(iphdr->ip_src);

      int data2cpy = ippart->ip_hl;
      for (i = 0; i < data2cpy; i++)
	*(icmppart->data + i) = *(pack2send + offset1 + i);
      uint16_t csum = cksum(pack2send + offset2, offset3);
      icmppart->icmp_sum = htons(csum);

      ippart->ip_sum = htons(cksum(pack2send + offset1, 4*ippart->ip_hl));
      memcpy(ethpart->ether_dhost, ethhdr->ether_shost, ETHER_ADDR_LEN);
      memcpy(ethpart->ether_shost, place2send->addr, ETHER_ADDR_LEN);
    
      ethpart->ether_type = htons(ethertype_ip);

      sr_send_packet(sr, pack2send, offset4, place2send->name);
      break;
    default :
      fprintf(stderr, "Error: bad icmp_code for ICMP code 3: %d\n", icmpcode);
      break;
    }
    break;
  case 11 : /* time exceeded */
    switch(icmpcode) {
    case 0 :
      offset3 = sizeof(sr_icmp_hdr_t);
      offset4 = offset2 + offset3;
      uint8_t* pack2send = (uint8_t*)malloc(offset4);
      sr_ethernet_hdr_t* ethpart = (sr_ethernet_hdr_t*)(pack2send);
      sr_ip_hdr_t* ippart = (sr_ip_hdr_t*)(pack2send + offset1);
      sr_icmp_t3_hdr_t* icmppart = (sr_icmp_t3_hdr_t*)(pack2send + offset2);
      

      icmppart->icmp_type = 11;
      icmppart->icmp_code = 0;
      icmppart->icmp_sum = 0;
      icmppart->icmp_sum = cksum(pack2send + offset2, offset3);

      ippart->ip_hl = 5;
      ippart->ip_v = 4;
      ippart->ip_tos = 0;
      ippart->ip_len = htons(4*ippart->ip_hl + offset3);
      ippart->ip_id = 0;
      ippart->ip_off = IP_DF;
      ippart->ip_ttl = htons(INIT_TTL);
      ippart->ip_p = htons(0x001);
      ippart->ip_sum = 0;
      ippart->ip_src = htonl(place2send->ip)/* current location? */;
      ippart->ip_dst = htonl(iphdr->ip_src);

      ippart->ip_sum = cksum(pack2send + offset1, 4*ippart->ip_hl);

      memcpy(ethpart->ether_dhost, ethhdr->ether_shost, ETHER_ADDR_LEN);
      memcpy(ethpart->ether_shost, place2send->addr, ETHER_ADDR_LEN);
      
      ethpart->ether_type = htons(ethertype_ip);

      sr_send_packet(sr, pack2send, offset4, place2send->name);
      break;
    default :
      fprintf(stderr, "Error: bad icmp_code for ICMP code 11: %d\n", icmpcode);
    }
    break;
  default :
    fprintf(stderr, "Error: bad icmp_type: %d\n", icmptype);
    break;
  }
}

void sr_handlepacket(struct sr_instance* sr,
		     uint8_t * packet/* lent */,
		     unsigned int len,
		     char* interface/* lent */)
{
  /* REQUIRES */
  assert(sr);
  assert(packet);
  assert(interface);

  printf("*** -> Received packet of length %d \n",len);
  
  /* fill in code here */
  sr_ethernet_hdr_t *ehdr = (sr_ethernet_hdr_t*)(packet);
  print_hdr_eth(packet);
  switch(ntohs(ehdr->ether_type)) {
  case ethertype_ip: ; 
    
    sr_ip_hdr_t *iphdr = (sr_ip_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t));

    if ((len >= 42)  && (cksum(iphdr, iphdr->ip_hl*4) == 0) ) {
    
      if (ehdr->ether_dhost != sr_get_interface(sr, interface)->addr) {

	if (iphdr->ip_ttl <= 1) {
	
	  icmp_reply(sr, packet, sr_get_interface(sr,interface), 11, 0);
	  break;
	}   

	
	/* Decrement TTL, recompute checksum */
	iphdr->ip_ttl --;
	iphdr->ip_sum = 0;
	uint16_t csum =  cksum(iphdr, iphdr->ip_hl*4);
	iphdr->ip_sum = htons(csum);
	
      
	/* Search routing table for longest prefix match with dest IP */
	struct sr_rt *search = sr->routing_table;

	/* ATTEMPT AT IMPLEMENTING ABOVE */
	struct sr_rt *best_match = NULL;
	while (search != NULL) {
	  
	  if ((search->mask.s_addr & iphdr->ip_dst) == search->dest.s_addr) {
	   
	    if (best_match == NULL || search->mask.s_addr > best_match->mask.s_addr) {

	      best_match = search;
	    }
	  }
	  search = search->next;
	}
	if (best_match == NULL) {
	
	  icmp_reply(sr, packet, sr_get_interface(sr,interface), 3, 1);
	} else {
	  
	  struct sr_arpentry *arpentry = sr_arpcache_lookup(&sr->cache, htonl(best_match->dest.s_addr));
	  if (arpentry != NULL) {
	   
	    /* If its there, forward IP datagram */

	    memcpy(ehdr->ether_dhost, arpentry->mac, ETHER_ADDR_LEN);

	    sr_send_packet(sr, packet, len, best_match->interface);
	  
	    free(arpentry);
	  } else {
	   
	    struct sr_arpreq *req = sr_arpcache_queuereq(&sr->cache,best_match->dest.s_addr,packet, len, best_match->interface);
	    handle_arpreq(sr,req);
	    
	  }
	
	}
      } else {

	if (ntohs(iphdr->ip_p) == 0x001) {
	  /* ping */
	 
	  icmp_reply(sr, packet, sr_get_interface(sr,interface), 0, 0);
	} else {
	 
	  icmp_reply(sr, packet, sr_get_interface(sr,interface), 3, 3);
	}
      }
    }
    break;
  case ethertype_arp : ;    
    /* what to do if ethernet frame is ARP */
    
    printf("what are we doing in arp lol\n");
    sr_arp_hdr_t *arphdr = (sr_arp_hdr_t*)(packet + sizeof(sr_ethernet_hdr_t));

    if (ntohs(arphdr->ar_hrd) == 0x0001) {
      int op_code =  ntohs(arphdr->ar_op);

      if (op_code == arp_op_request) { /* request */

	print_hdr_arp(packet+sizeof(sr_ethernet_hdr_t));
	handle_arpreq_recieved(sr, sr_get_interface(sr, interface), arphdr);

	/* free(arpentry); */
      } else if (op_code == arp_op_reply) { /* reply */

	handle_arprep(sr, arphdr);

	/* free(arpentry); */
	
      }
    }
 
 
    break;
  
  default:
    break;
  }
}/* end sr_ForwardPacket */

