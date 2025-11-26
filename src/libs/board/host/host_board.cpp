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
  // Step 1: Create the dispatcher with an empty receiver map initially
  // We'll build the actual receiver map after creating components
  dispatcher_.emplace(receiver_map_);

  // Step 2: Create the transport with the dispatcher using configured endpoints
  auto transport_result{mcu::ZmqTransport::Create(
      endpoints_.to_emulator, endpoints_.from_emulator, *dispatcher_)};
  if (!transport_result) {
    return std::unexpected(transport_result.error());
  }
  zmq_transport_ = std::move(transport_result.value());

  // Step 3: Create all components with the transport
  user_led_1_ = std::make_unique<mcu::HostPin>("LED 1", *zmq_transport_);
  user_led_2_ = std::make_unique<mcu::HostPin>("LED 2", *zmq_transport_);
  user_button_1_ = std::make_unique<mcu::HostPin>("Button 1", *zmq_transport_);
  uart_1_ = std::make_unique<mcu::HostUart>("UART 1", *zmq_transport_);
  i2c_1_ = std::make_unique<mcu::HostI2CController>("I2C 1", *zmq_transport_);

  // Step 4: Now build the receiver map with all components
  receiver_map_ = mcu::ReceiverMap{
      {IsJson, std::ref(*user_led_1_)},    {IsJson, std::ref(*user_led_2_)},
      {IsJson, std::ref(*user_button_1_)}, {IsJson, std::ref(*uart_1_)},
      {IsJson, std::ref(*i2c_1_)},
  };

  // Step 5: Recreate the dispatcher with the actual receiver map
  dispatcher_.emplace(receiver_map_);

  // Step 6: Configure pins
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
