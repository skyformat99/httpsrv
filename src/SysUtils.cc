//
// This file is part of httpsrv
// Copyright (c) Antonino Calderone (antonino.calderone@gmail.com)
// All rights reserved.
// Licensed under the MIT License.
// See COPYING file in the project root for full license information.
//

/* -------------------------------------------------------------------------- */

#include "SysUtils.h"
#include "StrUtils.h"
#include <sstream>
#include <iomanip>
#include <ctime>


/* -------------------------------------------------------------------------- */

void SysUtils::convertDurationInTimeval(const TimeoutInterval &d, timeval &tv)
{
   std::chrono::microseconds usec = std::chrono::duration_cast<std::chrono::microseconds>(d);

   if (usec <= std::chrono::microseconds(0))
   {
      tv.tv_sec = tv.tv_usec = 0;
   }
   else
   {
      tv.tv_sec = static_cast<long>(usec.count() / 1000000LL);
      tv.tv_usec = static_cast<long>(usec.count() % 1000000LL);
   }
}

/* -------------------------------------------------------------------------- */

void SysUtils::getUtcTime(std::string &retTime)
{
   std::stringstream ss;
   std::time_t t = std::time(nullptr);
   std::tm tm = *std::gmtime(&t);

   ss << std::put_time(&tm, "%c %Z");
   retTime = ss.str();
}

/* -------------------------------------------------------------------------- */

#ifdef WIN32

/* -------------------------------------------------------------------------- */
// MS Visual C++

/* -------------------------------------------------------------------------- */

#pragma comment(lib, "Ws2_32.lib")

/* -------------------------------------------------------------------------- */

bool SysUtils::initCommunicationLib()
{
   // Windows Socket library initialization
   WORD wVersionRequested = WINSOCK_VERSION;
   WSADATA wsaData = {0};

   return 0 == WSAStartup(wVersionRequested, &wsaData);
}

/* -------------------------------------------------------------------------- */

int SysUtils::closeSocketFd(int sd)
{
   return ::closesocket(sd);
}

/* -------------------------------------------------------------------------- */

#else

#include <signal.h>

/* -------------------------------------------------------------------------- */
// Other C++ platform

/* -------------------------------------------------------------------------- */

bool SysUtils::initCommunicationLib()
{
   // Prevents a SIGPIPE if it tried to write to a socket that had been 
   // shutdown for writing or isn't connected 
   signal(SIGPIPE, SIG_IGN);
   return true;
}

/* -------------------------------------------------------------------------- */

int SysUtils::closeSocketFd(int sd)
{
   return ::close(sd);
}

/* -------------------------------------------------------------------------- */

#endif
