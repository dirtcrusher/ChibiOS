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
 * @file    simulator/posix/hal_can_lld.c
 * @brief   Posix simulator low level CAN driver source.
 *
 * @addtogroup POSIX_CAN
 * @{
 */

#include "hal.h"

#if HAL_USE_CAN || defined(__DOXYGEN__)

#include <string.h>

CANDriver CAND1;

void can_lld_init(void) {
  canObjectInit(&CAND1);
  CAND1.socket_fd = 0;

  ibqObjectInit(&(CAND1.rx_input_queue),
                false,
                (uint8_t*) (&(CAND1.rx_input_buffer)),
                BQ_BUFFER_SIZE(CAN_RX_FIFO_SIZE, sizeof(CANRxFrame) * CAN_RX_FIFO_SIZE),
                CAN_RX_FIFO_SIZE,
                NULL,
                &CAND1);
}

void can_lld_start(CANDriver *canp) {
  chDbgCheck(canp != NULL);

  canp->socket_fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
  if (canp->socket_fd < 0) {
    osalSysHalt("Socket creating failed");
  }

  strcpy(canp->ifr.ifr_name, canp->config->channel_name);
  ioctl(canp->socket_fd, SIOCGIFINDEX, &canp->ifr);

  memset(&(canp->addr), 0, sizeof(canp->addr));
  canp->addr.can_family = AF_CAN;
  canp->addr.can_ifindex = canp->ifr.ifr_ifindex;

  if (bind(canp->socket_fd, (struct sockaddr*)&(canp->addr), sizeof(canp->addr)) < 0) {
    osalSysHalt("Socket binding failed");
  }
}

void can_lld_stop(CANDriver *canp) {
  chDbgCheck(canp != NULL);

  if (close(canp->socket_fd) < 0) {
    osalSysHalt("Socket close failed");
  }
  canp->socket_fd = 0;
}

bool can_lld_is_tx_empty(CANDriver *canp, canmbx_t mailbox) {
  chDbgCheck(canp != NULL);
  chDbgCheck(mailbox <= CAN_TX_MAILBOXES);

  struct pollfd poll_fd = {
    canp->socket_fd,
    POLLOUT,
    0
  };

  int num_fd = poll(&poll_fd, 1, 0);
  if (num_fd < 0) {
    osalSysHalt("Socket poll error");
  }

  return num_fd != 0;
}

void can_lld_transmit(CANDriver *canp,
                      canmbx_t mailbox,
                      const CANTxFrame *ctfp) {
  chDbgCheck(canp != NULL);
  chDbgCheck(mailbox <= CAN_TX_MAILBOXES);
  chDbgCheck(ctfp != NULL);

  struct can_frame frame;
  frame.can_id =
          (ctfp->ERR == CAN_ERR_ERROR ? CAN_ERR_FLAG : 0u) |
          (ctfp->RTR == CAN_RTR_REMOTE ? CAN_RTR_FLAG : 0u) |
          (ctfp->IDE == CAN_IDE_EXT ? CAN_EFF_FLAG : 0u) |
          (ctfp->CAN_ID);
  frame.can_dlc = ctfp->DLC;
  memcpy(frame.data, ctfp->data8, 8);

  if(write(canp->socket_fd, &frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
    osalSysHalt("Couldn't send CAN frame");
  }
}

bool can_lld_is_rx_nonempty(CANDriver *canp, canmbx_t mailbox) {
  chDbgCheck(canp != NULL);
  chDbgCheck(mailbox <= CAN_RX_MAILBOXES);

  return !ibqIsEmptyI(&(canp->rx_input_queue));
}

void can_lld_receive(CANDriver *canp,
                     canmbx_t mailbox,
                     CANRxFrame *crfp) {
  chDbgCheck(canp != NULL);
  chDbgCheck(mailbox <= CAN_RX_MAILBOXES);
  chDbgCheck(crfp != NULL);

  if (ibqGetFullBufferTimeoutS(&(canp->rx_input_queue), TIME_IMMEDIATE) != MSG_OK) {
    return;
  }
  memcpy(crfp, canp->rx_input_queue.ptr, sizeof(CANRxFrame));
  ibqReleaseEmptyBufferS(&(canp->rx_input_queue));
}

void can_lld_abort(CANDriver *canp, canmbx_t mailbox) {
  chDbgCheck(canp != NULL);
  chDbgCheck(mailbox <= CAN_TX_MAILBOXES);

  // NOOP
}

#if CAN_USE_SLEEP_MODE || defined(__DOXYGEN__)

void can_lld_sleep(CANDriver *canp) {
  chDbgCheck(canp != NULL);
  // NOOP
}

void can_lld_wakeup(CANDriver *canp) {
  chDbgCheck(canp != NULL);
  // NOOP
}

#endif /* CAN_USE_SLEEP_MODE */

static bool can_lld_serve_interrupt_driver(CANDriver* canp) {
  struct pollfd poll_fd = {
    canp->socket_fd,
    POLLIN,
    0
  };

  int num_fd = poll(&poll_fd, 1, 0);
  if (num_fd < 0) {
    osalSysHalt("Socket poll error");
  }

  if (num_fd == 0) {
    return false;
  }

  struct can_frame frame;
  int n = read(canp->socket_fd, &frame, sizeof(struct can_frame));
  if (n < 0) {
    osalSysHalt("Socket read error");
  }

  osalSysLockFromISR();
  CANRxFrame* rx_frame = (CANRxFrame*) ibqGetEmptyBufferI(&(canp->rx_input_queue));

  rx_frame->IDE = (frame.can_id & CAN_EFF_FLAG) == CAN_EFF_FLAG ? CAN_IDE_EXT : CAN_IDE_STD;
  rx_frame->RTR = (frame.can_id & CAN_RTR_FLAG) == CAN_RTR_FLAG ? CAN_RTR_REMOTE : CAN_RTR_DATA;
  rx_frame->ERR = (frame.can_id & CAN_ERR_FLAG) == CAN_ERR_FLAG ? CAN_ERR_ERROR : CAN_ERR_DATA;
  rx_frame->DLC = frame.can_dlc;
  rx_frame->CAN_ID = frame.can_id & CAN_EFF_MASK;
  memcpy(rx_frame->data8, frame.data, 8);

  ibqPostFullBufferI(&(canp->rx_input_queue), sizeof(CANRxFrame));

  chThdDequeueNextI(&(canp->rxqueue), MSG_OK);

  osalSysUnlockFromISR();
  return true;
}

bool can_lld_serve_interrupt(void) {
  OSAL_IRQ_PROLOGUE();

  bool did_interrupt = false;

  if (CAND1.state == CAN_READY) {
    did_interrupt |= can_lld_serve_interrupt_driver(&CAND1);
  }

  OSAL_IRQ_EPILOGUE();

  return did_interrupt;
}

#endif /* HAL_USE_CAN */

/** @} */