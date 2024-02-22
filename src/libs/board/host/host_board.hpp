#pragma once

#include <expected>
#include <functional>
#include <map>
#include <string>

#include "libs/board/board.hpp"
#include "libs/common/error.hpp"
#include "libs/mcu/host/host_i2c.hpp"
#include "libs/mcu/host/host_pin.hpp"
#include "libs/mcu/host/message_dispatcher.hpp"
#include "libs/mcu/host/message_receiver.hpp"
#include "libs/mcu/host/zmq_transport.hpp"

namespace board {

struct HostBoard : public Board {
  HostBoard() = default;
  HostBoard(const HostBoard&) = delete;
  HostBoard(HostBoard&&) = delete;
  auto operator=(const HostBoard&) -> HostBoard& = delete;
  auto operator=(HostBoard&&) -> HostBoard& = delete;
  ~HostBoard() override = default;

  auto Init() -> std::expected<void, common::Error> override;
  auto UserLed1() -> mcu::Pin& override;
  auto UserButton1() -> mcu::Pin& override;
  auto I2C1() -> mcu::I2CController& override;

 private:
  mcu::ReceiverMap receiver_map_{};
  mcu::MessageDispatcher dispatcher_{receiver_map_};
  mcu::ZmqTransport zmq_transport_{"ipc:///tmp/device_emulator.ipc",
                                   "ipc:///tmp/emulator_device.ipc",
                                   dispatcher_};
  mcu::HostPin user_led_1_{"LED 1", zmq_transport_};
  mcu::HostPin user_button_1_{"Button 1", zmq_transport_};
  mcu::HostI2CController i2c1_{};
};
}  // namespace board
