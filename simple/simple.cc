#include <uhd/utils/thread.hpp>
#include <uhd/utils/safe_main.hpp>
#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/exception.hpp>
#include <uhd/types/tune_request.hpp>
#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include <boost/thread.hpp>
#include <iostream>

// UHD_SAFE_MAIN is a catch-all wrapper of main. Why in this syntax??
int UHD_SAFE_MAIN(int argc, char *argv[]) {
	uhd::set_thread_priority_safe();
		// Sets "scheduling priority" ??
		// Documentation says this is identical to "set_thread_priority" 
		// but doesn't throw in case of failure
	

	std::string device_args("addr=192.168.40.2"); 
	uhd::usrp::multi_usrp::sptr usrp = uhd::usrp::multi_usrp::make(device_args);
	std::cout << std::endl;
    std::cout << boost::format("Made Multi USRP with IP: %s...") % device_args << std::endl;
		// Set IP address of USRP in format required to "make device"
		// Can find this from uhd_find_devices in terminal


	std::string ref("internal");
	std::cout << boost::format("Source of motherboard clock: %f...") % ref << std::endl;
    usrp->set_clock_source(ref);
		// This sets the source for a reference clock that is default 10 MHz.
		// Can be set to "internal", "external", and multiple-in-multiple-out "MIMO".
	
	// Intereresting documentation about the clock: In the x310, the Digital
	// Down-Converter and Up-Converter (used in DAC and ADC) run on the same
	// clock as the "header passing packets." This allows timed commands to 
	// be executed with extreme precision. However, this also limits the
	// amount of commands that can be stored in the command queue. If the
	// device is not streaming the data simultaneously, adding commands can 
	// shut down the device. Managing the DDC and DUC queue will be vital to
	// continuous receiving and transmitting of signals.


	std::string subdev("A:0");
	usrp->set_rx_subdev_spec(subdev);
	std::cout << boost::format("Using Device: %s") % usrp->get_pp_string() << std::endl;
		// Format for setting the sub-device: the daughterboard inside of the radio
		// which communicates with the antenna. The format is 
			// <motherboard slot>:<daughterboard name>
		// Here, we are using the RF A motherboard slot (where the antennas are plugged
		// in) and setting the daughterboard frontend name to 0. You can check possible
		// daughterboard frontend names for specific boards by probing device configuration
		// with uhd_usrp_probe. The last line prints the device configuration.


	double rate(1e6); // The Rx sampling rate in Samples/second.
	if (rate <= 0.0) {
        std::cerr << "Please specify a valid sample rate" << std::endl;
        return ~0;
		// Ensures a positive sampling rate is selected.
    }
	usrp->set_rx_rate(rate);
	std::cout << boost::format("Set Rx rate to: %f MS/s...") % (rate / 1e6) << std::endl;
	std::cout << boost::format("measured Rx rate: %f MS/s...") % (usrp->get_rx_rate() / 1e6) << std::endl << std::endl;
		// Sets the frequency that the received signal is sampled/stored at.
		// Allows this to be checked with the true sampling rate.
	
	
	double freq(915e6); // The Rx center frequency in Samples/second.
	uhd::tune_request_t tune_request(freq); // Tells daughterboard to tune to specific center freq.
    usrp->set_rx_freq(tune_request);
	std::cout << boost::format("Set Rx center freq to: %f MHz...") % (freq / 1e6) << std::endl;
	std::cout << boost::format("Measured Rx center freq: %f MHz...") % (usrp->get_rx_freq() / 1e6) << std::endl << std::endl;
	

	double gain(10); // Gain in dB for RF signal.
	usrp->set_rx_gain(gain);
	std::cout << boost::format("Set Rx Gain to: %f dB...") % gain << std::endl;
	std::cout << boost::format("Measured Rx Gain to: %f dB...") % usrp->get_rx_gain() << std::endl << std::endl;

	
	double bw(1e6); // Frontend bandwidth for IF signal. 
	usrp->set_rx_bandwidth(bw);
	std::cout << boost::format("Set Rx bandwidth to: %f MHz...") % (bw / 1e6) << std::endl;
	std::cout << boost::format("Measured Rx bandwidth: %f MHz...") % (usrp->get_rx_bandwidth() / 1e6) << std::endl << std::endl;


	std::string ant("RX2"); // The two antennae on A are "TX/RX" and "RX2"
	usrp->set_rx_antenna(ant);
	std::cout << boost::format("Set Rx antenna to: %s") % ant << std::endl;
	std::cout << boost::format("Detected Rx antenna: %s") % usrp->get_rx_antenna() << std::endl << std::endl;


	return EXIT_SUCCESS;
}
