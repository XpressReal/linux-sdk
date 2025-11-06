#pragma once

/**
 * watchdog_kick - kick watchdog
 */
void watchdog_kick(void);

/**
 * watchdog_disable - disable watchdog
 */
void watchdog_disable(void);

/**
 * watchdog_enable - enable watchdog
 * @timeout: timeout value in second (max=159)
 */
void watchdog_enable(unsigned int timeout);
