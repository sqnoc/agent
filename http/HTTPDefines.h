/*
 * HTTPDefines.h
 *
 *  Created on: 23/lug/2013
 *      Author: stefano
 */

#ifndef HTTPDEFINES_H_
#define HTTPDEFINES_H_

#include <string>

#define CRLF "\015\012"

extern std::string HTTPProtocolPrefix;
extern std::string HTTPHost;
extern std::string HTTPContentType;
extern std::string HTTPContentLength;
extern std::string HTTPUserAgent;

extern std::string HostFromConnectionString(std::string string);

#endif /* HTTPDEFINES_H_ */
