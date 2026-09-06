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

  user_led_1_ = std::make_unique<mcu::HostPin>("LED 1", *zmq_transport_,
                                               mcu::PinDirection::kOutput);
  user_led_2_ = std::make_unique<mcu::HostPin>("LED 2", *zmq_transport_,
                                               mcu::PinDirection::kOutput);
  user_button_1_ = std::make_unique<mcu::HostPin>("Button 1", *zmq_transport_,
                                                  mcu::PinDirection::kInput);
  uart_1_ = std::make_unique<mcu::HostUart>("UART 1", *zmq_transport_);
  i2c_1_ = std::make_unique<mcu::HostI2CController>("I2C 1", *zmq_transport_);

  receiver_map_ = mcu::ReceiverMap{
      std::ref(*user_led_1_), std::ref(*user_led_2_), std::ref(*user_button_1_),
      std::ref(*uart_1_),     std::ref(*i2c_1_),
  };

  // No Configure() chain: the pins were given their direction above. What is
  // left here is what genuinely cannot happen at construction -- the transport
  // has to connect first, and that can fail, which a constructor could not
  // report. That is why this board builds its peripherals in Init() while
  // NucleoF767ZiBoard holds them as members: an emulated peripheral depends on
  // a socket, a real one only on registers that are always there.
  return {};
}
auto HostBoard::UserLed1() -> mcu::OutputPin& { return *user_led_1_; }
auto HostBoard::UserLed2() -> mcu::OutputPin& { return *user_led_2_; }
auto HostBoard::UserButton1() -> mcu::InputPin& { return *user_button_1_; }
auto HostBoard::I2C1() -> mcu::I2CController& { return *i2c_1_; }
auto HostBoard::Uart1() -> mcu::Uart& { return *uart_1_; }
}  // namespace board
