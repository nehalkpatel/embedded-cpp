#pragma once

#include <cstdint>
#include <expected>

#include "board.hpp"
#include "error.hpp"
#include "host_i2c.hpp"
#include "host_pin.hpp"

namespace board {

struct HostBoard : public Board {
  HostBoard(zmq::socket_ref sref) : sref_(sref) {}
  HostBoard(const HostBoard&) = delete;
  HostBoard(HostBoard&&) = delete;
  auto operator=(const HostBoard&) -> HostBoard& = delete;
  auto operator=(HostBoard&&) -> HostBoard& = delete;
  ~HostBoard() override = default;

  auto Init() -> std::expected<void, Error> override;
  auto UserLed1() -> mcu::Pin& override;
  auto UserButton1() -> mcu::Pin& override;
  auto I2C1() -> mcu::I2CController& override;

 private:
  zmq::socket_ref sref_;
  mcu::HostPin user_led_1_{"LED 1", sref_};
  mcu::HostPin user_button_1_{"Button 1", sref_};
  mcu::HostI2CController i2c1_{};
};
}  // namespace board
