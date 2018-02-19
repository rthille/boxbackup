// --------------------------------------------------------------------------
//
// File
//		Name:    BoxTime.h
//		Purpose: How time is represented
//		Created: 2003/10/08
//
// --------------------------------------------------------------------------

#ifndef BOXTIME__H
#define BOXTIME__H

#include <string>

// Time is presented as a signed 64 bit integer, in microseconds
typedef int64_t box_time_t;

#define NANO_SEC_IN_SEC		(1000000000LL)
#define NANO_SEC_IN_USEC 	(1000)
#define NANO_SEC_IN_USEC_LL (1000LL)
#define MICRO_SEC_IN_SEC 	(1000000)
#define MICRO_SEC_IN_SEC_LL	(1000000LL)
#define MICRO_SEC_IN_MILLI_SEC 	(1000)
#define MILLI_SEC_IN_SEC		(1000)
#define MILLI_SEC_IN_SEC_LL	(1000LL)

box_time_t GetCurrentBoxTime();

inline box_time_t SecondsToBoxTime(time_t Seconds)
{
	return ((box_time_t)Seconds * MICRO_SEC_IN_SEC_LL);
}
inline uint64_t MilliSecondsToBoxTime(int64_t milliseconds)
{
	return ((box_time_t)milliseconds * 1000);
}
inline time_t BoxTimeToSeconds(box_time_t Time)
{
	return Time / MICRO_SEC_IN_SEC_LL;
}
inline uint64_t BoxTimeToMilliSeconds(box_time_t Time)
{
	return Time / MILLI_SEC_IN_SEC_LL;
}
inline uint64_t BoxTimeToMicroSeconds(box_time_t Time)
{
	return Time;
}

std::string FormatTime(box_time_t time, bool includeDate,
	bool showMicros = false);

void ShortSleep(box_time_t duration, bool logDuration);

#endif // BOXTIME__H
