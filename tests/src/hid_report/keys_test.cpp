#include <catch2/catch.hpp>

#include <pqrs/karabiner/driverkit/virtual_hid_device.hpp>


#if 0
TEST_CASE("keys") {
  {
    pqrs::karabiner_virtual_hid_device::hid_report::keys keys;
    uint8_t expected[32];

    REQUIRE(keys.count() == 0);
    REQUIRE(keys.empty());
    memset(expected, 0, sizeof(expected));
    REQUIRE(memcmp(keys.get_raw_value(), expected, sizeof(expected)) == 0);

    keys.insert(10);
    REQUIRE(keys.count() == 1);
    REQUIRE(!keys.empty());
    REQUIRE(keys.exists(10));
    REQUIRE(!keys.exists(20));
    expected[0] = 10;
    REQUIRE(memcmp(keys.get_raw_value(), expected, sizeof(expected)) == 0);

    keys.insert(10);
    REQUIRE(keys.count() == 1);
    REQUIRE(!keys.empty());
    REQUIRE(memcmp(keys.get_raw_value(), expected, sizeof(expected)) == 0);

    keys.erase(20);
    REQUIRE(keys.count() == 1);
    REQUIRE(!keys.empty());
    REQUIRE(memcmp(keys.get_raw_value(), expected, sizeof(expected)) == 0);

    keys.erase(10);
    REQUIRE(keys.count() == 0);
    REQUIRE(keys.empty());
    expected[0] = 0;
    REQUIRE(memcmp(keys.get_raw_value(), expected, sizeof(expected)) == 0);

    keys.erase(10);
    REQUIRE(keys.count() == 0);
    REQUIRE(keys.empty());
    REQUIRE(memcmp(keys.get_raw_value(), expected, sizeof(expected)) == 0);

    keys.insert(10);
    REQUIRE(keys.count() == 1);
    REQUIRE(!keys.empty());
    expected[0] = 10;
    REQUIRE(memcmp(keys.get_raw_value(), expected, sizeof(expected)) == 0);

    keys.insert(20);
    REQUIRE(keys.count() == 2);
    REQUIRE(!keys.empty());
    expected[1] = 20;
    REQUIRE(memcmp(keys.get_raw_value(), expected, sizeof(expected)) == 0);

    keys.clear();
    REQUIRE(keys.count() == 0);
    REQUIRE(keys.empty());
    expected[0] = 0;
    expected[1] = 0;
    REQUIRE(memcmp(keys.get_raw_value(), expected, sizeof(expected)) == 0);
  }

  {
    // Overflow

    pqrs::karabiner_virtual_hid_device::hid_report::keys keys;
    REQUIRE(keys.count() == 0);

    for (int i = 0; i < 32; ++i) {
      keys.insert(i + 1);
      REQUIRE(keys.count() == (i + 1));
    }

    keys.insert(10);
    REQUIRE(keys.count() == 32);

    keys.insert(20);
    REQUIRE(keys.count() == 32);
  }
}

TEST_CASE("buttons") {
  {
    pqrs::karabiner_virtual_hid_device::hid_report::buttons buttons;
    REQUIRE(buttons.get_raw_value() == 0);
    REQUIRE(buttons.empty());

    buttons.insert(1);
    REQUIRE(buttons.get_raw_value() == 0x1);
    REQUIRE(!buttons.empty());

    buttons.insert(32);
    REQUIRE(buttons.get_raw_value() == 0x80000001);
    REQUIRE(!buttons.empty());

    buttons.insert(0);
    REQUIRE(buttons.get_raw_value() == 0x80000001);
    REQUIRE(!buttons.empty());

    buttons.insert(33);
    REQUIRE(buttons.get_raw_value() == 0x80000001);
    REQUIRE(!buttons.empty());

    buttons.erase(1);
    REQUIRE(buttons.get_raw_value() == 0x80000000);
    REQUIRE(!buttons.empty());

    buttons.clear();
    REQUIRE(buttons.get_raw_value() == 0);
    REQUIRE(buttons.empty());
  }
}
#endif
