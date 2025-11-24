#pragma once

#include <expected>
#include <functional>
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

  const mcu::ReceiverMap receiver_map_{
      {IsJson, user_led_1_},
      {IsJson, user_led_2_},
      {IsJson, user_button_1_},
      {IsJson, uart_1_},
      {IsJson, i2c_1_},
  };
  mcu::Dispatcher dispatcher_{receiver_map_};
  mcu::ZmqTransport zmq_transport_{"ipc:///tmp/device_emulator.ipc",
                                   "ipc:///tmp/emulator_device.ipc",
                                   dispatcher_};
  mcu::HostPin user_led_1_{"LED 1", zmq_transport_};
  mcu::HostPin user_led_2_{"LED 2", zmq_transport_};
  mcu::HostPin user_button_1_{"Button 1", zmq_transport_};
  mcu::HostUart uart_1_{"UART 1", zmq_transport_};
  mcu::HostI2CController i2c_1_{"I2C 1", zmq_transport_};
};
}  // namespace board
