/*
    ChibiOS - Copyright (C) 2006..2018 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

/**
 * @file    simulator/posix/hal_can_lld.h
 * @brief   Posix simulator low level CAN driver header.
 *
 * @addtogroup POSIX_CAN
 * @{
 */

#ifndef HAL_CAN_LLD_H
#define HAL_CAN_LLD_H

#if HAL_USE_CAN || defined(__DOXYGEN__)

#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <sys/ioctl.h>
#include <poll.h>

#define CAN_TX_MAILBOXES            1
#define CAN_RX_MAILBOXES            1

#if !defined(CAN_RX_FIFO_SIZE) || defined(__DOXYGEN__)
#define CAN_RX_FIFO_SIZE 4
#endif

#define CAN_IDE_STD 0 /**< @brief Standard id. */
#define CAN_IDE_EXT 1 /**< @brief Extended id. */

#define CAN_RTR_DATA   0 /**< @brief Data frame.   */
#define CAN_RTR_REMOTE 1 /**< @brief Remote frame. */

#define CAN_ERR_DATA  0 /**< @brief Data frame.  */
#define CAN_ERR_ERROR 1 /**< @brief Error frame. */

/**
 * @brief   Type of a structure representing an CAN driver.
 */
typedef struct CANDriver CANDriver;

/**
 * @brief   Type of a transmission mailbox index.
 */
typedef uint8_t canmbx_t;

#if (CAN_ENFORCE_USE_CALLBACKS == TRUE) || defined(__DOXYGEN__)
typedef void (*can_callback_t)(CANDriver *canp, uint32_t flags);
#endif /* CAN_ENFORCE_USE_CALLBACKS == TRUE */

typedef struct {
  uint8_t  ERR : 1;
  uint8_t  RTR : 1;

  uint8_t  IDE : 1;
  uint32_t CAN_ID : 29; /**< @brief CAN ID (standard or remote) */

  uint8_t DLC;
  union {
    uint8_t  data8[8];  /**< @brief Frame data. */
    uint16_t data16[4]; /**< @brief Frame data. */
    uint32_t data32[2]; /**< @brief Frame data. */
    uint64_t data64[1]; /**< @brief Frame data. */
  };
} CANTxFrame;

typedef struct {
  uint8_t  ERR : 1;
  uint8_t  RTR : 1;

  uint8_t  IDE : 1;
  uint32_t CAN_ID : 29; /**< @brief CAN ID (standard or remote) */

  uint8_t DLC;
  union {
    uint8_t  data8[8];  /**< @brief Frame data. */
    uint16_t data16[4]; /**< @brief Frame data. */
    uint32_t data32[2]; /**< @brief Frame data. */
    uint64_t data64[1]; /**< @brief Frame data. */
  };
} CANRxFrame;

typedef struct {
  const char* channel_name;
} CANConfig;

struct CANDriver{
  /**
   * @brief Driver state.
   */
  canstate_t state;

  /**
   * @brief Current configuration data.
   */
  const CANConfig *config;

  /**
   * @brief Transmission threads queue.
   */
  threads_queue_t txqueue;

  /**
   * @brief Receive threads queue.
   */
  threads_queue_t rxqueue;

#if (CAN_ENFORCE_USE_CALLBACKS == FALSE) || defined(__DOXYGEN__)
  /**
   * @brief   One or more frames become available.
   */
  event_source_t rxfull_event;

  /**
   * @brief   The transmission mailbox become available.
   */
  event_source_t txempty_event;

  /**
   * @brief   A CAN bus error happened.
   */
  event_source_t error_event;

#if CAN_USE_SLEEP_MODE || defined (__DOXYGEN__)
  /**
   * @brief   Entering sleep state event.
   */
  event_source_t sleep_event;

  /**
   * @brief   Exiting sleep state event.
   */
  event_source_t wakeup_event;
#endif /* CAN_USE_SLEEP_MODE */

#else /* CAN_ENFORCE_USE_CALLBACKS == TRUE */
  /**
   * @brief   One or more frames become available.
   */
  can_callback_t rxfull_cb;

  /**
   * @brief   The transmission mailbox become available.
   */
  can_callback_t txempty_cb;

  /**
   * @brief   A CAN bus error happened.
   */
  can_callback_t error_cb;

#if (CAN_USE_SLEEP_MODE == TRUE) || defined (__DOXYGEN__)
  /**
   * @brief   Exiting sleep state.
   */
  can_callback_t wakeup_cb;
#endif /* CAN_USE_SLEEP_MODE == TRUE */
#endif /* CAN_ENFORE_USE_CALLBACKS */
  /* End of the mandatory fields.*/

  /**
   * @brief The file descriptor of the socket.
   */
  int socket_fd;

  /**
   * @brief TODO
   */
  struct ifreq ifr;

  /**
   * @brief TODO
   */
  struct sockaddr_can addr;

  /**
   * @brief TODO
   */
  CANRxFrame rx_input_buffer[CAN_RX_FIFO_SIZE];

  /**
   * @brief TODO
   */
  input_buffers_queue_t rx_input_queue;
};

/*===========================================================================*/
/* Driver macros.                                                            */
/*===========================================================================*/

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

extern CANDriver CAND1;

#ifdef __cplusplus
extern "C" {
#endif
  void can_lld_init(void);
  void can_lld_start(CANDriver *canp);
  void can_lld_stop(CANDriver *canp);
  bool can_lld_is_tx_empty(CANDriver *canp, canmbx_t mailbox);
  void can_lld_transmit(CANDriver *canp,
                        canmbx_t mailbox,
                        const CANTxFrame *crfp);
  bool can_lld_is_rx_nonempty(CANDriver *canp, canmbx_t mailbox);
  void can_lld_receive(CANDriver *canp,
                       canmbx_t mailbox,
                       CANRxFrame *ctfp);
  void can_lld_abort(CANDriver *canp, canmbx_t mailbox);
#if CAN_USE_SLEEP_MODE
  void can_lld_sleep(CANDriver *canp);
  void can_lld_wakeup(CANDriver *canp);
#endif /* CAN_USE_SLEEP_MODE */
  bool can_lld_serve_interrupt(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_USE_CAN */

#endif /* HAL_CAN_LLD_H */

/** @} */