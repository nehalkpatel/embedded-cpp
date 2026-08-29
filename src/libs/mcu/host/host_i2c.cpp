#include "host_i2c.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

#include "libs/common/error.hpp"
#include "libs/mcu/host/emulator_message_json_encoder.hpp"
#include "libs/mcu/host/host_emulator_messages.hpp"
#include "libs/mcu/i2c.hpp"

namespace mcu {

auto HostI2CController::SendData(uint16_t address,
                                 std::span<const std::byte> data)
    -> std::expected<void, common::Error> {
  const I2CEmulatorRequest request{
      .name = name_,
      .operation = OperationType::kSend,
      .address = address,
      .data = std::vector<std::byte>(data.begin(), data.end()),
  };
  return Transact<I2CEmulatorResponse>(transport_, request)
      .transform([](const I2CEmulatorResponse&) {});
}

auto HostI2CController::ReceiveData(uint16_t address,
                                    std::span<std::byte> buffer)
    -> std::expected<size_t, common::Error> {
  const I2CEmulatorRequest request{
      .name = name_,
      .operation = OperationType::kReceive,
      .address = address,
      .data = {},
      .size = buffer.size(),
  };
  return Transact<I2CEmulatorResponse>(transport_, request)
      .transform([buffer](const I2CEmulatorResponse& response) {
        const size_t bytes_to_copy{
            std::min(response.data.size(), buffer.size())};
        std::copy_n(response.data.begin(), bytes_to_copy, buffer.begin());
        return bytes_to_copy;
      });
}

auto HostI2CController::Receive(std::string_view message)
    -> std::expected<std::string, common::Error> {
  static_cast<void>(message);
  return std::unexpected(common::Error::kUnhandled);
}

}  // namespace mcu
