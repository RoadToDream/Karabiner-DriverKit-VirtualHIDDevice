#pragma once

#include <IOKit/IOKitLib.h>
#include <array>
#include <nod/nod.hpp>
#include <optional>
#include <os/log.h>
#include <pqrs/dispatcher.hpp>
#include <pqrs/hid.hpp>
#include <pqrs/karabiner/driverkit/virtual_hid_device.hpp>
#include <pqrs/osx/iokit_return.hpp>
#include <pqrs/osx/iokit_service_monitor.hpp>

class io_service_client final : public pqrs::dispatcher::extra::dispatcher_client {
public:
  // Signals (invoked from the shared dispatcher thread)

  nod::signal<void(std::optional<bool>)> virtual_hid_keyboard_ready_callback;
  nod::signal<void(std::optional<bool>)> virtual_hid_pointing_ready_callback;

  // Methods

  io_service_client(void) : dispatcher_client() {
  }

  ~io_service_client(void) {
    detach_from_dispatcher([this] {
      close_connection();

      service_monitor_ = nullptr;
    });
  }

  void async_start(void) {
    enqueue_to_dispatcher([this] {
      if (auto matching_dictionary = IOServiceNameMatching("org_pqrs_Karabiner_DriverKit_VirtualHIDDeviceRoot")) {
        service_monitor_ = std::make_unique<pqrs::osx::iokit_service_monitor>(weak_dispatcher_,
                                                                              matching_dictionary);

        service_monitor_->service_matched.connect([this](auto&& registry_entry_id, auto&& service_ptr) {
          close_connection();

          // Use the last matched service.
          open_connection(service_ptr);
        });

        service_monitor_->service_terminated.connect([this](auto&& registry_entry_id) {
          close_connection();

          // Use the next service
          service_monitor_->async_invoke_service_matched();
        });

        service_monitor_->async_start();

        CFRelease(matching_dictionary);
      }
    });
  }

  void async_virtual_hid_keyboard_initialize(pqrs::hid::country_code::value_t country_code) const {
    enqueue_to_dispatcher([this, country_code] {
      std::array<uint64_t, 1> input = {type_safe::get(country_code)};

      call_scalar_method(pqrs::karabiner::driverkit::virtual_hid_device::user_client_method::virtual_hid_keyboard_initialize,
                         input.data(),
                         input.size());
    });
  }

  void async_virtual_hid_keyboard_terminate(void) const {
    enqueue_to_dispatcher([this] {
      call(pqrs::karabiner::driverkit::virtual_hid_device::user_client_method::virtual_hid_keyboard_terminate);
    });
  }

  void async_virtual_hid_keyboard_ready(void) const {
    enqueue_to_dispatcher([this] {
      auto ready = call_ready(pqrs::karabiner::driverkit::virtual_hid_device::user_client_method::virtual_hid_keyboard_ready);

      enqueue_to_dispatcher([this, ready] {
        virtual_hid_keyboard_ready_callback(ready);
      });
    });
  }

  void async_virtual_hid_keyboard_reset(void) const {
    enqueue_to_dispatcher([this] {
      call(pqrs::karabiner::driverkit::virtual_hid_device::user_client_method::virtual_hid_keyboard_reset);
    });
  }

  void async_virtual_hid_pointing_initialize(void) const {
    enqueue_to_dispatcher([this] {
      call(pqrs::karabiner::driverkit::virtual_hid_device::user_client_method::virtual_hid_pointing_initialize);
    });
  }

  void async_virtual_hid_pointing_terminate(void) const {
    enqueue_to_dispatcher([this] {
      call(pqrs::karabiner::driverkit::virtual_hid_device::user_client_method::virtual_hid_pointing_terminate);
    });
  }

  void async_virtual_hid_pointing_ready(void) const {
    enqueue_to_dispatcher([this] {
      auto ready = call_ready(pqrs::karabiner::driverkit::virtual_hid_device::user_client_method::virtual_hid_pointing_ready);

      enqueue_to_dispatcher([this, ready] {
        virtual_hid_pointing_ready_callback(ready);
      });
    });
  }

  void async_virtual_hid_pointing_reset(void) const {
    enqueue_to_dispatcher([this] {
      call(pqrs::karabiner::driverkit::virtual_hid_device::user_client_method::virtual_hid_pointing_reset);
    });
  }

  void async_post_report(const pqrs::karabiner::driverkit::virtual_hid_device::hid_report::keyboard_input& report) const {
    enqueue_to_dispatcher([this, report] {
      post_report(
          pqrs::karabiner::driverkit::virtual_hid_device::user_client_method::virtual_hid_keyboard_post_report,
          &report,
          sizeof(report));
    });
  }

  void async_post_report(const pqrs::karabiner::driverkit::virtual_hid_device::hid_report::consumer_input& report) const {
    enqueue_to_dispatcher([this, report] {
      post_report(
          pqrs::karabiner::driverkit::virtual_hid_device::user_client_method::virtual_hid_keyboard_post_report,
          &report,
          sizeof(report));
    });
  }

  void async_post_report(const pqrs::karabiner::driverkit::virtual_hid_device::hid_report::apple_vendor_keyboard_input& report) const {
    enqueue_to_dispatcher([this, report] {
      post_report(
          pqrs::karabiner::driverkit::virtual_hid_device::user_client_method::virtual_hid_keyboard_post_report,
          &report,
          sizeof(report));
    });
  }

  void async_post_report(const pqrs::karabiner::driverkit::virtual_hid_device::hid_report::apple_vendor_top_case_input& report) const {
    enqueue_to_dispatcher([this, report] {
      post_report(
          pqrs::karabiner::driverkit::virtual_hid_device::user_client_method::virtual_hid_keyboard_post_report,
          &report,
          sizeof(report));
    });
  }

  void async_post_report(const pqrs::karabiner::driverkit::virtual_hid_device::hid_report::pointing_input& report) const {
    enqueue_to_dispatcher([this, report] {
      post_report(
          pqrs::karabiner::driverkit::virtual_hid_device::user_client_method::virtual_hid_pointing_post_report,
          &report,
          sizeof(report));
    });
  }

private:
  // This method is executed in the dispatcher thread.
  void open_connection(pqrs::osx::iokit_object_ptr s) {
    if (!connection_) {
      service_ = s;

      io_connect_t c;
      pqrs::osx::iokit_return r = IOServiceOpen(*service_, mach_task_self(), 0, &c);

      if (r) {
        connection_ = pqrs::osx::iokit_object_ptr(c);
      } else {
        os_log_error(OS_LOG_DEFAULT, "IOServiceOpen error: %{public}s", r.to_string().c_str());
        connection_.reset();
      }
    }
  }

  // This method is executed in the dispatcher thread.
  void close_connection(void) {
    if (connection_) {
      IOServiceClose(*connection_);
      connection_.reset();
    }

    service_.reset();
  }

  // This method is executed in the dispatcher thread.
  pqrs::osx::iokit_return call(pqrs::karabiner::driverkit::virtual_hid_device::user_client_method user_client_method) const {
    if (!connection_) {
      return kIOReturnNotOpen;
    }

    return IOConnectCallStructMethod(*connection_,
                                     static_cast<uint32_t>(user_client_method),
                                     nullptr,
                                     0,
                                     nullptr,
                                     0);
  }

  // This method is executed in the dispatcher thread.
  pqrs::osx::iokit_return call_scalar_method(pqrs::karabiner::driverkit::virtual_hid_device::user_client_method user_client_method,
                                             const uint64_t* input,
                                             uint32_t input_count) const {
    if (!connection_) {
      return kIOReturnNotOpen;
    }

    return IOConnectCallScalarMethod(*connection_,
                                     static_cast<uint32_t>(user_client_method),
                                     input,
                                     input_count,
                                     nullptr,
                                     0);
  }

  // This method is executed in the dispatcher thread.
  std::optional<bool> call_ready(pqrs::karabiner::driverkit::virtual_hid_device::user_client_method user_client_method) const {
    if (!connection_) {
      return std::nullopt;
    }

    uint64_t output[1] = {0};
    uint32_t output_count = 1;
    auto kr = IOConnectCallScalarMethod(*connection_,
                                        static_cast<uint32_t>(user_client_method),
                                        nullptr,
                                        0,
                                        output,
                                        &output_count);

    if (kr != kIOReturnSuccess) {
      return std::nullopt;
    }

    return static_cast<bool>(output[0]);
  }

  // This method is executed in the dispatcher thread.
  pqrs::osx::iokit_return post_report(pqrs::karabiner::driverkit::virtual_hid_device::user_client_method user_client_method,
                                      const void* report,
                                      size_t report_size) const {
    if (!connection_) {
      return kIOReturnNotOpen;
    }

    return IOConnectCallStructMethod(*connection_,
                                     static_cast<uint32_t>(user_client_method),
                                     report,
                                     report_size,
                                     nullptr,
                                     0);
  }

  std::unique_ptr<pqrs::osx::iokit_service_monitor> service_monitor_;
  pqrs::osx::iokit_object_ptr service_;
  pqrs::osx::iokit_object_ptr connection_;
};
