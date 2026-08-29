#include "host_board.hpp"

#include <expected>
#include <utility>

#include "libs/common/error.hpp"
#include "libs/mcu/i2c.hpp"
#include "libs/mcu/pin.hpp"
#include "libs/mcu/uart.hpp"

namespace board {
HostBoard::HostBoard(Endpoints endpoints) : endpoints_(std::move(endpoints)) {}

auto HostBoard::Init() -> std::expected<void, common::Error> {
  // Ordering: the transport needs the dispatcher (a member, already built),
  // the components need the transport, and the receiver map needs the
  // components. The dispatcher sees the filled map through its reference.
  auto transport_result{mcu::ZmqTransport::Create(
      endpoints_.to_emulator, endpoints_.from_emulator, dispatcher_)};
  if (!transport_result) {
    return std::unexpected(transport_result.error());
  }
  zmq_transport_ = std::move(transport_result.value());

  user_led_1_ = std::make_unique<mcu::HostPin>("LED 1", *zmq_transport_);
  user_led_2_ = std::make_unique<mcu::HostPin>("LED 2", *zmq_transport_);
  user_button_1_ = std::make_unique<mcu::HostPin>("Button 1", *zmq_transport_);
  uart_1_ = std::make_unique<mcu::HostUart>("UART 1", *zmq_transport_);
  i2c_1_ = std::make_unique<mcu::HostI2CController>("I2C 1", *zmq_transport_);

  receiver_map_ = mcu::ReceiverMap{
      std::ref(*user_led_1_), std::ref(*user_led_2_), std::ref(*user_button_1_),
      std::ref(*uart_1_),     std::ref(*i2c_1_),
  };

  return user_led_1_->Configure(mcu::PinDirection::kOutput)
      .and_then([this]() {
        return user_led_2_->Configure(mcu::PinDirection::kOutput);
      })
      .and_then([this]() {
        return user_button_1_->Configure(mcu::PinDirection::kInput);
      });
}
auto HostBoard::UserLed1() -> mcu::OutputPin& { return *user_led_1_; }
auto HostBoard::UserLed2() -> mcu::OutputPin& { return *user_led_2_; }
auto HostBoard::UserButton1() -> mcu::InputPin& { return *user_button_1_; }
auto HostBoard::I2C1() -> mcu::I2CController& { return *i2c_1_; }
auto HostBoard::Uart1() -> mcu::Uart& { return *uart_1_; }
}  // namespace board
