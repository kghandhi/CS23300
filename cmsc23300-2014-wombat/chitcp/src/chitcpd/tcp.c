/*
 *  chiTCP - A simple, testable TCP stack
 *
 *  Implementation of the TCP protocol.
 *
 *  chiTCP follows a state machine approach to implementing TCP.
 *  This means that there is a handler function for each of
 *  the TCP states (CLOSED, LISTEN, SYN_RCVD, etc.). If an
 *  event (e.g., a packet arrives) while the connection is
 *  in a specific state (e.g., ESTABLISHED), then the handler
 *  function for that state is called, along with information
 *  about the event that just happened.
 *
 *  Each handler function has the following prototype:
 *
 *  int f(serverinfo_t *si, chisocketentry_t *entry, tcp_event_type_t event);
 *
 *  si is a pointer to the chiTCP server info. The functions in
 *       this file will not have to access the data in the server info,
 *       but this pointer is needed to call other functions.
 *
 *  entry is a pointer to the socket entry for the connection that
 *          is being handled. The socket entry contains the actual TCP
 *          data (variables, buffers, etc.), which can be extracted
 *          like this:
 *
 *            tcp_data_t *tcp_data = &entry->socket_state.active.tcp_data;
 *
 *          Other than that, no other fields in "entry" should be read
 *          or modified.
 *
 *  event is the event that has caused the TCP thread to wake up. The
 *          list of possible events corresponds roughly to the ones
 *          specified in http://tools.ietf.org/html/rfc793#section-3.9.
 *          They are:
 *
 *            APPLICATION_CONNECT: Application has called socket_connect()
 *            and a three-way handshake must be initiated.
 *
 *            APPLICATION_SEND: Application has called socket_send() and
 *            there is unsent data in the send buffer.
 *
 *            APPLICATION_RECEIVE: Application has called socket_recv() and
 *            any received-and-acked data in the recv buffer will be
 *            collected by the application (up to the maximum specified
 *            when calling socket_recv).
 *
 *            APPLICATION_CLOSE: Application has called socket_close() and
 *            a connection tear-down should be initiated.
 *
 *            PACKET_ARRIVAL: A packet has arrived through the network and
 *            needs to be processed (RFC 793 calls this "SEGMENT ARRIVES")
 *
 *            TIMEOUT: A timeout (e.g., a retransmission timeout) has
 *            happened.
 *
 */

/*
 *  Copyright (c) 2013-2014, The University of Chicago
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or withsend
 *  modification, are permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 *  - Neither the name of The University of Chicago nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software withsend specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY send OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "chitcp/log.h"
#include "chitcp/buffer.h"
#include "serverinfo.h"
#include "connection.h"
#include "tcp.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

int acceptability_test(tcp_data_t *data, tcp_packet_t *pack);

void send_ACK(serverinfo_t *si, chisocketentry_t *entry, tcp_data_t *data);

void update_WND_and_UNA(tcp_data_t *data, tcp_packet_t *pack);

void always_on_fb(serverinfo_t *si, chisocketentry_t *entry, tcp_data_t *data, tcp_packet_t *pack); 

void use_data(serverinfo_t *si, chisocketentry_t *entry, tcp_data_t *data, tcp_packet_t *pack);

int inside_window(tcp_data_t *data, tcp_packet_t *pack);

int MIN(int a, int b);

int send_all(serverinfo_t *si, chisocketentry_t *entry, tcp_data_t *data);

int chitcpd_tcp_state_handle_CLOSED(serverinfo_t *si, chisocketentry_t *entry, tcp_event_type_t event){
  tcp_data_t *data = &entry->socket_state.active.tcp_data;

  if (event == APPLICATION_CONNECT) { 
    /* Initializes the data */
   
    data->ISS = rand() % 1000 + 1;
    data->SND_UNA = data->ISS;
    data->SND_NXT = data->ISS + 1;
    
    data->RCV_WND = circular_buffer_available(&data->recv);
   
    tcp_packet_t *pack = (tcp_packet_t*)malloc(sizeof(tcp_packet_t));
    chitcpd_tcp_packet_create(entry,pack,NULL,0);
    tcphdr_t *SYN = TCP_PACKET_HEADER(pack);
 
    SYN->seq = chitcp_htonl(data->ISS);
    SYN->ack_seq = chitcp_htonl(data->ISS + 1);
    SYN->syn = 1;
    SYN->win = chitcp_htons(data->RCV_WND);
    
    chilog_tcp(CRITICAL,pack,LOG_OUTBOUND);  
    chitcpd_send_tcp_packet(si,entry,pack);
   
    chitcpd_update_tcp_state(si,entry,SYN_SENT);
    chitcp_tcp_packet_free(pack);
 

  } else if (event == CLEANUP) {
    chitcpd_free_socket_entry(si, entry);
    pthread_exit(NULL);
    /* Any additional cleanup goes here */
  } else
    chilog(WARNING, "In CLOSED state, received unexpected event.");
 
  return CHITCP_OK;
}

int chitcpd_tcp_state_handle_LISTEN(serverinfo_t *si, chisocketentry_t *entry, tcp_event_type_t event){
  tcp_data_t *data = &entry->socket_state.active.tcp_data;

  if (event == PACKET_ARRIVAL) {
    pthread_mutex_lock(&data->lock_pending_packets);
    tcp_packet_t *pack = list_fetch(&(data->pending_packets));
    pthread_mutex_unlock(&data->lock_pending_packets);
    tcphdr_t *head = TCP_PACKET_HEADER(pack);
    if (head->syn) {
      data->RCV_NXT = SEG_SEQ(pack) + 1;
      data->IRS = SEG_SEQ(pack);
      data->ISS = rand() % 1000 + 1;

      data->RCV_WND = circular_buffer_available(&data->recv);     

      tcp_packet_t *new_pack = (tcp_packet_t*)malloc(sizeof(tcp_packet_t));
      chitcpd_tcp_packet_create(entry,new_pack,NULL,0);
      tcphdr_t *SYN_ACK = TCP_PACKET_HEADER(new_pack);

      SYN_ACK->seq = chitcp_htonl(data->ISS);
      SYN_ACK->ack_seq = chitcp_htonl(data->RCV_NXT);
      SYN_ACK->syn = 1;
      SYN_ACK->ack = 1;
      SYN_ACK->win = chitcp_htons(data->RCV_WND);
     
      chilog_tcp(CRITICAL,new_pack,LOG_OUTBOUND);  
      chitcpd_send_tcp_packet(si,entry,new_pack);

      data->SND_WND = SEG_WND(pack);

      data->SND_NXT = data->ISS + 1;
      data->SND_UNA = data->ISS;
      
      chitcpd_update_tcp_state(si,entry,SYN_RCVD);
     
      chitcp_tcp_packet_free(pack);
      chitcp_tcp_packet_free(new_pack);
      return CHITCP_OK;
    }
  } else
    chilog(WARNING, "In LISTEN state, received unexpected event.");
 
  return CHITCP_OK;
}

int chitcpd_tcp_state_handle_SYN_RCVD(serverinfo_t *si, chisocketentry_t *entry, tcp_event_type_t event){
  tcp_data_t *data = &entry->socket_state.active.tcp_data;
  if (event == PACKET_ARRIVAL){
    pthread_mutex_lock(&data->lock_pending_packets);
    tcp_packet_t *pack = list_fetch(&(data->pending_packets));
    pthread_mutex_unlock(&data->lock_pending_packets);
    tcphdr_t *head = TCP_PACKET_HEADER(pack);

    if (acceptability_test(data,pack)){
      //Its acceptable, but is this true in all cases or just receiving information

      if (head->ack && ((data->SND_UNA <= SEG_ACK(pack)) && (SEG_ACK(pack) <= data->SND_NXT))){
	data->SND_UNA = data->SND_NXT;
	data->SND_WND = SEG_WND(pack);
	
	chitcpd_update_tcp_state(si,entry,ESTABLISHED);
      } else {
	send_ACK(si, entry, data);
	data->SND_WND = SEG_WND(pack);
      }
    }
    chitcp_tcp_packet_free(pack);
  } else
    chilog(WARNING, "In SYN_RCVD state, received unexpected event.");
 
  return CHITCP_OK;
}

int chitcpd_tcp_state_handle_SYN_SENT(serverinfo_t *si, chisocketentry_t *entry, tcp_event_type_t event){
  tcp_data_t *data = &entry->socket_state.active.tcp_data;
  if (event == PACKET_ARRIVAL){
    pthread_mutex_lock(&data->lock_pending_packets);
    tcp_packet_t *pack = list_fetch(&(data->pending_packets));
    pthread_mutex_unlock(&data->lock_pending_packets);
    tcphdr_t *head = TCP_PACKET_HEADER(pack);
      /* This condition checks if there is an ack that it is acceptable and there is a syn*/
      if (((head->ack && ((data->SND_UNA <= SEG_ACK(pack)) && (SEG_ACK(pack) <= data->SND_NXT))) || !head->ack) && head->syn){
	
	data->RCV_NXT = SEG_SEQ(pack)+1;
	data->IRS = SEG_SEQ(pack);
	
	if (head->ack){ //only if its an ack
	  data->SND_WND = SEG_WND(pack);
	  data->SND_UNA = SEG_ACK(pack);
	}

	if (data->SND_UNA > data->ISS){
	  send_ACK(si, entry, data);

	  data->SND_UNA = data->SND_NXT;

	  chitcpd_update_tcp_state(si,entry,ESTABLISHED);
 
	  chitcp_tcp_packet_free(pack);
	  return CHITCP_OK;
	} else {
	  tcp_packet_t *new_pack = (tcp_packet_t*)malloc(sizeof(tcp_packet_t));
	  chitcpd_tcp_packet_create(entry,new_pack,NULL,0);
	  tcphdr_t *SYN_ACK = TCP_PACKET_HEADER(new_pack);
	  
	  SYN_ACK->seq = chitcp_htonl(data->ISS);
	  SYN_ACK->ack_seq = chitcp_htonl(data->RCV_NXT);
	  SYN_ACK->syn = 1;
	  SYN_ACK->ack = 1;
	  SYN_ACK->win = chitcp_htons(data->RCV_WND);

	  chilog_tcp(CRITICAL,new_pack,LOG_OUTBOUND);
	  chitcpd_send_tcp_packet(si,entry,new_pack);
	  chitcpd_update_tcp_state(si,entry,SYN_RCVD);
 
	  chitcp_tcp_packet_free(pack);
	  chitcp_tcp_packet_free(new_pack);
	  return CHITCP_OK;
	}
      }
    } else
    chilog(WARNING, "In SYN_SENT state, received unexpected event.");
 
  return CHITCP_OK;
}


int chitcpd_tcp_state_handle_ESTABLISHED(serverinfo_t *si, chisocketentry_t *entry, tcp_event_type_t event){
  tcp_data_t *data = &entry->socket_state.active.tcp_data;
  if (event == APPLICATION_SEND) {      
    send_all(si,entry,data);
  } else if (event == PACKET_ARRIVAL) {
    while (!list_empty(&data->pending_packets) && circular_buffer_available(&data->recv)){
      pthread_mutex_lock(&data->lock_pending_packets);
      tcp_packet_t *pack = list_fetch(&(data->pending_packets));
      pthread_mutex_unlock(&data->lock_pending_packets);
      tcphdr_t * head = TCP_PACKET_HEADER(pack);
    
      if (!acceptability_test(data, pack)) {
	send_ACK(si, entry, data);
	chitcp_tcp_packet_free(pack);
	return CHITCP_OK; 
      }
     
      if (!head->syn && head->ack) {
	if (inside_window(data, pack))
	  update_WND_and_UNA(data, pack);

	if (TCP_PAYLOAD_LEN(pack))
	  use_data(si, entry, data, pack);

	if (head->fin){
	  always_on_fb(si, entry, data, pack);  
	  chitcpd_update_tcp_state(si, entry, CLOSE_WAIT);
	}	
      }
      chitcp_tcp_packet_free(pack);
    }
  } else if (event == APPLICATION_RECEIVE){

    data->RCV_WND = circular_buffer_available(&data->recv); 
  

  } else if (event == APPLICATION_CLOSE){
    
    send_all(si,entry,data);
    
    tcp_packet_t *new_pack = (tcp_packet_t*)malloc(sizeof(tcp_packet_t));
    chitcpd_tcp_packet_create(entry,new_pack,NULL,0);
    tcphdr_t *FIN = TCP_PACKET_HEADER(new_pack);
	
    FIN->seq = chitcp_htonl(data->SND_NXT);
    FIN->ack_seq = chitcp_htonl(data->RCV_NXT);
    FIN->fin = 1;
    FIN->win = chitcp_htons(data->RCV_WND);

    chilog_tcp(CRITICAL,new_pack,LOG_OUTBOUND);
    chitcpd_send_tcp_packet(si,entry,new_pack);
    
    circular_buffer_close(&data->recv);
    circular_buffer_close(&data->send);
    
    chitcpd_update_tcp_state(si,entry,FIN_WAIT_1);

    chitcp_tcp_packet_free(new_pack);
  }
  else
    chilog(WARNING, "In ESTABLISHED state, received unexpected event (%i).", event);

  return CHITCP_OK;
}


int chitcpd_tcp_state_handle_FIN_WAIT_1(serverinfo_t *si, chisocketentry_t *entry, tcp_event_type_t event) {
  tcp_data_t *data = &entry->socket_state.active.tcp_data;
  if (event == PACKET_ARRIVAL) {
    while (!list_empty(&data->pending_packets) && circular_buffer_available(&data->recv)){
    pthread_mutex_lock(&data->lock_pending_packets);
    tcp_packet_t *pack = list_fetch(&(data->pending_packets));
    pthread_mutex_unlock(&data->lock_pending_packets);
    tcphdr_t *head = TCP_PACKET_HEADER(pack);
   
      if (!acceptability_test(data, pack)) {
	send_ACK(si, entry, data);
	chitcp_tcp_packet_free(pack);
	return CHITCP_OK;
      }
      if (head->ack) { 
	if (inside_window(data, pack)) {
	  update_WND_and_UNA(data, pack);
	  chitcpd_update_tcp_state(si, entry, FIN_WAIT_2);
	}
	
	if (TCP_PAYLOAD_LEN(pack)) 
	  use_data(si, entry, data, pack);
	
	if (head->fin){ 
	  always_on_fb(si, entry, data, pack);
	  
	  if (inside_window(data, pack)){
	    chitcpd_update_tcp_state(si, entry, TIME_WAIT);
	  } else {
	    chitcpd_update_tcp_state(si, entry, CLOSING);
	  }
	}
      }
      chitcp_tcp_packet_free(pack);
    }
  } else
    chilog(WARNING, "In FIN_WAIT_1 state, received unexpected event (%i).", event);

  return CHITCP_OK;
}

int chitcpd_tcp_state_handle_FIN_WAIT_2(serverinfo_t *si, chisocketentry_t *entry, tcp_event_type_t event) {
  tcp_data_t *data = &entry->socket_state.active.tcp_data;
  if (event == PACKET_ARRIVAL) {
   
    while (!list_empty(&data->pending_packets) && circular_buffer_available(&data->recv)) {
     pthread_mutex_lock(&data->lock_pending_packets);
     tcp_packet_t *pack = list_fetch(&(data->pending_packets));
     pthread_mutex_unlock(&data->lock_pending_packets);
     tcphdr_t *head = TCP_PACKET_HEADER(pack); 
     if (!acceptability_test(data, pack)) {
	send_ACK(si, entry, data);
	chitcp_tcp_packet_free(pack);
	return CHITCP_OK; 
      }
 
      if (head->ack) { 
	if (inside_window(data, pack))
	  update_WND_and_UNA(data, pack);

	if (TCP_PAYLOAD_LEN(pack)) 
	  use_data(si, entry, data, pack);
	
	if (head->fin) {
	  always_on_fb(si, entry, data, pack); 
	  chitcpd_update_tcp_state(si, entry, TIME_WAIT);
	}
      }
      chitcp_tcp_packet_free(pack);
      }
  } else
    chilog(WARNING, "In FIN_WAIT_2 state, received unexpected event (%i).", event);

  return CHITCP_OK;
}


int chitcpd_tcp_state_handle_CLOSE_WAIT(serverinfo_t *si, chisocketentry_t *entry, tcp_event_type_t event) {
  tcp_data_t *data = &entry->socket_state.active.tcp_data;
  if (event == APPLICATION_CLOSE) {
   
    tcp_packet_t *new_pack = (tcp_packet_t*)malloc(sizeof(tcp_packet_t));
    chitcpd_tcp_packet_create(entry,new_pack,NULL,0);
    tcphdr_t *FIN = TCP_PACKET_HEADER(new_pack);

    send_all(si, entry, data);

    FIN->seq = chitcp_htonl(data->SND_NXT);
    FIN->ack_seq = chitcp_htonl(data->RCV_NXT);
    FIN->fin = 1;
    FIN->win = chitcp_htons(data->RCV_WND);

    chilog_tcp(CRITICAL,new_pack,LOG_OUTBOUND);
    chitcpd_send_tcp_packet(si,entry,new_pack);
    
    circular_buffer_close(&data->recv);
    circular_buffer_close(&data->send);

    chitcpd_update_tcp_state(si,entry,LAST_ACK);
    
    chitcp_tcp_packet_free(new_pack);
  } else if (event == PACKET_ARRIVAL) {
    while (!list_empty(&data->pending_packets)) {
     pthread_mutex_lock(&data->lock_pending_packets);
      tcp_packet_t *pack = list_fetch(&(data->pending_packets));
      pthread_mutex_unlock(&data->lock_pending_packets);
      tcphdr_t *head = TCP_PACKET_HEADER(pack);
      
      if (!acceptability_test(data, pack)) {
	send_ACK(si, entry, data);
	chitcp_tcp_packet_free(pack);
	return CHITCP_OK; 
      }
      if (head->ack) { 
	if (inside_window(data, pack))
	  update_WND_and_UNA(data, pack);

	if (head->fin){
	  always_on_fb(si, entry, data, pack);
	 
	}
      }
      chitcp_tcp_packet_free(pack);
    }
  } else
    chilog(WARNING, "In CLOSE_WAIT state, received unexpected event (%i).", event);

  return CHITCP_OK;
}

int chitcpd_tcp_state_handle_CLOSING(serverinfo_t *si, chisocketentry_t *entry, tcp_event_type_t event) {
  tcp_data_t *data = &entry->socket_state.active.tcp_data;
  if (event == PACKET_ARRIVAL) {
    while (!list_empty(&data->pending_packets)) {
      pthread_mutex_lock(&data->lock_pending_packets);
      tcp_packet_t *pack = list_fetch(&(data->pending_packets));
      pthread_mutex_unlock(&data->lock_pending_packets);
      tcphdr_t *head = TCP_PACKET_HEADER(pack);

      if (!acceptability_test(data, pack)) {
	send_ACK(si, entry, data);
	chitcp_tcp_packet_free(pack);
	return CHITCP_OK; 
      }
      if (head->ack) { 
	if (inside_window(data, pack)) {
	  update_WND_and_UNA(data, pack);
	  chitcpd_update_tcp_state(si, entry, TIME_WAIT);
     
	}

	if (head->fin)
	  always_on_fb(si, entry, data, pack);
      }
      chitcp_tcp_packet_free(pack);
    }
  } else
    chilog(WARNING, "In CLOSING state, received unexpected event (%i).", event);

  return CHITCP_OK;
}

int chitcpd_tcp_state_handle_TIME_WAIT(serverinfo_t *si, chisocketentry_t *entry, tcp_event_type_t event) {
  chilog(WARNING, "Running handler for TIME_WAIT. This should not happen.");

  return CHITCP_OK;
}

int chitcpd_tcp_state_handle_LAST_ACK(serverinfo_t *si, chisocketentry_t *entry, tcp_event_type_t event) {
  tcp_data_t *data = &entry->socket_state.active.tcp_data;
  if (event == PACKET_ARRIVAL) {

    while (!list_empty(&data->pending_packets)){
      pthread_mutex_lock(&data->lock_pending_packets);
      tcp_packet_t *pack = list_fetch(&(data->pending_packets));
      pthread_mutex_unlock(&data->lock_pending_packets);
      
      tcphdr_t *head  = TCP_PACKET_HEADER(pack);
    
      if (!acceptability_test(data, pack)) {
	send_ACK(si, entry, data);
	chitcp_tcp_packet_free(pack);
	return CHITCP_OK; 
      }
      if (head->ack) {
	if (inside_window(data, pack)) {
	  circular_buffer_free(&data->recv);
	  circular_buffer_free(&data->send);
	  list_destroy(&data->pending_packets);
	  pthread_mutex_destroy(&data->lock_pending_packets);
	  pthread_mutex_destroy(&data->lock_withheld_packets);
	  list_destroy(&data->withheld_packets);
	  free(data);
	  
	  chitcpd_update_tcp_state(si, entry, CLOSED);
	}
      }
      chitcp_tcp_packet_free(pack);
    }
  } else
    chilog(WARNING, "In LAST_ACK state, received unexpected event (%i).", event);

  return CHITCP_OK;
}
 
/*                                                           */
/*     Any additional functions you need should go here      */
/*                                                           */

int acceptability_test(tcp_data_t *data, tcp_packet_t *pack) {
  if (!SEG_LEN(pack)) {
    if (!data->RCV_WND) {
      return SEG_SEQ(pack) == data->RCV_NXT;
    } else if (data->RCV_WND > 0) {
      int cond1 = data->RCV_NXT <= SEG_SEQ(pack);
      int cond2 = SEG_SEQ(pack) < data->RCV_NXT + data->RCV_WND;
      return cond1 && cond2;
    }
  } else if (SEG_LEN(pack) > 0 && data->RCV_WND > 0) {
    int cond1a = data->RCV_NXT <= SEG_SEQ(pack);
    int cond1b = SEG_SEQ(pack) < data->RCV_NXT + data->RCV_WND;
    int cond2a = data->RCV_NXT <= SEG_SEQ(pack) + SEG_LEN(pack) - 1;
    int cond2b = SEG_SEQ(pack) + SEG_LEN(pack) - 1 < data->RCV_NXT + data->RCV_WND;
    return (cond1a && cond1b) || (cond2a && cond2b);
  } 
  return 0;
}

void update_WND_and_UNA(tcp_data_t *data, tcp_packet_t *pack) {
  data->SND_UNA = SEG_ACK(pack);
  data->SND_WND = SEG_WND(pack);
  return;
}

void send_ACK(serverinfo_t *si, chisocketentry_t *entry, tcp_data_t *data){
  tcp_packet_t *new_pack = (tcp_packet_t*)malloc(sizeof(tcp_packet_t));
  chitcpd_tcp_packet_create(entry,new_pack,NULL,0);
  tcphdr_t *ACK = TCP_PACKET_HEADER(new_pack);
  ACK->seq = chitcp_htonl(data->SND_NXT);
  ACK->ack_seq = chitcp_htonl(data->RCV_NXT);
  ACK->ack = 1;
  ACK->win = chitcp_htons(data->RCV_WND);

  chilog_tcp(CRITICAL,new_pack,LOG_OUTBOUND);
  chitcpd_send_tcp_packet(si,entry,new_pack);
  chitcp_tcp_packet_free(new_pack);
  return;
}

void always_on_fb(serverinfo_t *si, chisocketentry_t *entry, tcp_data_t *data, tcp_packet_t *pack) {
  data->RCV_NXT = SEG_SEQ(pack); 
  send_ACK(si, entry, data);
  circular_buffer_close(&data->recv);
  return;
}

void use_data(serverinfo_t *si, chisocketentry_t *entry, tcp_data_t *data, tcp_packet_t *pack) {
  uint8_t *payload = TCP_PAYLOAD_START(pack);	  
  int written = circular_buffer_write(&(data->recv), payload, TCP_PAYLOAD_LEN(pack), 1);
   
  data->RCV_NXT += written;
  data->RCV_WND -= written;

  send_ACK(si, entry, data);
  return;
}

int inside_window(tcp_data_t *data, tcp_packet_t *pack) {
  return (data->SND_UNA < SEG_ACK(pack) && SEG_ACK(pack) <= data->SND_NXT);
}

int MIN(int a, int b){
  return (a < b) ? a : b;
}

int send_all(serverinfo_t *si, chisocketentry_t *entry, tcp_data_t *data){
  int sent = 0;
  int to_send = 0;
  int num_allowed = 0;
  tcp_packet_t *new_pack;
  tcphdr_t *ACK;
  uint8_t *dst;
    
  num_allowed = data->SND_UNA + data->SND_WND - data->SND_NXT;
  to_send =  MIN(num_allowed, MIN(MIN(TCP_MSS, data->SND_WND), circular_buffer_count(&data->send)));
    
  while (circular_buffer_count(&data->send) && (to_send > 0)){

    new_pack = (tcp_packet_t*)malloc(sizeof(tcp_packet_t) + to_send);
    dst = (uint8_t*)malloc(sizeof(uint8_t) * to_send);

    sent = circular_buffer_read(&(data->send), dst, to_send, BUFFER_BLOCKING);

    chitcpd_tcp_packet_create(entry, new_pack, dst, sent);
    ACK = TCP_PACKET_HEADER(new_pack);
    ACK->seq = chitcp_htonl(data->SND_NXT);
    ACK->ack_seq = chitcp_htonl(data->RCV_NXT);
    ACK->ack = 1;
    ACK->win = chitcp_htons(data->RCV_WND);

    data->SND_NXT += sent;
    data->SND_WND -= sent;

    chilog_tcp(CRITICAL,new_pack,LOG_OUTBOUND);
    chitcpd_send_tcp_packet(si,entry,new_pack);
   
    to_send = MIN(num_allowed, MIN(MIN(TCP_MSS, data->SND_WND), circular_buffer_count(&data->send)));
    
    chitcp_tcp_packet_free(new_pack);
    free(dst);
  }
  return CHITCP_OK;
}
