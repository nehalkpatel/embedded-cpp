#pragma once

#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "libs/board/board.hpp"
#include "libs/common/error.hpp"
#include "libs/mcu/host/dispatcher.hpp"
#include "libs/mcu/host/host_i2c.hpp"
#include "libs/mcu/host/host_pin.hpp"
#include "libs/mcu/host/host_uart.hpp"
#include "libs/mcu/host/receiver.hpp"
#include "libs/mcu/host/zmq_transport.hpp"

namespace board {

class HostBoard : public Board {
 public:
  HostBoard() = default;
  HostBoard(const HostBoard&) = delete;
  HostBoard(HostBoard&&) = delete;
  auto operator=(const HostBoard&) -> HostBoard& = delete;
  auto operator=(HostBoard&&) -> HostBoard& = delete;
  ~HostBoard() override = default;

  auto Init() -> std::expected<void, common::Error> override;
  auto UserLed1() -> mcu::OutputPin& override;
  auto UserLed2() -> mcu::OutputPin& override;
  auto UserButton1() -> mcu::InputPin& override;
  auto I2C1() -> mcu::I2CController& override;
  auto Uart1() -> mcu::Uart& override;

 private:
  static constexpr auto IsJson(const std::string_view& message) -> bool {
    return message.starts_with("{") && message.ends_with("}");
  }

  // Store components (order matters for destruction)
  std::unique_ptr<mcu::HostPin> user_led_1_{};
  std::unique_ptr<mcu::HostPin> user_led_2_{};
  std::unique_ptr<mcu::HostPin> user_button_1_{};
  std::unique_ptr<mcu::HostUart> uart_1_{};
  std::unique_ptr<mcu::HostI2CController> i2c_1_{};

  // Receiver map and dispatcher (built in Init() after components exist)
  mcu::ReceiverMap receiver_map_{};
  std::optional<mcu::Dispatcher> dispatcher_{};
  std::unique_ptr<mcu::ZmqTransport> zmq_transport_{};
};
}  // namespace board
