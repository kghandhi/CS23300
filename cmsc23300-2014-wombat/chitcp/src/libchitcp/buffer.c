/*
 *  chiTCP - A simple, testable TCP stack
 *
 *  Functions to utilize circular buffer structure
 */

/*
 *  Copyright (c) 2013-2014, The University of Chicago
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
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
 *    software without specific prior written permission.
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
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include "chitcp/buffer.h"

int circular_buffer_init(circular_buffer_t *buf, uint32_t maxsize)
{
  buf->start = 0;
  buf->end = 0;
  buf->seq_initial = 0;
  buf->seq_start = 0;
  buf->seq_end = 0;
  buf->maxsize = maxsize;
  buf->closed = 0;
  buf->empty = 1;
  buf->full = 0;
  pthread_mutex_init(&buf->lock, NULL);
  pthread_cond_init(&buf->notFull, NULL);
  pthread_cond_init(&buf->notEmpty, NULL);
  buf->data = (uint8_t*)malloc(maxsize*sizeof(uint8_t));
  if (buf->data == NULL)
    return CHITCP_ENOMEM;
  for (int i = 0; i < maxsize; i++)
    *(buf->data + i) = 7*i;
  return CHITCP_OK;
}

int circular_buffer_set_seq_initial(circular_buffer_t *buf, uint32_t seq_initial)
{
  buf->seq_initial = seq_initial;
  buf->seq_start = seq_initial;
  buf->seq_end = seq_initial + buf->end;

  return CHITCP_OK;
}


int circular_buffer_write(circular_buffer_t *buf, uint8_t *data, uint32_t len, bool_t blocking)
{
  if (buf->closed)
    return 0;
  
  pthread_mutex_lock(&buf->lock);
  int spaceleft = circular_buffer_available(buf);
  int seq_end_update_offset = 0; //ABE: used for when seq_end must be updated while blocking to avoid screwing things up when we update it before returning
 
  if (spaceleft >= len || blocking == BUFFER_BLOCKING) {
    len = len <= buf->maxsize ? len : buf->maxsize;

    uint32_t space_needed = len;
    uint32_t j = 0;
    uint32_t k = buf->end;
    if (k == buf->start && spaceleft)
      buf->empty = 1;
    while (space_needed) {
      if (k == buf->start && !buf->empty) {
	buf->full = 1;
	buf->end = buf->start;
	seq_end_update_offset += circular_buffer_available(buf);
	buf->seq_end += circular_buffer_available(buf);
	//uint32_t available;
	//while (!(available = circular_buffer_available(buf)))
	while (buf->full) {
	  //if (buf->closed) {
	  //pthread_mutex_unlock(&buf->lock);
	  //return j;
	  pthread_cond_wait (&buf->notFull, &buf->lock);
	}
	if (k == buf->start)
	  buf->empty = 1;
      }
      while ((k != buf->start || buf->empty) && k < buf->maxsize && space_needed) {
	if (buf->closed) {
	  pthread_mutex_unlock(&buf->lock);
	  return j;
	}
	*(buf->data + k++) = *(data + j++);
	pthread_cond_signal(&buf->notEmpty);
	buf->empty = 0;
	space_needed--;
      }
      if (k == buf->maxsize)
	k = 0;
    }
    buf->end = k;
    buf->seq_end += len - seq_end_update_offset;
    pthread_mutex_unlock(&buf->lock);
    return len;
  }
  return CHITCP_EWOULDBLOCK;
}


int circular_buffer_read(circular_buffer_t *buf, uint8_t *dst, uint32_t len, bool_t blocking)
{
  if (buf->closed)
    return 0;

  //int len_buf = circular_buffer_count(buf);
  pthread_mutex_lock(&buf->lock);
  if (blocking == BUFFER_BLOCKING) {
    //while(!len_buf) {
    while(buf->empty) {
      //if (buf->closed)
      //return 0;
      //len_buf = circular_buffer_count(buf);
      pthread_cond_wait (&buf->notEmpty, &buf->lock);
    }
    if (buf->closed)
      return 0;
  } else if (buf->empty) {
    pthread_mutex_unlock(&buf->lock);
    return CHITCP_EWOULDBLOCK;
  }
  
  //pthread_mutex_lock(&buf->lock);
  int len_buf = circular_buffer_count(buf);
  int toread = len < len_buf ? len : len_buf;
  int read_counter = toread;

  if(dst) {
    uint32_t j = 0;
    uint32_t k = buf->start;
    while(read_counter--) {
      if (buf->closed) {
	pthread_mutex_unlock(&buf->lock);
	return 0;
      }
      *(dst + j++) = *(buf->data + k++);
      if (k == buf->maxsize)
	k = 0;
      pthread_cond_signal(&buf->notFull);
      buf->full = 0;
    }
    buf->start = k;
  }
  buf->seq_start += toread;

  if (buf->start == buf->end  && toread)
    buf->empty = 1;

  pthread_mutex_unlock(&buf->lock);
  return toread;
}

int circular_buffer_peek(circular_buffer_t *buf, uint8_t *dst, uint32_t len, bool_t blocking)
{
  if (buf->closed)
    return 0;

  //int len_buf = circular_buffer_count(buf);
  pthread_mutex_lock(&buf->lock);
  if (blocking == BUFFER_BLOCKING) {
    //while(!len_buf) {
    while(buf->empty) {
      //if (buf->closed)
      //return 0;
      //len_buf = circular_buffer_count(buf);
      pthread_cond_wait (&buf->notEmpty, &buf->lock);
    }
    if (buf->closed)
      return 0;
  } else if (buf->empty) {
    pthread_mutex_unlock(&buf->lock);
    return CHITCP_EWOULDBLOCK;
  }
  
  //pthread_mutex_lock(&buf->lock);
  int len_buf = circular_buffer_count(buf);
  int toread = len < len_buf ? len : len_buf;
  int read_counter = toread;

  if(dst) {
    uint32_t j = 0;
    uint32_t k = buf->start;
    while(read_counter--) {
      if (buf->closed) {
	pthread_mutex_unlock(&buf->lock);
	return 0;
      }
      *(dst + j++) = *(buf->data + k++);
      if (k == buf->maxsize)
	k = 0;
      pthread_cond_signal(&buf->notFull);
      buf->full = 0;
    }
  }
  /* int len_buf = circular_buffer_count(buf); */

  /* if (blocking == BUFFER_BLOCKING) */
  /*   while(!len_buf) { */
  /*     if (buf->closed) */
  /* 	return 0; */
  /*     len_buf = circular_buffer_count(buf); */
  /*   } */
  /* else if (!len_buf) */
  /*   return CHITCP_EWOULDBLOCK; */

  /* int toread = len < len_buf ? len : len_buf; */
  /* int read_counter = toread; */

  /* if(dst) { */
  /*   uint32_t j = 0; */
  /*   uint32_t k = buf->start; */
  /*   while(read_counter--) { */
  /*     if (buf->closed) */
  /* 	return 0; */
  /*     *(dst + j++) = *(buf->data + k++); */
  /*     if (k == buf->maxsize) */
  /* 	k = 0; */
  /*   } */
  /* } */

  pthread_mutex_unlock(&buf->lock);
  return toread;
}

int circular_buffer_first(circular_buffer_t *buf)
{
  return buf->seq_start;
}

int circular_buffer_next(circular_buffer_t *buf)
{
  return buf->seq_end;
}

int circular_buffer_capacity(circular_buffer_t *buf)
{
  return buf->maxsize;
}

int circular_buffer_count(circular_buffer_t *buf)
{
  return buf->seq_end - buf->seq_start;
}

int circular_buffer_available(circular_buffer_t *buf)
{
  return buf->maxsize - (buf->seq_end - buf->seq_start);
}

int circular_buffer_dump(circular_buffer_t *buf)
{
  printf("# # # # # # # # # # # # # # # # #\n");

  printf("maxsize: %i\n", buf->maxsize);
  printf("count: %i\n", buf->seq_end - buf->seq_start);

  printf("start: %i\n", buf->start);
  printf("end: %i\n", buf->end);

  for(int i = 0; i < buf->maxsize; i++)
    {
      printf("data[%i] = %i", i, buf->data[i]);
      if(i == buf->start)
	printf("  <<< START");
      if(i == buf->end)
	printf("  <<< END");
      printf("\n");
    }
  printf(" # # # # # # # # # # # # # # # # #\n");

  return CHITCP_OK;
}

int circular_buffer_close(circular_buffer_t *buf)
{
  buf->closed = 1;
  pthread_cond_signal(&buf->notEmpty);
  pthread_cond_signal(&buf->notFull);
  return CHITCP_OK;
}

int circular_buffer_free(circular_buffer_t *buf)
{
  free(buf->data);
  return CHITCP_OK;
}
