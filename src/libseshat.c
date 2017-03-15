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
int register_service_(const char *service);
bool send_message(int wrp_request, const char *service);
int wait_for_reply(char **buf);


/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/


/* See libseshat.h for details. */
int seshat_register( const char *service, const char *url )
{
    int result = -1;
    
    assert(service && url);
    
    if (0 == init_lib_seshat(url)) {
        result = register_service_(service);
    }
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
    int timeout_val = 5000; // 5 seconds
    
    if (NULL != __current_url_) {
        return 0; 
    } else {
        assert(url);
    }
    
    __current_url_ = (char *) malloc(strlen(url) + 1);
    assert(__current_url_);
    
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
    assert(service);
    if (send_message(WRP_MSG_TYPE__RETREIVE, service)) {
        char *buf = NULL;
        if (wait_for_reply(&buf) > 0) {
           // char *url = NULL;
           // parse buffer which is a wrp message, and get the URL
           // allocate memory for URL
            nn_freemsg(buf);
            // return url;
        }
    }
    return NULL;
}

int register_service_(const char *service)
{
    char *buf = NULL;
    bool result = send_message(WRP_MSG_TYPE__SVC_REGISTRATION, service);

    if (wait_for_reply(&buf) > 0) {
       // char *url = NULL;
       // result = parse buffer which is a wrp message
        nn_freemsg(buf);
    }    
   
    return (result ? 0 : -1);
}

bool send_message(int wrp_request, const char *service)
{
    wrp_msg_t *msg;
    int bytes_sent;
    
    assert(service);
    msg = (wrp_msg_t *) malloc(sizeof(wrp_msg_t));
    
    switch (wrp_request) {
        case WRP_MSG_TYPE__RETREIVE:
            msg->u.crud.path = (char *) service;
            break;
        case WRP_MSG_TYPE__SVC_REGISTRATION:
            msg->u.crud.payload = __current_url_;
            break;
        default : 
            free(msg);
            return false;
    }

    msg->msg_type = wrp_request;
    
    if ((bytes_sent =  nn_send(__scoket_handle_, msg, sizeof(wrp_msg_t), 0)) > 0) {
        printf("libseshat: Sent %d bytes (size of struct %d)\n", bytes_sent, (int ) sizeof(wrp_msg_t));
    }

    free(msg);
    
    return (bytes_sent == (int ) sizeof(wrp_msg_t));
}

/*
 * nanomsg allocates memory for buf
 * caller {char *buf; wait_for_reply(&buf);}
 * caller must free *buf with nn_freemsg()
 */
int wait_for_reply(char **buf) 
{
    int bytes;

    bytes = nn_recv (__scoket_handle_, buf, NN_MSG, 0);
    
    return bytes;
}