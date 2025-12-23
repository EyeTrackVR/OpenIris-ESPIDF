set(
  requires
  Helpers
)

if ("$ENV{IDF_TARGET}" STREQUAL "esp32s3")
list(APPEND requires
            driver 
            esp_adc
)
endif()

set(
  source_files
  ""
)

if ("$ENV{IDF_TARGET}" STREQUAL "esp32s3")
list(APPEND source_files
            "Monitoring/AdcSampler.cpp"
            "Monitoring/BatteryMonitor.cpp"
            "Monitoring/CurrentMonitor_esp32s3.cpp"
            "Monitoring/MonitoringManager_esp32s3.cpp"
)
else()
list(APPEND source_files
            "Monitoring/CurrentMonitor_esp32.cpp"
            "Monitoring/MonitoringManager_esp32.cpp"
)
endif()


idf_component_register(SRCS ${source_files}
  INCLUDE_DIRS "Monitoring"
  REQUIRES ${requires}
)