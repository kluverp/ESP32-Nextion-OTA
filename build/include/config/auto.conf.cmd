deps_config := \
	/home/peter/esp/esp-idf-v3.2/components/app_trace/Kconfig \
	/home/peter/esp/esp-idf-v3.2/components/aws_iot/Kconfig \
	/home/peter/esp/esp-idf-v3.2/components/bt/Kconfig \
	/home/peter/esp/esp-idf-v3.2/components/driver/Kconfig \
	/home/peter/esp/esp-idf-v3.2/components/esp32/Kconfig \
	/home/peter/esp/esp-idf-v3.2/components/esp_adc_cal/Kconfig \
	/home/peter/esp/esp-idf-v3.2/components/esp_event/Kconfig \
	/home/peter/esp/esp-idf-v3.2/components/esp_http_client/Kconfig \
	/home/peter/esp/esp-idf-v3.2/components/esp_http_server/Kconfig \
	/home/peter/esp/esp-idf-v3.2/components/ethernet/Kconfig \
	/home/peter/esp/esp-idf-v3.2/components/fatfs/Kconfig \
	/home/peter/esp/esp-idf-v3.2/components/freemodbus/Kconfig \
	/home/peter/esp/esp-idf-v3.2/components/freertos/Kconfig \
	/home/peter/esp/esp-idf-v3.2/components/heap/Kconfig \
	/home/peter/esp/esp-idf-v3.2/components/libsodium/Kconfig \
	/home/peter/esp/esp-idf-v3.2/components/log/Kconfig \
	/home/peter/esp/esp-idf-v3.2/components/lwip/Kconfig \
	/home/peter/esp/esp-idf-v3.2/components/mbedtls/Kconfig \
	/home/peter/esp/esp-idf-v3.2/components/mdns/Kconfig \
	/home/peter/esp/esp-idf-v3.2/components/mqtt/Kconfig \
	/home/peter/esp/esp-idf-v3.2/components/nvs_flash/Kconfig \
	/home/peter/esp/esp-idf-v3.2/components/openssl/Kconfig \
	/home/peter/esp/esp-idf-v3.2/components/pthread/Kconfig \
	/home/peter/esp/esp-idf-v3.2/components/spi_flash/Kconfig \
	/home/peter/esp/esp-idf-v3.2/components/spiffs/Kconfig \
	/home/peter/esp/esp-idf-v3.2/components/tcpip_adapter/Kconfig \
	/home/peter/esp/esp-idf-v3.2/components/vfs/Kconfig \
	/home/peter/esp/esp-idf-v3.2/components/wear_levelling/Kconfig \
	/home/peter/esp/esp-idf-v3.2/components/bootloader/Kconfig.projbuild \
	/home/peter/esp/esp-idf-v3.2/components/esptool_py/Kconfig.projbuild \
	/home/peter/esp/ESP32-Nextion-OTA/main/Kconfig.projbuild \
	/home/peter/esp/esp-idf-v3.2/components/partition_table/Kconfig.projbuild \
	/home/peter/esp/esp-idf-v3.2/Kconfig

include/config/auto.conf: \
	$(deps_config)

ifneq "$(IDF_CMAKE)" "n"
include/config/auto.conf: FORCE
endif

$(deps_config): ;
