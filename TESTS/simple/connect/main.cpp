
#include "mbed.h"
#include "FATFileSystem.h"
#include "simple-mbed-cloud-client.h"

#include "utest/utest.h"
#include "unity/unity.h"
#include "greentea-client/test_env.h"

#if defined(MBED_CONF_APP_TEST_CONNECT_HEADER_FILE)
#include MBED_CONF_APP_TEST_CONNECT_HEADER_FILE
#else
#include "EthernetInterface.h"
#endif

#if defined(MBED_CONF_APP_TEST_BLOCK_DEVICE_HEADER_FILE)
#include MBED_CONF_APP_TEST_BLOCK_DEVICE_HEADER_FILE
#else
#include "SDBlockDevice.h"
#endif


using namespace utest::v1;

static const ConnectorClientEndpointInfo* endpointInfo;
void registered(const ConnectorClientEndpointInfo *endpoint) {
    printf("Connected to Mbed Cloud. Device ID: %s\n", endpoint->internal_endpoint_name.c_str());
    endpointInfo = endpoint;
}

void smcc_register(void){

	int timeout = 0;
	char _key[20] = {};
	char _value[128] = {};

	// Storage definition.
#if defined(MBED_CONF_APP_TEST_BLOCK_DEVICE_OBJECT)
	MBED_CONF_APP_TEST_BLOCK_DEVICE_OBJECT
#else
	SDBlockDevice bd(MBED_CONF_APP_SPI_MOSI, MBED_CONF_APP_SPI_MISO, MBED_CONF_APP_SPI_CLK, MBED_CONF_APP_SPI_CS);
#endif
	FATFileSystem fs ("sd", &bd);

	// Connection definition.
#if defined(MBED_CONF_APP_TEST_SOCKET_OBJECT)
	MBED_CONF_APP_TEST_SOCKET_OBJECT
#else
	EthernetInterface net;
#endif

#if defined(MBED_CONF_APP_TEST_SOCKET_CONNECT)
	nsapi_error_t status = MBED_CONF_APP_TEST_SOCKET_CONNECT
#else
	nsapi_error_t status = net.connect();
#endif

	// Must have IP address.
	TEST_ASSERT_NOT_EQUAL(net.get_ip_address(), NULL);
	if (net.get_ip_address() == NULL) {
		printf("ERROR: No IP address obtained from network.");
	}
	// Connection must be successful.
	TEST_ASSERT_EQUAL(status, 0);
	if (status == 0 && net.get_ip_address() != NULL) {
		printf("Connected to network successfully. IP address: %s\n", net.get_ip_address());
	}

	SimpleMbedCloudClient client(&net, &bd, &fs);

	// SimpleMbedCloudClient initialization must be successful.
	int client_status = client.init();
	TEST_ASSERT_EQUAL(client_status, 0);
	if (client_status == 0) {
		printf("Simple Mbed Cloud Client initialization successful. \r\n");
	}

	// Registration to Mbed Cloud must be successful.
	client.on_registered(&registered);
	client.register_and_connect();

	timeout = 5000;
	while (timeout && !client.is_client_registered()) {
		timeout--;
		wait_ms(1);
	}

	TEST_ASSERT_TRUE(client.is_client_registered());
	if (client.is_client_registered()) {
		printf("Simple Mbed Cloud Client successfully registered to Mbed Cloud.\r\n");
	}

	// Allow 500ms for Mbed Cloud to update the device directory.
    timeout = 500;
    while (timeout) {
    	timeout--;
    	wait_ms(1);
    }

    // Start host tests with device id
    printf("Starting Mbed Cloud verification using Python SDK...\r\n");
    greentea_send_kv("device_api_registration", endpointInfo->internal_endpoint_name.c_str());

    // Wait for Host Test and API response (blocking here)
    greentea_parse_kv(_key, _value, sizeof(_key), sizeof(_value));

    // Ensure the state is 'registered' in the Device Directory
    TEST_ASSERT_EQUAL_STRING_MESSAGE("registered", _value, "Device is registered.");

    // Deregister from Mbed Cloud
    client.client_unregistered();
    client.close();

	// Close connection
	net.disconnect();
}

utest::v1::status_t greentea_setup(const size_t number_of_cases)
{

    GREENTEA_SETUP(30*60, "sdk_host_tests");
    return greentea_test_setup_handler(number_of_cases);
}

Case cases[] = {
    Case("Simple Cloud Client Register", smcc_register),
};

Specification specification(greentea_setup, cases);

int main()
{
    return !Harness::run(specification);
}
