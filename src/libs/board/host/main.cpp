#include <array>
#include <expected>
#include <iostream>
#include <zmq.hpp>
// #include <zmqpp/zmqpp.hpp>

#include "app.hpp"
#include "host_board.hpp"


auto main() -> int {

  zmq::context_t ctx;
  zmq::socket_t sock(ctx, zmq::socket_type::pair);
  sock.connect("ipc:///tmp/device_emulator.ipc");
  sock.send(zmq::str_buffer("Hello, world"), zmq::send_flags::dontwait);
  zmq::socket_ref sref = sock;

  board::HostBoard board{sref};
  if (!app::app_main(board)) {
    std::cout << "app_main failed" << std::endl;
    return 1;
  }


  zmq::message_t msg;
  auto res = sock.recv(msg, zmq::recv_flags::none);
  if (res) {
    std::cout << "Received message: " << msg.to_string_view() << std::endl;
  }
  // zmqpp::context context;
  // zmqpp::socket puller(context, zmqpp::socket_type::pull);
  // puller.bind("inproc://test");
  // zmqpp::socket pusher(context, zmqpp::socket_type::push);
  // pusher.connect("inproc://test");
  // pusher.send("hello world!");
  // wait_for_socket(puller);
  // std::string message;
  // puller.receive(message);

  /*
  mcu::HostPin<0> pin0;
  pin0.set_direction(mcu::PinDirection::output);
  pin0.set_high();
  mcu::HostI2C i2c;
  auto i2c_status = i2c.SendData(0x01, {0x01, 0x02, 0x03});
  auto i2c_data = i2c.ReceiveData(0x01);
  (void)i2c_data;
  if (i2c_status.error()) {
    // std::cout << "i2c write error" << std::endl;
  }
  // std::cout << "i2c_data: " << static_cast<int>(i2c_data.value()[0])
            // << static_cast<int>(i2c_data.value()[1]) << std::endl;
  */
  return 0;
}
