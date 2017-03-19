/**
 * Copyright 2017 Comcast Cable Communications Management, LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "libseshat.h"
#include <errno.h>
#include <nanomsg/nn.h>
#include <nanomsg/pipeline.h>
#include <nanomsg/reqrep.h>
#include <uuid/uuid.h>

#include "wrp-c.h"


/*----------------------------------------------------------------------------*/
/*                                   Macros                                   */
/*----------------------------------------------------------------------------*/
/* none */

/*----------------------------------------------------------------------------*/
/*                               Data Structures                              */
/*----------------------------------------------------------------------------*/
/* none */

/*----------------------------------------------------------------------------*/
/*                            File Scoped Variables                           */
/*----------------------------------------------------------------------------*/
static char *__current_url_ = NULL;
static int __scoket_handle_ = -1;

/*----------------------------------------------------------------------------*/
/*                             Function Prototypes                            */
/*----------------------------------------------------------------------------*/
int init_lib_seshat(const char *url);
bool lib_seshat_is_initialized(void);
char *discover_service_data(const char *service);
int register_service_(const char *service, const char *url);
bool send_message(int wrp_request, const char *service,
                  const char *url, char *uuid);
int wait_for_reply(wrp_msg_t **msg, char *uuid_str);


/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/
int shutdown_seshat_lib (void)
{
    // Implement me!!
    
    return 0;
}

/* See libseshat.h for details. */
int seshat_register( const char *service, const char *url )
{
    int result = -1;
    
    assert(service && url && __current_url_);
    
    result = register_service_(service, url);
    errno = EAGAIN; // Need to set this appropriately

    return result;
}

/* See libseshat.h for details. */
char* seshat_discover( const char *service )
{
    
    if (0 == lib_seshat_is_initialized()) {
        if (discover_service_data(service)) {
                errno = EAGAIN; // ?? Who should set this
        } else {
            errno = 0;
        }
    }

    return NULL;
}
/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/
int init_lib_seshat(const char *url) {
    int timeout_val = 5001; // ms
 
    assert(url);

    if (NULL != __current_url_) {
        if (0 == strcmp(url, __current_url_)) {
            return 0; 
        } 
        
        return -1;
    } 
    
    
    __current_url_ = strdup(url);
    
    __scoket_handle_ = nn_socket(AF_SP, NN_REQ);
    
    assert(__scoket_handle_ >= 0);
    
    if (0 != nn_setsockopt (__scoket_handle_, NN_SOL_SOCKET, NN_RCVTIMEO,
            &timeout_val, sizeof(timeout_val))) {
        printf("libseshat: Failed to set wait time out!\n");
        free(__current_url_);
        __current_url_ = NULL;
        return -1;
    }    
    
    if (nn_connect(__scoket_handle_, __current_url_) < 0) {
        printf("libseshat:Socket connect failed!\n");
        nn_shutdown(__scoket_handle_, 0);
        free(__current_url_);
        __current_url_ = NULL;
        return -1;
    }    
    
    return 0;
}


bool lib_seshat_is_initialized(void)
{
    
    return (__current_url_ && __scoket_handle_ >= 0);
    
}


char *discover_service_data(const char *service)
{
    uuid_t uuid;
    char uuid_str[128];
    char *response = NULL;
    
    assert(service);
    
    bzero(uuid_str, 128);
    uuid_generate_time_safe(uuid);
    uuid_unparse_lower(uuid, uuid_str);
    
    if (send_message(WRP_MSG_TYPE__RETREIVE, service,
                     (const char *) NULL, uuid_str))
    {
        wrp_msg_t *msg = NULL;
        if (wait_for_reply(&msg, uuid_str) > 0) {            
            if (200 == msg->u.auth.status && 
                WRP_MSG_TYPE__RETREIVE == msg->msg_type)
            {
              response = strdup(msg->u.crud.payload);
            }
            wrp_free_struct(msg);
       }
    }
    return response;
}

int register_service_(const char *service, const char *url)
{
    uuid_t uuid;
    char uuid_str[128];
    bool result;
    wrp_msg_t *msg = NULL;
    
    bzero(uuid_str, 128);
    uuid_generate_time_safe(uuid);
    uuid_unparse_lower(uuid, uuid_str);
    result = send_message(WRP_MSG_TYPE__SVC_REGISTRATION, service,
                          url, uuid_str
                          );

    if (result && wait_for_reply(&msg, uuid_str) > 0) {
        if (200 == msg->u.auth.status && 
            WRP_MSG_TYPE__SVC_REGISTRATION == msg->msg_type)
        {
            result = true;
        }
        
        wrp_free_struct(msg);
    }    
   
    return (result ? 0 : -1);
}

bool send_message(int wrp_request, const char *service,
                  const char *url, char *uuid)
{
    wrp_msg_t *msg;
    int bytes_sent;
    ssize_t payload_size;
    char *payload_bytes;
    
    assert(service);
    msg = (wrp_msg_t *) malloc(sizeof(wrp_msg_t));
    
    switch (wrp_request) {
        case WRP_MSG_TYPE__RETREIVE:
            msg->u.crud.path = (char *) service;
            break;
        case WRP_MSG_TYPE__SVC_REGISTRATION:
            msg->u.crud.payload = (char *) url;
            msg->u.crud.path    = (char *) service;
            break;
        default : 
            free(msg);
            return false;
    }

    msg->msg_type = wrp_request;
    msg->u.crud.transaction_uuid = strdup(uuid);
    
    payload_size =  wrp_struct_to( (const wrp_msg_t *) msg, WRP_BYTES,
                                   (void **) &payload_bytes);
    
    if (0 >= payload_size) {
       free(msg->u.crud.transaction_uuid);
       free(msg);
       return false;
    }
     
    if ((bytes_sent =  nn_send(__scoket_handle_, payload_bytes, payload_size, 0)) > 0) {
        printf("libseshat: Sent %d bytes (size of struct %d)\n", bytes_sent, (int ) payload_size);
    }

    free(payload_bytes);
    free(msg->u.crud.transaction_uuid);
    free(msg);
    
    return (bytes_sent == (int ) sizeof(wrp_msg_t));
}

/*
 * Caller must free msg on success return of 0.
 * returns -1 on failure 
 */
int wait_for_reply(wrp_msg_t **msg, char *uuid_str) 
{
    int bytes;
    ssize_t wrp_len;
    char *buf;

    bytes = nn_recv (__scoket_handle_, &buf, NN_MSG, 0);

    if (0 >= bytes) {
        return -1;
    }
   
    wrp_len = wrp_to_struct ( buf, bytes, WRP_BYTES, msg);
    
    nn_freemsg(buf);

    if (0 >= wrp_len || (NULL == msg)) {
        return -1;
    }
    
    if ((*msg)->u.crud.transaction_uuid && 
        (*msg)->u.crud.transaction_uuid[0] && 
        strcmp(uuid_str, (*msg)->u.crud.transaction_uuid)) {
      
        return 0;
    } else {
        wrp_free_struct(*msg);
    }
    
    return -1;
}

