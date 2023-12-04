#pragma once

#include <cstdint>
#include <expected>

#include "libs/board/board.hpp"
#include "libs/common/error.hpp"
#include "libs/mcu/host/host_i2c.hpp"
#include "libs/mcu/host/host_pin.hpp"
#include "libs/mcu/host/zmq_transport.hpp"

namespace board {

struct HostBoard : public Board {
  HostBoard()
      : zmq_transport_{"ipc:///tmp/device_emulator.ipc",
                       "ipc:///tmp/emulator_device.ipc"} {}
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
  mcu::ZmqTransport zmq_transport_;
  mcu::HostPin user_led_1_{"LED 1", zmq_transport_};
  mcu::HostPin user_button_1_{"Button 1", zmq_transport_};
  mcu::HostI2CController i2c1_{};
};
}  // namespace board
