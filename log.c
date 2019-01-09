#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "log.h"
#include "system_config.h" 
#include SYSTEM_CONFIG	

#ifdef VERBOSE_DEBUG
#include "uart.h"
#include "ups.h"
#endif

static const char *level_names[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
static LogLevel loggingLevel = LOG_TRACE;

void log_set_level(LogLevel level)
{
	loggingLevel = level;
}

void log_log(LogLevel level, const char *file, int line, const char* func, const char *fmt, ...) 
{
	va_list args;
	char output[200];

	if (level < loggingLevel) return;

#ifdef VERBOSE_DEBUG   
	sprintf(output, "%s %-5s %s:%d %s: ", runtimeString(), level_names[level], file, line, func);
    usart_putstr(output, VERBOSE_DEBUG_PORT);
#endif
	va_start(args, fmt);
	vsprintf(output, fmt, args);
#ifdef VERBOSE_DEBUG
    usart_putstr(output, VERBOSE_DEBUG_PORT);
#endif
	va_end(args);
	sprintf(output, "\n\r");
#ifdef VERBOSE_DEBUG    
    usart_putstr(output, VERBOSE_DEBUG_PORT);
#endif
}
