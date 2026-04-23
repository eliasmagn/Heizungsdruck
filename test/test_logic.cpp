#include <unity.h>

#include "modules/AppConfig.h"
#include "modules/JsonCodec.h"
#include "modules/PressureMath.h"
#include "modules/PressureStateMachine.h"

void test_adc_to_bar_linear() {
  AppConfig cfg = defaultConfig();
  PressureMath math(cfg);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.0f, math.adcToBar(cfg.calib.adcLow));
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 10.0f, math.adcToBar(cfg.calib.adcHigh));
}

void test_filter_rejects_spike() {
  AppConfig cfg = defaultConfig();
  PressureMath math(cfg);
  std::vector<int> samples{1000, 1001, 999, 3000, 1002, 998, 1001};
  int filtered = math.robustFilter(samples);
  TEST_ASSERT_INT_WITHIN(20, 1000, filtered);
}

void test_state_hysteresis() {
  AppConfig cfg = defaultConfig();
  cfg.alarm.lowBar = 1.0f;
  cfg.alarm.highBar = 2.0f;
  cfg.alarm.hysteresisBar = 0.1f;
  PressureStateMachine sm(cfg);

  PressureReading r;
  r.valid = true;
  r.fault = SensorFault::NONE;

  r.pressureBar = 0.8f;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(PressureState::LOW), static_cast<int>(sm.update(r)));
  r.pressureBar = 1.05f;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(PressureState::LOW), static_cast<int>(sm.update(r)));
  r.pressureBar = 1.2f;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(PressureState::OK), static_cast<int>(sm.update(r)));
}

void test_config_validation() {
  AppConfig cfg = defaultConfig();
  std::string err;
  TEST_ASSERT_TRUE(cfg.validate(err));
  cfg.alarm.lowBar = 2.0f;
  cfg.alarm.highBar = 1.0f;
  TEST_ASSERT_FALSE(cfg.validate(err));
}

void test_json_roundtrip() {
  AppConfig cfg = defaultConfig();
  cfg.alarm.lowBar = 0.9f;
  std::string json = configToJson(cfg);
  AppConfig out = defaultConfig();
  std::string err;
  TEST_ASSERT_TRUE(configFromJson(json, out, err));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.9f, out.alarm.lowBar);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_adc_to_bar_linear);
  RUN_TEST(test_filter_rejects_spike);
  RUN_TEST(test_state_hysteresis);
  RUN_TEST(test_config_validation);
  RUN_TEST(test_json_roundtrip);
  return UNITY_END();
}
