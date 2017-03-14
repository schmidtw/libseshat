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
#ifndef __LIBSESHAT_H__
#define __LIBSESHAT_H__

/**
 *  Registers a service with Seshat.
 *
 *  @service the service name to register with Seshat (example: "webpa")
 *  @url     the url to reach the service at (example: "tcp://127.0.0.1:6543")
 *
 *  @return 0 if successful, error otherwise (Use ERRNO values & list them here)
 */
int seshat_register(const char *service, const char *url);

/**
 *  Discovers how to reach a service.
 *
 *  @service the service to find the url for
 *
 *  @return the url of the service if available, NULL otherwise.
 *  Use ERRNO values & list them here, returns 
 */
char* seshat_discover(const char *service);

#endif
