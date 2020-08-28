#pragma once

#include "logger.hpp"
#include <filesystem>
#include <pqrs/dispatcher.hpp>
#include <pqrs/karabiner/driverkit/virtual_hid_device_driver.hpp>
#include <pqrs/karabiner/driverkit/virtual_hid_device_service.hpp>
#include <pqrs/local_datagram.hpp>

class virtual_hid_device_service_server final : public pqrs::dispatcher::extra::dispatcher_client {
public:
  virtual_hid_device_service_server(void) : dispatcher_client(),
                                            ready_timer_(*this) {
    //
    // Preparation
    //

    remove_server_socket_file();
    create_rootonly_directory();

    //
    // Creation
    //

    create_io_service_client();
    create_server();

    logger::get_logger()->info("virtual_hid_device_service_server is initialized");
  }

  virtual ~virtual_hid_device_service_server(void) {
    detach_from_dispatcher([this] {
      ready_timer_.stop();

      server_ = nullptr;
      io_service_client_ = nullptr;
    });

    logger::get_logger()->info("virtual_hid_device_service_server is terminated");
  }

private:
  void create_rootonly_directory(void) const {
    std::error_code error_code;
    std::filesystem::create_directories(
        pqrs::karabiner::driverkit::virtual_hid_device_service::constants::rootonly_directory,
        error_code);
    if (error_code) {
      logger::get_logger()->error(
          "virtual_hid_device_service_server::{0} create_directories error: {1}",
          __func__,
          error_code.message());
      return;
    }

    std::filesystem::permissions(
        pqrs::karabiner::driverkit::virtual_hid_device_service::constants::rootonly_directory,
        std::filesystem::perms::owner_all,
        error_code);
    if (error_code) {
      logger::get_logger()->error(
          "virtual_hid_device_service_server::{0} permissions error: {1}",
          __func__,
          error_code.message());
      return;
    }
  }

  void remove_server_socket_file(void) const {
    std::filesystem::path path(pqrs::karabiner::driverkit::virtual_hid_device_service::constants::server_socket_file_path);

    if (std::filesystem::exists(path)) {
      std::error_code error_code;
      std::filesystem::remove(path, error_code);

      if (error_code) {
        logger::get_logger()->error(
            "virtual_hid_device_service_server::{0} remove error: {1}",
            __func__,
            error_code.message());
        return;
      }
    }
  }

  void set_server_socket_file_permissions(void) const {
    std::error_code error_code;
    std::filesystem::permissions(
        pqrs::karabiner::driverkit::virtual_hid_device_service::constants::server_socket_file_path,
        std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
        error_code);
    if (error_code) {
      logger::get_logger()->error(
          "virtual_hid_device_service_server::{0} permissions error: {1}",
          __func__,
          error_code.message());
      return;
    }
  }

  void create_io_service_client(void) {
    io_service_client_ = std::make_unique<io_service_client>();

    io_service_client_->virtual_hid_keyboard_ready_callback.connect([this](auto&& ready) {
      if (virtual_hid_keyboard_ready_ != ready) {
        virtual_hid_keyboard_ready_ = ready;

        logger::get_logger()->info(
            "virtual_hid_device_service_server virtual_hid_keyboard_ready_ is changed: {0}",
            ready ? (*ready ? "true" : "false") : "std::nullopt");
      }
    });

    io_service_client_->virtual_hid_pointing_ready_callback.connect([this](auto&& ready) {
      if (virtual_hid_pointing_ready_ != ready) {
        virtual_hid_pointing_ready_ = ready;

        logger::get_logger()->info(
            "virtual_hid_device_service_server virtual_hid_pointing_ready_ is changed: {0}",
            ready ? (*ready ? "true" : "false") : "std::nullopt");
      }
    });

    io_service_client_->async_start();

    ready_timer_.start(
        [this] {
          if (io_service_client_) {
            io_service_client_->async_virtual_hid_keyboard_ready();
            io_service_client_->async_virtual_hid_pointing_ready();
          }
        },
        std::chrono::milliseconds(1000));
  }

  void create_server(void) {
    server_ = std::make_unique<pqrs::local_datagram::server>(
        weak_dispatcher_,
        pqrs::karabiner::driverkit::virtual_hid_device_service::constants::server_socket_file_path.data(),
        pqrs::karabiner::driverkit::virtual_hid_device_service::constants::local_datagram_buffer_size);
    server_->set_server_check_interval(std::chrono::milliseconds(3000));
    server_->set_reconnect_interval(std::chrono::milliseconds(1000));

    server_->bound.connect([this] {
      logger::get_logger()->info("virtual_hid_device_service_server: bound");

      set_server_socket_file_permissions();
    });

    server_->bind_failed.connect([](auto&& error_code) {
      logger::get_logger()->error("virtual_hid_device_service_server: bind_failed: {0}",
                                  error_code.message());
    });

    server_->closed.connect([] {
      logger::get_logger()->info("virtual_hid_device_service_server: closed");
    });

    server_->received.connect([this](auto&& buffer, auto&& sender_endpoint) {
      if (buffer) {
        if (buffer->empty()) {
          return;
        }

        auto p = &((*buffer)[0]);
        auto size = buffer->size();

        auto request = pqrs::karabiner::driverkit::virtual_hid_device_service::request(*p);
        --size;

        switch (request) {
          case pqrs::karabiner::driverkit::virtual_hid_device_service::request::none:
            break;

          case pqrs::karabiner::driverkit::virtual_hid_device_service::request::virtual_hid_keyboard_initialize:
            if (io_service_client_) {
              if (sizeof(pqrs::hid::country_code::value_t) != size) {
                logger::get_logger()->warn("virtual_hid_device_service_server: received: virtual_hid_keyboard_initialize buffer size error");
                return;
              }

              io_service_client_->async_virtual_hid_keyboard_initialize(
                  *(reinterpret_cast<pqrs::hid::country_code::value_t*>(p)));
            }
            break;

          case pqrs::karabiner::driverkit::virtual_hid_device_service::request::virtual_hid_keyboard_terminate:
            if (io_service_client_) {
              io_service_client_->async_virtual_hid_keyboard_terminate();
            }
            break;

          case pqrs::karabiner::driverkit::virtual_hid_device_service::request::virtual_hid_keyboard_ready:
            async_send_ready_result(
                pqrs::karabiner::driverkit::virtual_hid_device_service::response::virtual_hid_keyboard_ready_result,
                virtual_hid_keyboard_ready_,
                sender_endpoint);
            break;

          case pqrs::karabiner::driverkit::virtual_hid_device_service::request::virtual_hid_keyboard_reset:
            if (io_service_client_) {
              io_service_client_->async_virtual_hid_keyboard_reset();
            }
            break;

          case pqrs::karabiner::driverkit::virtual_hid_device_service::request::virtual_hid_pointing_initialize:
            if (io_service_client_) {
              io_service_client_->async_virtual_hid_pointing_initialize();
            }
            break;

          case pqrs::karabiner::driverkit::virtual_hid_device_service::request::virtual_hid_pointing_terminate:
            if (io_service_client_) {
              io_service_client_->async_virtual_hid_pointing_terminate();
            }
            break;

          case pqrs::karabiner::driverkit::virtual_hid_device_service::request::virtual_hid_pointing_ready:
            async_send_ready_result(
                pqrs::karabiner::driverkit::virtual_hid_device_service::response::virtual_hid_pointing_ready_result,
                virtual_hid_pointing_ready_,
                sender_endpoint);
            break;

          case pqrs::karabiner::driverkit::virtual_hid_device_service::request::virtual_hid_pointing_reset:
            if (io_service_client_) {
              io_service_client_->async_virtual_hid_pointing_reset();
            }
            break;

          case pqrs::karabiner::driverkit::virtual_hid_device_service::request::post_keyboard_input_report:
            async_post_report<pqrs::karabiner::driverkit::virtual_hid_device_driver::hid_report::keyboard_input>(p, size);
            break;

          case pqrs::karabiner::driverkit::virtual_hid_device_service::request::post_consumer_input_report:
            async_post_report<pqrs::karabiner::driverkit::virtual_hid_device_driver::hid_report::consumer_input>(p, size);
            break;

          case pqrs::karabiner::driverkit::virtual_hid_device_service::request::post_apple_vendor_keyboard_input_report:
            async_post_report<pqrs::karabiner::driverkit::virtual_hid_device_driver::hid_report::apple_vendor_keyboard_input>(p, size);
            break;

          case pqrs::karabiner::driverkit::virtual_hid_device_service::request::post_apple_vendor_top_case_input_report:
            async_post_report<pqrs::karabiner::driverkit::virtual_hid_device_driver::hid_report::apple_vendor_top_case_input>(p, size);
            break;

          case pqrs::karabiner::driverkit::virtual_hid_device_service::request::post_pointing_input_report:
            async_post_report<pqrs::karabiner::driverkit::virtual_hid_device_driver::hid_report::pointing_input>(p, size);
            break;
        }
      }
    });

    server_->async_start();
  }

  // This method is executed in the dispatcher thread.
  void async_send_ready_result(pqrs::karabiner::driverkit::virtual_hid_device_service::response response,
                               std::optional<bool> ready,
                               std::shared_ptr<asio::local::datagram_protocol::endpoint> endpoint) {
    if (server_) {
      if (ready) {
        uint8_t buffer[] = {
            static_cast<std::underlying_type<pqrs::karabiner::driverkit::virtual_hid_device_service::response>::type>(response),
            *ready,
        };

        server_->async_send(buffer, sizeof(buffer), endpoint);
      }
    }
  }

  // This method is executed in the dispatcher thread.
  template <typename T>
  void async_post_report(const uint8_t* buffer, size_t buffer_size) {
    if (io_service_client_) {
      if (sizeof(T) != buffer_size) {
        logger::get_logger()->warn("virtual_hid_device_service_server: post_report buffer size error");
        return;
      }

      io_service_client_->async_post_report(*(reinterpret_cast<const T*>(buffer)));
    }
  }

  std::unique_ptr<io_service_client> io_service_client_;
  std::unique_ptr<pqrs::local_datagram::server> server_;
  std::optional<bool> virtual_hid_keyboard_ready_;
  std::optional<bool> virtual_hid_pointing_ready_;
  pqrs::dispatcher::extra::timer ready_timer_;
};